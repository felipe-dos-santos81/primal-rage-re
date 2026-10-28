#!/usr/bin/env python3
"""The capture manifest and the gate-hardening checks (plan
2026-10-27-claim-coverage, Task 3).

Two independent instruments, both stdlib-only so the capture harness and
`gate-check` can run them without the diff tooling:

1. A per-capture manifest (`tools/capture_manifest.json`) pinning each of the
   35 `CAPTURE_DIRS` identities. The 30 k11 gameplay/menu captures are pinned by
   `poll.log` sha256, the frame count, the `.dro` inventory and (where the
   poll.log carries S records) the first/last S frame; the five frame-dump
   captures (`data/smk-captures/{twi5,twg}`, `data/title-captures/{title,title2,
   frontend}`) are pinned by frame count plus a content hash over the sorted
   frame files (name + sha256). A declared absence (`gp-u14-win-live`, U14 R1)
   is recorded with `declared: true` and a reason and is SKIPPED by `--check` —
   the captures are git-ignored, so a checkout without them still passes. A
   declared gap that reappears is stale and fails (the same rule as
   `gate-check`).

2. A pin-baseline tripwire (`tools/pin_baseline.sha256`): the sha256 over the
   marker-delimited pin regions of `Makefile`, `tools/audio_compare.py`,
   `tools/k11_compare.py`, `tools/gp_compare.py` and `tools/title_compare.py`.
   `--check-pins` recomputes the digest and fails on a mismatch, so a silent
   `MAX_MISSING`/`MIN_FIRST` change is caught; `--update-pins` re-pins it and
   requires a measured `--reason`, which is stored in the baseline header.

The preflight is split: `--preflight-verify` checks the verify preconditions
(`data/game/C/PRAGE.EXE`, `data/game/C/INDEX`,
`port/tests/ghidra_data.bin`, the 2040-zero CMOS rule; capstone/unicorn stay
skip-if-absent, the documented `diff-verify` behaviour), while
`--preflight-capture` (called only by `gp_capture.py`/`gp-capture`) checks
`dosbox-x`, `ffmpeg`, a writable capture root, the guest `memsize` against the
host's free memory, and a load average above the host's cores. A failed
preflight writes no capture.

Usage:
  capture_manifest.py --generate                     write tools/capture_manifest.json
  capture_manifest.py --check                        validate the manifest against the captures
  capture_manifest.py --check-pins [--baseline F]    recompute the pin digest and compare
  capture_manifest.py --update-pins --reason "..." [--baseline F]
  capture_manifest.py --preflight-verify
  capture_manifest.py --preflight-capture [--memsize N]
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST_PATH = os.path.join(REPO_ROOT, 'tools', 'capture_manifest.json')
PIN_BASELINE_PATH = os.path.join(REPO_ROOT, 'tools', 'pin_baseline.sha256')

GAME_DIR = os.path.join(REPO_ROOT, 'data', 'game', 'C')
GHIDRA_DATA = os.path.join(REPO_ROOT, 'port', 'tests', 'ghidra_data.bin')
CAPTURE_ROOT = os.path.join(REPO_ROOT, 'data', 'k11-captures')

CMOS_BYTES = 2040

# The files whose marker-delimited regions the pin baseline hashes, in order.
PIN_BASELINE_FILES = (
    'Makefile',
    'tools/audio_compare.py',
    'tools/k11_compare.py',
    'tools/gp_compare.py',
    'tools/title_compare.py',
)

# The one declared gap in CAPTURE_DIRS (U14 R1, record §F2): a present path is
# stale, an absent one is skipped. Everything else must be present.
DECLARED_GAPS = {
    'data/k11-captures/gp-u14-win-live':
        'U14 R1 fought win not authorized (design approval); the one attempt is '
        'preserved as gp-u14-win-live.failed (record 2026-10-26-fought-match-truth-derivations §F2)',
}

FRAME_RE = re.compile(r'^frame_.*\.(raw|raw\.gz)$')
BEGIN_RE = re.compile(r'^#\s*>>>\s+PIN-BASELINE-BEGIN(?:\s+(.*?))?\s*$')
END_RE = re.compile(r'^#\s*<<<\s+PIN-BASELINE-END\b')


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 16), b''):
            h.update(chunk)
    return h.hexdigest()


# ── CAPTURE_DIRS (read from the Makefile, the authoritative list) ─────────────

MAKEFILE_VARS = {
    'K11_CAPTURES': 'data/k11-captures',
    'SMK_CAPTURES': 'data/smk-captures',
    'TITLE_CAPTURES': 'data/title-captures',
}


def read_capture_dirs(makefile='Makefile'):
    """The Makefile's CAPTURE_DIRS entries (relative paths), continuation lines
    joined and the three root variables expanded."""
    with open(makefile) as f:
        lines = f.read().splitlines()
    dirs = []
    for i, line in enumerate(lines):
        if not line.startswith('CAPTURE_DIRS'):
            continue
        rest = line.split('=', 1)[1].strip()
        if rest.endswith('\\'):
            rest = rest[:-1].strip()
        if rest:
            dirs.append(rest)
        j = i + 1
        while j < len(lines):
            l = lines[j].strip()
            if l.endswith('\\'):
                l = l[:-1].strip()
                if l:
                    dirs.append(l)
                j += 1
            else:
                if l:
                    dirs.append(l)
                break
        break
    def expand(entry):
        return re.sub(r'\$\(([A-Za-z_][A-Za-z0-9_]*)\)',
                      lambda m: MAKEFILE_VARS.get(m.group(1), m.group(0)), entry)
    return [expand(d) for d in dirs]


def kind_for(path):
    if path.startswith('data/smk-captures/') or path.startswith('data/title-captures/'):
        return 'frame-dump'
    return 'k11'


# ── identities ────────────────────────────────────────────────────────────────

def frame_count(dirpath):
    return sum(1 for n in os.listdir(dirpath) if FRAME_RE.match(n))


def dro_inventory(dirpath):
    return sorted(n for n in os.listdir(dirpath) if n.lower().endswith('.dro'))


def poll_log_s_frames(dirpath):
    """(first, last) of the poll.log's S-record `f` fields (hex per gp_session
    format_s), or (None, None) when there are none (a k11 service capture)."""
    path = os.path.join(dirpath, 'poll.log')
    if not os.path.isfile(path):
        return None, None
    first = last = None
    with open(path) as f:
        for line in f:
            if not line.startswith('S '):
                continue
            m = re.search(r'\bf=([0-9A-Fa-f]+)\b', line)
            if m:
                fv = int(m.group(1), 16)
                if first is None:
                    first = fv
                last = fv
    return first, last


def frame_dump_content_hash(dirpath):
    """The content hash over the sorted frame files (name + sha256), pinning both
    the file set and each frame's bytes."""
    frames = sorted(n for n in os.listdir(dirpath) if FRAME_RE.match(n))
    h = hashlib.sha256()
    for n in frames:
        h.update(n.encode('utf-8'))
        h.update(b' ')
        h.update(sha256_file(os.path.join(dirpath, n)).encode('ascii'))
        h.update(b'\n')
    return h.hexdigest()


def compute_identity(rel_path, root):
    kind = kind_for(rel_path)
    abs_path = os.path.join(root, rel_path)
    if not os.path.isdir(abs_path):
        return {'path': rel_path, 'kind': kind, 'declared': True,
                'reason': DECLARED_GAPS.get(rel_path, 'undeclared absence')}
    if kind == 'frame-dump':
        return {'path': rel_path, 'kind': kind,
                'frame_count': frame_count(abs_path),
                'content_hash': frame_dump_content_hash(abs_path)}
    entry = {'path': rel_path, 'kind': kind,
             'poll_log_sha256': sha256_file(os.path.join(abs_path, 'poll.log')),
             'frame_count': frame_count(abs_path),
             'dro': dro_inventory(abs_path)}
    first, last = poll_log_s_frames(abs_path)
    if first is not None:
        entry['first_s_frame'] = first
        entry['last_s_frame'] = last
    return entry


def generate_manifest(capture_dirs, root):
    return {'version': 1,
            'captures': [compute_identity(d, root) for d in capture_dirs]}


# ── manifest check ────────────────────────────────────────────────────────────

def check_k11(entry, dirpath):
    poll = os.path.join(dirpath, 'poll.log')
    if not os.path.isfile(poll):
        return 'poll.log missing'
    if sha256_file(poll) != entry['poll_log_sha256']:
        return 'poll.log sha256 mismatch'
    if frame_count(dirpath) != entry['frame_count']:
        return 'frame count %d != %d' % (frame_count(dirpath), entry['frame_count'])
    if dro_inventory(dirpath) != list(entry['dro']):
        return '.dro inventory %r != %r' % (dro_inventory(dirpath), list(entry['dro']))
    first, last = poll_log_s_frames(dirpath)
    if first != entry.get('first_s_frame') or last != entry.get('last_s_frame'):
        return 'first/last S frame %r/%r != %r/%r' % (
            first, last, entry.get('first_s_frame'), entry.get('last_s_frame'))
    return None


def check_frame_dump(entry, dirpath):
    if frame_count(dirpath) != entry['frame_count']:
        return 'frame count %d != %d' % (frame_count(dirpath), entry['frame_count'])
    if frame_dump_content_hash(dirpath) != entry['content_hash']:
        return 'content hash mismatch'
    return None


def check_entry(entry, dirpath):
    if entry['kind'] == 'frame-dump':
        return check_frame_dump(entry, dirpath)
    return check_k11(entry, dirpath)


def check_manifest(manifest, root, capture_dirs):
    """Validate the manifest against the captures. Returns (ok, problems,
    present_checked, declared_skipped)."""
    problems = []
    by_path = {e['path']: e for e in manifest['captures']}
    expected, actual = set(capture_dirs), set(by_path)
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if missing:
        problems.append('manifest is missing %d CAPTURE_DIRS entries: %s'
                        % (len(missing), ', '.join(missing)))
    if extra:
        problems.append('manifest has %d entries not in CAPTURE_DIRS: %s'
                        % (len(extra), ', '.join(extra)))
    present_checked = declared_skipped = 0
    for rel in capture_dirs:
        entry = by_path.get(rel)
        if entry is None:
            problems.append('%s: not in manifest' % rel)
            continue
        abs_path = os.path.join(root, rel)
        if entry.get('declared'):
            if os.path.isdir(abs_path):
                problems.append('%s: declared gap is present (stale)' % rel)
            else:
                declared_skipped += 1
            continue
        if not os.path.isdir(abs_path):
            problems.append('%s: capture absent but not declared' % rel)
            continue
        p = check_entry(entry, abs_path)
        if p:
            problems.append('%s: %s' % (rel, p))
        else:
            present_checked += 1
    return (not problems), problems, present_checked, declared_skipped


# ── pin baseline ──────────────────────────────────────────────────────────────

def pin_regions(files):
    """[(path, name, body)] of the marker-delimited regions, in file order and
    line order. Nested BEGIN and stray END markers are errors."""
    regions = []
    for path in files:
        with open(path) as f:
            lines = f.read().splitlines()
        start = None
        name = None
        for lineno, line in enumerate(lines, 1):
            if BEGIN_RE.match(line):
                if start is not None:
                    raise SystemExit('nested PIN-BASELINE-BEGIN in %s at line %d' % (path, lineno))
                start = lineno
                m = BEGIN_RE.match(line)
                name = (m.group(1) or 'line-%d' % lineno).strip()
            elif END_RE.match(line):
                if start is None:
                    raise SystemExit('stray PIN-BASELINE-END in %s at line %d' % (path, lineno))
                body = '\n'.join(lines[start:lineno - 1])
                regions.append((path, name, body))
                start = name = None
        if start is not None:
            raise SystemExit('unterminated PIN-BASELINE-BEGIN in %s at line %d' % (path, start))
    return regions


def compute_pin_digest(files):
    h = hashlib.sha256()
    for path, name, body in pin_regions(files):
        h.update(('region %s:%s\n' % (path, name)).encode('utf-8'))
        h.update(body.encode('utf-8'))
        h.update(b'\n')
    return h.hexdigest()


def read_baseline(path):
    """(digest, reason) from a baseline file, or (None, None)."""
    if not os.path.isfile(path):
        return None, None
    digest = reason = None
    with open(path) as f:
        for line in f:
            s = line.strip()
            if s.startswith('# reason:'):
                reason = s[len('# reason:'):].strip()
            elif s and not s.startswith('#'):
                digest = s
    return digest, reason


def write_baseline(path, digest, reason):
    with open(path, 'w') as f:
        f.write('# Pin-baseline tripwire. Regenerate ONLY with a measured reason:\n')
        f.write('#   python3 tools/capture_manifest.py --update-pins --reason "..."\n')
        f.write('# reason: %s\n' % reason)
        f.write('%s\n' % digest)


def check_pins(baseline_path, files):
    problems = []
    digest = compute_pin_digest(files)
    stored, reason = read_baseline(baseline_path)
    if stored is None:
        problems.append('pin baseline missing: %s' % baseline_path)
    elif digest != stored:
        problems.append('pin baseline mismatch (stored %s, computed %s); '
                        're-pin with --update-pins --reason' % (stored, digest))
    return (not problems), problems, digest


# ── preflight ─────────────────────────────────────────────────────────────────

def cmos_check(game_dir):
    for name in os.listdir(game_dir):
        if name.upper() != 'CMOS':
            continue
        with open(os.path.join(game_dir, name), 'rb') as f:
            data = f.read()
        if len(data) != CMOS_BYTES or any(data):
            return 'CMOS is %d bytes (must be %d zero bytes or absent)' % (len(data), CMOS_BYTES)
        return None
    return None


def preflight_verify(game_dir=GAME_DIR, ghidra_data=GHIDRA_DATA):
    problems = []
    for rel in ('PRAGE.EXE', 'INDEX'):
        path = os.path.join(game_dir, rel)
        if not os.path.isfile(path):
            problems.append('%s missing: %s' % (rel, path))
    if not os.path.isfile(ghidra_data):
        problems.append('ghidra_data.bin missing: %s' % ghidra_data)
    cmos = cmos_check(game_dir)
    if cmos:
        problems.append(cmos)
    return (not problems), problems


def available_memory():
    try:
        with open('/proc/meminfo') as f:
            for line in f:
                if line.startswith('MemAvailable:'):
                    return int(line.split()[1]) * 1024
    except OSError:
        pass
    return None


def preflight_capture(capture_root, memsize=None, load_avg=None, ncores=None,
                      avail=None, which=shutil.which):
    problems = []
    for tool in ('dosbox-x', 'ffmpeg'):
        if not which(tool):
            problems.append('required tool not on PATH: %s' % tool)
    if not os.path.isdir(capture_root) or not os.access(capture_root, os.W_OK):
        problems.append('capture root not writable: %s' % capture_root)
    if memsize is not None:
        a = available_memory() if avail is None else avail
        if a is not None and a < memsize * 1024 * 1024:
            problems.append('host free memory %d MB < guest memsize %d MB'
                            % (a // (1024 * 1024), memsize))
    la = (os.getloadavg()[0] if hasattr(os, 'getloadavg') else None) if load_avg is None else load_avg
    nc = (os.cpu_count() or 1) if ncores is None else ncores
    if la is not None and nc and la > nc:
        problems.append('load average %.2f above the host %d cores' % (la, nc))
    return (not problems), problems


# ── CLI ───────────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--generate', action='store_true')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--check-pins', action='store_true')
    ap.add_argument('--update-pins', action='store_true')
    ap.add_argument('--reason')
    ap.add_argument('--baseline', default=PIN_BASELINE_PATH)
    ap.add_argument('--manifest', default=MANIFEST_PATH)
    ap.add_argument('--preflight-verify', action='store_true')
    ap.add_argument('--preflight-capture', action='store_true')
    ap.add_argument('--memsize', type=int)
    a = ap.parse_args()

    if a.generate:
        dirs = read_capture_dirs(os.path.join(REPO_ROOT, 'Makefile'))
        manifest = generate_manifest(dirs, REPO_ROOT)
        with open(a.manifest, 'w') as f:
            json.dump(manifest, f, indent=2)
            f.write('\n')
        print('capture_manifest: wrote %d captures to %s' % (len(manifest['captures']), a.manifest))
        return 0

    if a.check:
        dirs = read_capture_dirs(os.path.join(REPO_ROOT, 'Makefile'))
        with open(a.manifest) as f:
            manifest = json.load(f)
        ok, problems, present, skipped = check_manifest(manifest, REPO_ROOT, dirs)
        for p in problems:
            print('capture_manifest: FAIL: %s' % p)
        print('capture_manifest: %d present checked, %d declared skipped, %d problems'
              % (present, skipped, len(problems)))
        return 0 if ok else 1

    if a.check_pins:
        ok, problems, digest = check_pins(a.baseline, PIN_BASELINE_FILES)
        for p in problems:
            print('capture_manifest: FAIL: %s' % p)
        print('capture_manifest: pin baseline %s (digest %s)' % ('ok' if ok else 'MISMATCH', digest))
        return 0 if ok else 1

    if a.update_pins:
        if not a.reason:
            sys.exit('capture_manifest: --update-pins requires --reason "..." (the measured reason)')
        digest = compute_pin_digest(PIN_BASELINE_FILES)
        write_baseline(a.baseline, digest, a.reason)
        print('capture_manifest: wrote pin baseline %s (digest %s, reason: %s)'
              % (a.baseline, digest, a.reason))
        return 0

    if a.preflight_verify:
        ok, problems = preflight_verify()
        for p in problems:
            print('capture_manifest: FAIL: %s' % p)
        print('capture_manifest: verify preflight %s' % ('ok' if ok else 'FAILED'))
        return 0 if ok else 1

    if a.preflight_capture:
        ok, problems = preflight_capture(CAPTURE_ROOT, memsize=a.memsize)
        for p in problems:
            print('capture_manifest: FAIL: %s' % p)
        print('capture_manifest: capture preflight %s' % ('ok' if ok else 'FAILED'))
        return 0 if ok else 1

    ap.print_help()
    return 2


if __name__ == '__main__':
    sys.exit(main())

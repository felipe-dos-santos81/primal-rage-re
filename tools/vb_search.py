#!/usr/bin/env python3
"""The mode-0x21 route search runner (plan 2026-11-01-volleyball-truth Task 2).

Reads a gp port-script v2 (tools/gp_session.py port-script), inserts a
parameter grid of frame-keyed key/bits steps, replays every variant through the
GP driver (`./build/run_tests`, PR_GP_DUMP) with PR_GP_FAST=1 (no 60 Hz sleep)
and PR_GP_VOLLEY=1 (the driver's V/Vg observer), and ranks the traces by the
trigger's own state:

  stage   afc == 0        DS_00104AFC, the 0x4B559 gate's stage word
  join    b1f == 3        DS_00104B1F, the two-human match
  lie     the first V line with ty == 4 and +0x1C bit 7 (the case-3 lying body;
          a fall has bit 7 too, so the type filter names the lie)
  count   the max V line +0x1F (the 0x4B579 `> 7` gate's count)
  mode    any T line with mode == 0x21 (the 0x4BD98 entry)

The table's afc_l/b1f_l are the LAST T line's values; the state line records
the first fight-mode (mode 6) T line with afc == 0 (afc0@) and the first T line
with b1f == 3 (b1f3@), so a trace that runs past the vs is not misread. The
rank uses those ever-reached flags.

Grid syntax (repeat --grid for a Cartesian product):

  --grid NAME=VALUE[,VALUE...]
  VALUE = STEP[;STEP...]
  STEP  = FRAME:KIND:VALUE
  KIND  = key  -> BIOS word, `key FRAME SCAN ASCII` (the Enter word is
                  scan 1C, ascii 0D, written 1C0D)
        | bits -> the pad key-state word, `bits FRAME KB` (e.g. 0100 = p1.b0)
  FRAME accepts decimal or 0xHEX.

Example (two cursor positions x two join frames):

  python3 tools/vb_search.py --base /tmp/join.script \
      --grid 'cursor=318:bits:0100;320:bits:0000, 338:bits:0100;340:bits:0000' \
      --grid 'join=700:bits:0001, 706:bits:0001' --jobs 8 --end 2600

The base script's first line (its `# gp port script v2: scenario NAME`) is
preserved: the driver selects the scenario's pinned fn_resolve miss set from it.
`--end` overrides the base's end. A cut (end < the base's end) rewrites the
first line to `(cut at N)`, mirroring gp_session.port_script, and drops the
base's steps past the cut (the driver allows a cut replay a subset of the
pinned miss set); a later value extends the observed window and drops a stale
`(cut at N)` marker. Without `--end` the end is max(base end, the last inserted
step), and an inserted step past the end is refused.

A `poke` overlapping the R30 trigger words DS_00104B1F or DS_00104ABC is
refused with its line number; --allow-poke admits a labelled, non-trigger
harness write only. An inserted `key` at an `arm pad` base's arm frame is
refused (the arm's bits must stay the script's first step).

Output: the ranked table and the winning script path on stdout (deterministic
for a given base and grid: variants are the Cartesian product in CLI order,
values in listed order, and the sort has no timing key; the table's script
column is the per-variant file name under the workdir, and the winner line
names its absolute path). Progress and timings go to stderr. Per-variant
scripts, driver logs and the top variants' traces are kept under a fresh /tmp
workdir (--workdir overrides; --keep-dumps keeps the frame dumps too). The
state-best failed variant keeps its trace too (--keep-failed-traces N, default
1) and its state/Vg are printed. Nothing is written into the repository and
data/ is only read by the driver; the tool is not part of `make verify`.

Exit status: 0 with a winner, 2 when no variant passed the driver's checks,
1 on a usage/parse error.
"""
import argparse
import concurrent.futures
import itertools
import os
import re
import resource
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_RUN_TESTS = os.path.join(ROOT, 'build', 'run_tests')
DEFAULT_GAME_DIR = os.path.join(ROOT, 'data', 'game', 'C')

# A caller's driver selection would make run_tests run another driver; a stale
# dump path would overwrite somebody else's dump. The search sets its own.
DRIVER_ENVS = ('PR_ATTRACT_DUMP', 'PR_TITLE_DUMP', 'PR_FRONTEND_DUMP', 'PR_FRONTEND_DET',
               'PR_K11_DUMP', 'PR_RESTART', 'PR_GP_DUMP', 'PR_GP_SCRIPT', 'PR_AUDIO_DUMP')

STEP_RE = re.compile(r'^(0x[0-9a-fA-F]+|\d+):(key|bits):([0-9a-fA-F]{1,8})$')
ORDER = {'key': 0, 'bits': 1, 'poke': 2}
# R30 (docs/superpowers/specs/2026-10-30-final-claims.md): no poke may write the
# two-human trigger's words. A base script may still carry labelled, non-trigger
# harness pokes; only these ranges are refused (--allow-poke overrides).
PROTECTED_POKES = ((0x00104B1F, 1, 'DS_00104B1F'),
                   (0x00104ABC, 4, 'DS_00104ABC'))


class ScriptError(Exception):
    pass


def read_base(path, allow_poke):
    """(head lines, [(frame, kind, line)], end, enter_frame, pad_arm) of a v2
    script. A poke overlapping an R30 trigger word is refused by line."""
    try:
        with open(path) as f:
            lines = f.read().splitlines()
    except OSError as e:
        raise ScriptError('cannot read %s: %s' % (path, e))
    head, events, end, enter_frame = [], [], None, None
    pad_arm = False
    for lineno, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line:
            continue
        parts = line.split()
        if line.startswith('#'):
            head.append(line)
        elif parts[0] == 'arm' and parts[1:2] == ['pad']:
            head.append(line)
            pad_arm = True
        elif parts[0] in ('enter_frame', 'enter_state'):
            head.append(line)
            if parts[0] == 'enter_frame':
                enter_frame = int(parts[1], 0)
        elif parts[0] in ('key', 'bits', 'poke'):
            if len(parts) < 2:
                raise ScriptError('%s:%d: bad %s line' % (path, lineno, parts[0]))
            if parts[0] == 'poke':
                if len(parts) != 4:
                    raise ScriptError('%s:%d: bad poke line' % (path, lineno))
                try:
                    addr = int(parts[2], 16)
                    data = bytes.fromhex(parts[3])
                except ValueError:
                    raise ScriptError('%s:%d: bad poke address/bytes' % (path, lineno))
                if not data or len(data) > 16:
                    raise ScriptError('%s:%d: empty or over-long poke' % (path, lineno))
                if not allow_poke:
                    for paddr, plen, pname in PROTECTED_POKES:
                        if addr < paddr + plen and paddr < addr + len(data):
                            raise ScriptError(
                                '%s:%d: poke %08X len %d overlaps the R30 trigger '
                                'word %s; pass --allow-poke for a labelled '
                                'non-trigger harness write'
                                % (path, lineno, addr, len(data), pname))
            events.append((int(parts[1], 0), parts[0], line))
        elif parts[0] == 'end':
            end = int(parts[1], 0)
        else:
            raise ScriptError('%s:%d: unknown line %r' % (path, lineno, line))
    if enter_frame is None:
        raise ScriptError('%s: no enter_frame line' % path)
    if end is None:
        raise ScriptError('%s: no end line' % path)
    return head, events, end, enter_frame, pad_arm


def parse_grid(text):
    """`NAME=SPEC[,SPEC...]` -> (NAME, [(label, [(frame, kind, word)])])."""
    name, sep, body = text.partition('=')
    name = name.strip()
    if not sep or not name:
        raise ScriptError('--grid %r is not NAME=VALUE,...' % text)
    values = []
    for raw in body.split(','):
        raw = raw.strip()
        if not raw:
            raise ScriptError('--grid %s: empty value' % name)
        steps = []
        for raw_step in raw.split(';'):
            m = STEP_RE.match(raw_step.strip())
            if m is None:
                raise ScriptError('--grid %s=%s: %r is not FRAME:key|bits:HEX'
                                  % (name, raw, raw_step))
            frame, kind, word = int(m.group(1), 0), m.group(2), int(m.group(3), 16)
            if word > 0xFFFF:
                raise ScriptError('--grid %s=%s: %s value %X is not 16-bit'
                                  % (name, raw, kind, word))
            steps.append((frame, kind, word))
        values.append((raw, steps))
    return name, values


def retitle(head, end, base_end):
    """The generator's first line, with its `(cut at N)` marker tracked
    (gp_session.port_script:1804). An equal end keeps the base's line exactly;
    a shorter end marks the cut; a longer end drops a stale marker."""
    if end == base_end or not head \
            or not head[0].startswith('# gp port script v2: scenario '):
        return head
    line = re.sub(r' \(cut at \d+\)$', '', head[0])
    if end < base_end:
        line += ' (cut at %d)' % end
    return [line] + head[1:]


def build_script(head, base_events, axes, combo, end, enter_frame, pad_arm):
    """The variant text and its (frame, kind, word) inserted steps. Base steps
    past a cut end are dropped (the replay never applies them); inserted steps
    past it are refused."""
    events = [(f, ORDER[kind], i, line) for i, (f, kind, line) in enumerate(base_events)
              if f <= end]
    inserted = []
    seq = len(base_events)
    for (name, _), (label, steps) in zip(axes, combo):
        for frame, kind, word in steps:
            if frame < enter_frame:
                raise ScriptError('%s=%s: frame %d is before the arm (%d)'
                                  % (name, label, frame, enter_frame))
            if frame > end:
                raise ScriptError('%s=%s: frame %d is past the end %d (use --end)'
                                  % (name, label, frame, end))
            if pad_arm and kind == 'key' and frame == enter_frame:
                raise ScriptError('%s=%s: a key at the pad arm frame %d would sort '
                                  'before the arm\'s first bits step'
                                  % (name, label, frame))
            if kind == 'key':
                line = 'key %d %02X %02X' % (frame, word >> 8, word & 0xFF)
            else:
                line = 'bits %d %04X' % (frame, word)
            events.append((frame, ORDER[kind], seq, line))
            inserted.append((frame, kind, word))
            seq += 1
    events.sort()
    text = '\n'.join(head + [e[3] for e in events] + ['end %d' % end]) + '\n'
    return text, inserted


def parse_trace(path):
    """The T/V/Vg state the ranking needs. A truncated or partial line is
    skipped (counted in `truncated`) and the variant is kept."""
    t = dict(frames=0, afc=0, afc0_frame=None, b1f=0, b1f3_frame=None, lie=None,
             max_n=0, max_n_frame=None, mode21=False, mode21_frame=None, v=0,
             last_vg=None, truncated=0)
    try:
        fh = open(path)
    except OSError:
        return t
    with fh:
        for line in fh:
            try:
                if line.startswith('T '):
                    d = dict(tok.split('=', 1) for tok in line.split()[1:] if '=' in tok)
                    f = int(d['f'], 16)
                    afc, b1f, mode = int(d['afc'], 16), int(d['b1f'], 16), int(d['mode'], 16)
                    t['frames'] += 1
                    t['afc'] = afc
                    if mode == 6 and afc == 0 and t['afc0_frame'] is None:
                        t['afc0_frame'] = f
                    t['b1f'] = b1f
                    if b1f == 3 and t['b1f3_frame'] is None:
                        t['b1f3_frame'] = f
                    if mode == 0x21 and not t['mode21']:
                        t['mode21'] = True
                        t['mode21_frame'] = f
                elif line.startswith('Vg '):
                    vg = dict(tok.split('=', 1) for tok in line.split()[1:] if '=' in tok)
                    int(vg['f'], 16)             # an incomplete line is skipped
                    t['last_vg'] = vg
                elif line.startswith('V '):
                    d = dict(tok.split('=', 1) for tok in line.split()[1:] if '=' in tok)
                    f = int(d['f'], 16)
                    ty, fl, n = int(d['ty'], 16), int(d['fl'], 16), int(d['n'], 16)
                    t['v'] += 1
                    if n > t['max_n']:
                        t['max_n'] = n
                        t['max_n_frame'] = f
                    if ty == 4 and fl & 0x80 and t['lie'] is None:
                        t['lie'] = f
            except (ValueError, KeyError, IndexError):
                t['truncated'] += 1
                continue
    return t


def ranked_key(r):
    """Deterministic rank: driver validity, then the brief's state order, then
    the grid index. afc/b1f use the ever-reached frames, so a trace that runs
    past the vs is not misread. No wall time, thread order or path is a key."""
    t = r['trace']
    lie = t['lie']
    return (1 if r['ok'] else 0,
            1 if t['frames'] else 0,
            1 if t['afc0_frame'] is not None else 0,
            1 if t['b1f3_frame'] is not None else 0,
            1 if lie is not None else 0,
            -lie if lie is not None else -(1 << 40),
            t['max_n'],
            1 if t['mode21'] else 0,
            -r['idx'])


def run_variant(idx, label, script, workdir, args):
    name = 'v%04d' % idx
    script_path = os.path.join(workdir, name + '.script')
    dump = os.path.join(workdir, name + '.dump')
    shutil.rmtree(dump, ignore_errors=True)
    os.makedirs(dump)
    with open(script_path, 'w') as f:
        f.write(script)
    env = {k: v for k, v in os.environ.items() if k not in DRIVER_ENVS}
    env.update(PR_GP_DUMP=dump, PR_GP_SCRIPT=script_path, PR_GAME_DIR=args.game_dir,
               PR_GP_FAST='1', PR_GP_VOLLEY='1')
    start = time.monotonic()
    timed_out = False
    try:
        proc = subprocess.run([args.run_tests], cwd=ROOT, env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=args.timeout)
        out = proc.stdout.decode('utf-8', 'replace')
        rc = proc.returncode
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b'').decode('utf-8', 'replace')
        rc, timed_out = -1, True
    wall = time.monotonic() - start
    with open(os.path.join(workdir, name + '.log'), 'w') as f:
        f.write(out)
    ok = rc == 0 and 'all checks passed' in out
    reason = next((l.strip() for l in out.splitlines() if l.strip().startswith('FAIL ')),
                  'exit %d' % rc)
    trace = parse_trace(os.path.join(dump, 'trace.txt'))
    gp_log = os.path.join(dump, 'gp.log')
    if os.path.exists(gp_log):
        shutil.move(gp_log, os.path.join(workdir, name + '.gp.log'))
    trace_path = os.path.join(dump, 'trace.txt')
    if os.path.exists(trace_path):
        shutil.move(trace_path, os.path.join(workdir, name + '.trace'))
    if not args.keep_dumps:
        shutil.rmtree(dump, ignore_errors=True)
    return dict(idx=idx, name=name, label=label, script=script_path, ok=ok,
                timed_out=timed_out, rc=rc, wall=wall, trace=trace, reason=reason)


def fmt_lie(t):
    return '-' if t['lie'] is None else '%04X' % t['lie']


def fmt_frame(f):
    return '-' if f is None else '%04X' % f


def state_line(t):
    """The ever-reached fields and the last T line's values, one line."""
    return ('afc_last=%04X afc0@%s b1f_last=%02X b1f3@%s lie=%s '
            'maxn=%d@%s mode21=%s@%s'
            % (t['afc'], fmt_frame(t['afc0_frame']), t['b1f'],
               fmt_frame(t['b1f3_frame']), fmt_lie(t), t['max_n'],
               fmt_frame(t['max_n_frame']), 'yes' if t['mode21'] else 'no',
               fmt_frame(t['mode21_frame'])))


def vg_line(t):
    vg = t['last_vg']
    return None if not vg else ' '.join('%s=%s' % (k, v) for k, v in vg.items())


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--base', required=True,
                    help='the base gp port-script v2 to insert the grid into')
    ap.add_argument('--grid', action='append', default=[], metavar='NAME=VALUE,...',
                    help='one axis: frames/steps as FRAME:key|bits:HEXVALUE; repeat for a product')
    ap.add_argument('--end', type=lambda s: int(s, 0),
                    help='override the end frame (default: max(base end, last inserted step))')
    ap.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8),
                    help='parallel replays (default: min(CPUs, 8))')
    ap.add_argument('--timeout', type=float, default=900.0,
                    help='per-variant wall limit in seconds (default 900)')
    ap.add_argument('--limit', type=int,
                    help='run only the first N variants of the product')
    ap.add_argument('--workdir',
                    help='where the per-variant scripts/logs go (default: a fresh /tmp dir)')
    ap.add_argument('--keep-dumps', action='store_true',
                    help='keep each variant\'s frame_*.ipx dump (large)')
    ap.add_argument('--keep-traces', type=int, default=3,
                    help='how many ranked variants keep their trace.txt (default 3, 0 = all)')
    ap.add_argument('--keep-failed-traces', type=int, default=1,
                    help='how many state-best failed variants keep their trace (default 1)')
    ap.add_argument('--allow-poke', action='store_true',
                    help='admit a labelled non-trigger poke; R30 trigger-word writes '
                         'are refused otherwise')
    ap.add_argument('--dry-run', action='store_true',
                    help='print the variants without running them')
    ap.add_argument('--run-tests', default=DEFAULT_RUN_TESTS,
                    help='the driver binary (default: ./build/run_tests)')
    ap.add_argument('--game-dir', default=DEFAULT_GAME_DIR,
                    help='PR_GAME_DIR for the replay (default: data/game/C)')
    args = ap.parse_args()
    if args.jobs < 1 or args.timeout <= 0 or args.keep_traces < 0 \
            or args.keep_failed_traces < 0 \
            or (args.limit is not None and args.limit < 1):
        print('vb_search: --jobs/--timeout/--keep-traces/--keep-failed-traces/'
              '--limit out of range', file=sys.stderr)
        return 1
    args.run_tests = os.path.abspath(args.run_tests)
    args.game_dir = os.path.abspath(args.game_dir)

    try:
        head, base_events, base_end, enter_frame, pad_arm = \
            read_base(args.base, args.allow_poke)
        axes = [parse_grid(g) for g in args.grid]
    except ScriptError as e:
        print('vb_search: %s' % e, file=sys.stderr)
        return 1
    axis_values = [vals for _, vals in axes]
    total = 1
    for vals in axis_values:
        total *= len(vals)
    if args.limit is None and total > 100000:
        print('vb_search: the grid has %d variants; pass --limit N' % total,
              file=sys.stderr)
        return 1
    combos = list(itertools.islice(itertools.product(*axis_values),
                                   args.limit if args.limit is not None else total))

    scripts = []
    try:
        for combo in combos:
            end = args.end if args.end is not None else max(
                [base_end] + [s[0] for _, steps in combo for s in steps])
            if end < enter_frame:
                raise ScriptError('--end %d is before the arm %d' % (end, enter_frame))
            text, inserted = build_script(retitle(head, end, base_end), base_events,
                                          axes, combo, end, enter_frame, pad_arm)
            label = ' '.join('%s=%s' % (name, vals[0]) for (name, _), vals in zip(axes, combo))
            scripts.append((label, text, inserted, end))
    except ScriptError as e:
        print('vb_search: %s' % e, file=sys.stderr)
        return 1

    if args.dry_run:
        for idx, (label, text, inserted, end) in enumerate(scripts):
            print('v%04d %s end=%d' % (idx, label or '(base)', end))
            for frame, kind, word in inserted:
                print('    %s %d %04X' % (kind, frame, word))
        return 0

    if not os.access(args.run_tests, os.X_OK):
        print('vb_search: %s is not executable (build it first)' % args.run_tests,
              file=sys.stderr)
        return 1
    workdir = os.path.abspath(args.workdir or tempfile.mkdtemp(prefix='vb-search-'))
    os.makedirs(workdir, exist_ok=True)
    print('# vb_search: base=%s variants=%d jobs=%d workdir=%s'
          % (args.base, len(scripts), args.jobs, workdir))
    print('# rank  %-36s %-4s %-5s %-5s %-5s %-4s %-4s %-5s %s'
          % ('variant', 'ok', 'afc_l', 'b1f_l', 'lie', 'maxn', 'm21', 'frames', 'script'))

    usage0 = resource.getrusage(resource.RUSAGE_CHILDREN)
    t0 = time.monotonic()
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_variant, idx, label, text, workdir, args)
                   for idx, (label, text, _, _) in enumerate(scripts)]
        for k, fut in enumerate(concurrent.futures.as_completed(futures), 1):
            r = fut.result()
            results.append(r)
            print('# %d/%d %s %s f=%d %.2fs'
                  % (k, len(futures), r['name'], 'ok' if r['ok'] else
                     ('timeout' if r['timed_out'] else 'FAIL'),
                     r['trace']['frames'], r['wall']), file=sys.stderr)
    wall = time.monotonic() - t0
    usage1 = resource.getrusage(resource.RUSAGE_CHILDREN)
    results.sort(key=ranked_key, reverse=True)

    for rank, r in enumerate(results, 1):
        t = r['trace']
        status = 'ok' if r['ok'] else ('TIME' if r['timed_out'] else 'FAIL')
        print('%5d  %-36s %-4s %04X  %02X    %-5s %3d  %-3s  %5d  %s'
              % (rank, (r['label'] or '(base)')[:36], status, t['afc'], t['b1f'],
                 fmt_lie(t), t['max_n'], 'yes' if t['mode21'] else 'no',
                 t['frames'], r['name'] + '.script'))
    for r in results:
        if not r['ok']:
            print('# fail %s: %s' % (r['name'], r['reason'][:120]))
        if r['trace']['truncated']:
            print('# truncated %s: %d line(s) skipped'
                  % (r['name'], r['trace']['truncated']))

    if args.keep_traces:
        keep = set(r['name'] for r in results[:args.keep_traces])
    else:
        keep = set(r['name'] for r in results)
    failed = [r for r in results if not r['ok']]
    if args.keep_failed_traces:
        keep |= set(r['name'] for r in failed[:args.keep_failed_traces])
    for r in results:
        if r['name'] not in keep:
            try:
                os.remove(os.path.join(workdir, r['name'] + '.trace'))
            except OSError:
                pass

    print('# wall=%.2fs children_user=%.2fs children_sys=%.2fs'
          % (wall, usage1.ru_utime - usage0.ru_utime, usage1.ru_stime - usage0.ru_stime),
          file=sys.stderr)
    if failed:
        bf = failed[0]
        print('best-failed: %s (%s)' % (bf['script'], bf['label'] or '(base)'))
        print('best-failed-state: ' + state_line(bf['trace']))
        vg = vg_line(bf['trace'])
        if vg:
            print('best-failed-vg: ' + vg)
        if bf['name'] in keep:
            print('best-failed-trace: %s.trace' % os.path.join(workdir, bf['name']))
    winners = [r for r in results if r['ok']]
    if not winners:
        print('winner: none (%d variants, none passed the driver checks)'
              % len(results))
        return 2
    winner = winners[0]
    print('winner: %s' % winner['script'])
    print('winner-state: ' + state_line(winner['trace']))
    vg = vg_line(winner['trace'])
    if vg:
        print('winner-vg: ' + vg)
    print('winner-trace: %s.trace' % os.path.join(workdir, winner['name']))
    return 0


if __name__ == '__main__':
    sys.exit(main())

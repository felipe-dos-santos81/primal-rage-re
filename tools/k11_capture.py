#!/usr/bin/env python3
"""K11 ground-truth capture (plan 2026-09-30-named-gaps-a-k11-harness.md,
record §A.3). The pinned original (make title-pin) runs in DOSBox-X. AUTOTYPE
(or, by default, the poller: --input) types the scenario's keys and
DX-CAPTURE /V /O records video and OPL. A
poller thread reads the `[dosbox] memory file` (guest RAM, memory-mapped; a
runtime linear address is the file offset) and writes poll.log (format:
k11_session.py). It also applies the scenario's pokes once mode 0x27 is seen.

Writes only data/k11-captures/<scenario>/ (guard_out), and never data/game:
the capture is assembled in a hidden .<scenario>.partial sibling and renamed
into place only once every CHECK has passed, so a failed boot leaves no
partial capture dir behind.
The staging copy lives in a temp dir and mounts read-only, so the original's
CMOS save (0x1B084) cannot write. The CMOS in the game dir must be absent or
2040 zero bytes, because the port models the defaults path (config.c
config_validate). The one scenario with `writable` (save-reload, record §J3.2)
runs two boots: boot 1 mounts the staging copy without -ro, so the save lands
in the staging CMOS (replaced from its symlink by a real copy first), which is
then copied out as `CMOS` and checked; boot 2 reuses the staging read-only and
its frames/poll.log are the capture's. A capture-side CHECK validates each
boot's poll.log through k11_session.port_script: a key the harness did not
inject fails the capture. Usage:
  k11_capture.py --scenario NAME --out data/k11-captures/NAME
                 [--time-limit S] [--enter-wait S] [--pace S] [--exe PATH]
                 [--no-pokes] [--keep-avi] [--input inject|autotype]
--input inject (the default, record §A.9) types the scenario through the memory
file; --input autotype hands the key list to DOSBox-X's AUTOTYPE instead."""
import argparse
import hashlib
import mmap
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smk_capture as sc
import title_capture as tcap
import k11_session as ks
import k11_fields as kf

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAPTURE_ROOT = os.path.join(REPO_ROOT, 'data', 'k11-captures')
DEFAULT_GAME_DIR = os.path.join(REPO_ROOT, 'data', 'game', 'C')
CMOS_BYTES = 2040
ANCHOR_VA, ANCHOR = 0x8002D, b'RAGE.S16'                    # combat-fidelity record §0.2
CHECK_VA, CHECK = 0x9AFD8, bytes.fromhex('1400040080110032')  # demo-pose record §38.1
LOG_CON_ARGS = ['-set', 'dos log console=quiet']            # record §A.2
BDA_HEAD, BDA_TAIL, BDA_START, BDA_END = 0x41A, 0x41C, 0x480, 0x482

# --input inject (record §A.9): AUTOTYPE's typing stopped mid-scenario on this
# host (walk runs 1 and 2 typed 11/38 and 1/38 keys), so the poller types the
# scenario itself through the memory file, as the keyboard path would leave
# it: the IRQ1 key-state table [DS_00101514]+0x254+scan (bit 7 = released;
# demo-pose record §50-C.3, read by 0x1B610/0x1B6A0) is pressed for HOLD_S,
# and the BIOS word is appended to the int 16h ring. The words are the ones
# AUTOTYPE's taps left in the ring (walk.run1 poll.log: 1C0D, 50E0, 011B; the
# probe runs: 48E0); left/right follow the same grey-key E0 form. HOLD_S is
# AUTOTYPE's own press length as the game saw it (walk.run1 poll.log:1538..1541,
# kb=0040 from tick 0x5F2 to 0x5F5, 3 ticks); it is a stimulus, not a game value.
KEYTAB_OFF = 0x254
HOLD_S = 0.05
BIOS_WORD = {'enter': 0x1C0D, 'esc': 0x011B, 'up': 0x48E0, 'down': 0x50E0,
             'left': 0x4BE0, 'right': 0x4DE0}

# The five storage ranges of record §J3.3 (id, linear VA, length). The file
# values the raw tables hold are va - DATA_BASE_VA (the loader's fixups add the
# data object's base): the config image [0x2D48C] = 0x80CE4 read by
# config_file_load/save (0x1B006/0x1B0CA), kind 0's counters {len 0x26, source
# 0x85DB0} (0x2D444+2/+4) and the three high-score tables {count, vb, nb} at
# 0x2D3FC[t] over the bases 0x2D478[t] = 0x85E34/0x85EAC/0x85EC8 — all read
# from PRAGE.EXE at obj-0 file offset VA + 0x52E54. The poller locates them at
# runtime as base + file value (find_base's data base).
SAVE_RANGES = (
    ('cfg', 0x100CE4, 0x7F8),
    ('k0', 0x105DB0, 0x26),
    ('hs0', 0x105E34, 0x78),
    ('hs1', 0x105EAC, 0x1C),
    ('hs2', 0x105EC8, 5),
)
RANGE_IDS = tuple(rid for rid, _, _ in SAVE_RANGES)


def guard_out(out):
    real = os.path.realpath(out)
    root = os.path.realpath(CAPTURE_ROOT)
    if not os.path.normcase(real).casefold().startswith(os.path.normcase(root).casefold() + os.sep):
        raise SystemExit('k11_capture: --out must be a directory under %s: %s' % (CAPTURE_ROOT, out))
    return real


def stage_dir(out):
    """The hidden sibling a run assembles into before its CHECKs pass (§J3.2):
    a failed boot 2 must not leave a partial `out` a later oracle reads as a
    real capture."""
    return os.path.join(os.path.dirname(out), '.%s.partial' % os.path.basename(out))


def publish_stage(stage, out):
    """Move a fully written staged capture into place. An existing capture at
    `out` is moved aside and replaced, never merged into."""
    old = out + '.old'
    if os.path.exists(old):
        shutil.rmtree(old)
    if os.path.exists(out):
        os.rename(out, old)
    os.rename(stage, out)
    if os.path.exists(old):
        shutil.rmtree(old)


def find_base(buf):
    i = buf.find(ANCHOR)
    while i != -1:
        base = i - (ANCHOR_VA - ks.DATA_BASE_VA)
        chk = base + (CHECK_VA - ks.DATA_BASE_VA)
        if base >= 0 and bytes(buf[chk:chk + len(CHECK)]) == CHECK:
            return base
        i = buf.find(ANCHOR, i + 1)
    return None


def check_cmos(game_dir):
    for name in os.listdir(game_dir):
        if name.upper() == 'CMOS':
            with open(os.path.join(game_dir, name), 'rb') as f:
                data = f.read()
            if len(data) != CMOS_BYTES or any(data):
                raise SystemExit('k11_capture: %s is not %d zero bytes; the port models '
                                 'the defaults path (record §A.0)' % (name, CMOS_BYTES))
            return 'zero'
    return 'absent'


def cmos_valid(data):
    """The save file's invariant (config.c config_file_save, 0x1B041): 2040
    bytes whose last is the XOR of the first 0x7F7."""
    if data is None or len(data) != CMOS_BYTES:
        return False
    s = 0
    for b in data[:CMOS_BYTES - 1]:
        s ^= b
    return data[CMOS_BYTES - 1] == s


def saved_cmos_check(saved, staged):
    """(ok, text) for boot 1's save (§J3.2): the file exists, is a valid CMOS
    and differs from the staged copy (a save that wrote the untouched defaults
    would leave it equal). `staged` is the starting bytes, or None when the
    game dir had none."""
    if saved is None:
        return False, 'boot 1 wrote no CMOS into the staging copy'
    if not cmos_valid(saved):
        return False, 'the staged CMOS is not %d bytes with the XOR last byte (got %d)' \
                      % (CMOS_BYTES, len(saved))
    if staged is not None and saved == staged:
        return False, 'the staged CMOS is unchanged: no save reached the file'
    return True, 'boot 1 wrote %d bytes, the XOR invariant holds and it differs from the staged copy' \
                 % len(saved)


def stage_cmos_real(game):
    """The staging CMOS must be a real file, never a symlink into data/ (§J3.2):
    replace the symlink with a real copy of its bytes. Returns 'copied' (a link
    was replaced), 'file' (already real, boot 2's saved file) or 'absent'."""
    for name in os.listdir(game):
        if name.upper() != 'CMOS':
            continue
        path = os.path.join(game, name)
        if not os.path.islink(path):
            return 'file'
        data = b''
        if os.path.exists(path):
            with open(path, 'rb') as f:
                data = f.read()
        os.remove(path)
        if not data:
            return 'absent'
        with open(path, 'wb') as f:
            f.write(data)
        return 'copied'
    return 'absent'


def snapshot_tree(d):
    """{name: 'link:<target>'|'file'|'other'} for the writable-mount check
    (§J3.8(c))."""
    out = {}
    for n in os.listdir(d):
        p = os.path.join(d, n)
        if os.path.islink(p):
            out[n] = 'link:%s' % os.readlink(p)
        elif os.path.isfile(p):
            out[n] = 'file'
        else:
            out[n] = 'other'
    return out


def snapshot_mtimes(d):
    """{name: 'file:<mtime_ns>:<size>'|'other'} of a game dir, to prove nothing
    there was written through the staging's symlinks (§J3.8(c))."""
    out = {}
    for n in os.listdir(d):
        p = os.path.join(d, n)
        if os.path.isfile(p):
            st = os.stat(p)
            out[n] = 'file:%d:%d' % (st.st_mtime_ns, st.st_size)
        else:
            out[n] = 'other'
    return out


def staging_check(staging_before, staging_after, game_before, game_after):
    """(ok, text) for §J3.8(c): after the writable boot 1, no staging entry may
    have appeared or changed besides CMOS, no symlink target may have changed,
    and the game dir's mtimes/sizes must be untouched."""
    probs = []
    for n in sorted(set(staging_before) | set(staging_after)):
        if n.upper() == 'CMOS':
            continue
        if staging_before.get(n) != staging_after.get(n):
            probs.append('staging %s: %s -> %s' % (n, staging_before.get(n), staging_after.get(n)))
    for n in sorted(set(game_before) | set(game_after)):
        if game_before.get(n) != game_after.get(n):
            probs.append('game dir %s: %s -> %s' % (n, game_before.get(n), game_after.get(n)))
    return (not probs), '; '.join(probs) or 'no staging entry changed and the game dir is untouched'


def read_ranges(mm, base):
    """[(id, at, bytes)] for the five storage ranges: at = base + (va -
    DATA_BASE_VA), the file value the raw tables hold (record §J3.3)."""
    out = []
    for rid, va, length in SAVE_RANGES:
        at = base + va - ks.DATA_BASE_VA
        out.append((rid, at, bytes(mm[at:at + length])))
    return out


def ranges_logged(recs):
    """(ok, text): every storage range has at least one R record (§J3.3)."""
    rs = [r for r in recs if r['kind'] == 'R']
    missing = [rid for rid in RANGE_IDS if not any(r['id'] == rid for r in rs)]
    if missing:
        return False, 'no R record for %s' % ', '.join(missing)
    return True, '%d R records, every range logged' % len(rs)


def ranges_settled(recs):
    """(ok, text): ranges_logged and no range changes after the Enter (§J3.3:
    a move invalidates the single pinned frame, J3.8(b))."""
    ok, text = ranges_logged(recs)
    if not ok:
        return ok, text
    k, _ = ks.enter_record(recs)
    if k is None:
        return False, 'no K record: the Enter was never seen'
    late = [r for r in recs if r['kind'] == 'R' and r['ms'] > k['ms']]
    if late:
        r = late[0]
        return False, '%d R record(s) after the Enter (first: ms=%d id=%s)' % (len(late), r['ms'], r['id'])
    return True, 'no range changes after the Enter (ms=%d)' % k['ms']


def poll_checks(scenario, boot, lines, pokes):
    """The poll.log CHECKs for one boot: the four originals plus the script
    (k11_session.port_script, so a key the harness did not inject fails) and
    the storage ranges (boot 2: settled, §J3.3). Returns (checks, recs)."""
    if not lines:
        return [('poll.log', False)], []
    recs = [r for r in (ks.parse(x) for x in lines) if r]
    keys = [k for k in ks.keys_for(scenario, boot) if k != ',']
    kcount = sum(1 for x in recs if x['kind'] == 'K')
    first_k = next((x['ms'] for x in recs if x['kind'] == 'K'), None)
    saw27 = first_k is not None and any(x['kind'] == 'P' and x['ms'] >= first_k and x['mode'] == 0x27
                                        for x in recs)
    checks = [('base', any(x['kind'] == 'B' for x in recs)),
              ('keys %d/%d' % (kcount, len(keys)), kcount == len(keys)),
              ('mode 0x27 after the Enter', saw27),
              ('pokes', not pokes or any(x['kind'] == 'W' for x in recs))]
    try:
        ks.port_script(scenario, lines, boot)
        checks.append(('script', True))
    except ks.ScriptError as e:
        print('k11_capture: script: %s' % e)
        checks.append(('script', False))
    ok, why = ranges_settled(recs) if boot == 2 else ranges_logged(recs)
    if not ok:
        print('k11_capture: ranges: %s' % why)
    checks.append(('ranges', ok))
    return checks, recs


def print_checks(checks):
    for name, ok in checks:
        print('k11_capture: CHECK %s: %s' % (name, 'ok' if ok else 'FAIL'))


def dosbox_cmd(root, game, iso, scenario, time_limit, enter_wait, pace, autotype=True,
               boot=1, ro=True):
    """The DOSBox-X command line. ro=False is the save-reload boot-1 mount, the
    one writable run (§J3.2); every other scenario stays read-only. boot
    selects an AUTOTYPE key list (save-reload's boot2)."""
    mount = 'MOUNT C "%s" -ro' % game if ro else 'MOUNT C "%s"' % game
    return ([sc.which('dosbox-x'), '-defaultconf', '-fastlaunch', '-nopromptfolder',
             '-nogui', '-nomenu', '-time-limit', str(time_limit),
             '-set', 'sdl fullscreen=false',
             '-set', 'dosbox captures=%s' % os.path.join(root, 'avi'),
             '-set', 'dosbox memory file=%s' % os.path.join(root, 'guest.mem'),
             '-set', 'log logfile=%s' % os.path.join(root, 'dosbox.log')]
            + LOG_CON_ARGS
            + ['-c', mount,
               '-c', 'IMGMOUNT D "%s" -t iso' % iso,
               '-c', 'C:',
               ] + (['-c', ks.autotype_line(scenario, enter_wait, pace, boot)] if autotype else []) + [
               '-c', 'DX-CAPTURE /V /O PRAGE.EXE -f',
               '-c', 'EXIT'])


def u16(buf, off):
    return buf[off] | buf[off + 1] << 8


def bios_new_keys(mem, prev_tail, tail, start, end):
    out, t = [], prev_tail
    while t != tail and len(out) < 16:
        out.append(u16(mem, 0x400 + t))
        t += 2
        if t >= end:
            t = start
    return out


def schedule(keys, enter_wait, pace):
    """AUTOTYPE's timing: the first key after WAIT, then one PACE per list item
    (',' is an item that types nothing). [(seconds, key)]."""
    out, t = [], enter_wait
    for k in keys:
        if k != ',':
            out.append((t, k))
        t += pace
    return out


def bios_insert(mem, word):
    """Append `word` to the BIOS keyboard ring as int 9 would; False when full."""
    head, tail = u16(mem, BDA_HEAD), u16(mem, BDA_TAIL)
    start, end = u16(mem, BDA_START) or 0x1E, u16(mem, BDA_END) or 0x3E
    nxt = tail + 2
    if nxt >= end:
        nxt = start
    if nxt == head:
        return False
    mem[0x400 + tail:0x402 + tail] = word.to_bytes(2, 'little')
    mem[BDA_TAIL:BDA_TAIL + 2] = nxt.to_bytes(2, 'little')
    return True


def field_pokes(window, descs, pokes):
    img = bytearray(window)
    for _, fid, value in pokes:
        kf.set_(img, descs, fid, value)
    return [(i, window[i], img[i]) for i in range(len(img)) if img[i] != window[i]]


def read_vals(mm, base):
    vals = {}
    for name, ds, size in ks.POLL_FIELDS:
        off = base + ds - ks.DATA_BASE_VA
        vals[name] = int.from_bytes(mm[off:off + size], 'little')
    return vals


def read_kb(mm, base):
    ptr = int.from_bytes(mm[base + ks.KB_PTR_DS - ks.DATA_BASE_VA:][:4], 'little')
    if ptr == 0 or ptr + 0x2DA > len(mm):
        return 0
    return mm[ptr + 0x2D8] << 8 | mm[ptr + 0x2D9]


class Poller(threading.Thread):
    def __init__(self, mem_path, log_path, pokes, descs, stop, typing=()):
        super().__init__(daemon=True)
        self.mem_path, self.log_path, self.pokes, self.descs, self.stop = \
            mem_path, log_path, pokes, descs, stop
        self.typing = list(typing)      # [(seconds, key)] for --input inject
        self.held = []                  # [(release_at, linear, old)]

    def type_keys(self, mm, base, now, ms, log):
        for release_at, lin, old in [h for h in self.held if now >= h[0]]:
            mm[lin] = old | 0x80
            self.held.remove((release_at, lin, old))
            log.write('I ms=%d up=%08X tab=%02X\n' % (ms, lin, old | 0x80))
        ptr = int.from_bytes(mm[base + ks.KB_PTR_DS - ks.DATA_BASE_VA:][:4], 'little')
        while self.typing and now >= self.typing[0][0]:
            if ptr == 0 or ptr + KEYTAB_OFF + 0x100 > len(mm):
                return
            _, name = self.typing.pop(0)
            scan = ks.KEYS[name][0]
            lin = ptr + KEYTAB_OFF + scan
            old = mm[lin]
            mm[lin] = old & 0x7F
            self.held.append((now + HOLD_S, lin, old))
            ok = bios_insert(mm, BIOS_WORD[name])
            log.write('I ms=%d key=%04X down=%08X tab=%02X ring=%s\n'
                      % (ms, BIOS_WORD[name], lin, old, 1 if ok else 0))

    def apply(self, mm, base, ms, log):
        wlo = base + kf.WIN_LO - ks.DATA_BASE_VA
        window = bytes(mm[wlo:wlo + kf.WIN_HI - kf.WIN_LO])
        for off, old, new in field_pokes(window, self.descs, [p for p in self.pokes if p[0] == 'field']):
            mm[wlo + off] = new
            log.write('W ms=%d ds=%08X linear=%08X old=%02X new=%02X\n'
                      % (ms, kf.WIN_LO + off, wlo + off, old, new))
        for kind, ds, byte in self.pokes:
            if kind == 'ds_or':
                lin = base + ds - ks.DATA_BASE_VA
                old = mm[lin]
                mm[lin] = old | byte
                log.write('W ms=%d ds=%08X linear=%08X old=%02X new=%02X\n' % (ms, ds, lin, old, old | byte))

    def run(self):
        deadline = time.monotonic() + 60
        while not (os.path.exists(self.mem_path) and os.path.getsize(self.mem_path) >= 1 << 20):
            if self.stop.is_set() or time.monotonic() > deadline:
                return
            time.sleep(0.05)
        with open(self.mem_path, 'r+b') as fh, open(self.log_path, 'w') as log:
            mm = mmap.mmap(fh.fileno(), 0)          # MAP_SHARED, read-write
            t0 = self.t0 = time.monotonic()
            base, prev, prev_img, prev_tail, pending, scan_at = None, None, None, None, bool(self.pokes), 0.0
            ranges = None                            # {id: bytes}: the five storage ranges (§J3.3)
            wlen = kf.WIN_HI - kf.WIN_LO
            while not self.stop.is_set():
                ms = int((time.monotonic() - t0) * 1000)
                if base is None:
                    if time.monotonic() - scan_at > 0.2:
                        scan_at = time.monotonic()
                        base = find_base(mm)
                        if base is not None:
                            log.write('B ms=%d base=%08X\n' % (ms, base))
                    time.sleep(0.005)
                    continue
                vals, kb, tail = read_vals(mm, base), read_kb(mm, base), u16(mm, BDA_TAIL)
                cur = read_ranges(mm, base)
                if ranges is None:
                    ranges = {}
                    for rid, at, data in cur:
                        log.write('L ms=%d base=%08X id=%s at=%08X len=%d\n'
                                  % (ms, base, rid, at, len(data)))
                for rid, at, data in cur:
                    if ranges.get(rid) != data:
                        log.write('R ms=%d tick=%08X id=%s img=%s\n'
                                  % (ms, vals['tick'], rid, data.hex()))
                        ranges[rid] = data
                if prev_tail is None:
                    prev_tail = tail
                if tail != prev_tail:
                    start, end = u16(mm, BDA_START) or 0x1E, u16(mm, BDA_END) or 0x3E
                    for w in bios_new_keys(mm, prev_tail, tail, start, end):
                        log.write('K ms=%d tick=%08X f=%04X key=%04X\n' % (ms, vals['tick'], vals['f'], w))
                    prev_tail = tail
                if (vals, kb) != prev:
                    log.write(ks.format_p(ms, vals, kb, tail) + '\n')
                    prev = (vals, kb)
                wlo = base + kf.WIN_LO - ks.DATA_BASE_VA
                img = bytes(mm[wlo:wlo + wlen])
                if img != prev_img:
                    log.write('F ms=%d tick=%08X img=%s\n' % (ms, vals['tick'], img.hex()))
                    prev_img = img
                if pending and vals['mode'] == 0x27:
                    self.apply(mm, base, ms, log)
                    pending = False
                if self.typing or self.held:
                    self.type_keys(mm, base, time.monotonic() - t0, ms, log)
                time.sleep(0.0005)
            log.write('E ms=%d reason=%s rc=%d\n' % (int((time.monotonic() - t0) * 1000),
                                                     getattr(self, 'reason', 'exit'), getattr(self, 'rc', -1)))
            mm.close()


def cmos_path(dirpath):
    """The dir's CMOS file (the name is case-insensitive on DOS), or None."""
    for name in os.listdir(dirpath):
        if name.upper() == 'CMOS':
            return os.path.join(dirpath, name)
    return None


def read_cmos(dirpath):
    path = cmos_path(dirpath)
    if path is None:
        return None
    with open(path, 'rb') as f:
        return f.read()


def run_boot(root, game, iso, descs, scenario, boot, pokes, limit, enter_wait, pace,
             autotype, ro):
    """One DOSBox-X run of a boot, with its own avi/, guest.mem, poll.log and
    dosbox.log under root. Returns the command, the poll.log lines, the wall
    time, the exit code, the reason and the AVI paths."""
    os.makedirs(os.path.join(root, 'avi'), exist_ok=True)
    cmd = dosbox_cmd(root, game, iso, scenario, limit, enter_wait, pace,
                     autotype=autotype, boot=boot, ro=ro)
    print('k11_capture: %s' % shlex.join(cmd))
    stop = threading.Event()
    typing = schedule(ks.keys_for(scenario, boot), enter_wait, pace) if not autotype else ()
    poll = Poller(os.path.join(root, 'guest.mem'), os.path.join(root, 'poll.log'), pokes, descs, stop, typing)
    poll.start()
    t = time.monotonic()
    r = subprocess.run(cmd)
    wall = time.monotonic() - t
    poll.rc, poll.reason = r.returncode, ('time-limit' if wall >= limit - 1 else 'exit')
    stop.set()
    poll.join()
    lines = []
    poll_path = os.path.join(root, 'poll.log')
    if os.path.exists(poll_path):
        with open(poll_path) as f:
            lines = f.read().splitlines()
    avi_dir = os.path.join(root, 'avi')
    return dict(cmd=cmd, root=root, lines=lines, wall=wall, rc=r.returncode,
                reason=poll.reason, avi_dir=avi_dir,
                avis=sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.avi')))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scenario', required=True, choices=sorted(ks.SCENARIOS))
    ap.add_argument('--out', required=True)
    ap.add_argument('--game-dir', default=DEFAULT_GAME_DIR)
    ap.add_argument('--exe', default=tcap.PIN_EXE)
    ap.add_argument('--time-limit', type=int)
    ap.add_argument('--enter-wait', type=float, default=ks.ENTER_WAIT)
    ap.add_argument('--pace', type=float, default=ks.PACE)
    ap.add_argument('--no-pokes', action='store_true')
    ap.add_argument('--keep-avi', action='store_true')
    ap.add_argument('--input', choices=('inject', 'autotype'), default='inject',
                    help='inject: the poller types through the memory file (record §A.9)')
    a = ap.parse_args()
    out = guard_out(a.out)
    scn = ks.SCENARIOS[a.scenario]
    if not os.path.isfile(a.exe):
        sys.exit('k11_capture: %s missing (run make title-pin)' % a.exe)
    cmos = check_cmos(a.game_dir)                       # every scenario (J3.2)
    descs = kf.load_descriptors(a.exe)
    limit = a.time_limit or scn['time_limit']
    two_boot = 'boot2' in scn
    writable = bool(scn.get('writable'))
    autotype = a.input == 'autotype'
    root = tempfile.mkdtemp(prefix='k11cap-')
    stage = stage_dir(out)
    shutil.rmtree(stage, ignore_errors=True)            # a stale partial must not merge
    published = False
    try:
        game = tcap.stage(root, a.exe, a.game_dir)
        iso = os.path.join(root, 'CD', 'RAGECD.ISO')
        saved = staged_bytes = staging0 = gdir0 = None
        if writable:
            stage_cmos_real(game)                       # a real file, never a symlink into data/
            staged_bytes = read_cmos(game)
            staging0 = snapshot_tree(game)
            gdir0 = snapshot_mtimes(a.game_dir)
        pokes1 = () if a.no_pokes else ks.pokes_for(a.scenario, 1)
        b1 = run_boot(root, game, iso, descs, a.scenario, 1, pokes1, limit,
                      a.enter_wait, a.pace, autotype, ro=not writable)
        checks, _ = poll_checks(a.scenario, 1, b1['lines'], pokes1)
        if writable:
            saved = read_cmos(game)
            ok, why = saved_cmos_check(saved, staged_bytes)
            if not ok:
                print('k11_capture: save: %s' % why)
            checks.append(('save', ok))
            ok, why = staging_check(staging0, snapshot_tree(game),
                                    gdir0, snapshot_mtimes(a.game_dir))
            if not ok:
                print('k11_capture: staging: %s' % why)
            checks.append(('staging', ok))
        print_checks(checks)
        if not all(ok for _, ok in checks):
            return 1
        if writable:
            os.makedirs(os.path.join(stage, 'boot1'), exist_ok=True)
            shutil.copyfile(os.path.join(root, 'poll.log'), os.path.join(stage, 'boot1', 'poll.log'))
            with open(os.path.join(stage, 'CMOS'), 'wb') as f:
                f.write(saved)
        if two_boot:
            # Boot 2 reuses the staging copy (the file the save wrote) read-only,
            # with its own avi/, memory file and poll.log under root/boot2 (§J3.2).
            b2root = os.path.join(root, 'boot2')
            os.makedirs(b2root)
            pokes2 = ks.pokes_for(a.scenario, 2)
            b2 = run_boot(b2root, game, iso, descs, a.scenario, 2, pokes2,
                          scn['boot2']['time_limit'], a.enter_wait, a.pace, autotype, ro=True)
            checks2, _ = poll_checks(a.scenario, 2, b2['lines'], pokes2)
            print_checks(checks2)
            if not all(ok for _, ok in checks2):
                return 1
            final, final_limit = b2, scn['boot2']['time_limit']
        else:
            final, final_limit = b1, limit
        avis = final['avis']
        if not avis:
            sys.exit('k11_capture: no AVI in %s' % final['avi_dir'])
        paths = [os.path.join(final['avi_dir'], n) for n in avis]
        cap = [hashlib.md5(d).digest() for d in sc.read_avi_frames(paths)]
        start, logs = tcap.post_logo_start(cap, a.game_dir)
        hs, idx = tcap.collapse_from(cap, start)
        os.makedirs(stage, exist_ok=True)
        tcap.write_window(stage, idx, tcap.frames_at(paths, idx),
                          ['mode=k11-%s' % a.scenario, 'raw_window=%d..%d' % (idx[0], idx[-1]),
                           'twi5_last=%s twg_last=%s' % (logs.get('twi5'), logs.get('twg'))])
        for name in ('poll.log', 'dosbox.log'):
            src = os.path.join(final['root'], name)
            if os.path.exists(src):
                shutil.copyfile(src, os.path.join(stage, name))
        dros = sorted(f for f in os.listdir(final['avi_dir']) if f.lower().endswith('.dro'))
        for n in dros:
            shutil.copyfile(os.path.join(final['avi_dir'], n), os.path.join(stage, n))
        subprocess.run([sc.which('ffmpeg'), '-v', 'error', '-y', '-sseof', '-1', '-i', paths[-1],
                        '-update', '1', os.path.join(stage, 'last_frame.png')])
        ver = subprocess.run([sc.which('dosbox-x'), '-version'], capture_output=True, text=True)
        ver = ver.stdout + ver.stderr                             # the banner goes to stderr
        with open(os.path.join(stage, 'session.txt'), 'w') as f:
            f.write('scenario=%s\n' % a.scenario)
            f.write('dosbox=%s\n' % next((l for l in ver.splitlines() if 'DOSBox-X version' in l), '?'))
            f.write('argv=%s\n' % shlex.join(final['cmd']))
            with open(a.exe, 'rb') as fx:
                f.write('exe=%s sha256=%s\n' % (a.exe, hashlib.sha256(fx.read()).hexdigest()))
            f.write('cmos=%s pokes=%s input=%s enter_wait=%g pace=%g\n'
                    % (cmos, list(pokes1), a.input, a.enter_wait, a.pace))
            f.write('time_limit=%d wall_s=%.1f rc=%d\n' % (final_limit, final['wall'], final['rc']))
            f.write('avis=%s fps=%.4f dro=%s\n' % (avis, sc.ffprobe_fps(paths[0]), dros))
            if two_boot:
                f.write('boot1=%s\n' % shlex.join(b1['cmd']))
                f.write('boot1_time_limit=%d wall_s=%.1f rc=%d reason=%s\n'
                        % (limit, b1['wall'], b1['rc'], b1['reason']))
        publish_stage(stage, out)
        published = True
        print('k11_capture: wrote %d frames to %s (raw %d..%d), wall %.1fs'
              % (len(idx), out, idx[0], idx[-1], final['wall']))
        return 0
    finally:
        if not published:
            shutil.rmtree(stage, ignore_errors=True)    # no partial capture on any failure
        if a.keep_avi:
            print('k11_capture: kept %s' % root)
        else:
            shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())

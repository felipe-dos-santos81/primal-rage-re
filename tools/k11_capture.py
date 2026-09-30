#!/usr/bin/env python3
"""K11 ground-truth capture (plan 2026-09-30-named-gaps-a-k11-harness.md,
record §A.3). The pinned original (make title-pin) runs in DOSBox-X. AUTOTYPE
(or, by default, the poller: --input) types the scenario's keys and
DX-CAPTURE /V /O records video and OPL. A
poller thread reads the `[dosbox] memory file` (guest RAM, memory-mapped; a
runtime linear address is the file offset) and writes poll.log (format:
k11_session.py). It also applies the scenario's pokes once mode 0x27 is seen.

Writes only data/k11-captures/<scenario>/ (guard_out), and never data/game.
The staging copy lives in a temp dir and mounts read-only, so the original's
CMOS save (0x1B084) cannot write. The CMOS in the game dir must be absent or
2040 zero bytes, because the port models the defaults path (config.c
config_validate). Usage:
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


def guard_out(out):
    real = os.path.realpath(out)
    root = os.path.realpath(CAPTURE_ROOT)
    if not os.path.normcase(real).casefold().startswith(os.path.normcase(root).casefold() + os.sep):
        raise SystemExit('k11_capture: --out must be a directory under %s: %s' % (CAPTURE_ROOT, out))
    return real


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


def dosbox_cmd(root, game, iso, scenario, time_limit, enter_wait, pace, autotype=True):
    return ([sc.which('dosbox-x'), '-defaultconf', '-fastlaunch', '-nopromptfolder',
             '-nogui', '-nomenu', '-time-limit', str(time_limit),
             '-set', 'sdl fullscreen=false',
             '-set', 'dosbox captures=%s' % os.path.join(root, 'avi'),
             '-set', 'dosbox memory file=%s' % os.path.join(root, 'guest.mem'),
             '-set', 'log logfile=%s' % os.path.join(root, 'dosbox.log')]
            + LOG_CON_ARGS
            + ['-c', 'MOUNT C "%s" -ro' % game,
               '-c', 'IMGMOUNT D "%s" -t iso' % iso,
               '-c', 'C:',
               ] + (['-c', ks.autotype_line(scenario, enter_wait, pace)] if autotype else []) + [
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
    cmos = check_cmos(a.game_dir)
    pokes = () if a.no_pokes else scn['pokes']
    descs = kf.load_descriptors(a.exe)
    limit = a.time_limit or scn['time_limit']
    root = tempfile.mkdtemp(prefix='k11cap-')
    try:
        game = tcap.stage(root, a.exe, a.game_dir)
        iso = os.path.join(root, 'CD', 'RAGECD.ISO')
        os.makedirs(os.path.join(root, 'avi'))
        cmd = dosbox_cmd(root, game, iso, a.scenario, limit, a.enter_wait, a.pace,
                         autotype=a.input == 'autotype')
        print('k11_capture: %s' % shlex.join(cmd))
        stop = threading.Event()
        typing = schedule(scn['keys'], a.enter_wait, a.pace) if a.input == 'inject' else ()
        poll = Poller(os.path.join(root, 'guest.mem'), os.path.join(root, 'poll.log'), pokes, descs, stop, typing)
        poll.start()
        t = time.monotonic()
        r = subprocess.run(cmd)
        wall = time.monotonic() - t
        poll.rc, poll.reason = r.returncode, ('time-limit' if wall >= limit - 1 else 'exit')
        stop.set()
        poll.join()
        avi_dir = os.path.join(root, 'avi')
        avis = sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.avi'))
        if not avis:
            sys.exit('k11_capture: no AVI in %s' % avi_dir)
        paths = [os.path.join(avi_dir, n) for n in avis]
        cap = [hashlib.md5(d).digest() for d in sc.read_avi_frames(paths)]
        start, logs = tcap.post_logo_start(cap, a.game_dir)
        hs, idx = tcap.collapse_from(cap, start)
        os.makedirs(out, exist_ok=True)
        tcap.write_window(out, idx, tcap.frames_at(paths, idx),
                          ['mode=k11-%s' % a.scenario, 'raw_window=%d..%d' % (idx[0], idx[-1]),
                           'twi5_last=%s twg_last=%s' % (logs.get('twi5'), logs.get('twg'))])
        for name in ('poll.log', 'dosbox.log'):
            src = os.path.join(root, name)
            if os.path.exists(src):
                shutil.copyfile(src, os.path.join(out, name))
        dros = sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.dro'))
        for n in dros:
            shutil.copyfile(os.path.join(avi_dir, n), os.path.join(out, n))
        subprocess.run([sc.which('ffmpeg'), '-v', 'error', '-y', '-sseof', '-1', '-i', paths[-1],
                        '-update', '1', os.path.join(out, 'last_frame.png')])
        ver = subprocess.run([sc.which('dosbox-x'), '-version'], capture_output=True, text=True)
        ver = ver.stdout + ver.stderr                             # the banner goes to stderr
        with open(os.path.join(out, 'session.txt'), 'w') as f:
            f.write('scenario=%s\n' % a.scenario)
            f.write('dosbox=%s\n' % next((l for l in ver.splitlines() if 'DOSBox-X version' in l), '?'))
            f.write('argv=%s\n' % shlex.join(cmd))
            with open(a.exe, 'rb') as fx:
                f.write('exe=%s sha256=%s\n' % (a.exe, hashlib.sha256(fx.read()).hexdigest()))
            f.write('cmos=%s pokes=%s input=%s enter_wait=%g pace=%g\n'
                    % (cmos, list(pokes), a.input, a.enter_wait, a.pace))
            f.write('time_limit=%d wall_s=%.1f rc=%d\n' % (limit, wall, r.returncode))
            f.write('avis=%s fps=%.4f dro=%s\n' % (avis, sc.ffprobe_fps(paths[0]), dros))
        recs = []
        if os.path.exists(os.path.join(out, 'poll.log')):          # absent: no memory file (F5)
            with open(os.path.join(out, 'poll.log')) as f:
                recs = [ks.parse(l) for l in f if l.strip()]
        keys = [k for k in scn['keys'] if k != ',']
        kcount = sum(1 for x in recs if x['kind'] == 'K')
        first_k = next((x['ms'] for x in recs if x['kind'] == 'K'), None)
        saw27 = first_k is not None and any(x['kind'] == 'P' and x['ms'] >= first_k and x['mode'] == 0x27 for x in recs)
        checks = [('base', any(x['kind'] == 'B' for x in recs)),
                  ('keys %d/%d' % (kcount, len(keys)), kcount == len(keys)),
                  ('mode 0x27 after the Enter', saw27),
                  ('pokes', not pokes or any(x['kind'] == 'W' for x in recs))]
        for name, ok in checks:
            print('k11_capture: CHECK %s: %s' % (name, 'ok' if ok else 'FAIL'))
        print('k11_capture: wrote %d frames to %s (raw %d..%d), wall %.1fs'
              % (len(idx), out, idx[0], idx[-1], wall))
        return 0 if all(ok for _, ok in checks) else 1
    finally:
        if a.keep_avi:
            print('k11_capture: kept %s' % root)
        else:
            shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())

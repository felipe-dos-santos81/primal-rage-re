#!/usr/bin/env python3
"""Gameplay ground-truth capture (spec 2026-09-30-gameplay-ground-truth-design.md
§4.1, record §G.5). The pinned original runs in DOSBox-X under DX-CAPTURE /V /O.
A poller thread maps the [dosbox] memory file, takes one consistent snapshot
per frame while the master loop spins ([DS_0010150C] - 1 == [DS_00101508],
0x256C6), fires the scenario's frame-keyed injections from that state and
writes poll.log (format: gp_session). Writes only data/k11-captures/gp-*/.
A scenario in gp_session.STOP_AT_END stops STOP_TAIL frames after its end.
Every capture fails a CHECK on keyboard input the harness did not inject:
nobody may type into DOSBox-X while it runs.
Usage: gp_capture.py --scenario NAME --out data/k11-captures/NAME [--exe PATH]
                     [--time-limit S] [--keep-avi] [--no-pad-bios] [--no-stop-at-end]
       gp_capture.py check-input DIR      (audit an existing capture's poll.log)"""
import argparse
import gzip
import hashlib
import mmap
import os
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs
import k11_capture as kc
import smk_capture as sc
import title_capture as tcap


def read_snap(mm, base):
    out = {}
    for name, ds, size in gs.SNAP_FIELDS:
        o = base + ds - gs.DATA_BASE_VA
        out[name] = int.from_bytes(mm[o:o + size], 'little')
    return out


def spinning(v):
    return (v['t50c'] - 1) & 0xFFFFFFFF == v['t508']


def consistent(v1, v2):
    return v1['f'] == v2['f'] and v1['t508'] == v2['t508']


def ring_steps(old, new, start, end):
    """The BIOS head after each word consumed from `old` to `new` (the key
    loop 0x24D08..0x24EE7 drains the ring in one iteration, record §G.4)."""
    out, h = [], old
    while h != new and len(out) < 16:
        h += 2
        if h >= end:
            h = start
        out.append(h)
    return out


class Injector:
    """Writes key presses into the IRQ1 key-state table (bit 7 clear = down)
    and, for a key or (pad_bios) a pad, queues its BIOS word once."""

    def __init__(self, mm, ptr, log, pad_bios=True):
        self.mm, self.ptr, self.log, self.pad_bios = mm, ptr, log, pad_bios
        self.held = []            # [(release_at_f, lin, old, name, step)]

    def press(self, step, f, name, scan, word, hold, late, ms):
        lin = self.ptr + gs.KEYTAB_OFF + scan
        old = self.mm[lin]
        self.mm[lin] = old & 0x7F
        # Pressed in the spin of F - 1 = f; iterations F .. F + hold - 1 sample it,
        # so it is released in the spin of F - 1 + hold (spec §4.1; record §G.7).
        self.held.append((f + hold, lin, old, name, step))
        bios = word if (name in gs.KEYS or self.pad_bios) else None
        ring = kc.bios_insert(self.mm, word) if bios is not None else None
        self.log.write('I ms=%d f=%04X step=%d press=%s scan=%02X lin=%08X old=%02X bios=%s ring=%s late=%d\n'
                       % (ms, f, step, name, scan, lin, old, '-' if bios is None else '%04X' % bios,
                          '-' if ring is None else int(ring), late))

    def release_due(self, f, ms):
        for h in [h for h in self.held if f >= h[0]]:
            self.mm[h[1]] = h[2] | 0x80
            self.held.remove(h)
            self.log.write('I ms=%d f=%04X step=%d release=%s lin=%08X\n' % (ms, f, h[4], h[3], h[1]))


def accept(v, v2, v3):
    """A snapshot v2 is logged only when v was in the spin state and neither an
    iteration nor an ISR tick came between v and v3, the re-read after v2
    (spec §4.1's double read, closed on both sides; record §G.5.2)."""
    return spinning(v) and consistent(v, v2) and consistent(v2, v3)


def fire(sched, f):
    """The steps due at the spin snapshot f, each with its own lateness:
    late = 1 unless f is exactly its frame F - 1 (spec §4.1; review 1)."""
    return [(step, act, int(f != sched.frame_of[step] - 1)) for step, act in sched.due(f)]


FRAME_BYTES = 320 * 200 * 3
MEM_WAIT_S = 60        # s, harness: how long the poller waits for DOSBox-X's memory file (as k11_capture)
LOG_CON_ARGS = ['-set', 'dos log console=quiet']      # record named-gaps-a §A.2
# Stop at the script's end (gp_session.STOP_AT_END). SIGTERM reaches SDL2's own
# handler (SDL_QuitInit), which queues SDL_QUIT; DOSBox-X's GFX_Events answers it
# with `if (CheckQuit()) throw(0)`, and CheckQuit returns true at once for
# `quit warning=false` (the default `auto` may open a dialog while a program
# runs). throw(0) reaches the same kill-switch branch of main's `catch (int x)`
# as -time-limit's throw(1) (src/hardware/pic.cpp), the path every capture so
# far ended on (DOSBox-X 2026.08.31 sources; task capture-hygiene report).
QUIT_ARGS = ['-set', 'dosbox quit warning=false']
# Harness values, not game values. STOP_TAIL: frames logged after the end frame
# before the SIGTERM, one second at 60.05 Hz; the end frame's image reaches the
# AVI at the next 70.09 Hz retrace (< 1 game frame), the rest is margin for the
# poller's and SDL's latency. STOP_GRACE_S: how long DOSBox-X may take to exit
# after it (the time-limit exits took wall_s - time_limit = 0.7..3.2 s in the
# four gp sessions); then it is killed and the stop CHECK fails.
STOP_TAIL = 60
STOP_GRACE_S = 30


def guard_gp(out):
    """--out must be data/k11-captures/gp-<name> exactly: a direct child of the
    capture root (review 1: not nested inside another capture)."""
    real = kc.guard_out(out)
    if (os.path.dirname(real) != os.path.realpath(kc.CAPTURE_ROOT)
            or not os.path.basename(real).startswith('gp-')):
        raise SystemExit('gp_capture: --out must be data/k11-captures/gp-<scenario>: %s' % out)
    return real


def stage_dir(out):
    """The sibling directory a run writes into before its CHECKs pass."""
    return os.path.join(os.path.dirname(out), '.%s.partial' % os.path.basename(out))


def publish(stage, out, ok):
    """Move a staged capture into place only when every CHECK passed; a failing
    run goes to <out>.failed and leaves a good capture at <out> untouched."""
    dest = out if ok else out + '.failed'
    old = dest + '.old'
    if os.path.exists(old):
        shutil.rmtree(old)
    if os.path.exists(dest):
        os.rename(dest, old)
    os.rename(stage, dest)
    if os.path.exists(old):
        shutil.rmtree(old)
    if ok and os.path.exists(out + '.failed'):     # a stale failed run must not outlive a good one
        shutil.rmtree(out + '.failed')
    return dest


def run_checks(name, lines, sched, n_frames, n_want):
    """The capture's CHECKs, [(label, ok)] (record §G.5.2, review 1)."""
    recs = [gs.parse(l) for l in lines if l.strip()]
    try:
        gs.port_script(name, lines)
        script_ok, why = True, ''
    except gs.ScriptError as e:
        script_ok, why = False, ' (%s)' % e
    enter = next((i for i, x in enumerate(recs) if x['kind'] == 'I' and x.get('press') == 'enter'), None)
    first27 = next((i for i, x in enumerate(recs) if x['kind'] in ('S', 'P') and x.get('mode') == 0x27), None)
    ordered = (enter is not None and first27 is not None and first27 > enter
               and recs[first27]['f'] >= recs[enter]['f'])
    snap = gs.snapshots(lines)
    kbraw = sum(1 for v in snap.values() if v['kb'] != gs.raw_to_kb(v['raw']))
    return [('base', any(x['kind'] == 'B' for x in recs)),
            ('steps fired %d/%d' % (sched.fired, sched.total), sched.fired == sched.total),
            ('end frame reached', any(x['kind'] == 'X' for x in recs)),
            ('mode 0x27 after the Enter', ordered),
            ('snapshots kb == raw (%d differ)' % kbraw, kbraw == 0),
            ('frames written %d/%d' % (n_frames, n_want), n_frames == n_want),
            ('port script v2%s' % why, script_ok),
            input_check(lines)]


# The key bitmap bit each key-state scan sets (PAD's third column; p1.b0 and
# p1.start share 0x0100, p2.b0 and p2.start 0x0001). The host-owned ISR sampler
# 0x1BBAC fills [DS_00101514]+0x2D8/+0x2D9 from the key-state table (demo-pose
# record, 0x1BBAC row), so a BIOS key whose scan is a pad's (U11's n, alt-s,
# alt-m) sets that pad's bit too: an injection explains a bit by its scan.
SCAN_BIT = {}
for _scan, _word, _bit in gs.PAD.values():
    SCAN_BIT[_scan] = SCAN_BIT.get(_scan, 0) | _bit
# The int 16h ring when the poll.log does not say (it logs no BDA start/end):
# kc.bios_insert's fallback 0x1E..0x3E, the BIOS default; every head and tail in
# gp-pads, gp-idle-loss, gp-u5-charsel and gp-u6-moves lies in 0x1E..0x3C.
RING_START, RING_END = 0x1E, 0x3E


def unscripted_input(lines):
    """Keyboard input the harness did not inject (task capture-hygiene; the
    gp-u6-moves contamination, U6b task-7 report §6.5), from the records alone.

    Key words: the int 16h tail moves only when a word is inserted (int 9 or
    kc.bios_insert), so between two S records it must advance by exactly the
    words the I records logged between them queued (ring=1). The poller logs an
    injection's I record before the next S reads the tail (Poller.run), and
    Injector.press queues a press's word once (no repeat), so no tolerance
    applies. The poll.log carries no ring contents, so a word is checked by
    count and frame, not by key.

    Pad bits: an I press at the spin of f (logged after S(f)) and its release at
    the spin of r (also after S(r)) explain the scan's bit in S(f+1)..S(r), and
    only there; a bit set outside every such window is an unscripted press, a
    bit clear inside one an unscripted release (a physical key-up overwrites the
    key-state byte). Measured on the clean captures: every one of the 31 pad
    windows of gp-pads and gp-u5-charsel shows its bit exactly in S(f+1)..S(r).

    Blind spots: the check detects stray typing, it does not prove its absence.
    It cannot see a stray press of a pad key inside that key's own scripted
    hold window (the bit is set either way), input before the first or after
    the last S record, a stray key-up of a non-pad key (no bitmap bit and no
    ring word), or a pad tap pressed and released between two S records.

    Returns dict(words=[f...], presses=[(f, bit)...], releases=[(f, bit)...])
    with each event's first S frame."""
    recs = [r for r in (gs.parse(l) for l in lines) if r]
    words, queued, prev_tail = [], 0, None
    held, wins = {}, {}                  # lin -> [(f, scan)]; bit -> [(lo, hi)]
    snaps = []
    for r in recs:
        k = r['kind']
        if k == 'I' and 'press' in r:
            if r.get('bios') is not None and r.get('ring') == 1:
                queued += 1
            if r['scan'] in SCAN_BIT:
                held.setdefault(r['lin'], []).append((r['f'], r['scan']))
        elif k == 'I' and 'release' in r:
            if held.get(r['lin']):
                f0, scan = held[r['lin']].pop(0)
                wins.setdefault(SCAN_BIT[scan], []).append((f0 + 1, r['f']))
        elif k == 'S':
            if prev_tail is not None:
                extra = len(ring_steps(prev_tail, r['tail'], RING_START, RING_END)) - queued
                words += [r['f']] * max(extra, 0)
            prev_tail, queued = r['tail'], 0
            snaps.append(r)
    for lst in held.values():            # still held when the log ends
        for f0, scan in lst:
            wins.setdefault(SCAN_BIT[scan], []).append((f0 + 1, 0x10000))
    presses, releases = [], []
    seen, bad = set(), {}
    for s in snaps:
        if s['f'] in seen:               # gs.snapshots: the first S of a frame
            continue
        seen.add(s['f'])
        for bit in (1 << i for i in range(16)):
            on = bool(s['kb'] & bit)
            want = any(lo <= s['f'] <= hi for lo, hi in wins.get(bit, ()))
            state = None if on == want else ('press' if on else 'release')
            if state is not None and bad.get(bit) != state:
                (presses if on else releases).append((s['f'], bit))
            bad[bit] = state
    return dict(words=words, presses=presses, releases=releases)


def input_check(lines):
    """The CHECK (label, ok) for unscripted_input."""
    u = unscripted_input(lines)
    firsts = u['words'][:1] + [f for f, _ in u['presses'][:1] + u['releases'][:1]]
    if not firsts:
        return 'no unscripted input', True
    return ('unscripted input: %d key words, %d pad presses, %d pad releases, first at f=%X'
            % (len(u['words']), len(u['presses']), len(u['releases']), min(firsts)), False)


def write_frames(out, paths, indices):
    """Stream the AVI frames at `indices` (ascending raw indices) to
    frame_%05d.raw.gz; window.txt maps each to its raw index."""
    want = {ix: k for k, ix in enumerate(indices)}
    n = 0
    for i, data in enumerate(sc.read_avi_frames(paths)):
        k = want.get(i)
        if k is None:
            continue
        with gzip.open(os.path.join(out, 'frame_%05d.raw.gz' % k), 'wb', compresslevel=1) as f:
            f.write(data)
        n += 1
        if n == len(indices):
            break
    with open(os.path.join(out, 'window.txt'), 'w') as f:
        for k, raw in enumerate(indices):
            f.write('%05d %d\n' % (k, raw))
    return n


def dosbox_cmd(root, game, iso, time_limit, stop_at_end=False):
    return ([sc.which('dosbox-x'), '-defaultconf', '-fastlaunch', '-nopromptfolder',
             '-nogui', '-nomenu', '-time-limit', str(time_limit),
             '-set', 'sdl fullscreen=false',
             '-set', 'dosbox captures=%s' % os.path.join(root, 'avi'),
             '-set', 'dosbox memory file=%s' % os.path.join(root, 'guest.mem'),
             '-set', 'log logfile=%s' % os.path.join(root, 'dosbox.log')]
            + LOG_CON_ARGS + (QUIT_ARGS if stop_at_end else [])
            + ['-c', 'MOUNT C "%s" -ro' % game, '-c', 'IMGMOUNT D "%s" -t iso' % iso, '-c', 'C:',
               '-c', 'DX-CAPTURE /V /O PRAGE.EXE -f', '-c', 'EXIT'])


class Poller(threading.Thread):
    def __init__(self, mem_path, log_path, steps, stop, pad_bios=True):
        super().__init__(daemon=True)
        self.mem_path, self.log_path, self.stop, self.pad_bios = mem_path, log_path, stop, pad_bios
        self.sched = gs.Schedule(steps)
        self.end_seen = False

    stop_tail = None        # frames after the end frame to SIGTERM `proc` (None: run to the time limit)
    proc = None             # the DOSBox-X Popen
    signal_f = signal_t = None

    def stop_if_due(self, f):
        """SIGTERM DOSBox-X once S(F + stop_tail) is logged, F the end frame."""
        if (self.stop_tail is None or self.proc is None or self.signal_f is not None
                or not self.sched.ended(f) or f < self.sched.end_frame + self.stop_tail):
            return False
        self.proc.send_signal(signal.SIGTERM)
        self.signal_f, self.signal_t = f, time.monotonic()
        return True

    def run(self):
        deadline = time.monotonic() + MEM_WAIT_S
        while not (os.path.exists(self.mem_path) and os.path.getsize(self.mem_path) >= 1 << 20):
            if self.stop.is_set() or time.monotonic() > deadline:
                return
            time.sleep(0.05)
        with open(self.mem_path, 'r+b') as fh, open(self.log_path, 'w') as log:
            mm = mmap.mmap(fh.fileno(), 0)
            t0 = time.monotonic()
            base = ptr = inj = None
            last_f = last_ms = None
            head = None
            while not self.stop.is_set():
                now = time.monotonic() - t0
                ms = int(now * 1000)
                if base is None:
                    base = kc.find_base(mm)
                    if base is None:
                        time.sleep(0.2)
                    continue
                v = read_snap(mm, base)
                if ptr is None:
                    ptr = int.from_bytes(mm[base + gs.KB_PTR_DS - gs.DATA_BASE_VA:][:4], 'little')
                    if ptr == 0:
                        ptr = None
                        time.sleep(0.005)
                        continue
                    log.write('B ms=%d base=%08X ptr=%08X\n' % (ms, base, ptr))
                    inj = Injector(mm, ptr, log, self.pad_bios)
                h = kc.u16(mm, gs.BDA_HEAD)
                if head is not None and h != head:
                    f_after = read_snap(mm, base)['f']
                    start, end = kc.u16(mm, kc.BDA_START) or 0x1E, kc.u16(mm, kc.BDA_END) or 0x3E
                    for step_head in ring_steps(head, h, start, end):
                        log.write('H ms=%d f=%04X head=%04X\n' % (ms, f_after, step_head))
                head = h
                if (v['mode'], v['st']) != last_ms:
                    log.write('P ms=%d f=%04X mode=%04X st=%04X tick=%08X\n'
                              % (ms, v['f'], v['mode'], v['st'], v['tick']))
                    self.sched.on_mode(v['f'], v['mode'])
                    last_ms = (v['mode'], v['st'])
                for step, act in self.sched.due_boot(now):
                    for name, scan, word, hold in gs.expand(act):
                        inj.press(step, v['f'], name, scan, word, hold, 0, ms)
                if spinning(v) and v['f'] != last_f:
                    v2 = read_snap(mm, base)
                    kb = mm[ptr + 0x2D8] << 8 | mm[ptr + 0x2D9]
                    bh, bt = kc.u16(mm, gs.BDA_HEAD), kc.u16(mm, gs.BDA_TAIL)
                    v3 = read_snap(mm, base)
                    if accept(v, v2, v3):
                        log.write(gs.format_s(ms, v2, kb, bh, bt) + '\n')
                        f = v2['f']
                        self.sched.on_mode(f, v2['mode'])
                        inj.release_due(f, ms)
                        for step, act, late in fire(self.sched, f):
                            for name, scan, word, hold in gs.expand(act):
                                inj.press(step, f, name, scan, word, hold, late, ms)
                        if not self.end_seen and self.sched.ended(f):
                            log.write('X ms=%d f=%04X step=%d end\n' % (ms, f, self.sched.i))
                            self.end_seen = True
                        self.stop_if_due(f)
                        last_f = f
                time.sleep(0.0003)
            log.write('E ms=%d reason=%s rc=%d\n' % (int((time.monotonic() - t0) * 1000),
                                                     getattr(self, 'reason', 'exit'), getattr(self, 'rc', -1)))
            mm.close()


def wait_dosbox(proc, poll, grace=STOP_GRACE_S):
    """DOSBox-X's exit code; killed (rc -9) when still running `grace` s after
    the poller's SIGTERM."""
    while True:
        try:
            return proc.wait(timeout=0.5)
        except subprocess.TimeoutExpired:
            if poll.signal_t is not None and time.monotonic() - poll.signal_t > grace:
                proc.kill()
                return proc.wait()


def check_input_main(argv):
    """check-input DIR: the unscripted-input CHECK over DIR/poll.log (read-only).
    Exit 0 ok, 1 FAIL, 2 not a gp capture (no poll.log, or no S records)."""
    if len(argv) != 1:
        sys.exit('usage: gp_capture.py check-input DIR')
    path = os.path.join(argv[0], 'poll.log')
    if not os.path.isfile(path):
        print('gp_capture: check-input: no poll.log in %s' % argv[0])
        return 2
    with open(path) as f:
        lines = f.read().splitlines()
    if not any(l.startswith('S ') for l in lines):
        print('gp_capture: check-input: no S records: not a gp poll.log (%s)' % path)
        return 2
    label, ok = input_check(lines)
    u = unscripted_input(lines)
    print('check=%s %s' % ('ok' if ok else 'FAIL', label))
    for what, ev in (('key words at f', u['words']), ('pad presses at f:bit', u['presses']),
                     ('pad releases at f:bit', u['releases'])):
        if ev:
            items = ['%X' % e if isinstance(e, int) else '%X:%04X' % e for e in ev]
            print('gp_capture: check-input: %d %s %s%s' % (len(ev), what, ' '.join(items[:12]),
                                                           ' ...' if len(items) > 12 else ''))
    return 0 if ok else 1


def main():
    if sys.argv[1:2] == ['check-input']:
        return check_input_main(sys.argv[2:])
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scenario', required=True, choices=sorted(n for n in gs.SCENARIOS if n.startswith('gp-')))
    ap.add_argument('--out', required=True)
    ap.add_argument('--game-dir', default=kc.DEFAULT_GAME_DIR)
    ap.add_argument('--exe', default=tcap.PIN_EXE)
    ap.add_argument('--time-limit', type=int)
    ap.add_argument('--keep-avi', action='store_true')
    ap.add_argument('--no-pad-bios', action='store_true')
    ap.add_argument('--no-stop-at-end', action='store_true')
    a = ap.parse_args()
    out = guard_gp(a.out)
    scn = gs.SCENARIOS[a.scenario]
    if not os.path.isfile(a.exe):
        sys.exit('gp_capture: %s missing (run make title-pin)' % a.exe)
    cmos = kc.check_cmos(a.game_dir)
    limit = a.time_limit or scn['time_limit']
    stop_end = a.scenario in gs.STOP_AT_END and not a.no_stop_at_end
    root = tempfile.mkdtemp(prefix='gpcap-')
    try:
        game = tcap.stage(root, a.exe, a.game_dir)
        iso = os.path.join(root, 'CD', 'RAGECD.ISO')
        os.makedirs(os.path.join(root, 'avi'))
        cmd = dosbox_cmd(root, game, iso, limit, stop_end)
        print('gp_capture: %s' % shlex.join(cmd))
        stop = threading.Event()
        poll = Poller(os.path.join(root, 'guest.mem'), os.path.join(root, 'poll.log'),
                      scn['steps'], stop, pad_bios=not a.no_pad_bios)
        poll.start()
        t = time.monotonic()
        proc = subprocess.Popen(cmd)
        poll.stop_tail, poll.proc = (STOP_TAIL if stop_end else None), proc
        rc = wait_dosbox(proc, poll)
        wall = time.monotonic() - t
        poll.rc, poll.reason = rc, ('end' if poll.signal_f is not None
                                    else 'time-limit' if wall >= limit - 1 else 'exit')
        stop.set()
        poll.join()
        avi_dir = os.path.join(root, 'avi')
        avis = sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.avi'))
        if not avis:
            sys.exit('gp_capture: no AVI in %s' % avi_dir)
        paths = [os.path.join(avi_dir, n) for n in avis]
        cap = [hashlib.md5(d).digest() for d in sc.read_avi_frames(paths)]
        start, logs = tcap.post_logo_start(cap, a.game_dir)
        _, idx = tcap.collapse_from(cap, start)
        stage = stage_dir(out)
        if os.path.exists(stage):
            shutil.rmtree(stage)
        os.makedirs(stage)
        n = write_frames(stage, paths, idx)
        for name in ('poll.log', 'dosbox.log'):
            src = os.path.join(root, name)
            if os.path.exists(src):
                shutil.copyfile(src, os.path.join(stage, name))
        dros = sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.dro'))
        for d in dros:
            shutil.copyfile(os.path.join(avi_dir, d), os.path.join(stage, d))
        subprocess.run([sc.which('ffmpeg'), '-v', 'error', '-y', '-sseof', '-1', '-i', paths[-1],
                        '-update', '1', os.path.join(stage, 'last_frame.png')])
        ver = subprocess.run([sc.which('dosbox-x'), '-version'], capture_output=True, text=True)
        ver = ver.stdout + ver.stderr
        with open(a.exe, 'rb') as fx:
            sha = hashlib.sha256(fx.read()).hexdigest()
        lines = []
        if os.path.exists(os.path.join(stage, 'poll.log')):        # absent: no memory file
            with open(os.path.join(stage, 'poll.log')) as f:
                lines = f.read().splitlines()
        checks = run_checks(a.scenario, lines, poll.sched, n, len(idx))
        if stop_end:
            checks.append(('stopped at the end (SIGTERM at f=%s, rc=%d)'
                           % ('-' if poll.signal_f is None else '%04X' % poll.signal_f, rc),
                           poll.signal_f is not None and rc == 0))
        ok = all(c for _, c in checks)
        with open(os.path.join(stage, 'session.txt'), 'w') as f:
            f.write('scenario=%s\n' % a.scenario)
            f.write('dosbox=%s\n' % next((l for l in ver.splitlines() if 'DOSBox-X version' in l), '?'))
            f.write('argv=%s\n' % shlex.join(cmd))
            f.write('exe=%s sha256=%s\n' % (a.exe, sha))
            f.write('cmos=%s pad_bios=%d\n' % (cmos, int(not a.no_pad_bios)))
            f.write('time_limit=%d wall_s=%.1f rc=%d\n' % (limit, wall, rc))
            if stop_end:
                f.write('stop_at_end=1 tail=%d signal_f=%s\n'
                        % (STOP_TAIL, '-' if poll.signal_f is None else '%04X' % poll.signal_f))
            f.write('avis=%s fps=%.4f dro=%s frames=%d raw_window=%d..%d avi_frames=%d twg_last=%s\n'
                    % (avis, sc.ffprobe_fps(paths[0]), dros, n, idx[0], idx[-1], len(cap), logs.get('twg')))
            for name, c in checks:
                f.write('check=%s %s\n' % ('ok' if c else 'FAIL', name))
        snaps = sorted(gs.snapshots(lines))
        missed = sum(b - a - 1 for a, b in zip(snaps, snaps[1:]) if b > a + 1)
        print('gp_capture: snapshots %d, f %s..%s, %d frames missed (spec §3.7)'
              % (len(snaps), snaps and '%X' % snaps[0], snaps and '%X' % snaps[-1], missed))
        for name, c in checks:
            print('gp_capture: CHECK %s: %s' % (name, 'ok' if c else 'FAIL'))
        dest = publish(stage, out, ok)
        print('gp_capture: wrote %d frames to %s (raw %d..%d), wall %.1fs%s'
              % (n, dest, idx[0], idx[-1], wall, '' if ok else '; a CHECK failed, %s left as it was' % out))
        return 0 if ok else 1
    finally:
        if a.keep_avi:
            print('gp_capture: kept %s' % root)
        else:
            shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())

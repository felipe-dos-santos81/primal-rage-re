#!/usr/bin/env python3
"""K11 harness scenarios, the DOSBox-X AUTOTYPE line, the live-RAM poll log and
the port-script generator (record 2026-09-30-named-gaps-a-derivations.md §A.3).

A scenario is an AUTOTYPE key list (',' is one extra PACE of delay; the first
key is the mode-3 Enter, 0x24ECF..0x24EE0) plus pokes applied once, when the
poll first sees mode 0x27.

poll.log (written by k11_capture.py), one record per line:
  B ms=<int> base=<hex8>                          the data object's runtime base
  P ms=<int> f=.. st=.. mode=.. menu=.. ent=.. diag=.. tick=.. ktime=.. kword=..
            latch=.. raw=.. pad=.. kb=<hex4> bios=<hex4>   any value changed
  K ms=<int> tick=<hex8> f=<hex4> key=<hex4>      one BIOS keyboard-buffer insert
  F ms=<int> tick=<hex8> img=<hex>                the field window [0x105DAF,0x105E30) changed
  L ms=<int> base=<hex8> id=<cfg|k0|hs0|hs1|hs2> at=<hex8> len=<int>
                                                  one storage range's runtime locate (§J3.3)
  R ms=<int> tick=<hex8> id=<...> img=<hex>       one storage range's bytes changed
  W ms=<int> ds=<hex8> linear=<hex8> old=<hex2> new=<hex2>   one poked byte
  E ms=<int> reason=<exit|time-limit> rc=<int>    the run ended
  I ms=<int> key=<hex4> down=<hex8> tab=<hex2> ring=<0|1>   one injected key press
  I ms=<int> up=<hex8> tab=<hex2>                 its release (k11_capture --input inject)

  The L/R records carry the five storage ranges of record §J3.3 (the config
  image, the kind-0 counters and the three high-score tables): L once per
  range when the data base is found, R on the range's every observed value
  (the first read included, as F).

The port script (read by test_k11_oracle, port/tests/test_game.c):
  enter_frame <dec>        DS_000EF6DC after the iteration that took mode 3 to 0x27
  enter_state <hex>        DS_000F0A64 then
  ds_or <ds hex> <byte hex>          at the Enter
  field <id hex> <value hex>         at the Enter (config_field_set)
  pad <dtick> <kb hex4> <nticks>     hold [DS_00101514]+0x2D8/+0x2D9 = kb >> 8, kb & 0xFF
  key <dtick> <scan hex> <ascii hex> queue in the int 16h buffer
  end <dtick>
dtick counts DS_00101500 ticks from the Enter's K record. A scenario with a
`boot2` sub-entry (save-reload, §J3.1) selects its keys/pokes with --boot:
boot 2 is the one-Enter load run over the file boot 1 wrote. Usage:
  k11_session.py autotype --scenario NAME [--boot {1,2}] [--enter-wait S] [--pace S]
  k11_session.py port-script --scenario NAME [--boot {1,2}] --capture DIR --out FILE
  k11_session.py check-fields --scenario NAME --port DIR [--exe PATH]"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import k11_fields as kf

DATA_BASE_VA = 0x80000
KB_PTR_DS = 0x101514       # DS_00101514 -> the key-config/BIOS record (+0x2D8/+0x2D9)
ENTER_WAIT = 25.0          # s: inside the boot attract (logos end ~19 s, title ~31 s; §A.4 checks it)
PACE = 1.0                 # s between keys
END_TAIL_TICKS = 180       # harness window after the last key (not a game value)

POLL_FIELDS = (
    ('f', 0x0EF6DC, 2),      # DS_000EF6DC the master frame counter (0x24CDB)
    ('st', 0x0F0A64, 2),     # DS_000F0A64 the 0x11D04 state
    ('mode', 0x104B00, 2),   # DS_00104B00 the mode word (0x24EE0 stores 0x27)
    ('menu', 0x107414, 1),   # DS_00107414 menu_step's active byte (0x2EBA8 clears it)
    ('ent', 0x10741C, 4),    # DS_0010741C the menu's current entry
    ('diag', 0x107410, 4),   # DS_00107410 TEST CONTROLS' flags (0x2FA1C)
    ('tick', 0x101500, 4),   # DS_00101500 the ISR clock 0x500BB reads
    ('ktime', 0x105F2C, 4),  # DS_00105F2C the key/menu stamp 0x2EB94 subtracts
    ('kword', 0x105F28, 4),  # DS_00105F28 the last BIOS key word (0x2EB3F)
    ('latch', 0x105F30, 4),  # DS_00105F30 the latched key (0x24D3E, 0x2EB66)
    ('raw', 0x0E1C30, 4),    # DS_000E1C30 the pad's previous raw word
    ('pad', 0x0E1C34, 4),    # DS_000E1C34 the pad level
)

KEYS = {'enter': (0x1C, 0x0D), 'esc': (0x01, 0x1B), 'up': (0x48, 0x00),
        'down': (0x50, 0x00), 'left': (0x4B, 0x00), 'right': (0x4D, 0x00)}
ARROW_SCANS = (0x48, 0x50, 0x4B, 0x4D)

WALK = tuple((
    'enter enter down down esc down enter enter esc '        # MAIN, START (2 rows), back, GAME OPTIONS, CONFIG OPTIONS
    'down enter esc esc esc esc esc '                        # STATISTICS: page 1, page 2, histograms 0..2
    'down enter esc down enter esc down enter esc down enter esc '   # SOUND, MUSIC, MODIFY CONTROLS, CONFIGURE KEYBOARD
    'down enter esc down enter esc down enter esc esc'       # TEST CONTROLS, ADJUST VOLUME, 2 PLAYER HANDICAP, leave
).split())

# save-reload (record §J3.1, track J task 7), boot 1: the run that saves. Its
# first eight keys are the walk's own first eight (to CONFIG OPTIONS), then one
# `right` tap (record 0 CREDITS, field 0x29) and two Escs: the first leaves the
# editor (0x33578 writes the field via 0x2DA0C), the second leaves the OPTIONS
# MENU, whose menu_run return calls config_file_save() at 0x2CBAF. Boot 1 runs
# on a writable mount (the one scenario that does), so the save lands in the
# staging copy. Boot 2 is the one-Enter load run over that file: it must not
# re-save, so it is mounted read-only and sends nothing after the Enter.
SAVE_RELOAD_KEYS = tuple('enter enter down down esc down enter enter right esc esc'.split())

SCENARIOS = {
    'walk': dict(keys=WALK, pokes=(), time_limit=75),
    'idle': dict(keys=('enter',), pokes=(), time_limit=90),
    'menuesc': dict(keys=('enter', ',', ',', 'esc'), pokes=(), time_limit=60),
    'diags': dict(keys=tuple('enter down enter down down down down down down enter , , , esc esc'.split()),
                  pokes=(('ds_or', 0x107410, 0x10),), time_limit=55),
    'de': dict(keys=tuple('enter down enter down enter , , , esc esc esc esc esc esc'.split()),
               pokes=(('field', 8, 0xFFFF), ('field', 6, 0x0001)), time_limit=55),   # spec §8, record §A.1.2
    'save-reload': dict(keys=SAVE_RELOAD_KEYS, pokes=(), time_limit=55, writable=True,
                        boot2=dict(keys=('enter',), time_limit=45)),
}


class ScriptError(Exception):
    pass


def key_word(name):
    scan, asc = KEYS[name]
    return scan << 8 | asc


def keys_for(name, boot=1):
    """The scenario's key list: boot 1 (the default), or the `boot2` sub-entry's
    (only save-reload has one)."""
    sc = SCENARIOS[name]
    return sc['keys'] if boot == 1 else sc['boot2']['keys']


def pokes_for(name, boot=1):
    sc = SCENARIOS[name]
    return sc['pokes'] if boot == 1 else sc['boot2'].get('pokes', ())


def normalise(word):
    """int 16h AH=0 hands a grey arrow's 0xE0 ascii back as 0 (0x24D26); the
    BIOS buffer the poller reads keeps the 0xE0."""
    if (word >> 8) in ARROW_SCANS and (word & 0xFF) == 0xE0:
        return word & 0xFF00
    return word


def autotype_line(name, enter_wait=ENTER_WAIT, pace=PACE, boot=1):
    return 'AUTOTYPE -w %g -p %g %s' % (enter_wait, pace, ' '.join(keys_for(name, boot)))


def format_p(ms, vals, kb, bios):
    body = ' '.join('%s=%0*X' % (n, 2 * sz, vals[n]) for n, _, sz in POLL_FIELDS)
    return 'P ms=%d %s kb=%04X bios=%04X' % (ms, body, kb, bios)


def parse(line):
    parts = line.split()
    if not parts:
        return None
    rec = {'kind': parts[0]}
    for part in parts[1:]:
        key, _, val = part.partition('=')
        if key in ('ms', 'rc', 'len'):
            rec[key] = int(val)
        elif key in ('reason', 'id'):
            rec[key] = val
        elif key == 'img':
            rec[key] = bytes.fromhex(val)
        else:
            rec[key] = int(val, 16)
    return rec


def enter_record(recs):
    """The mode-3 Enter as the capture shows it: (the first K record, the first
    P record with mode 0x27 at or after it), or (None, None). Its P record's
    `f` is the port script's enter_frame."""
    kev = [r for r in recs if r['kind'] == 'K']
    if not kev:
        return None, None
    k = kev[0]
    p27 = next((r for r in recs if r['kind'] == 'P' and r['ms'] >= k['ms'] and r['mode'] == 0x27), None)
    return k, p27


def port_script(name, lines, boot=1):
    recs = [r for r in (parse(l) for l in lines) if r]
    keys = [k for k in keys_for(name, boot) if k != ',']
    kev = [r for r in recs if r['kind'] == 'K']
    if not kev:
        raise ScriptError('no K record: the BIOS keyboard buffer never advanced (plan Task 14 F2)')
    got = [normalise(r['key']) for r in kev]
    want = [key_word(k) for k in keys]
    aborted = any(r['kind'] == 'E' and r.get('reason') == 'exit' for r in recs)
    if aborted and len(got) < len(want) and got == want[:len(got)]:
        # The game left DOSBox-X before its last keys (the de scenario's #DE
        # abort, record §A.8): the script covers the keys the game received.
        keys, want = keys[:len(got)], want[:len(got)]
    if got != want:
        raise ScriptError('observed keys %s differ from scenario %s %s'
                          % (['%04X' % w for w in got], name, ['%04X' % w for w in want]))
    k0, p27 = enter_record(recs)
    if p27 is None:
        raise ScriptError('mode 0x27 never observed after the Enter (raise --enter-wait?)')
    ms0 = k0['ms']
    t0 = k0['tick']
    dt = lambda tick: (tick - t0) & 0xFFFFFFFF
    timed = []
    for kname, r in zip(keys[1:], kev[1:]):
        scan, asc = KEYS[kname]
        timed.append((dt(r['tick']), 1, 'key %d %02X %02X' % (dt(r['tick']), scan, asc)))
    prev, start = 0, 0
    for r in recs:
        if r['kind'] != 'P' or r['ms'] < ms0:
            continue
        if r['kb'] != prev:
            if prev:
                timed.append((start, 0, 'pad %d %04X %d' % (start, prev, max(1, dt(r['tick']) - start))))
            prev, start = r['kb'], dt(r['tick'])
    if prev:
        raise ScriptError('the key bitmap is still held at the end of the poll log')
    last = max([d for d, _, _ in timed] + [0])
    out = ['# k11 port script v1: scenario %s' % name,
           'enter_frame %d' % p27['f'],
           'enter_state %04X' % p27['st']]
    out += ['%s %X %X' % (kind, a, b) for kind, a, b in pokes_for(name, boot)]
    out += [text for _, _, text in sorted(timed)]
    out.append('end %d' % (last + END_TAIL_TICKS))
    return '\n'.join(out) + '\n'


def check_fields(name, port_dir, exe):
    descs = kf.load_descriptors(exe)
    n = kf.WIN_HI - kf.WIN_LO
    with open(os.path.join(port_dir, 'fimg_before.bin'), 'rb') as f:
        before = bytearray(f.read())
    with open(os.path.join(port_dir, 'fimg_after.bin'), 'rb') as f:
        after = f.read()
    if len(before) != n or len(after) != n:
        print('k11_session: check-fields: the port window is not %d bytes' % n)
        return 1
    for kind, a, b in SCENARIOS[name]['pokes']:
        if kind == 'field':
            kf.set_(before, descs, a, b)
    for i in range(n):
        if before[i] != after[i]:
            print('k11_session: check-fields: %s: DS %08X python %02X port %02X'
                  % (name, kf.WIN_LO + i, before[i], after[i]))
            return 1
    print('k11_session: check-fields: %s: tools/k11_fields.set_ matches config_field_set over %d bytes'
          % (name, n))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('autotype', 'port-script', 'check-fields'))
    ap.add_argument('--scenario', required=True, choices=sorted(SCENARIOS))
    ap.add_argument('--boot', type=int, choices=(1, 2), default=1,
                    help='save-reload: the boot whose keys/script is meant (default 1)')
    ap.add_argument('--enter-wait', type=float, default=ENTER_WAIT)
    ap.add_argument('--pace', type=float, default=PACE)
    ap.add_argument('--capture')
    ap.add_argument('--out')
    ap.add_argument('--port')
    ap.add_argument('--exe', default='data/game/C/PRAGE.EXE')
    a = ap.parse_args()
    if a.cmd == 'autotype':
        print(autotype_line(a.scenario, a.enter_wait, a.pace, a.boot))
        return 0
    if a.cmd == 'check-fields':
        return check_fields(a.scenario, a.port, a.exe)
    with open(os.path.join(a.capture, 'poll.log')) as f:
        try:
            text = port_script(a.scenario, f.read().splitlines(), a.boot)
        except ScriptError as e:
            print('k11_session: port-script: %s: %s' % (a.scenario, e))
            return 1
    with open(a.out, 'w') as f:
        f.write(text)
    print('k11_session: port-script: %s: wrote %s (%d lines)' % (a.scenario, a.out, text.count('\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

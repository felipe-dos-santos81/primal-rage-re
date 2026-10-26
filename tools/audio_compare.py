#!/usr/bin/env python3
"""Tolerance-free register-stream oracle: the port's OPL writes vs a capture's.

The reference is the original's own DBRAWOPL (`.dro`) trace; the port's stream
is the `PR_AUDIO_DUMP` file (port/tests/test_game.c). The claim is song choice,
notes, patches and tempo in the captured window; rendered audibility is not
claimed.

The only alignment is each song's **first key-on**: the port section's first
`0xB0..0xB8`/`0x1B0..0x1B8` write with the key bit, and the capture key-on with
the same register and value. A candidate anchor is verified against the port's
first few relative ticks before it is accepted, so a repeated note elsewhere in
the capture cannot be mistaken for the section start.

Both streams are reduced the way the title oracle reduces them
(`port/spec/audio.md`, "Known capture divergences"):

  * anchor at the section's first key-on;
  * drop a write whose value equals the last kept value for that register
    (write-on-change collapse; the capture records write-on-change only, the
    port writes every family unconditionally; the shadow is seeded from the
    capture's pre-anchor state on both sides);
  * drop `0xBD` and the carrier-TL registers `0x43..0x45/0x4B..0x4D/0x53..0x55`
    on MIDI channels 1 and 4 only (the engine's per-channel volume residual).
    The capture records no MIDI channel, so a captured write inherits the
    attribution of the port's raw write at the same relative tick/register.

The comparison is **per music tick, as a sorted multiset** (the declared
intra-tick write-order model difference: the port keys expired voices in
ascending OPL channel order where the driver's per-tick visit order is not
pinned). The capture's ms is the emulator's accumulated inter-write delay, so a
write's label can round to the adjacent tick (the same selmo note-off is at
479..484 ms in three captures): a group, or one write of it, may sit one tick
earlier or later in the capture. That is the declared whole-tick model, not a
fitted window: the write content and order across ticks are exact, and a
dropped, extra or wrong write has no matching group and fails at its tick. The
per-tick multiset equality is reported as the stronger measured form (it is
exact for most sections).

Each section's **final tick** is the request boundary and is not claimed: the
section end is a game-frame event while the capture's music clock is its own
(the measured 0.8 % span difference, record 2026-10-25-music-truth-derivations
§M3.4). Its writes are reported, not compared.

A section that cannot be anchored is a **named gap**, not a silent skip: the
measured `TRACE_PINS` table (or `--allow-unanchored HASH[,HASH]`) names the
sections a trace's capture cannot carry (the driver's voice cursor advanced by
the boundary's extra capture note). A section that anchors but differs on a
measured capture-side artifact (a tick split plus a carrier-TL write-on-change
state, §M4.4) is a named gap too, through `--allow-diff HASH[,HASH]`. Any other
unanchored or differing section fails. A capture with no `.dro` at all is
skipped by name (exit 3); the caller prints it.

Exit codes: 0 = every required section compared and matched (and the claimed
tick total is at least the pinned minimum); 1 = a first difference, an
unallowed unanchored/differing section, or a short claim; 2 = usage/parse
error; 3 = the capture is absent (the caller prints the skip by name).

Stdlib only.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import opl_trace

# The measured gameplay inventory (record 2026-10-25-music-truth-derivations
# §M1.6.2): 28 `gp-*` capture directories holding 29 `.dro` files (gp-pads holds
# two). The 8 other k11 captures and the 2 audio captures are not gameplay and
# are named apart by `summary`.
GP_TRACES = [
    'gp-idle-loss', 'gp-idle-loss-run2', 'gp-keys-fight', 'gp-pads', 'gp-twop',
    'gp-u10-ending', 'gp-u12-endurance', 'gp-u13-ending2', 'gp-u15-join',
    'gp-u16-twop2', 'gp-u17-device2', 'gp-u18-talon-ending',
    'gp-u19-vertigo-ending', 'gp-u20-chaos-ending', 'gp-u21-armadon-ending',
    'gp-u22-diablo-ending', 'gp-u23-u16-finisher', 'gp-u5-charsel',
    'gp-u6-moves', 'gp-u6-moves-b', 'gp-u8-attract-start', 'gp-u8-endurance',
    'gp-u8-handicap', 'gp-u8-left-training', 'gp-u8-right-arcade',
    'gp-u8-right-training', 'gp-u8-tug-of-war', 'gp-u9-win',
]

# Not gameplay: the service-menu/save/diagnostic k11 captures (8 files across 6
# directories) and the two title captures the report-only test reads.
OTHER_K11_TRACES = ['de', 'diags', 'idle', 'menuesc', 'save-reload', 'walk']

# The measured per-trace pins (record 2026-10-25-music-truth-derivations §M4.5):
#   name: (minimum total claimed ticks, allow-unanchored bank hashes,
#          allow-diff bank hashes)
# The minimum is the total of two runs on the phase tree; both runs produced
# every value below byte-identically. The allowances are the measured sections a
# trace cannot carry: the driver voice cursor advanced by the section boundary's
# extra capture note (unanchored) or the capture's tick-split/carrier-TL
# write-on-change artifact (diff). A section not in either list must anchor and
# match. `gp-u17-device2` has no frame-keyed port script (wall-paced; record
# §H6.9) and its `.dro` is the 248 ms boot fragment, so it is skipped by name.
TRACE_PINS = {
    'gp-idle-loss': (1498, (), ()),
    'gp-idle-loss-run2': (1508, (), ()),
    'gp-keys-fight': (1508, (), ()),
    'gp-pads': (30, (), ()),
    'gp-twop': (956, (0x20DA829D,), ()),
    'gp-u10-ending': (176, (0xF1774E70, 0x20DA829D, 0x3BA4C376), ()),
    'gp-u12-endurance': (1028, (), ()),
    'gp-u13-ending2': (356, (0xF1774E70, 0xBC44EF50, 0x3BA4C376), ()),
    'gp-u15-join': (1498, (), ()),
    'gp-u16-twop2': (956, (0x20DA829D,), ()),
    'gp-u17-device2': (0, (), ()),
    'gp-u18-talon-ending': (964, (0x3BA4C376,), ()),
    'gp-u19-vertigo-ending': (296, (0xF1774E70, 0x3D536DDE, 0x3BA4C376), ()),
    'gp-u20-chaos-ending': (416, (0xF1774E70, 0x86946EB7, 0x3BA4C376), ()),
    'gp-u21-armadon-ending': (1200, (0x3BA4C376,), ()),
    'gp-u22-diablo-ending': (991, (0xBF1C78E2, 0x3BA4C376), ()),
    'gp-u23-u16-finisher': (30, (0x20DA829D,), (0x286ED662, 0xF1774E70)),
    'gp-u5-charsel': (896, (0xF1774E70,), ()),
    'gp-u6-moves': (1508, (), ()),
    'gp-u6-moves-b': (1504, (), ()),
    'gp-u8-attract-start': (1663, (0xF1774E70,), ()),
    'gp-u8-endurance': (613, (), ()),
    'gp-u8-handicap': (1611, (), ()),
    'gp-u8-left-training': (1463, (), ()),
    'gp-u8-right-arcade': (1563, (), ()),
    'gp-u8-right-training': (1311, (), ()),
    'gp-u8-tug-of-war': (1196, (), ()),
    'gp-u9-win': (176, (0xF1774E70, 0x20DA829D, 0x3BA4C376), ()),
}

# The register shadow is indexed by the full 9-bit register so the second OPL2
# bank (0x1E0..0x1F5) does not alias the first (0xE0..0xF5). Same size as the
# title oracle's OPL_SHADOW_REGS (port/tests/test_audio.c).
SHADOW_REGS = 0x200

# The first relative ticks a candidate anchor must match before the section is
# compared. A constant verification prefix, not a tolerance: the full section
# is still compared, and a wrong anchor has to fail this before it can be
# mistaken for the section start.
ANCHOR_TICKS = 4

# The capture's music tick rate: 120 sequencer ticks/s (the title anchor's
# `ms = (tick - 60) * 1000 // 120`, tools/opl_seq.py:705). The capture's ms is
# its own accumulated delay, never a poll timestamp.
TICKS_PER_SECOND = 120


def is_key_on(reg, val):
    return (0xB0 <= reg <= 0xB8 or 0x1B0 <= reg <= 0x1B8) and (val & 0x20)


def documented_excluded(reg, attr):
    """The port deliberately does not reproduce these against a capture.

    `0xBD` unconditionally, and the carrier-TL registers on MIDI channels 1 and
    4 only (the engine's per-channel volume residual, `port/spec/audio.md`).
    """
    lo = reg & 0xFF
    if lo == 0xBD:
        return True
    if lo in (0x43, 0x44, 0x45, 0x4B, 0x4C, 0x4D, 0x53, 0x54, 0x55):
        return attr in (1, 4)
    return False


def resource_name(handle, index_names):
    """The raw handle is `(index << 23) | offset` (record §M1.4.1)."""
    index = handle >> 23
    off = handle & 0x7FFFFF
    if 0 <= index < len(index_names):
        return '%s+0x%X' % (index_names[index], off)
    return 'handle:0x%08X' % handle


def read_index_names(game_dir):
    if not game_dir:
        return []
    path = os.path.join(game_dir, 'INDEX')
    try:
        with open(path, 'rb') as f:
            data = f.read()
    except OSError:
        return []
    names = []
    for i in range(len(data) // 20):
        names.append(data[i * 20:i * 20 + 12].split(b'\0')[0].decode('latin1'))
    return names


class Section:
    """A run of register writes with non-decreasing song ticks. A new song (or
    a same-song restart) drops the tick counter to 0, which starts a section."""

    def __init__(self, writes):
        self.writes = writes
        self.first_frame = writes[0][0]
        self.dc = 0
        self.bank = 0
        self.anchor_idx = None
        self.anchor_tick = None
        for i, w in enumerate(writes):
            if is_key_on(w[3], w[4]):
                self.anchor_idx = i
                self.anchor_tick = w[1]
                break

    @property
    def last_tick(self):
        return max(w[1] for w in self.writes)


def parse_port(path):
    """Returns (sections, s_lines). The `dc`/bank come from the first S record
    at or after the section's first write frame (the S line is emitted after
    the iteration's writes)."""
    s_lines = []
    writes = []
    with open(path) as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == 'S':
                fields = dict(kv.split('=', 1) for kv in p[2:] if '=' in kv)
                s_lines.append({
                    'frame': int(p[1]),
                    'cc': int(fields.get('cc', '0'), 16),
                    'dc': int(fields.get('dc', '0'), 16),
                    'vol': int(fields.get('vol', '0'), 16),
                    'bank': int(fields.get('bank', '0'), 16),
                    'inits': int(fields.get('inits', '0')),
                    'ticks': int(fields.get('ticks', '0')),
                })
            elif p[0] == 'W':
                writes.append((int(p[1]), int(p[2]), int(p[3]),
                               int(p[4], 16), int(p[5], 16), int(p[6], 16)))
    if not writes:
        return [], s_lines
    groups = []
    cur = []
    prev_tick = None
    for w in writes:
        if prev_tick is not None and w[1] < prev_tick:
            groups.append(cur)
            cur = []
        cur.append(w)
        prev_tick = w[1]
    groups.append(cur)
    sections = [Section(g) for g in groups]
    for s in sections:
        rec = next((r for r in s_lines if r['frame'] >= s.first_frame), None)
        if rec is None and s_lines:
            rec = s_lines[-1]
        if rec is not None:
            s.dc = rec['dc']
            s.bank = rec['bank']
    return sections, s_lines


def parse_capture(path):
    with open(path, 'rb') as f:
        header, events = opl_trace.parse(f.read())
    return header, events


def capture_files(path):
    """The `.dro` files in a capture directory, sorted (gp-pads holds two)."""
    if os.path.isfile(path):
        return [path]
    if not os.path.isdir(path):
        return []
    return [os.path.join(path, f) for f in sorted(os.listdir(path))
            if f.lower().endswith('.dro')]


def poll_x_frame(capture_dir):
    path = os.path.join(capture_dir, 'poll.log')
    if not os.path.isfile(path):
        return None
    last = None
    with open(path) as f:
        for line in f:
            p = line.split()
            if p and p[0] == 'X':
                for kv in p[1:]:
                    if kv.startswith('f='):
                        last = int(kv[2:], 16)
    return last


def first_music_key_on(events):
    """The first key-on after a write gap > 1000 ms (the music after the boot,
    record §M1.6.2). The title fragment's own first key-on is not music: the
    boot's notes are contiguous, and the next section begins after the gap."""
    prev_ms = None
    gap_start = None
    for ms, reg, val in events:
        if prev_ms is not None and ms - prev_ms > 1000:
            gap_start = ms
        if gap_start is not None and is_key_on(reg, val):
            return ms
        prev_ms = ms
    return None


def _shadow_from(events, upto):
    shadow = bytearray(SHADOW_REGS)
    for k in range(upto):
        _, reg, val = events[k]
        shadow[reg & (SHADOW_REGS - 1)] = val
    return shadow


def _port_attr_map(section):
    """(relative tick, register) -> the MIDI channel of the port's raw write.
    The capture records no channel; the port knows the voice's. The last write
    at a (tick, register) wins, as the title oracle's linear scan does."""
    at = section.anchor_tick
    attrs = {}
    for _f, t, _w, reg, _val, attr in section.writes[section.anchor_idx:]:
        rel = t - at
        if rel >= 0:
            attrs[(rel, reg)] = attr
    return attrs


def _reduce_port(section, shadow):
    at = section.anchor_tick
    out = []
    for _f, t, _w, reg, val, attr in section.writes[section.anchor_idx:]:
        rel = t - at
        if rel < 0:
            continue
        sh = reg & (SHADOW_REGS - 1)
        excluded = documented_excluded(reg, attr)
        if val == shadow[sh]:
            continue
        # The shadow tracks the hardware state even when the register is not
        # compared: an excluded write still leaves its value in the chip, and
        # the capture's own write-on-change suppression sees that value. (A
        # shadow that skipped excluded writes desynchronised the two sides and
        # made a later compared write appear or vanish; record §M4.)
        shadow[sh] = val
        if excluded:
            continue
        out.append((rel, reg, val, attr))
    return out


def _reduce_capture(events, anchor_idx, attrs, shadow):
    anchor_ms = events[anchor_idx][0]
    out = []
    for ms, reg, val in events[anchor_idx:]:
        rel = ((ms - anchor_ms) * TICKS_PER_SECOND + 500) // 1000
        attr = attrs.get((rel, reg), 0xFF)
        sh = reg & (SHADOW_REGS - 1)
        excluded = documented_excluded(reg, attr)
        if val == shadow[sh]:
            continue
        shadow[sh] = val
        if excluded:
            continue
        out.append((rel, reg, val, attr))
    return out


def _by_tick(stream):
    d = {}
    for rel, reg, val, _ in stream:
        d.setdefault(rel, []).append((reg, val))
    for k in d:
        d[k].sort()
    return d


def _tick_diff(port_ticks, capture_ticks, lo, hi):
    """First relative tick in [lo, hi) whose multiset differs, with both sides."""
    for rel in range(lo, hi):
        if port_ticks.get(rel, []) != capture_ticks.get(rel, []):
            return rel, port_ticks.get(rel, []), capture_ticks.get(rel, [])
    return None


def _fmt_tick(ws):
    if not ws:
        return '<none>'
    return ' '.join('%#04x=%#04x' % (r, v) for r, v in ws)


# The capture's DRO ms is the emulator's accumulated inter-write delay; the
# same selmo note-off is at 479..484 ms in three captures (record §M4), so a
# write within ~0.6 tick of a 1/120 s boundary rounds to an adjacent label. The
# declared whole-tick model, implemented by align_groups: a tick group (or one
# write of it) may match one tick earlier or later in the capture. The write
# content, per-tick multiset and order across ticks are exact; the order
# *within* a tick is the declared intra-tick model.


class AlignResult:
    def __init__(self):
        self.ok = True
        self.jittered = 0
        self.claimed = 0
        self.first_tick = None
        self.first_port = None
        self.first_cap = None
        self.reason = None


def _counter(ws):
    import collections
    return collections.Counter(ws)


def align_groups(port_groups, cap_groups):
    """Compare the port's claimed tick groups against the capture's groups.

    Groups are sorted `(register, value)` multisets, so the declared intra-tick
    write-order model needs no per-write transposition rule. A group may match
    the capture's group at the same tick, at the next tick, or the previous
    one (the declared one-tick capture-label slack); a single write may move
    between adjacent groups by the same slack. A dropped, extra or wrong
    write has no matching group and fails at its tick.

    `port_groups` are the claimed ticks 0..N-1; `cap_groups` go to N so a
    claimed write whose capture label is one tick late can still match."""
    r = AlignResult()
    P = [sorted(g) for g in port_groups]
    C = [sorted(g) for g in cap_groups]
    i = j = 0
    n = len(P)
    while i < n:
        pg = sorted(P[i])
        cg = sorted(C[j]) if j < len(C) else []
        if pg == cg:
            i += 1
            j += 1
            r.claimed += 1
            continue
        nxt_p = sorted(P[i + 1]) if i + 1 < len(P) else []
        nxt_c = sorted(C[j + 1]) if j + 1 < len(C) else []
        # A whole group one tick later in the capture.
        if pg and pg == nxt_c:
            i += 1
            j += 2
            r.claimed += 1
            r.jittered += 1
            continue
        # A whole later port group against the capture's current group.
        if cg and cg == nxt_p:
            i += 2
            j += 1
            r.claimed += 1
            r.jittered += 1
            continue
        # One write whose capture label is one tick later: move it back.
        dp = _counter(pg) - _counter(cg)
        dc = _counter(cg) - _counter(pg)
        if sum(dp.values()) == 1 and not dc:
            w = next(iter(dp))
            if w in nxt_c:
                nxt_c.remove(w)
                C[j + 1] = nxt_c
                C[j] = sorted(cg + [w])
                r.jittered += 1
                continue
        # One write whose capture label is one tick earlier: move it forward.
        if sum(dc.values()) == 1 and not dp:
            w = next(iter(dc))
            # Only a write the port actually has one tick later may move: a
            # dropped write must not walk into the capture's tail.
            if j + 1 < len(C) and w in nxt_p:
                C[j] = sorted(x for x in cg if x != w)
                C[j + 1] = sorted(nxt_c + [w])
                r.jittered += 1
                continue
        r.ok = False
        r.first_tick = i
        r.first_port = pg
        r.first_cap = cg
        r.reason = 'tick groups differ'
        return r
    return r


class SectionResult:
    def __init__(self, section, index):
        self.section = section
        self.index = index
        self.status = 'unanchored'
        self.capture_path = None
        self.anchor_ms = None
        self.anchor_event = None
        self.claimed = 0
        self.boundary_rel = None
        self.boundary_equal = None
        self.boundary_note = None
        self.first_diff = None
        self.first_diff_kind = None
        self.first_diff_port = None
        self.first_diff_cap = None
        self.first_transposition = None
        self.jittered = 0
        self.per_tick_equal = 0
        self.reduced = 0
        self.reason = None


def compare_section(section, index, captures, allow_unanchored, allow_diff):
    res = SectionResult(section, index)
    if section.anchor_idx is None:
        res.status = 'no-key-on'
        res.reason = 'the section has no key-on to anchor'
        return res
    at = section.anchor_tick
    last_rel = section.last_tick - at
    if last_rel < 1:
        res.status = 'no-claim'
        res.reason = 'the section is one tick; nothing before its boundary tick'
        return res
    attrs = _port_attr_map(section)
    key_w = section.writes[section.anchor_idx]
    cands = []
    for ci, (path, _header, events) in enumerate(captures):
        for j, (ms, reg, val) in enumerate(events):
            if reg == key_w[3] and val == key_w[4] and is_key_on(reg, val):
                cands.append((ci, j, ms))
    prefix = min(ANCHOR_TICKS, last_rel)
    for ci, j, ms in cands:
        path, _header, events = captures[ci]
        shadow = _shadow_from(events, j)
        pw = _reduce_port(section, bytearray(shadow))
        cw = _reduce_capture(events, j, attrs, bytearray(shadow))
        pd = _by_tick(pw)
        cd = _by_tick(cw)
        # The anchor prefix: the port's first `prefix` ticks against the
        # capture's groups at the same ticks (a candidate that cannot match
        # them is a repeated note elsewhere, not this section).
        pre = align_groups([pd.get(t, []) for t in range(prefix)],
                           [cd.get(t, []) for t in range(prefix + 1)])
        if not pre.ok:
            continue
        res.status = 'anchored'
        res.capture_path = path
        res.anchor_ms = ms
        res.anchor_event = j
        res.boundary_rel = last_rel
        res.boundary_equal = pd.get(last_rel, []) == cd.get(last_rel, [])
        # The section's final tick is the request boundary, not claimed.
        groups_p = [pd.get(t, []) for t in range(last_rel)]
        groups_c = [cd.get(t, []) for t in range(last_rel + 1)]
        al = align_groups(groups_p, groups_c)
        res.claimed = al.claimed
        res.jittered = al.jittered
        res.reduced = sum(len(groups_p[t]) for t in range(min(al.claimed, last_rel)))
        eq = _tick_diff(pd, cd, 0, last_rel)
        res.per_tick_equal = eq[0] if eq is not None else last_rel
        if not al.ok:
            # A first difference in the last boundary window is the request
            # boundary itself (the section ends on a game frame, the capture's
            # stop burst can be a tick earlier or later): report it, do not
            # claim that tick. Anything earlier is a real difference.
            if al.first_tick >= last_rel - 1:
                res.status = 'anchored'
                res.boundary_note = ('the first group difference is at the '
                                     'request boundary tick %d (not claimed)'
                                     % al.first_tick)
            elif section.bank in allow_diff:
                res.status = 'diff-allowed'
                res.first_diff_kind = al.reason
                res.first_diff = al.first_tick
                res.first_diff_port = al.first_port
                res.first_diff_cap = al.first_cap
                res.reason = ('the measured capture-side register/write-on-change '
                              'artifact at this section (allowed by name)')
            else:
                res.status = 'diff'
                res.first_diff_kind = al.reason
                res.first_diff = al.first_tick
                res.first_diff_port = al.first_port
                res.first_diff_cap = al.first_cap
        return res
    if section.bank in allow_unanchored:
        res.status = 'unanchored-allowed'
        res.reason = ('no anchor in the capture for this bank (allowed by name: '
                      'the measured section-boundary voice-cursor gap)')
    else:
        res.status = 'unanchored'
        res.reason = ('no capture key-on %#04x=%#04x with matching first %d ticks '
                      '(%d candidate%s)' % (key_w[3], key_w[4], prefix,
                                            len(cands), '' if len(cands) == 1 else 's'))
    return res


def summarize_section(res, index_names):
    s = res.section
    name = resource_name(s.dc, index_names) if s.dc else 'boot'
    head = 'section %d %s bank=%08X' % (res.index, name, s.bank)
    if res.status == 'anchored':
        extra = ('anchor capture %d ms (frame %d tick %d); claimed ticks 0..%d '
                 '(%d ticks, %d reduced writes)'
                 % (res.anchor_ms, s.writes[s.anchor_idx][0], s.anchor_tick,
                    res.claimed - 1, res.claimed, res.reduced))
        extra += '; per-tick multiset equal through tick %d' % (res.per_tick_equal - 1)
        if res.per_tick_equal == 0:
            extra = extra.replace('equal through tick -1', 'not equal at tick 0')
        if res.jittered:
            extra += '; %d declared whole-tick label slack move(s)' % res.jittered
        if res.boundary_note:
            extra += '; ' + res.boundary_note
        extra += ('; boundary tick %d %s (not claimed)'
                  % (res.boundary_rel, 'equal' if res.boundary_equal else 'differs'))
        return head + ': ' + extra
    if res.status in ('diff', 'diff-allowed'):
        return (head + ': FIRST DIFFERENCE (%s) at port relative tick %d: '
                'port [%s] vs capture [%s]%s'
                % (res.first_diff_kind, res.first_diff,
                   _fmt_tick(res.first_diff_port), _fmt_tick(res.first_diff_cap),
                   ' (DIFF ALLOWED named gap: ' + res.reason + ')'
                   if res.status == 'diff-allowed' else ''))
    if res.status == 'unanchored-allowed':
        return head + ': UNANCHORED named gap (' + res.reason + ')'
    return head + ': ' + res.status.replace('-', ' ') + ' (' + str(res.reason) + ')'


def compare_trace(name, port_path, capture_paths, min_ticks=0,
                  allow_unanchored=(), allow_diff=(), game_dir=None,
                  verbose=False, json_out=None):
    sections, _s_lines = parse_port(port_path)
    captures = []
    for path in capture_paths:
        if not os.path.exists(path):
            continue
        try:
            header, events = parse_capture(path)
        except (OSError, ValueError) as exc:
            print('audio-compare: %s: cannot decode %s: %s' % (name, path, exc))
            return 2
        captures.append((path, header, events))
    if not captures:
        print('audio-compare: SKIP %s: no capture at %s'
              % (name, ', '.join(capture_paths)))
        return 3
    index_names = read_index_names(game_dir)
    allow = set(allow_unanchored)
    allow_d = set(allow_diff)
    results = []
    total = 0
    failed = []
    named_gaps = []
    print('audio-compare: %s (capture %s)'
          % (name, ', '.join(os.path.basename(p) for p, _, _ in captures)))
    for i, s in enumerate(sections):
        if s.anchor_idx is None and s.dc == 0:
            continue                      # the boot's two OPL reset writes
        r = compare_section(s, i, captures, allow, allow_d)
        results.append(r)
        print('  ' + summarize_section(r, index_names))
        if r.status == 'anchored':
            total += r.claimed
        elif r.status == 'diff':
            failed.append('section %s first difference (%s) at relative tick %d'
                          % (resource_name(s.dc, index_names), r.first_diff_kind,
                             r.first_diff))
        elif r.status in ('unanchored-allowed', 'diff-allowed'):
            named_gaps.append('%08X' % s.bank)
        elif r.status == 'no-claim':
            # A one-tick section (the replay ends at the next song's start) has
            # no claim to make; it is reported by name, and --min-ticks is what
            # stops a trace shrinking to nothing.
            named_gaps.append('%08X' % s.bank)
        elif r.status in ('unanchored', 'no-key-on'):
            failed.append('section %s %s (%s)'
                          % (resource_name(s.dc, index_names), r.status, r.reason))
        if verbose:
            print('    writes=%d anchor_tick=%s last_tick=%s'
                  % (len(s.writes), s.anchor_tick, s.last_tick))
    print('audio-compare: %s: %d compared ticks over %d section(s); %d named gap(s) %s'
          % (name, total, sum(1 for r in results if r.status == 'anchored'),
             len(named_gaps), ('[' + ' '.join(named_gaps) + ']') if named_gaps else ''))
    if total < min_ticks:
        failed.append('claimed %d ticks, pinned minimum %d' % (total, min_ticks))
    ok = not failed
    print('audio-compare: %s: %s' % (name, 'ok' if ok else 'FAIL'))
    for f in failed:
        print('audio-compare: %s: FAIL: %s' % (name, f))
    if json_out:
        rec = {
            'name': name,
            'ok': ok,
            'total_ticks': total,
            'min_ticks': min_ticks,
            'sections': len(results),
            'compared': sum(1 for r in results if r.status == 'anchored'),
            'named_gaps': named_gaps,
            'captures': [os.path.basename(p) for p, _, _ in captures],
            'failed': failed,
        }
        parent = os.path.dirname(json_out)
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(json_out, 'w') as f:
            f.write(json.dumps(rec, sort_keys=True) + '\n')
    return 0 if ok else 1


def cmd_compare(a):
    name = a.name or os.path.basename(a.port)
    min_ticks = a.min_ticks
    allow = [int(x, 16) for x in a.allow_unanchored.split(',') if x]
    allow_diff = [int(x, 16) for x in a.allow_diff.split(',') if x]
    pin = TRACE_PINS.get(name)
    if pin is not None:
        if min_ticks == 0:
            min_ticks = pin[0]
        allow = list(pin[1]) + allow
        allow_diff = list(pin[2]) + allow_diff
    return compare_trace(name, a.port, a.capture, min_ticks, allow, allow_diff,
                         a.game_dir, a.verbose, a.json_out)


def cmd_capture_end(a):
    x = poll_x_frame(a.capture)
    if x is None:
        print(0)
        return 0
    print(min(a.max, x) if a.max is not None else x)
    return 0


def cmd_summary(a):
    """The covered/missing inventory, read from the per-trace reports the
    oracle writes (`compare --json-out`) and the capture tree."""
    rows = {}
    if a.jsonl and os.path.exists(a.jsonl):
        with open(a.jsonl) as f:
            for line in f:
                line = line.strip()
                if line:
                    rec = json.loads(line)
                    rows[rec['name']] = rec
    if a.reports and os.path.isdir(a.reports):
        import glob
        for path in sorted(glob.glob(os.path.join(a.reports, '*', 'report.json'))):
            try:
                with open(path) as f:
                    rec = json.load(f)
            except (OSError, ValueError):
                continue
            rows.setdefault(rec.get('name', ''), rec)
    missing, present = [], []
    total_files = 0
    music_files = 0
    for name in GP_TRACES:
        files = capture_files(os.path.join(a.captures, name))
        if not files:
            missing.append(name)
            continue
        present.append((name, files))
        total_files += len(files)
        for path in files:
            try:
                _h, events = parse_capture(path)
            except (OSError, ValueError):
                continue
            if first_music_key_on(events) is not None:
                music_files += 1
    others = []
    for name in OTHER_K11_TRACES:
        others += capture_files(os.path.join(a.captures, name))
    audio = capture_files(a.audio_captures) if a.audio_captures else []
    print('audio-compare: gameplay inventory: %d traces, %d .dro files '
          '(%d with a music section), %d director%s absent'
          % (len(GP_TRACES), total_files, music_files, len(missing),
             'y' if len(missing) == 1 else 'ies'))
    for name, files in present:
        rec = rows.get(name)
        if rec is None:
            print('  %-24s %d file(s): NOT RUN' % (name, len(files)))
            continue
        if rec.get('skipped'):
            print('  %-24s %d file(s): SKIP (%s)' % (name, len(files), rec['skipped']))
            continue
        gaps = (' named-gap ' + ' '.join(rec['named_gaps'])) if rec['named_gaps'] else ''
        print('  %-24s %d file(s): %s %d ticks (pinned %d)%s'
              % (name, len(files), 'ok' if rec['ok'] else 'FAIL',
                 rec['total_ticks'], rec['min_ticks'], gaps))
    for name in missing:
        print('  %-24s SKIP (no capture directory)' % name)
    measured = [r['total_ticks'] for r in rows.values() if not r.get('skipped')]
    if measured:
        print('audio-compare: minimum measured claim: %d ticks' % min(measured))
    print('audio-compare: not gameplay: %d k11 .dro file(s) (%s)'
          % (len(others),
             ', '.join(os.path.relpath(p, a.captures) for p in others) or 'none'))
    print('audio-compare: not gameplay: %d audio-capture .dro file(s) (%s)'
          % (len(audio), ', '.join(os.path.basename(p) for p in audio) or 'none'))
    return 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)

    c = sub.add_parser('compare', help='compare one port dump against its capture file(s)')
    c.add_argument('--name')
    c.add_argument('--port', required=True)
    c.add_argument('--capture', action='append', required=True)
    c.add_argument('--min-ticks', type=int, default=0)
    c.add_argument('--allow-unanchored', default='')
    c.add_argument('--allow-diff', default='')
    c.add_argument('--game-dir')
    c.add_argument('--json-out')
    c.add_argument('--verbose', action='store_true')
    c.set_defaults(func=cmd_compare)

    e = sub.add_parser('capture-end', help='the capture X frame, capped (for --end)')
    e.add_argument('--capture', required=True)
    e.add_argument('--max', type=int)
    e.set_defaults(func=cmd_capture_end)

    s = sub.add_parser('summary', help='the covered/missing trace inventory')
    s.add_argument('--captures', required=True)
    s.add_argument('--audio-captures')
    s.add_argument('--jsonl')
    s.add_argument('--reports')
    s.set_defaults(func=cmd_summary)

    t = sub.add_parser('self-test', help='point at the unit test module')
    t.set_defaults(func=lambda _a: (print('self-test: python3 -m unittest '
                                        'tools.tests.test_audio_compare'), 0)[1])

    a = ap.parse_args(argv)
    return a.func(a)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

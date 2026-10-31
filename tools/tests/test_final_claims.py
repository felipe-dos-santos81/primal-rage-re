# tools/tests/test_final_claims.py
"""Guard: the terminal claim register and the AGENTS gap statements agree.

Final truth Task 8 made `docs/superpowers/specs/2026-10-30-final-claims.md` the
single source of truth for every remaining gap. This test pins:

* the required register rows, their status and one evidence token each (a
  missing row, a changed status or a lost address/capture fails);
* every terminal/declared row's evidence is an address, a file:line, a
  `data/k11-captures/...` capture or a `.failed` artifact (a prose-only terminal
  row fails);
* the measured counters (the differential, coverage, capture inventory, pin
  digest and ported figures) are present in the register;
* every `AGENTS.md` gap statement has a mapped register row, and the narrow
  phrases that introduce a gap statement ("stays declared", "declared
  terminal", "INCONCLUSIVE", ...) all carry a mapped anchor (a new unmapped gap
  statement fails).

The mutation cases at the bottom run the same checkers on mutated copies, so
the assertions are able to fail without editing the real files.
"""
import os
import re
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REGISTER_REL = 'docs/superpowers/specs/2026-10-30-final-claims.md'
AGENTS_REL = 'AGENTS.md'

STATUSES = ('closed', 'gated', 'terminal', 'declared')

# id -> (expected status, one evidence token that must be in the evidence cell).
# The tokens are address/file/capture fragments the register's §2 rows carry.
EXPECTED = {
    'R01': ('closed', ('config.c', 'diff_verify.py')),
    'R02': ('terminal', ('gp-u13-band-dac', '56b900b1')),
    'R03': ('declared', ('gp-u14-twop-win.failed', '0x13A8')),
    'R04': ('terminal', ('gp-u14-win-live.failed', '0x73F')),
    'R05': ('declared', ('0x4B470', '0x4BD98')),
    'R06': ('closed', ('gp-bowling-2', '0x832')),
    'R07': ('closed', ('gp-u29-continue-taken', '0x2791C')),
    'R08': ('closed', ('gp-u28-continue', '0x18C2')),
    'R09': ('gated', ('TRACE_CLAIMED_BANKS', '29,656')),
    'R10': ('terminal', ('TRACE_RESIDUE', '1,437')),
    'R11': ('declared', ('0x0A8C', '0x05E4')),
    'R12': ('declared', ('0x37640', '0x24559')),
    'R13': ('terminal', ('port/src/platform/audio/opl/opl.c', '49,716')),
    'R14': ('terminal', ('gp-u17-device2', 'H6.9')),
    'R15': ('terminal', ('0x1531', '0x0EAE')),
    'R16': ('declared', ('gp-u23-u16-finisher', '0x27')),
    'R17': ('terminal', ('1033/1033', '810/845')),
    'R18': ('terminal', ('675', '0x29D60')),
    'R19': ('terminal', ('0x2EA68', 'S.10')),
    'R20': ('terminal', ('759 portable', '315')),
    'R21': ('terminal', ('K11_OPEN_END', 'G.16')),
    'R22': ('terminal', ('gp_compare', 'G.16')),
    'R23': ('terminal', ('0x29C78', '0x37DD4')),
    'R24': ('terminal', ('SCAN3_ALLOWED', 'K.10')),
    'R25': ('terminal', ('180', 'gp-u18-talon-ending')),
    'R26': ('terminal', ('W.13', 'gp-u10-ending')),
    'R27': ('declared', ('43/44', 'gp-u14-win-live')),
    'R28': ('declared', ('M6.6', 'music-truth')),
    'R29': ('terminal', ('TRACE_PREFIX_PINS', '29,656')),
    'R30': ('declared', ('0x4BD98', 'flow.c:1632')),
    'R31': ('terminal', ('UNROWED_CENSUS', 'AGENTS.md:511')),
    'R32': ('terminal', ('0x29D60', '0x2EA30')),
}

# Counter strings that must appear in the register (its §3 and §2 rows).
MUST_MENTION = (
    '1033/1033', '14993/14993', '825/845', '810/845', 'WEAKER` 12',
    '8 unrowed', 'HIT 675 / NOT-HIT 80 / UNINSTRUMENTED 4',
    'd3b6969f60a489c910ca1fa0f8daafb0dbead9b0fc172dda193260d3b6cd3c07',
    '43/44', '798 1203 66', '759 759 100', '44 explicit paths',
    '0x29D60', '0x2EA30', '0x2EA64', '0x500BB',
    '0x37FF6', '0x2587C', '0x4E11C', '0x2791C', '0x2CADC', '0x0EAE',
    '0x1531', '66% raw',
)

# Every AGENTS gap statement, mapped to its register row. Each phrase must be
# present in AGENTS.md and its row must exist. A phrase that disappears is a
# stale pin; a row that disappears is a missing register row.
AGENTS_ANCHORS = (
    ('appears are not claimed** (a named gap', 'R22'),
    ('named gap stays its cause, not isolated', 'R24'),
    ('U9/U10 captures cover character 0', 'R23'),
    ('while the U14 live-win remains a distinct gap', 'R04'),
    ('terminal with the measured arcade damage race', 'R04'),
    ('**Named gaps:**', 'R13'),
    ('the 26 code-reachable banks are claimed', 'R09'),
    ('10 are requested', 'R10'),
    ('requested at runtime but unattributable', 'R11'),
    ('§H6.9\'s named gap', 'R14'),
    ('a separate label-slack/stop named gap', 'R25'),
    ('Task 6 is declared, not taken', 'R28'),
    ('`0x21` (volleyball) stays declared', 'R05'),
    ('old "stays declared / never opened / no oracle', 'R06'),
    ('The proof debt is declared terminal', 'R17'),
    ('is declared; **U14 R1**', 'R14'),
    ('declared absent', 'R27'),
    ('declared terminal difference on `0x2EA68`', 'R19'),
    ('gaps (record §S.10 and §S.8\'s PLAUSIBLE items, declared terminal by', 'R19'),
    ('a declared terminal difference, or a declared byte difference', 'R17'),
    ('every one is a declared terminal with a per-row reason', 'R17'),
    ('is **PASS-A terminal**', 'R02'),
    ('5120 band', 'R02'),
    ('The movie audio needs no port', 'R19'),
    ('The storage layer is ported', 'R19'),
    ('is now published', 'R06'),
    ('Task 5 measured it', 'R06'),
    ('taken-continue `0x2791C`', 'R07'),
    ('gp-u29-continue-taken', 'R07'),
    ('The continue screen, countdown and expiry are captured and gated', 'R08'),
    ('U16 R2', 'R15'),
    ('U16 finisher', 'R16'),
    ('U17', 'R14'),
    ('1033/1033', 'R17'),
    ('HIT 675 / NOT-HIT 80', 'R18'),
    ('0x29D60', 'R18'),
    ('0x2CADC', 'R01'),
    ('0x7714', 'R12'),
    ('0x1CCB0', 'R12'),
    ('0x0A8C', 'R11'),
    ('0x05E4', 'R11'),
    ('rendered audibility', 'R13'),
    ('host-owned/deferred', 'R20'),
    ('759 of 759', 'R20'),
    ('memsize=64', 'R26'),
    ('43/44', 'R27'),
    ('gp-u14-win-live.failed', 'R04'),
    ('gp-u14-twop-win.failed', 'R03'),
    ('gp-bowling-2', 'R06'),
    ('gp-u13-band-dac', 'R02'),
    ('0x4E11C', 'R06'),
    ('0x2791C', 'R07'),
    ('0x37FF6', 'R04'),
    ('0x2587C', 'R05'),
    ('0x0EAE', 'R15'),
    ('0x1531', 'R15'),
)

# Narrow status phrases that introduce a gap statement. Every AGENTS line
# containing one of these must also contain a mapped anchor above.
TRIGGERS = (
    'Named gaps left by the phase:',
    '**Named gaps:**',
    'stays declared',
    'stays a declared gap',
    'remains a distinct gap',
    'is **INCONCLUSIVE**',
    'declared terminal',
    'declared absent',
    'are declared',
    'is declared',
    'were open',
)

# Evidence that satisfies the terminal/declared rule. A prose-only cell must
# fail; these are the address / file:line / capture / .failed shapes.
EVIDENCE_RE = re.compile(
    r'0x[0-9A-Fa-f]{4,}'                                  # an address
    r'|[\w./-]+\.(?:md|py|c|h|txt|json|sha256|csv)(?::\d+)?'  # a file (line)
    r'|data/k11-captures/[\w.-]+'                          # a capture path
    r'|gp-[a-z0-9-]+'                                     # a capture/scenario
    r'|TRACE_[A-Z_]+'                                     # a pinned table
    r'|K11_[A-Z_]+'                                       # a K11 pin
    r'|memsize=64'
)


def parse_register(text):
    """Return (rows, problems). rows: id -> dict of the six cells."""
    rows = {}
    problems = []
    for lineno, line in enumerate(text.splitlines(), 1):
        s = line.strip()
        if not re.match(r'^\|\s*R\d+\s*\|', s):
            continue
        cells = [c.strip() for c in s.strip('|').split('|')]
        if len(cells) != 6:
            problems.append('register:%d: row has %d cells, want 6'
                            % (lineno, len(cells)))
            continue
        rid, gap, status, evidence, reason, residual = cells
        if rid in rows:
            problems.append('register:%d: duplicate row %s' % (lineno, rid))
            continue
        if status not in STATUSES:
            problems.append('register:%d: row %s has status %r (want one of %s)'
                            % (lineno, rid, status, ', '.join(STATUSES)))
        if not gap or not evidence or not reason or not residual:
            problems.append('register:%d: row %s has an empty cell' % (lineno, rid))
        rows[rid] = {'gap': gap, 'status': status, 'evidence': evidence,
                     'reason': reason, 'residual': residual, 'line': lineno}
    return rows, problems


def check_register(text):
    """Return the list of problems; empty means the register is clean."""
    rows, problems = parse_register(text)

    for rid, (status, tokens) in EXPECTED.items():
        row = rows.get(rid)
        if row is None:
            problems.append('register: missing required row %s' % rid)
            continue
        if row['status'] != status:
            problems.append('register:%d: %s status %r, expected %r'
                            % (row['line'], rid, row['status'], status))
        for token in tokens:
            if token not in row['evidence']:
                problems.append('register:%d: %s evidence lacks %r'
                                % (row['line'], rid, token))

    for rid, row in rows.items():
        if row['status'] in ('terminal', 'declared'):
            if not EVIDENCE_RE.search(row['evidence']):
                problems.append(
                    'register:%d: %s is %s but its evidence names no address, '
                    'file:line, capture or .failed'
                    % (row['line'], rid, row['status']))

    for token in MUST_MENTION:
        if token not in text:
            problems.append('register: measured token %r is absent' % token)

    if re.search(r'\|\s*open\s*\|', text):
        problems.append('register: an "open" status row exists')
    return problems


def check_agents(text, register_text):
    """Return the list of problems between AGENTS and the register."""
    rows, problems = parse_register(register_text)
    anchor_phrases = []
    for phrase, rid in AGENTS_ANCHORS:
        anchor_phrases.append(phrase)
        if phrase not in text:
            problems.append('AGENTS: gap anchor %r is absent' % phrase)
        if rid not in rows:
            problems.append('AGENTS: anchor %r maps to missing register row %s'
                            % (phrase, rid))

    for lineno, line in enumerate(text.splitlines(), 1):
        if any(t in line for t in TRIGGERS):
            if not any(p in line for p in anchor_phrases):
                problems.append(
                    'AGENTS:%d: gap statement has no register anchor: %r'
                    % (lineno, line.strip()[:120]))
    return problems


class TestFinalClaims(unittest.TestCase):
    def _read(self, rel):
        with open(os.path.join(ROOT, rel), 'r', errors='ignore') as f:
            return f.read()

    def test_the_register_and_agents_pass(self):
        register = self._read(REGISTER_REL)
        agents = self._read(AGENTS_REL)
        self.assertEqual(check_register(register), [])
        self.assertEqual(check_agents(agents, register), [])

    def test_the_register_is_the_readme_pointer(self):
        readme = self._read('README.md')
        agents = self._read(AGENTS_REL)
        self.assertIn(REGISTER_REL, readme)
        self.assertIn(REGISTER_REL, agents)

    # --- the mutation cases (the assertion can fail) ---------------------

    def test_a_missing_row_fails(self):
        register = self._read(REGISTER_REL)
        lines = [l for l in register.splitlines()
                 if not l.strip().startswith('| R04 |')]
        self.assertTrue(check_register('\n'.join(lines)))

    def test_an_unsupported_terminal_row_fails(self):
        register = self._read(REGISTER_REL)
        bad = register + '\n| R99 | a made-up gap | terminal | none | prose only | none |\n'
        problems = check_register(bad)
        self.assertTrue(any('R99' in p for p in problems), problems)

    def test_a_changed_status_fails(self):
        register = self._read(REGISTER_REL)
        bad = register.replace('| R04 | U14 R1 arcade fought win | terminal |',
                               '| R04 | U14 R1 arcade fought win | declared |')
        self.assertNotEqual(bad, register)
        self.assertTrue(check_register(bad))

    def test_a_missing_counter_fails(self):
        register = self._read(REGISTER_REL)
        bad = register.replace('810/845', '809/845')
        self.assertNotEqual(bad, register)
        self.assertTrue(check_register(bad))

    def test_a_new_unmapped_agents_gap_line_fails(self):
        agents = self._read(AGENTS_REL)
        register = self._read(REGISTER_REL)
        bad = agents + '\n- **A made-up mode stays declared.**\n'
        problems = check_agents(bad, register)
        self.assertTrue(any('no register anchor' in p for p in problems),
                        problems)

    def test_a_stale_agents_anchor_fails(self):
        agents = self._read(AGENTS_REL)
        register = self._read(REGISTER_REL)
        bad = agents.replace('The proof debt is declared terminal',
                             'The proof debt was declared terminal')
        self.assertNotEqual(bad, agents)
        self.assertTrue(check_agents(bad, register))


if __name__ == '__main__':
    unittest.main()

# tools/tests/test_port_progress.py
"""The classification-truth counting rule (plan Task 4, record-§C4).

Every `FN_` address has exactly one class — ported / host-owned / deferred /
replaced / runtime — and the ported registry is the strict column-0-header /
`fn_register` / declared-exception rule `tools/diff_verify.py`'s closure rule
shares. `0x62003` (the runtime exit chain) must not be credited; the measured
raw/portable counts are the regression gate; and no inline credit may be left
undeclared.
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import port_progress as pp

CLASSES = ('ported', 'host-owned', 'deferred', 'replaced', 'runtime')


class TestPortProgress(unittest.TestCase):
    def test_62003_is_not_credited(self):
        # The runtime exit chain has no column-0 header, no fn_register and no
        # declared exception, so the strict registry does not credit it.
        self.assertNotIn(0x62003, pp.ported_set())

    def test_62003_is_not_a_declared_exception(self):
        # The exception table must not name the exit chain; adding it would be
        # the one-liner that silently re-credits it.
        self.assertNotIn(0x62003, pp.PORT_PROGRESS_EXCEPTIONS)

    def test_reverting_the_12484_header_drops_it(self):
        # Mutation proof: the credit is the column-0 header, not the trailing
        # call-site comment. Reverting `/* 0x12484 —` to the old `/* PORT:
        # 0x12484.` note drops 0x12484 from the registry (and the raw count).
        text = pp.port_tree_text().replace('/* 0x12484 —', '/* PORT: 0x12484.')
        self.assertNotIn(0x12484, pp.ported_set(text))

    def test_every_fn_has_exactly_one_class(self):
        classes = pp.class_map()
        self.assertEqual(set(classes), set(pp.fn_addresses()))
        for a, c in classes.items():
            self.assertIn(c, CLASSES)

    def test_replaced_and_classification_invariants(self):
        cls = pp.classification()
        self.assertTrue(all(c in ('host-owned', 'deferred', 'replaced')
                            for c in cls.values()))
        replaced = {a for a, c in cls.items() if c == 'replaced'}
        self.assertTrue(replaced, 'the replaced set must be non-empty')
        # every replaced address is in the runtime region, so the portable
        # denominator is unaffected.
        self.assertTrue(all(a >= pp.RUNTIME_BASE for a in replaced))

    def test_every_inline_credit_is_declared(self):
        # Every trailing `/* 0xADDR` credit without a column-0 header or
        # fn_register must be a declared PORT_PROGRESS_EXCEPTIONS entry or an
        # address the classification declares host-owned/deferred/replaced.
        undeclared = pp.inline_credits() - set(pp.PORT_PROGRESS_EXCEPTIONS)
        self.assertFalse(undeclared, 'undeclared inline credits: %s' %
                         ' '.join('%05X' % a for a in sorted(undeclared)))

    def test_the_measured_counts(self):
        raw, n, p_done, p_total, breakdown = pp.counts()
        self.assertEqual((raw, n), (798, 1203))
        self.assertEqual((p_done, p_total), (759, 759))
        self.assertEqual(breakdown['replaced'], 37)
        self.assertEqual(breakdown['runtime'], 315)

    def test_removing_a_replaced_line_is_detected(self):
        # Mutation proof: the class map reads the classification file, not a
        # hardcoded 37 — dropping one replaced line re-classifies it as runtime.
        cls = pp.classification()
        a = next(a for a, c in cls.items() if c == 'replaced')
        mutated = {k: v for k, v in cls.items() if k != a}
        self.assertEqual(pp.class_map(cls=mutated)[a], 'runtime')
        self.assertEqual(sum(1 for c in pp.class_map(cls=mutated).values()
                             if c == 'replaced'), 36)


if __name__ == '__main__':
    unittest.main()

# tools/tests/test_coverage_census.py
"""The coverage census's tests (plan 2026-10-28-state-coverage, Task 3).

The census classifies every portable address HIT / NOT-HIT / UNINSTRUMENTED from
the instrumentation scan (not the dump); `--check` fails on a new NOT-HIT and
prints `improved:` / exits 2 on a new hit until `--update-baseline --reason`
re-pins; a missing capture is reported and excluded, never a NOT-HIT; and the
`.c`-header guard fails on a column-0 header with no counter. Each behaviour is
fed a synthetic universe, so the test proves the decision logic independent of
the live port tree, plus two live-tree gates (the guard passes; the four
no-body exceptions are the only uninstrumented portable addresses).
"""
import io
import os
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import coverage_census as cc
import port_progress as pp


def three_addrs():
    """A synthetic universe: a0 instrumented+hit, a1 instrumented+unhit, a2 an
    uninstrumented declared exception, a3 an uninstrumented undeclared address."""
    return 0x10000, 0x10001, 0x10002, 0x10003


class TestClassify(unittest.TestCase):
    def test_split_is_decided_by_the_scan_not_the_dump(self):
        a0, a1, a2, _ = three_addrs()
        # a0 is hit, a1 is instrumented but has no dump entry, a2 is a declared
        # exception. The dump only records a0.
        classes, failures = cc.classify(
            hit_addrs={a0}, instrumented={a0, a1}, portable={a0, a1, a2},
            exceptions={a2})
        self.assertEqual(classes, {a0: 'HIT', a1: 'NOT-HIT', a2: 'UNINSTRUMENTED'})
        self.assertEqual(failures, set())

    def test_an_instrumented_address_with_no_dump_entry_is_not_hit(self):
        a0, a1, _, _ = three_addrs()
        classes, _ = cc.classify({a0}, {a0, a1}, {a0, a1}, {})
        self.assertEqual(classes[a1], 'NOT-HIT')

    def test_a_declared_exception_is_uninstrumented(self):
        _, _, a2, _ = three_addrs()
        classes, _ = cc.classify(set(), set(), {a2}, {a2})
        self.assertEqual(classes[a2], 'UNINSTRUMENTED')

    def test_an_uninstrumented_undeclared_address_is_a_guard_failure(self):
        _, _, _, a3 = three_addrs()
        classes, failures = cc.classify(set(), set(), {a3}, {})
        self.assertEqual(classes, {})
        self.assertEqual(failures, {a3})


class TestReport(unittest.TestCase):
    def _report(self, dumps, tags, classes, failures=()):
        buf = io.StringIO()
        cc.report(dumps, tags, classes, set(failures), buf)
        return buf.getvalue()

    def test_prints_the_matrix_and_the_split(self):
        a0, a1, a2, _ = three_addrs()
        classes = {a0: 'HIT', a1: 'NOT-HIT', a2: 'UNINSTRUMENTED'}
        out = self._report({'alpha': {a0}, 'beta': {a0, a1}},
                           ['alpha', 'beta'], classes)
        self.assertIn('driver alpha 0x10000\n', out)
        self.assertIn('driver beta 0x10000 0x10001\n', out)
        self.assertIn('coverage: split HIT=1 NOT-HIT=1 UNINSTRUMENTED=1\n', out)
        self.assertIn('hit 0x10000\n', out)
        self.assertIn('not-hit 0x10001\n', out)
        self.assertIn('uninstrumented 0x10002\n', out)

    def test_a_missing_tag_is_reported_and_not_nothit(self):
        a0, a1, _, _ = three_addrs()
        classes = {a0: 'HIT', a1: 'NOT-HIT'}
        # `gone` is expected but has no dump; it must be reported and its
        # absence must not push anything into NOT-HIT (only a1 is NOT-HIT).
        out = self._report({'alpha': {a0}}, ['alpha', 'gone'], classes)
        self.assertIn('missing gone\n', out)
        self.assertIn('coverage: tags present=1 missing=1 total=2\n', out)
        self.assertIn('not-hit 0x10001\n', out)
        self.assertNotIn('0x10000 not-hit', out)


class TestBaseline(unittest.TestCase):
    def test_write_then_read_round_trips_and_skips_comments(self):
        a0, a1, a2, _ = three_addrs()
        classes = {a0: 'HIT', a1: 'NOT-HIT', a2: 'UNINSTRUMENTED'}
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'base.txt')
            cc.write_baseline(path, classes, 'measured on the synthetic set')
            with open(path) as f:
                text = f.read()
            self.assertTrue(text.startswith('# reason: measured on the synthetic set\n'))
            self.assertIn('0x10001 NOT-HIT\n', text)
            self.assertEqual(cc.read_baseline(path), classes)

    def test_compare_baseline_flags_new_nothit_and_new_hit(self):
        a0, a1, _, _ = three_addrs()
        classes = {a0: 'HIT', a1: 'NOT-HIT'}
        # a1 was HIT before -> a regression; a0 was NOT-HIT before -> improved.
        new_nothit, new_hit = cc.compare_baseline(
            classes, {a0: 'NOT-HIT', a1: 'HIT'})
        self.assertEqual(new_nothit, [a1])
        self.assertEqual(new_hit, [a0])

    def test_compare_baseline_ignores_unchanged_classes(self):
        a0, a1, _, _ = three_addrs()
        classes = {a0: 'HIT', a1: 'NOT-HIT'}
        self.assertEqual(cc.compare_baseline(classes, {a0: 'HIT', a1: 'NOT-HIT'}),
                         ([], []))

    def test_read_baseline_skips_comments_and_blanks(self):
        a0, a1, _, _ = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'base.txt')
            with open(path, 'w') as f:
                f.write('# reason: a comment\n\n   \n0x10000 HIT\n0x10001 NOT-HIT\n')
            self.assertEqual(cc.read_baseline(path),
                             {a0: 'HIT', a1: 'NOT-HIT'})

    def test_read_baseline_rejects_a_malformed_line(self):
        for bad in ('garbage line', '0x10000', '0x10000 BOGUS', '0x10000 HIT extra'):
            with tempfile.TemporaryDirectory() as d:
                path = os.path.join(d, 'base.txt')
                with open(path, 'w') as f:
                    f.write('0x10000 HIT\n' + bad + '\n')
                with self.assertRaises(ValueError) as cm:
                    cc.read_baseline(path)
                self.assertIn('unparseable baseline line', str(cm.exception), bad)

    def test_read_baseline_rejects_a_non_hex_address(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'base.txt')
            with open(path, 'w') as f:
                f.write('0xZZZZ HIT\n')
            with self.assertRaises(ValueError) as cm:
                cc.read_baseline(path)
            self.assertIn('bad address', str(cm.exception))


class TestDumpParsing(unittest.TestCase):
    def test_parse_tags_splits_spaces_and_commas(self):
        self.assertEqual(cc.parse_tags('check attract title'),
                         ['check', 'attract', 'title'])
        self.assertEqual(cc.parse_tags('check,attract,title'),
                         ['check', 'attract', 'title'])
        self.assertEqual(cc.parse_tags(' check  ,attract '),
                         ['check', 'attract'])
        self.assertEqual(cc.parse_tags(''), [])

    def test_read_hits_file_parses_the_dump_format(self):
        a0, a1, _, _ = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 't.hits')
            with open(path, 'w') as f:
                f.write('fn-hit t 0x10000 n=1\nfn-hit t 0x10001 n=3\n')
            self.assertEqual(cc.read_hits_file(path), {a0, a1})

    def test_read_hits_dir_uses_the_basename_as_the_tag(self):
        a0, a1, _, _ = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'alpha.hits'), 'w') as f:
                f.write('fn-hit alpha 0x10000 n=1\n')
            with open(os.path.join(d, 'beta.hits'), 'w') as f:
                f.write('fn-hit beta 0x10001 n=1\n')
            self.assertEqual(cc.read_hits_dir(d),
                             {'alpha': {a0}, 'beta': {a1}})


class TestRunCensus(unittest.TestCase):
    def _run(self, dumps, tags, hit, instrumented, portable, baseline,
             check=False, update=False, reason=''):
        return cc.run_census(dumps, tags, hit, instrumented, portable, baseline,
                             check, update, reason)

    def test_check_with_a_new_nothit_exits_1(self):
        a0, a1, _, _ = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'base.txt')
            cc.write_baseline(path, {a0: 'HIT', a1: 'HIT'}, 'all hit')
            code, out = self._run({'alpha': {a0}}, ['alpha'], {a0},
                                  {a0, a1}, {a0, a1}, path, check=True)
            self.assertEqual(code, 1)
            self.assertIn('coverage: new NOT-HIT: 0x10001', out)

    def test_check_with_a_new_hit_exits_2(self):
        a0, a1, _, _ = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'base.txt')
            cc.write_baseline(path, {a0: 'NOT-HIT', a1: 'NOT-HIT'}, 'nothing hit')
            code, out = self._run({'alpha': {a0}}, ['alpha'], {a0},
                                  {a0, a1}, {a0, a1}, path, check=True)
            self.assertEqual(code, 2)
            self.assertIn('improved: 0x10000', out)

    def test_check_with_an_absent_baseline_is_unpinned_and_exits_0(self):
        # plan Step 5: the first of the two runs has no baseline yet, so
        # `make coverage`'s --check must print `unpinned:` and succeed (the
        # pin happens after the two runs are compared). --check stays strict
        # only once a baseline exists.
        a0, a1, _, _ = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'absent.txt')  # never written
            code, out = self._run({'alpha': {a0}}, ['alpha'], {a0},
                                  {a0, a1}, {a0, a1}, path, check=True)
            self.assertEqual(code, 0)
            self.assertIn('unpinned:', out)
            self.assertIn('coverage: split HIT=1 NOT-HIT=1 UNINSTRUMENTED=0', out)

    def test_check_with_a_malformed_baseline_exits_1(self):
        # A corrupted baseline line must fail the census, not be silently
        # skipped (review finding #2).
        a0, a1, _, _ = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'base.txt')
            with open(path, 'w') as f:
                f.write('# reason: corrupt\n0x10000 HIT\nnot a baseline line\n')
            code, out = self._run({'alpha': {a0}}, ['alpha'], {a0},
                                  {a0, a1}, {a0, a1}, path, check=True)
            self.assertEqual(code, 1)
            self.assertIn('unparseable baseline line', out)

    def test_a_new_hit_repins_with_update_baseline_reason(self):
        a0, a1, _, _ = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'base.txt')
            cc.write_baseline(path, {a0: 'NOT-HIT', a1: 'NOT-HIT'}, 'nothing hit')
            code, out = self._run({'alpha': {a0}}, ['alpha'], {a0},
                                  {a0, a1}, {a0, a1}, path, update=True,
                                  reason='re-pinned after the alpha gain')
            self.assertEqual(code, 0)
            self.assertIn('coverage: baseline written', out)
            self.assertEqual(cc.read_baseline(path), {a0: 'HIT', a1: 'NOT-HIT'})

    def test_update_baseline_without_reason_exits_2(self):
        a0, a1, _, _ = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'base.txt')
            code, out = self._run({'alpha': {a0}}, ['alpha'], {a0},
                                  {a0, a1}, {a0, a1}, path, update=True)
            self.assertEqual(code, 2)
            self.assertIn('--update-baseline requires --reason', out)

    def test_guard_failure_exits_1_and_is_not_classified(self):
        _, _, _, a3 = three_addrs()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'base.txt')
            code, out = self._run({}, [], set(), set(), {a3}, path)
            self.assertEqual(code, 1)
            self.assertIn('guard failure', out)
            self.assertIn('0x10003', out)


class TestGuard(unittest.TestCase):
    def _files(self, *bodies):
        return [('f%d.c' % i, t) for i, t in enumerate(bodies)]

    def test_a_seamed_function_passes(self):
        files = self._files('/* 0x10000 — seamed */\nvoid f(void) { PR_SEAM(0x10000); }')
        self.assertEqual(cc.check_instrumented(files, portable={0x10000}), [])

    def test_a_pr_hit_function_passes(self):
        files = self._files('/* 0x10001 — hit */\nvoid g(void) { PR_HIT(0x10001); }')
        self.assertEqual(cc.check_instrumented(files, portable={0x10001}), [])

    def test_a_declared_exception_passes(self):
        files = self._files('/* 0x29D60 — bare ret */\nvoid h(void) { return; }')
        self.assertEqual(cc.check_instrumented(files, portable={0x29D60}), [])

    def test_a_new_header_with_no_counter_fails(self):
        files = self._files('/* 0x10002 — no counter */\nvoid k(void) { return; }')
        failing = cc.check_instrumented(files, portable={0x10002})
        self.assertEqual(failing, [(0x10002, 'f0.c')])

    def test_a_header_with_no_counter_in_a_second_file_fails(self):
        files = self._files('/* 0x10000 — hit */\nvoid f(void){ PR_HIT(0x10000); }',
                            '/* 0x10003 — bare */\nvoid m(void){ return; }')
        self.assertEqual(cc.check_instrumented(files, portable={0x10000, 0x10003}),
                         [(0x10003, 'f1.c')])

    def test_a_data_address_header_is_not_a_guard_target(self):
        # Data comments (`/* 0x804A8 — DS_... */`) are not function bodies; the
        # guard ignores them even though the regex matches a column-0 `0x`.
        files = self._files('/* 0x804A8 — DS_000804A8 */\n#define THING 1\n')
        self.assertEqual(cc.check_instrumented(files, portable={0x10000}), [])

    def test_a_runtime_header_is_not_a_guard_target(self):
        # The runtime region (>= 0x5D000) is declared runtime, not census scope.
        files = self._files('/* 0x65431 — soft restart */\nvoid r(void) { return; }')
        self.assertEqual(cc.check_instrumented(files, portable={0x10000}), [])

    def test_the_live_tree_guard_passes(self):
        failing = cc.check_instrumented(cc.port_c_files())
        self.assertEqual(failing, [], 'a .c column-0 header with no counter: %s'
                         % failing)

    def test_the_live_tree_has_only_the_four_uninstrumented_exceptions(self):
        files = cc.port_c_files()
        instrumented = cc.instrumented_c(files)
        portable = cc.portable_addrs()
        uninstrumented = portable - instrumented
        self.assertEqual(uninstrumented, cc.UNINSTRUMENTED)

    def test_every_portable_address_is_classifiable(self):
        files = cc.port_c_files()
        instrumented = cc.instrumented_c(files)
        portable = cc.portable_addrs()
        classes, failures = cc.classify(set(), instrumented, portable)
        self.assertEqual(failures, set())
        self.assertEqual(len(classes), 759)
        self.assertEqual(sum(1 for c in classes.values() if c == 'UNINSTRUMENTED'),
                         4)

    def test_the_instrumented_scan_matches_the_d14_measurement(self):
        # §D1.4 re-measured: 755 of 759 portable addresses instrumented (the 4
        # no-body exceptions are the only uninstrumented), 32 of them via
        # fn_register. This pins the shared scan, not a copied number.
        instrumented = cc.instrumented_c(cc.port_c_files())
        portable = cc.portable_addrs()
        self.assertEqual(len(portable - instrumented), 4)


if __name__ == '__main__':
    unittest.main()

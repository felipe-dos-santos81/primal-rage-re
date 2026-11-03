# tools/tests/test_check_docs_claims.py
import os
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import check_docs_claims as cdc


class TestCheckDocsClaims(unittest.TestCase):
    def test_a_clean_tree_passes(self):
        # The repo tree, with every retired claim already removed, must pass.
        self.assertEqual(cdc.check(ROOT), [])

    def test_a_seeded_stale_phrase_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            # A minimal tree holding a clean copy of every pinned file.
            for rel, _ in cdc.CLAIMS:
                path = os.path.join(tmp, rel)
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, 'w') as f:
                    f.write('a clean line\n')
            # Seed one retired phrase and expect exactly that file flagged.
            rel, phrase = cdc.CLAIMS[0]
            with open(os.path.join(tmp, rel), 'a') as f:
                f.write('regression: %s\n' % phrase)
            problems = cdc.check(tmp)
            self.assertTrue(any(phrase in p for p in problems), problems)

    def test_every_pinned_file_exists(self):
        # A pin to a missing file can never pass (a missing file is a violation),
        # so the pin list must name real files for the clean-tree check to hold.
        for rel, _ in cdc.CLAIMS:
            self.assertTrue(os.path.isfile(os.path.join(ROOT, rel)), rel)

    def test_the_completion_truth_readme_guards_are_pinned(self):
        # Completion truth Task 4 moved the README title, row count and capture
        # inventory; the retired values must stay pinned as stale phrases.
        for phrase in ('Reverse Engineering 66%', 'It has 36 rows',
                       '45 explicit capture paths', '44/45 present',
                       'the two-human 2-0 win is game evidence only',
                       'U23 moves ratchet records'):
            self.assertIn(('README.md', phrase), cdc.CLAIMS, phrase)

    def test_the_final_fix_wave_guards_are_pinned(self):
        # The P2-idle and 110/133 statements and the zero-declared wording were
        # retired by the final fix wave; both files are pinned.
        for rel, phrase in (('README.md', 'It has 38 rows'),
                            ('README.md', 'No row is `declared`'),
                            ('README.md', '110/133'),
                            ('AGENTS.md', 'no row is `declared`'),
                            ('AGENTS.md', '110/133'),
                            ('AGENTS.md', 'P2 idle after the join')):
            self.assertIn((rel, phrase), cdc.CLAIMS, (rel, phrase))


if __name__ == '__main__':
    unittest.main()

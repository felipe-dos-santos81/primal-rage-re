# tools/tests/test_k11_session.py
import os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import k11_session as ks


def p(ms, tick, f=0x100, st=0, mode=3, menu=0, ent=0, kb=0):
    vals = {n: 0 for n, _, _ in ks.POLL_FIELDS}
    vals.update(f=f, st=st, mode=mode, menu=menu, ent=ent, tick=tick)
    return ks.format_p(ms, vals, kb, 0x1E)


def k(ms, tick, word):
    return 'K ms=%d tick=%08X f=0100 key=%04X' % (ms, tick, word)


class Session(unittest.TestCase):
    def test_autotype_walk_is_exact(self):
        self.assertEqual(
            ks.autotype_line('walk', 25, 1),
            'AUTOTYPE -w 25 -p 1 enter enter down down esc down enter enter esc '
            'down enter esc esc esc esc esc '
            'down enter esc down enter esc down enter esc down enter esc '
            'down enter esc down enter esc down enter esc esc')

    def test_scenario_keys_are_known(self):
        for name, sc in ks.SCENARIOS.items():
            for key in sc['keys']:
                self.assertTrue(key == ',' or key in ks.KEYS, (name, key))
            self.assertEqual(sc['keys'][0], 'enter', name)   # the mode-3 Enter

    def test_parse_round_trips_a_p_record(self):
        r = ks.parse(p(12, 0x1234, f=0x2A, mode=0x27, kb=0x40))
        self.assertEqual((r['kind'], r['ms'], r['tick'], r['f'], r['mode'], r['kb']),
                         ('P', 12, 0x1234, 0x2A, 0x27, 0x40))

    def test_port_script_from_a_synthetic_poll(self):
        lines = ['B ms=0 base=00266000',
                 p(5, 0x100),
                 k(10, 0x200, 0x1C0D),                        # the mode-3 Enter
                 p(11, 0x200, f=0x155, st=0, mode=0x27),
                 p(40, 0x220, f=0x160, mode=0x27, kb=0x0040), # a held arrow
                 k(41, 0x221, 0x50E0),                        # grey Down, ascii 0xE0 in the buffer
                 p(60, 0x226, f=0x162, mode=0x27, kb=0),
                 k(90, 0x260, 0x011B)]                        # Esc
        ks.SCENARIOS['_t'] = dict(keys=('enter', ',', 'down', 'esc'),
                                  pokes=(('ds_or', 0x107410, 0x10),), time_limit=10)
        try:
            out = ks.port_script('_t', lines)
        finally:
            del ks.SCENARIOS['_t']
        self.assertEqual(out, '# k11 port script v1: scenario _t\n'
                              'enter_frame 341\n'
                              'enter_state 0000\n'
                              'ds_or 107410 10\n'
                              'pad 32 0040 6\n'
                              'key 33 50 00\n'
                              'key 96 01 1B\n'
                              'end %d\n' % (96 + ks.END_TAIL_TICKS))

    def test_port_script_rejects_other_keys(self):
        lines = [k(10, 0x200, 0x1C0D), p(11, 0x200, mode=0x27), k(20, 0x210, 0x011B)]
        with self.assertRaises(ks.ScriptError):
            ks.port_script('menuesc', lines + [k(30, 0x220, 0x011B)])

    def test_port_script_accepts_a_key_prefix_only_after_an_exit(self):
        lines = [k(10, 0x200, 0x1C0D), p(11, 0x200, mode=0x27), k(20, 0x210, 0x011B)]
        with self.assertRaises(ks.ScriptError):       # the run was cut by the time limit
            ks.port_script('menuesc', lines[:2] + ['E ms=99 reason=time-limit rc=0'])
        out = ks.port_script('menuesc', lines[:2] + ['E ms=99 reason=exit rc=0'])
        self.assertNotIn('\nkey ', out)                 # only the Enter was received
        self.assertTrue(out.endswith('end %d\n' % ks.END_TAIL_TICKS))

    def test_port_script_needs_mode_27(self):
        with self.assertRaises(ks.ScriptError):
            ks.port_script('idle', [k(10, 0x200, 0x1C0D), p(11, 0x200, mode=3)])


if __name__ == '__main__':
    unittest.main()

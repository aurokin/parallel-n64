"""Check the actual semantic decoder against native, word-swapped RDRAM."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[3]
COMMON = REPO / 'tools/scenarios/lib/common.sh'


def native_words(cpu_bytes):
    return b''.join(cpu_bytes[i:i + 4][::-1] for i in range(0, len(cpu_bytes), 4))


def put(buf, offset, value, size=2, signed=False):
    buf[offset:offset + size] = value.to_bytes(size, 'big', signed=signed)


class MemoryDecoderContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.bundle = Path(self.temp.name)
        (self.bundle / 'traces').mkdir()
        self.output = self.bundle / 'semantic.json'
        self.raw_inputs = {}
        self.status = bytearray(232)
        # Logical GameStatus starts two CPU bytes into the aligned window.
        for offset, value in ((2, 4), (4, 0x102), (6, -257), (8, 7), (10, 2)):
            put(self.status, offset, value, signed=True)
        self.write('gamestatus', 0x800740A8, native_words(self.status))

    def write(self, name, base, raw):
        path = self.bundle / 'traces' / f'paper-mario-{name}.core-memory.txt'
        path.write_text(f'READ_CORE_MEMORY {base:08x} ' + raw.hex(' ') + '\n')
        self.raw_inputs[name] = bytes(raw)
        return path

    def decode(self, success=True):
        process = subprocess.run(
            ['bash', '-c', 'source "$1"; scenario_decode_paper_mario_semantic_state "$2" "$3"',
             'decoder-contract', str(COMMON), str(self.bundle), str(self.output)],
            capture_output=True, text=True, check=False,
        )
        if success:
            self.assertEqual(process.returncode, 0, process.stderr)
            return json.loads(self.output.read_text())
        self.assertNotEqual(process.returncode, 0)
        self.assertFalse(self.output.exists(), 'unqualified input produced semantic evidence')
        return process.stderr

    def test_distinct_adjacent_fields_and_raw_identity(self):
        for offset, value in ((0x24, 0xFD), (0x28, 0xFC), (0x2B, 0xFB),
                              (0x2C, 0xFA), (0xE2, 3), (0xE3, 1)):
            self.status[offset] = value
        put(self.status, 0xB0, 0x1234)
        put(self.status, 0xB2, 0xAA55)
        put(self.status, 0xE4, 0x12345678, 4)
        raw = native_words(self.status)
        trace = self.write('gamestatus', 0x800740A8, raw)
        original = trace.read_bytes()
        result = self.decode()
        status = result['paper_mario_us']['game_status']
        for name, expected in {'area_id': 4, 'prev_area': 0x102, 'did_area_change': -257,
                               'map_id': 7, 'entry_id': 2, 'intro_part': -3,
                               'startup_state': -4, 'title_screen_timer': -5,
                               'title_screen_dismiss_time': -6, 'frame_counter': 0x1234,
                               'save_slot': 3, 'load_type': 1, 'save_count': 0x12345678}.items():
            self.assertEqual(status[name], expected, name)
        source = result['sources']['gamestatus_window']
        self.assertEqual(source['window_sha256'], hashlib.sha256(raw).hexdigest())
        self.assertEqual(source['base_address'], '0x800740a8')
        self.assertEqual(source['window_size_bytes'], 232)
        self.assertEqual(source['logical_base_address'], '0x800740aa')
        self.assertEqual(source['logical_size_bytes'], 230)
        self.assertEqual(source['memory_format'], 'native-rdram-little-endian-32-bit-words')
        self.assertEqual(trace.read_bytes(), original)

    def test_captured_battle_native_window(self):
        # October 11 native battle window: a decoding regression, not an authority.
        raw = bytes.fromhex(
            '04 00 00 02 01 00 04 00 02 00 07 00 e8 03 e8 03 10 d3 d5 c1 00 00 34 44 00 00 a0 42 00 00 00 00'
            '00 00 b4 42 01 00 ff ff 00 00 00 06 00 00 00 0a 00 00 00 00 d0 00 d0 00 00 00 d0 00 00 00 00 00'
            '00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00'
            '00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00'
            '00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 34 c3 00 00 00 00'
            '00 00 34 c3 00 00 00 80 00 00 80 3f 00 00 00 00 00 00 16 36 4f 92 af ba 00 00 00 00 10 b5 31 80'
            '00 00 00 00 0c 00 01 00 28 01 14 00 00 00 c8 00 10 02 20 80 10 00 20 80 00 00 b4 00 00 00 00 00'
            '00 00 00 00 01 00 00 00'
        )
        self.write('gamestatus', 0x800740A8, raw)
        status = self.decode()['paper_mario_us']['game_status']
        self.assertEqual((status['area_id'], status['prev_area'], status['did_area_change'],
                          status['map_id'], status['entry_id']), (4, 4, 1, 7, 2))
        self.assertEqual(status['frame_counter'], 0x3616)
        self.assertEqual(status['save_count'], 1)

    def test_named_kmr03_slice(self):
        raw = bytes.fromhex('00 00 00 02 01 00 09 00 05 00 02 00') + bytes(220)
        self.write('gamestatus', 0x800740A8, raw)
        status = self.decode()['paper_mario_us']['game_status']
        self.assertEqual((status['area_id'], status['prev_area'], status['did_area_change'],
                          status['map_id'], status['entry_id'], status['map_name_candidate']),
                         (0, 9, 1, 2, 5, 'kmr_03'))

    def test_optional_bytes_halves_and_existing_word_values(self):
        mode = bytearray(20)
        put(mode, 0, 0x1234)
        put(mode, 2, 0xABCD)
        for offset, value in ((4, 0x80035D30), (8, 0x80035D54),
                              (12, 0x80112B90), (16, 0x80035DD0)):
            put(mode, offset, value, 4)
        mode_raw = native_words(mode)
        self.write('curgamemode', 0x80151700, mode_raw)
        self.write('transition', 0x800A0944, bytes.fromhex('fe ff 34 12 55 aa 01 00'))
        self.write('filemenu-current-menu', 0x8024C098, bytes.fromhex('aa bb cc 03'))
        self.write('save-slot-has-data', 0x80077A24, bytes.fromhex('00 01 00 01'))
        self.write('filemenu-pressed-buttons', 0x8024C084, bytes.fromhex('dd cc bb aa'))
        self.write('filemenu-held-buttons', 0x8024C08C, bytes.fromhex('78 56 34 12'))
        self.write('filemenu-menus', 0x80249B84, bytes.fromhex('44 33 22 81 88 77 66 85'))
        panel = bytearray.fromhex('02 fe fd 01 03 04 05 fe') + bytearray(20)
        for offset, value in ((8, 0x81223344), (16, 0x85667788), (20, 0x89AABBCC)):
            panel[offset:offset + 4] = value.to_bytes(4, 'little')
        self.write('filemenu-main-panel', 0x81220000, panel)
        confirm = bytearray(panel)
        confirm[:4] = bytes.fromhex('04 ff fe 01')
        self.write('filemenu-confirm-panel', 0x85660000, confirm)
        window = bytearray(32)
        window[:4] = bytes((5, 7, 9, 253))
        for offset, value in ((12, -100), (14, 200), (16, 300), (18, 400)):
            put(window, offset, value, signed=True)
        put(window, 4, 0x80243380, 4)
        put(window, 8, 2, 4)
        put(window, 20, 0x81234567, 4)
        window[28] = 11
        self.write('window-files-title', 0x8015A2F0, native_words(window))
        result = self.decode()
        game = result['paper_mario_us']
        self.assertEqual(game['cur_game_mode']['flags'], 0x1234)
        self.assertEqual(game['cur_game_mode']['padding_u16'], 0xABCD)
        self.assertEqual(game['cur_game_mode']['init_ptr'], '0x80035d30')
        self.assertEqual(game['cur_game_mode']['step_ptr'], '0x80035d54')
        self.assertEqual(game['cur_game_mode']['phase_guess'], 'world_callbacks')
        self.assertEqual(result['sources']['cur_game_mode']['window_sha256'], hashlib.sha256(mode_raw).hexdigest())
        self.assertEqual(game['map_transition'], {'state': 0x1234, 'state_time': -2, 'loaded_from_file_select': 1})
        menu = game['filemenu']
        self.assertEqual(menu['current_menu'], 3)
        self.assertEqual(menu['save_slot_has_data'], [True, False, True, False])
        self.assertEqual(menu['pressed_buttons']['raw'], 0xAABBCCDD)
        self.assertEqual(menu['held_buttons']['raw'], 0x12345678)
        self.assertEqual(menu['menus'], {'main_panel_ptr': '0x81223344', 'confirm_panel_ptr': '0x85667788'})
        for field, expected in {'initialized': True, 'col': -3, 'row': -2, 'selected': 2,
                                'state': -2, 'num_cols': 5, 'num_rows': 4, 'num_pages': 3,
                                'grid_data_ptr': '0x81223344', 'handle_input_ptr': '0x85667788',
                                'update_ptr': '0x89aabbcc'}.items():
            self.assertEqual(menu['main_panel'][field], expected, field)
        decoded_window = game['windows']['title']
        for field, expected in {'flags': 5, 'priority': 7, 'original_priority': 9,
                                'parent': -3, 'pos': {'x': -100, 'y': 200}, 'width': 300,
                                'height': 400, 'fp_update': '0x80243380', 'fp_pending': '0x00000002',
                                'draw_contents_ptr': '0x81234567', 'update_counter': 11}.items():
            self.assertEqual(decoded_window[field], expected, field)
        source_locations = {
            'curgamemode': ('cur_game_mode',),
            'transition': ('map_transition_state',),
            'filemenu-current-menu': ('filemenu_runtime', 'current_menu'),
            'filemenu-menus': ('filemenu_runtime', 'menus'),
            'filemenu-main-panel': ('filemenu_runtime', 'main_panel'),
            'filemenu-confirm-panel': ('filemenu_runtime', 'confirm_panel'),
            'filemenu-pressed-buttons': ('filemenu_runtime', 'pressed_buttons'),
            'filemenu-held-buttons': ('filemenu_runtime', 'held_buttons'),
            'save-slot-has-data': ('filemenu_runtime', 'save_slot_has_data'),
            'window-files-title': ('filemenu_windows', 'title'),
        }
        for trace_name, location in source_locations.items():
            with self.subTest(raw_identity=trace_name):
                raw = self.raw_inputs[trace_name]
                self.assertNotEqual(raw, native_words(raw), 'input must detect CPU-order hashing')
                source = result['sources']
                for field in location:
                    source = source[field]
                self.assertEqual(source['window_sha256'], hashlib.sha256(raw).hexdigest())

    def test_legacy_unaligned_gamestatus_cannot_qualify_scene(self):
        self.write('gamestatus', 0x800740AA, bytes.fromhex('00 02 01 00 04 00 02 00 07 00') + bytes(220))
        self.assertIn('omit areaID bytes', self.decode(success=False))

    def test_legacy_current_menu_byte_cannot_qualify_menu(self):
        self.write('filemenu-current-menu', 0x8024C098, bytes.fromhex('aa'))
        self.assertIn('complete 4-byte words', self.decode(success=False))

    def test_short_bad_base_and_malformed_inputs_fail_closed(self):
        for base, raw in ((0x800740A8, bytes(228)), (0x800740AC, bytes(232)),
                          (0x800740A8, bytes(231))):
            with self.subTest(base=base, size=len(raw)):
                self.write('gamestatus', base, raw)
                self.decode(success=False)
        trace = self.write('gamestatus', 0x800740A8, bytes(232))
        trace.write_text('READ_CORE_MEMORY 800740a8 zz\n')
        self.decode(success=False)


if __name__ == '__main__':
    unittest.main()

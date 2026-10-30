#!/usr/bin/env python3
"""Display-free benchmark math, input resolution and control contracts."""
import argparse
import csv
import gzip
import hashlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "benchmark"))
import run_benchmark as bench
import sample_memory as memory


def sample(fps, cpu=100, rss=200):
    return {"scene": "sm64", "engine": "dynamic_recompiler", "scale": "1x", "pack_sha256": None,
                "state_sha256": "state", "rom_content_sha256": "rom", "fps": fps,
                "cpu_percent": cpu, "max_rss_kib": rss}


def summary(values):
    return {"schema": bench.SCHEMA, "method": "linux-paired", "short_frames": 16, "long_frames": 1216,
                "cells": bench.summarize([sample(v) for v in values])}


class BenchmarkContracts(unittest.TestCase):
    def test_memory_sampler_proc_fixture(self):
        with tempfile.TemporaryDirectory() as temp:
            proc = Path(temp) / '123'
            (proc / 'fdinfo').mkdir(parents=True)
            status = proc / 'status'
            status.write_text('Name:\tfixture\nVmRSS:\t5000 kB\nVmHWM:\t6000 kB\n')
            drm = ('drm-pdev: 0000:01:00.0\ndrm-client-id: 7\n'
                   'drm-memory-vram: 1024 KiB\ndrm-total-vram0: 4 MiB\n'
                   'drm-resident-vram0: 2097152 B\ndrm-engine-gfx: 42 ns\n')
            # Memory fields from a captured amdgpu fdinfo sample.
            drm += ('drm-total-cpu:\t0\ndrm-resident-gds:\t0\n'
                    'drm-total-gtt:\t79260 KiB\n')
            for fd in ('3', '4'):
                (proc / 'fdinfo' / fd).write_text(drm)
            (proc / 'fdinfo' / '5').write_text(drm.replace('01:00.0', '02:00.0'))
            (proc / 'fdinfo' / '6').write_text('pos:\t0\nflags:\t0100000\n')
            clock = [0]

            def advance(seconds):
                self.assertEqual(seconds, 1)
                clock[0] += seconds
                rss = (5000, 5000, 3000, 4000, 5000, 5000)[int(clock[0])]
                status.write_text(f'VmRSS:\t{rss} kB\nVmHWM:\t6000 kB\n')

            output, summary = io.StringIO(), io.StringIO()
            with patch.object(memory.time, 'monotonic', side_effect=lambda: clock[0]), \
                    patch.object(memory.time, 'sleep', side_effect=advance), patch('sys.stdout', summary):
                memory.sample(proc, output, duration=5)
            rows = list(csv.DictReader(io.StringIO(output.getvalue())))
            self.assertEqual(sorted({float(r['elapsed_seconds']) for r in rows}), [0, 1, 2, 3, 4])
            first = {r['metric']: float(r['MB']) for r in rows if r['elapsed_seconds'] == '0.000'}
            self.assertEqual(first, {'VmRSS': 5.12, 'VmHWM': 6.144, 'drm-memory-vram': 2.097152,
                                     'drm-total-vram0': 8.388608, 'drm-resident-vram0': 4.194304,
                                     'drm-total-cpu': 0, 'drm-resident-gds': 0, 'drm-total-gtt': 162.32448})
            # A nonzero unitless value must also be interpreted as bytes.
            for fd in ('3', '4', '5'):
                path = proc / 'fdinfo' / fd
                path.write_text(path.read_text().replace('drm-total-cpu:\t0', 'drm-total-cpu:\t4096'))
            self.assertEqual(memory.read_memory(proc)['drm-total-cpu'], 0.008192)
            self.assertIn('VmRSS: peak=5.120000 MB slope=61.440000 MB/min (final half)', summary.getvalue())
            self.assertIn('drm-memory-vram: peak=2.097152 MB slope=0.000000', summary.getvalue())
            # Exit between ticks must finish and summarize without another sleep.
            with patch.object(memory.time, 'sleep', side_effect=lambda _: status.unlink()), \
                    patch('sys.stdout', io.StringIO()):
                memory.sample(proc, io.StringIO())

    def test_android_config_replaces_all_duplicate_controls(self):
        base = '# retained comment\nvideo_vsync = "true"\n  video_vsync="true"\naudio_sync = "true"\nother = "kept"\n'
        result = bench.merged_config(base, {"video_vsync": "false", "audio_sync": "false"})
        self.assertEqual(result.count('video_vsync'), 1)
        self.assertIn('video_vsync = "false"', result)
        self.assertIn('other = "kept"', result)
        self.assertIn('# retained comment', result)
        self.assertNotIn('"true"', result)

    def test_pair_subtracts_startup_wall_and_cpu_keeps_long_rss(self):
        short = {"frames": 16, "wall_seconds": .5, "user_seconds": .2, "sys_seconds": .1, "max_rss_kib": 900}
        long = {"frames": 1216, "wall_seconds": 4.5, "user_seconds": 3.2, "sys_seconds": 1.1, "max_rss_kib": 700}
        result = bench.paired(short, long)
        self.assertAlmostEqual(result.pop("cpu_percent"), 100)
        self.assertEqual(result, {"frames": 1200, "seconds": 4, "fps": 300, "max_rss_kib": 700})
        for bad in (short, short | {"frames": 1216, "wall_seconds": .4},
                    long | {"user_seconds": 0, "sys_seconds": 0}):
            with self.assertRaises(ValueError):
                bench.paired(short, bad)

    def test_summary_groups_pack_identity_and_uses_medians(self):
        rows = [sample(100, 80, 100), sample(110, 90, 200), sample(180, 150, 300),
                sample(50) | {"pack_sha256": "pack"}]
        cells = bench.summarize(rows)
        self.assertEqual(len(cells), 2)
        self.assertEqual((cells[0]["median_fps"], cells[0]["min_fps"], cells[0]["max_fps"]), (110, 100, 180))
        self.assertEqual((cells[0]["cpu_percent"], cells[0]["max_rss_kib"]), (90, 200))

    def test_compare_reports_spread_and_unmatched_cells(self):
        a = summary([100, 105, 110])
        same = bench.compare(a, a)[0]
        self.assertEqual(same["delta_percent"], 0)
        self.assertFalse(same["beyond_spread"])
        b = summary([109, 115, 120])
        self.assertFalse(bench.compare(a, b)[0]["beyond_spread"])
        b = summary([120, 126, 130])
        self.assertTrue(bench.compare(a, b)[0]["beyond_spread"])
        self.assertAlmostEqual(bench.compare(a, b)[0]["delta_percent"], 20)
        self.assertTrue(bench.compare(b, a)[0]["beyond_spread"])
        b["cells"][0]["pack_sha256"] = "different"
        self.assertEqual({r["status"] for r in bench.compare(a, b)}, {"added", "removed"})

    def test_compare_rejects_different_workloads(self):
        a = summary([100])
        for change in ({"method": "android-window"}, {"long_frames": 2416}):
            with self.assertRaises(ValueError):
                bench.compare(a, a | change)
        b = summary([100])
        for field in ("state_sha256", "savefile_sha256", "rom_content_sha256"):
            b = summary([100])
            b["cells"][0][field] = "different"
            with self.assertRaises(ValueError):
                bench.compare(a, b)

    def test_gnu_time_uses_child_wall_not_parent_wait(self):
        text = 'Elapsed (wall clock) time (h:mm:ss or m:ss): 1:02:03.45\nUser time (seconds): 12.30\nSystem time (seconds): 1.20\nMaximum resident set size (kbytes): 123456\n'
        result = bench.parse_time(text)
        self.assertEqual(result["wall_seconds"], 3723.45)
        self.assertEqual(result["max_rss_kib"], 123456)
        with self.assertRaises(ValueError):
            bench.parse_time(text.replace("Elapsed", "Missing"))

    def test_log_qualification_rejects_engine_and_failed_autoload(self):
        text = 'Starting R4300 emulator: Dynamic Recompiler\nAuto-loading savestate succeeded\nEnabling upscaling: 4x\n'
        self.assertTrue(bench.qualify_log(text, "dynamic_recompiler", "4x", None)["autoload_confirmed"])
        for candidate, engine, scale in ((text, "pure_interpreter", "4x"),
                                    (text.replace("succeeded", "failed"), "dynamic_recompiler", "4x"),
                                    (text, "dynamic_recompiler", "2x")):
            with self.assertRaises(ValueError):
                bench.qualify_log(candidate, engine, scale, None)

    def test_android_midpoint_and_rejects_paused_or_reset_counter(self):
        start = bench.parse_status('GET_STATUS PLAYING game frame=600\n', 10, 10.2)
        end = bench.parse_status('GET_STATUS PLAYING game frame=900\n', 12, 12.2)
        self.assertAlmostEqual(start["t"], 10.1)
        self.assertAlmostEqual(bench.window_metrics(start, end)["fps"], 150)
        for reply in ('GET_STATUS PAUSED game frame=900', 'GET_STATUS PLAYING frame=-1', 'garbage'):
            with self.assertRaises(ValueError):
                bench.parse_status(reply, 1, 2)
        for bad in (end | {"frame": 0}, end | {"t": 1}):
            with self.assertRaises(ValueError):
                bench.window_metrics(start, bad)

    def test_android_window_accounts_for_rtt_and_scheduling(self):
        now = [1.0]
        def sleep(seconds):
            now[0] += seconds
        replies = iter([{"frame": 100, "t": .9, "rtt": .2}, {"frame": 2100, "t": 10.95, "rtt": .3}])
        with patch.object(bench.socket, "socket", side_effect=AssertionError("no network in contract")):
            result = bench.sample_window(lambda: next(replies), 10, clock=lambda: now[0], sleep=sleep)
        self.assertAlmostEqual(now[0], 10.9)
        self.assertAlmostEqual(result["seconds"], 10.05)
        self.assertAlmostEqual(result["fps"], 2000 / 10.05)

    def test_android_stages_isolated_config_and_cleans_up_without_device(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            base = root / "base.cfg"
            base.write_text('network_cmd_enable = "false"\nsavestate_auto_load = "false"\n')
            rom, state = root / "game.z64", root / "game.state"
            rom.write_bytes(b"rom")
            state.write_bytes(b"state")
            args = bench.parser().parse_args(["--android", "fake", "--out", str(root / "result"),
                                             "--base-config", str(base)])
            calls = []
            log = 'Starting R4300 emulator: Dynamic Recompiler\nAuto-loading state succeeded\n'
            class FakeAndroid(bench.Android):
                def shell(self, *argv, check=True):
                    calls.append(argv)
                    return "100" if argv == ("getconf", "CLK_TCK") else ""
                def adb(self, *argv, check=True):
                    if argv[0] == "pull":
                        Path(argv[-1]).write_text(log)
                    return ""
            client = FakeAndroid(args)
            readings = [{"pid": "42", "t": 1, "ticks": 100, "max_rss_kib": 1000},
                        {"pid": "42", "t": 3, "ticks": 300, "max_rss_kib": 1200}]
            run = root / "run"
            with patch.object(client, "status", return_value={"frame": 100, "t": 1}), \
                 patch.object(client, "resources", side_effect=readings), \
                 patch.object(bench.time, "sleep"), \
                 patch.object(bench, "sample_window", return_value={"frames": 200, "seconds": 2, "fps": 100}):
                result = client.run({"rom": rom, "state": state, "savefile": None},
                                    "dynamic_recompiler", "1x", None, run)
            self.assertEqual(result["cpu_percent"], 100)
            self.assertEqual(result["max_rss_kib"], 1200)
            config = (run / "session.cfg").read_text()
            self.assertIn('network_cmd_enable = "true"', config)
            self.assertIn('savestate_auto_load = "true"', config)
            self.assertIn(client.remote, config)
            self.assertEqual(base.read_text().count('"false"'), 2)
            self.assertEqual(calls[-1], ("rm", "-rf", client.remote))
            with patch.object(client, "_run", side_effect=ValueError("failed staging")), \
                 self.assertRaisesRegex(ValueError, "failed staging"):
                client.run({}, "dynamic_recompiler", "1x", None, root / "failed")
            self.assertEqual(calls[-1], ("rm", "-rf", client.remote))

    def test_android_proc_stat_handles_thread_names_with_spaces(self):
        fields = ['S'] + ['0'] * 10 + ['123', '45'] + ['0'] * 10
        self.assertEqual(bench.parse_proc_stat('42 (RetroArch (main)) ' + ' '.join(fields)), 168)

    def test_inputs_reuse_verified_archive_blob_and_savefile_resolution(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            archive = root / 'game.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('nested/game.z64', b'rom')
            state_sha = hashlib.sha256(b'state').hexdigest()
            save_sha = hashlib.sha256(b'save').hexdigest()
            for folder, sha, suffix, payload in [('states', state_sha, '.state', b'state'),
                                                  ('savefiles', save_sha, '.srm', b'save')]:
                (root / folder).mkdir()
                (root / folder / (sha + suffix + '.gz')).write_bytes(gzip.compress(payload))
            rom_sha = bench.regression.sha256_file(archive)
            rom_map = root / 'rom-map.json'
            rom_map.write_text(json.dumps({rom_sha: str(archive)}))
            args = argparse.Namespace(rom=None, rom_map=rom_map, blobs=root)
            scene = {"scene": 'game', "scenario": 'group/anchor.json', "rom_sha256": rom_sha,
                         "state_sha256": state_sha, "savefile_sha256": save_sha}
            inputs = bench.prepare_scene(scene, args, root / 'staging')
            self.assertEqual(inputs['rom'].read_bytes(), b'rom')
            self.assertEqual(inputs['state'].read_bytes(), b'state')
            self.assertEqual(inputs['savefile'].read_bytes(), b'save')
            # A mislabeled or corrupted compressed blob must fail before runtime.
            (root / 'states' / (state_sha + '.state.gz')).write_bytes(gzip.compress(b'wrong'))
            with self.assertRaises(bench.regression.RunFailure):
                bench.prepare_scene(scene, args, root / 'bad')

    def test_manifest_has_eight_unique_pinned_scenes_and_no_private_paths(self):
        manifest = json.loads((REPO / 'tools/benchmark/scenes.json').read_text())
        self.assertEqual(len(manifest['scenes']), 8)
        self.assertEqual(len({s['scene'] for s in manifest['scenes']}), 8)
        for scene in manifest['scenes']:
            self.assertFalse(Path(scene['scenario']).is_absolute())
            for key in ('state_sha256', 'rom_sha256'):
                self.assertRegex(scene[key], r'^[0-9a-f]{64}$')

    def test_compare_cli_needs_no_emulator_inputs(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'summary.json'
            path.write_text(json.dumps(summary([100, 110, 105])))
            proc = subprocess.run([sys.executable, str(Path(bench.__file__)), '--compare', str(path), str(path)],
                                  text=True, capture_output=True, check=True)
            self.assertEqual(json.loads(proc.stdout)[0]['delta_fps'], 0)


if __name__ == '__main__':
    unittest.main()

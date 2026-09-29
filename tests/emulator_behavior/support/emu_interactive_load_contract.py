"""Exercise load completion over the real FIFO/log adapter, without an emulator."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

REPO = Path(__file__).resolve().parents[3]
ADAPTER = REPO / "tools/adapters/retroarch_interactive_session.sh"

FAKE_FRONTEND = r'''#!/usr/bin/env python3
import os
from pathlib import Path
import sys
import time

mode = Path(__file__).with_suffix(".mode").read_text()
if "--command" in sys.argv:
    if sys.argv[1:] != ["--verbose", "--command", "__N64_ADAPTER_LIST_COMMANDS__"]:
        raise SystemExit(3)
    if mode == "probe-timeout":
        time.sleep(30)
    print("[ERROR] [NetCMD] \tValid commands:")
    print("[ERROR] [NetCMD] \t\tLOAD_STATE_SLOT <slot number>")
    print("[ERROR] [NetCMD] \t\tLOAD_STATE_SLOT_PAUSED <slot number>")
    if mode == "unsupported":
        print("[ERROR] WAIT_LOAD_STATE No argument")
        print("[ERROR] [NetCMD] \t\tWAIT_LOAD_STATE_BROKEN No argument")
    else:
        print("[ERROR] [NetCMD] \t\tWAIT_LOAD_STATE No argument")
    raise SystemExit(1)
bundle = Path(sys.argv[1])
fd = os.open(bundle / "retroarch.stdin", os.O_RDWR)
stream = os.fdopen(fd)
log = bundle / "logs/retroarch.log"
attempt = 0
with log.open("a", buffering=1) as output:
    for command in stream:
        command = command.strip()
        if command.startswith("LOAD_STATE_SLOT"):
            attempt += 1
            with (bundle / "load.events").open("a") as events:
                events.write(f"load {time.monotonic()}\n")
            if mode == "ack-timeout":
                continue
            output.write(command + "\n")
        elif command == "WAIT_LOAD_STATE":
            if mode == "barrier-timeout":
                continue
            # Delayed failure catches the old 0.5 second false-success bug.
            time.sleep(0.8)
            if mode != "no-load":
                output.write('[INFO] [State] Loading state "rom.state4".\n')
            if mode == "failure" or (mode == "retry" and attempt == 1):
                output.write('[ERROR] [State] Failed to load state "rom.state4".\n')
            with (bundle / "load.events").open("a") as events:
                events.write(f"done {time.monotonic()}\n")
            output.write("WAIT_LOAD_STATE DONE\n")
'''


class LoadContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="adapter load ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        app = self.root / "fake frontend.app"
        self.binary = app / "Contents/MacOS/RetroArch"
        self.binary.parent.mkdir(parents=True)
        nib = app / "Contents/Resources/en.lproj/MainMenu.nib"
        nib.parent.mkdir(parents=True)
        nib.touch()
        self.binary.write_text(FAKE_FRONTEND)
        self.binary.chmod(0o755)
        self.mode = self.binary.with_suffix(".mode")
        self.mode.write_text("success")
        self.bundle = self.root / "session"
        (self.bundle / "logs").mkdir(parents=True)

    def adapter(self, *args):
        return subprocess.run(["bash", str(ADAPTER), *args], text=True,
                              capture_output=True, timeout=25, check=False)

    def session(self, mode):
        self.mode.write_text(mode)
        os.mkfifo(self.bundle / "retroarch.stdin")
        (self.bundle / "retroarch.session.env").write_text(f"RETROARCH_BIN={self.binary}\n")
        # Stale acknowledgements and errors must never decide this load.
        (self.bundle / "logs/retroarch.log").write_text(
            '[State] Failed to load state "old".\nWAIT_LOAD_STATE DONE\n')
        process = subprocess.Popen([sys.executable, str(self.binary), str(self.bundle)],
                                   start_new_session=True)
        def stop():
            process.terminate()
            process.wait(timeout=5)
        self.addCleanup(stop)
        (self.bundle / "session.pid").write_text(str(process.pid))

    def load(self, mode, paused=True):
        self.session(mode)
        args = ["load-slot", "--bundle-dir", str(self.bundle), "--slot", "4"]
        if paused:
            args.append("--paused")
        result = self.adapter(*args)
        commands = self.bundle / "logs/interactive.commands.log"
        return result, commands.read_text().splitlines() if commands.exists() else []

    def test_waits_for_completion_paused_and_unpaused(self):
        for paused in (True, False):
            with self.subTest(paused=paused):
                # Each fake session needs its own FIFO and process group.
                if not paused:
                    self.bundle = self.root / "session-unpaused"
                    (self.bundle / "logs").mkdir(parents=True)
                start = time.monotonic()
                result, commands = self.load("success", paused)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertGreaterEqual(time.monotonic() - start, 0.8)
                self.assertIn("loaded slot 4", result.stdout)
                verb = "LOAD_STATE_SLOT_PAUSED" if paused else "LOAD_STATE_SLOT"
                self.assertEqual(commands, [f"{verb} 4", "WAIT_LOAD_STATE"])

    def test_delayed_failure_is_not_success(self):
        result, commands = self.load("failure")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("failed to load state after retry", result.stderr)
        self.assertNotIn("loaded slot", result.stdout)
        self.assertEqual(commands, ["LOAD_STATE_SLOT_PAUSED 4", "WAIT_LOAD_STATE"] * 2)

    def test_explicit_failed_load_can_retry_after_barrier(self):
        result, commands = self.load("retry")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(commands, ["LOAD_STATE_SLOT_PAUSED 4", "WAIT_LOAD_STATE"] * 2)

        events = [line.split() for line in (self.bundle / "load.events").read_text().splitlines()]
        self.assertEqual([event[0] for event in events], ["load", "done", "load", "done"])
        self.assertGreaterEqual(float(events[2][1]) - float(events[1][1]), 1.0)

    def test_barrier_without_load_record_is_not_success(self):
        result, commands = self.load("no-load")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("without a state-load record", result.stderr)
        self.assertEqual(len(commands), 2)

    def test_barrier_timeout_does_not_retry_uncertain_load(self):
        result, commands = self.load("barrier-timeout")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("WAIT_LOAD_STATE DONE timed out", result.stderr)
        self.assertEqual(commands, ["LOAD_STATE_SLOT_PAUSED 4", "WAIT_LOAD_STATE"])

    def test_ack_timeout_does_not_send_barrier_or_retry(self):
        result, commands = self.load("ack-timeout")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("acknowledgement timed out", result.stderr)
        self.assertEqual(commands, ["LOAD_STATE_SLOT_PAUSED 4"])

    def test_unsupported_session_does_not_send_load_or_barrier(self):
        result, commands = self.load("unsupported")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Frontend load barrier unavailable", result.stderr)
        self.assertEqual(commands, [])

    def test_frontend_probe_status_and_private_diagnostics(self):
        for mode, expected in (("success", "ready"), ("unsupported", "unsupported"),
                               ("probe-timeout", "timeout")):
            with self.subTest(mode=mode):
                self.mode.write_text(mode)
                result = self.adapter("check-frontend", "--retroarch-bin", str(self.binary))
                self.assertEqual(result.stdout, f"FRONTEND_LOAD_BARRIER={expected}\n")
                self.assertEqual(result.returncode, 0 if expected == "ready" else 1)
                self.assertNotIn(str(self.root), result.stdout + result.stderr)
                self.assertNotIn("[NetCMD]", result.stdout + result.stderr)
        result = self.adapter("check-frontend", "--retroarch-bin", str(self.root / "missing"))
        self.assertEqual(result.stdout, "FRONTEND_LOAD_BARRIER=missing\n")
        self.assertEqual(result.returncode, 1)

    def test_start_rejects_unsupported_before_staging(self):
        self.mode.write_text("unsupported")
        inputs = []
        for name in ("rom", "core", "config"):
            path = self.root / name
            path.touch()
            inputs.append(str(path))
        bundle = self.root / "must-not-exist"
        result = self.adapter("start", "--bundle-dir", str(bundle), "--rom", inputs[0],
                              "--core", inputs[1], "--base-config", inputs[2],
                              "--retroarch-bin", str(self.binary))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Frontend load barrier unavailable", result.stderr)
        self.assertFalse(bundle.exists())


if __name__ == "__main__":
    unittest.main()

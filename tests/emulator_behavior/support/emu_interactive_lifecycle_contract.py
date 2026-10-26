"""Exercise interactive teardown and step ordering through the real adapter.

A fake frontend runs the adapter's actual setsid/flock/timeout launch chain,
answers the stdin commands the adapter waits on, and can play a wedged
RetroArch that ignores QUIT and TERM. No emulator or display is involved.
"""
import ctypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest

REPO = Path(__file__).resolve().parents[3]
ADAPTER = REPO / "tools/adapters/retroarch_interactive_session.sh"

# Adopt the detached chain so the test reaps it; an unreaped zombie would keep
# the adapter's process-group probes alive.
if sys.platform.startswith("linux"):
    if ctypes.CDLL(None, use_errno=True).prctl(36, 1, 0, 0, 0) != 0:
        raise OSError(ctypes.get_errno(), "PR_SET_CHILD_SUBREAPER failed")

# Models the agent-control frontend: STEP_FRAME unpauses and runs N frames
# before pausing again, GET_STATUS reports PAUSED/PLAYING with the frame.
FAKE_FRONTEND = r'''#!/usr/bin/env python3
import os
from pathlib import Path
import signal
import sys
import threading
import time

if "--command" in sys.argv:
    print("[ERROR] [NetCMD] \tValid commands:")
    for command in ("LOAD_STATE_SLOT <slot number>",
                    "LOAD_STATE_SLOT_PAUSED <slot number>", "WAIT_LOAD_STATE No argument"):
        print("[ERROR] [NetCMD] \t\t" + command)
    raise SystemExit(1)

state = Path(os.environ["FAKE_FRONTEND_STATE"])
wedged = os.environ.get("FAKE_FRONTEND_WEDGED") == "1"
frame_seconds = float(os.environ.get("FAKE_FRAME_SECONDS", "0.02"))

def on_term(signum, frame):
    with (state / "signals").open("a") as signals:
        signals.write("TERM\n")
    if not wedged:
        os._exit(143)

signal.signal(signal.SIGTERM, on_term)
(state / "pids").write_text(f"{os.getpid()} {os.getppid()}\n")

lock = threading.Lock()
paused = "--start-paused" in sys.argv
frame = 0
remaining = 0
slot = 0

def run_frames():
    global frame, remaining, paused
    while True:
        time.sleep(frame_seconds)
        with lock:
            if paused:
                continue
            frame += 1
            if remaining:
                remaining -= 1
                paused = remaining == 0

threading.Thread(target=run_frames, daemon=True).start()

def reply(text):
    print(text, flush=True)

for line in sys.stdin:
    command = line.strip()
    with lock:
        with (state / "received").open("a") as received:
            received.write(f"{command}|frame={frame}|paused={int(paused)}\n")
        if command == "PING":
            reply("PING OK")
        elif command == "GET_STATUS":
            reply(f"GET_STATUS {'PAUSED' if paused else 'PLAYING'} n64,fake,crc32=0,frame={frame}")
        elif command.startswith("SET_PAUSE"):
            paused = command.endswith("ON")
            reply(f"SET_PAUSE {'PAUSED' if paused else 'PLAYING'}")
        elif command.startswith("STEP_FRAME"):
            remaining = int(command.split()[1])
            paused = False
            reply(command)
        elif command.startswith(("SET_INPUT_PORT", "CLEAR_INPUT_PORT")):
            reply(command)
        elif command in ("STATE_SLOT_PLUS", "STATE_SLOT_MINUS"):
            # Silent keybind events; a count in drop-slot-commands loses that many.
            drop_file = state / "drop-slot-commands"
            drops = int(drop_file.read_text()) if drop_file.exists() else 0
            if drops:
                drop_file.write_text(str(drops - 1))
            else:
                slot += 1 if command == "STATE_SLOT_PLUS" else -1
        elif command == "SAVE_STATE":
            path = Path(os.environ["FAKE_STATE_DIR"]) / "fake core" / (
                "game.state" + (str(slot) if slot else ""))
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(f"frame={frame}")
            reply('[INFO] [State] Saving state "RAM", 4 bytes.')
            reply(f'[INFO] [State] Saving state "{path}", 4 bytes.')
        elif command == "WAIT_SAVE_STATE":
            reply("WAIT_SAVE_STATE DONE")
        elif command == "SCREENSHOT":
            # Like RetroArch: the file exists at once, empty until the encode ends.
            capture = Path(os.environ["FAKE_CAPTURE_DIR"]) / f"fake-{frame}.png"
            capture.parent.mkdir(parents=True, exist_ok=True)
            capture.touch()
            def encode(path=capture):
                time.sleep(float(os.environ.get("FAKE_CAPTURE_SECONDS", "0")))
                path.write_bytes(b"not a real png")
            threading.Thread(target=encode, daemon=True).start()
        elif command == "QUIT" and not wedged:
            break
'''


def alive(pid):
    stat = subprocess.run(["ps", "-o", "stat=", "-p", str(pid)],
                          capture_output=True, text=True).stdout.strip()
    return bool(stat) and not stat.startswith("Z")


class InteractiveLifecycleContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="adapter lifecycle ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        # The macOS start path requires an app bundle layout.
        app = self.root / "fake frontend.app"
        self.binary = app / "Contents/MacOS/fake-frontend"
        self.binary.parent.mkdir(parents=True)
        nib = app / "Contents/Resources/en.lproj/MainMenu.nib"
        nib.parent.mkdir(parents=True)
        nib.touch()
        self.binary.write_text(FAKE_FRONTEND)
        self.binary.chmod(0o755)
        for name in ("rom", "core", "base.cfg"):
            (self.root / name).write_text("offline fixture")
        self.state = self.root / "state"
        self.state.mkdir()
        self.bundle = self.root / "bundle"
        self.env = {**os.environ, "TMPDIR": str(self.root),
                    "RETROARCH_TTL_GRACE_SECONDS": "0",
                    "FAKE_FRONTEND_STATE": str(self.state)}
        self.known = set()
        self.reaping = True
        if sys.platform.startswith("linux"):
            reaper = threading.Thread(target=self.reap, daemon=True)
            reaper.start()
            self.addCleanup(reaper.join, 5)
        self.addCleanup(self.kill_known)

    def reap(self):
        while self.reaping:
            for pid in list(self.known):
                try:
                    os.waitpid(pid, os.WNOHANG)
                except ChildProcessError:
                    pass
            time.sleep(0.05)

    def kill_known(self):
        for pid in self.known:
            if alive(pid):
                os.kill(pid, 9)
        deadline = time.monotonic() + 5
        while any(alive(pid) for pid in self.known) and time.monotonic() < deadline:
            time.sleep(0.05)
        self.reaping = False

    def adapter(self, *args, timeout=40):
        return subprocess.run(["bash", str(ADAPTER), *args], env=self.env, text=True,
                              capture_output=True, timeout=timeout, check=False)

    def start(self, ttl, **env):
        self.env.update(env)
        result = self.adapter("start", "--bundle-dir", str(self.bundle),
                              "--rom", str(self.root / "rom"), "--core", str(self.root / "core"),
                              "--base-config", str(self.root / "base.cfg"),
                              "--retroarch-bin", str(self.binary),
                              "--ttl-seconds", str(ttl), "--start-paused")
        if "Another RetroArch process is already running" in result.stderr:
            self.skipTest("a RetroArch process is running on this host")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.leader = int((self.bundle / "session.pid").read_text())
        self.frontend, self.timeout_pid = map(int, (self.state / "pids").read_text().split())
        self.known |= {self.leader, self.frontend, self.timeout_pid}
        self.addCleanup(self.adapter, "stop", "--bundle-dir", str(self.bundle))

    def wait_dead(self, pids, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if not any(alive(pid) for pid in pids):
                return
            time.sleep(0.1)
        self.fail(f"still running after {seconds}s: {[pid for pid in pids if alive(pid)]}")

    def received(self):
        return [line.split("|") for line in (self.state / "received").read_text().splitlines()]

    def test_stop_after_quit_timeout_leaves_no_frontend(self):
        self.start(600, FAKE_FRONTEND_WEDGED="1")
        result = self.adapter("stop", "--bundle-dir", str(self.bundle))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("QUIT timed out", result.stdout)
        self.wait_dead([self.frontend, self.timeout_pid, self.leader], 5)
        self.assertIn("QUIT", [entry[0] for entry in self.received()])
        # The group TERM reached the frontend itself before the KILL.
        self.assertIn("TERM", (self.state / "signals").read_text().split())

    def test_ttl_expiry_kills_wedged_frontend(self):
        self.start(3, FAKE_FRONTEND_WEDGED="1", RETROARCH_TTL_KILL_AFTER_SECONDS="1")
        self.wait_dead([self.frontend, self.timeout_pid, self.leader], 15)
        # TERM at the TTL, then KILL after the kill-after delay.
        self.assertIn("TERM", (self.state / "signals").read_text().split())
        status = self.adapter("status", "--bundle-dir", str(self.bundle))
        self.assertNotEqual(status.returncode, 0)
        self.assertIn("is not running", status.stderr)

    def test_status_bounds_fifo_open_and_write(self):
        # Keep a real zombie group leader until cleanup: kill -0 succeeds,
        # just as it can between TTL teardown and the detached chain's reap.
        leader = subprocess.Popen([sys.executable, "-c", "pass"], start_new_session=True)
        self.addCleanup(leader.wait, 5)
        self.wait_dead([leader.pid], 5)
        os.killpg(leader.pid, 0)
        (self.bundle / "logs").mkdir(parents=True)
        (self.bundle / "session.pid").write_text(f"{leader.pid}\n")
        fifo = self.bundle / "retroarch.stdin"
        os.mkfifo(fifo)

        for blocked in ("no reader", "full pipe"):
            with self.subTest(blocked=blocked):
                reader = None
                try:
                    if blocked == "full pipe":
                        # Model a frontend which still owns stdin but has
                        # stopped draining commands. Fill without blocking.
                        reader = os.open(fifo, os.O_RDWR | os.O_NONBLOCK)
                        while True:
                            try:
                                os.write(reader, b"x" * 4096)
                            except BlockingIOError:
                                break
                    status = self.adapter("status", "--bundle-dir", str(self.bundle), timeout=10)
                    self.assertNotEqual(status.returncode, 0)
                    self.assertIn("FIFO write failed or timed out", status.stderr)
                    self.assertFalse((self.bundle / "logs/interactive.commands.log").exists())
                finally:
                    if reader is not None:
                        os.close(reader)

    def background_step(self, frames):
        process = subprocess.Popen(["bash", str(ADAPTER), "send", "--bundle-dir", str(self.bundle),
                                    "--command", f"STEP_FRAME {frames}"],
                                   env=self.env, text=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(lambda: process.kill() if process.poll() is None else None)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if (self.state / "received").exists() and any(
                    entry[0] == f"STEP_FRAME {frames}" for entry in self.received()):
                return process
            time.sleep(0.02)
        self.fail("STEP_FRAME was not delivered")

    def test_commands_refused_while_step_in_flight(self):
        self.start(600, FAKE_FRAME_SECONDS="0.25")
        step = self.background_step(40)
        # The waiting agent goes away (interrupt or wait timeout) mid-step.
        step.terminate()
        step.communicate(timeout=10)
        self.assertTrue((self.bundle / "session.step-target").exists())

        for command in ("SET_INPUT_PORT 0 0x1", "CLEAR_INPUT_PORT 0", "READ_CORE_MEMORY 0x0 4",
                        "SCREENSHOT", "SAVE_STATE", "LOAD_STATE_SLOT 1", "STEP_FRAME 1"):
            with self.subTest(command=command):
                result = self.adapter("send", "--bundle-dir", str(self.bundle),
                                      "--command", command)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("still in flight", result.stderr)
        result = self.adapter("input", "--bundle-dir", str(self.bundle),
                              "--mask", "0x1", "--frames", "1")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Refusing input", result.stderr)
        sent = {entry[0] for entry in self.received()}
        self.assertEqual(sent, {"PING", "GET_STATUS", "STEP_FRAME 40"})

        deadline = time.monotonic() + 20
        while "PAUSED" not in self.adapter("status", "--bundle-dir", str(self.bundle)).stdout:
            self.assertLess(time.monotonic(), deadline, "step never finished")
            time.sleep(0.2)
        result = self.adapter("send", "--bundle-dir", str(self.bundle),
                              "--command", "SET_INPUT_PORT 0 0x1")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([entry for entry in self.received() if entry[0].startswith("SET_INPUT")],
                         [["SET_INPUT_PORT 0 0x1", "frame=40", "paused=1"]])
        self.assertFalse((self.bundle / "session.step-target").exists())

    def test_concurrent_input_waits_for_running_step(self):
        self.start(600, FAKE_FRAME_SECONDS="0.1")
        step = self.background_step(10)
        result = self.adapter("input", "--bundle-dir", str(self.bundle),
                              "--mask", "0x1", "--frames", "2")
        self.assertEqual(result.returncode, 0, result.stderr)
        _, step_error = step.communicate(timeout=10)
        self.assertEqual(step.returncode, 0, step_error)
        inputs = [entry for entry in self.received()
                  if entry[0].startswith(("SET_INPUT", "CLEAR_INPUT", "STEP_FRAME"))]
        self.assertEqual(inputs, [["STEP_FRAME 10", "frame=0", "paused=1"],
                                  ["SET_INPUT_PORT 0 0x1", "frame=10", "paused=1"],
                                  ["STEP_FRAME 2", "frame=10", "paused=1"],
                                  ["CLEAR_INPUT_PORT 0", "frame=12", "paused=1"]])

    def screenshot(self, wait_seconds):
        return self.adapter("screenshot", "--bundle-dir", str(self.bundle),
                            "--timeout-seconds", str(wait_seconds))

    def test_screenshot_waits_for_a_slow_encode(self):
        self.start(600, FAKE_CAPTURE_DIR=str(self.bundle / "captures"), FAKE_CAPTURE_SECONDS="3")
        result = self.screenshot(8)
        self.assertEqual(result.returncode, 0, result.stderr)
        capture = Path(result.stdout.strip().splitlines()[-1])
        self.assertEqual(capture.read_bytes(), b"not a real png")

    def test_screenshot_timeout_names_the_unfinished_capture(self):
        self.start(600, FAKE_CAPTURE_DIR=str(self.bundle / "captures"), FAKE_CAPTURE_SECONDS="5")
        result = self.screenshot(1)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("fake-0.png was still being written after 1s (0 bytes)", result.stderr)

    def save_slot(self, slot):
        return self.adapter("save-slot", "--bundle-dir", str(self.bundle), "--slot", str(slot))

    def slot_files(self):
        return {path.name: path.read_text()
                for path in (self.bundle / "states").rglob("game.state*")}

    def test_save_slot_restores_and_retries_after_lost_slot_command(self):
        self.start(600, FAKE_STATE_DIR=str(self.bundle / "states"))
        self.assertEqual(self.save_slot(1).returncode, 0)
        self.assertEqual(self.adapter("send", "--bundle-dir", str(self.bundle),
                                      "--command", "STEP_FRAME 3").returncode, 0)
        before = self.slot_files()
        # The frontend drops the next slot command, so the first save lands in slot 1.
        (self.state / "drop-slot-commands").write_text("1")
        result = self.save_slot(2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("landed in slot 1 instead of 2", result.stderr)
        files = self.slot_files()
        self.assertEqual(files["game.state1"], before["game.state1"])
        self.assertEqual(set(files), {"game.state1", "game.state2"})
        self.assertEqual((self.bundle / "session.state-slot").read_text().strip(), "2")
        self.assertFalse((self.bundle / "save-slot.backup").exists())


if __name__ == "__main__":
    unittest.main()

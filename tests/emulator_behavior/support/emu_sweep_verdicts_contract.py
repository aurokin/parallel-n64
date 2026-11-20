"""Check the boot-sweep verdict rules on recorded and synthetic observations."""
import json
from pathlib import Path
import sys
import unittest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools/sweep"))
import verdicts  # noqa: E402

FIXTURES = REPO / "tests/emulator_behavior/fixtures/sweep/observations.json"
PLAN = {"frames": 4800, "captures": list(range(600, 4801, 600)), "frozen_min_captures": 5}


def recorded(name):
    """A run_scenarios record from the November 16 sweep, as a sweep observation."""
    raw = json.loads(FIXTURES.read_text())[name]
    return {"exit_class": raw["exit_class"], "error": raw["error"], "log_flags": raw["log_flags"],
            "frames_run": raw["frames_run"], "load_failure_logged": False,
            "captures": [{"frame": c["after_frames"], "sha256": c["capture_sha256"], "blank": c["blank"]}
                         for c in raw["checkpoints"]]}


def synthetic(hashes, *, exit_class="clean", flags=(), blank=(), frames=None, error=None, load=False):
    captures = [{"frame": f, "sha256": h, "blank": f in blank} for f, h in zip(PLAN["captures"], hashes)]
    return {"exit_class": exit_class, "error": error, "log_flags": list(flags), "load_failure_logged": load,
            "frames_run": PLAN["frames"] if frames is None else frames, "captures": captures}


class RecordedRuns(unittest.TestCase):
    def check(self, name, verdict, sub=None, **fields):
        result = verdicts.classify(recorded(name), PLAN)
        self.assertEqual((result["verdict"], result["sub_class"]), (verdict, sub), result)
        for key, value in fields.items():
            self.assertEqual(result[key], value, key)

    def test_clean_runs_keep_their_mechanical_class(self):
        self.check("clean-pass", "pass")
        self.check("clean-blank", "blank", first_nonblank_frame=None)

    def test_frozen_records_where_the_static_tail_starts(self):
        self.check("static-from-start", "frozen", "static-from-start", static_from_frame=600, static_captures=8)
        self.check("advanced-then-static", "frozen", "advanced-then-static", static_from_frame=1800,
                   static_captures=6)

    def test_quit_timeout_after_every_frame_is_not_a_hang(self):
        # The previous sweep called this "hang"; every frame and capture completed.
        self.check("quit-timeout", "quit-fail", "quit-timeout", screen="pass", complete=True)

    def test_stopped_stepping_is_a_hang_even_when_quit_also_timed_out(self):
        self.check("step-timeout", "hang", "step", complete=False, screen=None)
        self.check("step-timeout-then-quit-timeout", "hang", "step")
        self.assertIn("3003", verdicts.classify(recorded("step-timeout-then-quit-timeout"), PLAN)["reason"])


class SyntheticRuns(unittest.TestCase):
    def classify(self, observation):
        return verdicts.classify(observation, PLAN)

    def test_frozen_threshold_counts_trailing_identical_captures(self):
        four = self.classify(synthetic(["a", "b", "c", "d", "e", "x", "x", "x"]))
        self.assertEqual(four["verdict"], "pass")
        self.assertEqual((four["static_from_frame"], four["static_captures"]), (3600, 3))
        five = self.classify(synthetic(["a", "b", "c", "x", "x", "x", "x", "x"]))
        self.assertEqual((five["verdict"], five["sub_class"], five["static_from_frame"]),
                         ("frozen", "advanced-then-static", 2400))
        # An earlier static stretch that ends does not count.
        self.assertEqual(self.classify(synthetic(["x"] * 7 + ["y"]))["verdict"], "pass")

    def test_all_blank_outranks_static(self):
        result = self.classify(synthetic(["k"] * 8, blank=PLAN["captures"]))
        self.assertEqual(result["verdict"], "blank")
        went_dark = self.classify(synthetic(["a", "b", "c", "k", "k", "k", "k", "k"], blank=PLAN["captures"][3:]))
        self.assertEqual((went_dark["verdict"], went_dark["first_nonblank_frame"]), ("frozen", 600))

    def test_session_failures(self):
        live = ["a", "b"]
        self.assertEqual(self.classify(synthetic(live, exit_class="nonzero", frames=1300))["verdict"], "crash")
        ttl = self.classify(synthetic(live, exit_class="timeout", flags=["ttl-expired"], frames=1300))
        self.assertEqual((ttl["verdict"], ttl["sub_class"]), ("hang", "ttl"))
        alive = self.classify(synthetic(live + ["c"], exit_class="timeout", frames=1800))
        self.assertEqual((alive["verdict"], alive["sub_class"]), ("hang", "unresponsive"))

    def test_shutdown_failures_after_a_complete_run(self):
        hashes = list("abcdefgh")
        stop = self.classify(synthetic(hashes, exit_class="nonzero", flags=["stop-failed"]))
        self.assertEqual((stop["verdict"], stop["sub_class"], stop["screen"]), ("quit-fail", "stop-failed", "pass"))
        reaped = self.classify(synthetic(["x"] * 8, exit_class="orphan-reaped", flags=["orphan-reaped"]))
        self.assertEqual((reaped["verdict"], reaped["sub_class"], reaped["screen"]),
                         ("quit-fail", "orphan-reaped", "frozen"))

    def test_load_and_harness_failures(self):
        self.assertEqual(self.classify(synthetic([], exit_class="nonzero", frames=0, load=True))["verdict"],
                         "load-fail")
        error = self.classify(synthetic(["a"], exit_class="capture-failed", frames=1200))
        self.assertEqual((error["verdict"], error["sub_class"]), ("error", "capture-failed"))
        # A clean exit that is missing captures is evidence of a harness fault, not a pass.
        self.assertEqual(self.classify(synthetic(list("abcdefg")))["verdict"], "error")

    def test_load_failure_text(self):
        self.assertTrue(verdicts.load_failure_logged("[ERROR] Failed to load content 'x.z64'"))
        self.assertFalse(verdicts.load_failure_logged("[INFO] Content loaded"))


class BlankCaptures(unittest.TestCase):
    def test_near_black_noise_is_blank_and_content_is_not(self):
        from PIL import Image

        noisy = Image.new("RGB", (320, 240), (3, 2, 4))
        for x in range(0, 320, 4):
            noisy.putpixel((x, 0), (5 + x % 3, 1, 0))
        for i in range(300):  # 0.4% isolated bright pixels
            noisy.putpixel((i % 320, 10 + i // 320), (255, 255, 255))
        self.assertTrue(verdicts.capture_is_blank(noisy))
        logo = noisy.copy()
        for y in range(80, 160):
            for x in range(100, 220):
                logo.putpixel((x, y), (200, 30 + y % 50, 40))
        self.assertFalse(verdicts.capture_is_blank(logo))
        self.assertTrue(verdicts.capture_is_blank(Image.new("RGB", (64, 64), (0, 0, 255))))


if __name__ == "__main__":
    unittest.main()

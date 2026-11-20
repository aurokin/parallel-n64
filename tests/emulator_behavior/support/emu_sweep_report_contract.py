"""Check boot-sweep report assembly and shedding on synthetic result directories."""
import contextlib
import csv
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools/sweep"))
import report  # noqa: E402

ROMS = {"a" * 64: "Alpha (USA).z64", "b" * 64: "Beta (Japan).z64", "c" * 64: "Gamma (Europe).z64"}


def write_run(result_dir, rom, verdict, *, core="1" * 64, config="2" * 64, ended="2026-11-20T10:00:00Z",
              suffix=""):
    from PIL import Image

    key = f"{rom[:16]}-{core[:12]}-{config[:12]}{suffix}"
    run_dir = result_dir / "runs" / key
    (run_dir / "bundles/x/captures").mkdir(parents=True)
    (run_dir / "bundles/x/captures/shot.png").write_bytes(b"\x89PNG" + bytes(4096))
    Image.new("RGB", (1920, 218), (40, 40, 40)).save(run_dir / "contact.jpg")
    record = {"key": key, "core_sha256": core, "config_hash": config, "rom_sha256": rom, "ended": ended,
              "rom": {"name": ROMS[rom]}, "verdict": verdict, "sub_class": None, "screen": verdict,
              "frames_reached": 4800, "static_from_frame": None,
              "core": {"profile": result_dir.name, "library_name": "Core", "library_version": "1"}}
    (run_dir / "record.json").write_text(json.dumps(record))
    return run_dir


class Report(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.ours, self.next = self.root / "ours", self.root / "next"
        a, b, c = ROMS
        self.agree = [write_run(self.ours, a, "pass"), write_run(self.next, a, "pass", core="9" * 64)]
        self.differ = [write_run(self.ours, b, "frozen"), write_run(self.next, b, "pass", core="9" * 64)]
        self.only_ours = write_run(self.ours, c, "pass")

    def tearDown(self):
        self.tmp.cleanup()

    def columns(self, *dirs):
        return [col for d in dirs for col in report.columns_for(d.name, report.load_records(d))]

    def main(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = report.main([*map(str, argv)])
        return status, out.getvalue()

    def test_statuses(self):
        rows = {r["rom_name"]: r for r in report.compare(self.columns(self.ours, self.next))}
        self.assertEqual(rows["Alpha (USA).z64"]["status"], "agree")
        self.assertEqual(rows["Beta (Japan).z64"]["status"], "disagree")
        self.assertEqual(rows["Gamma (Europe).z64"]["status"], "incomplete")
        self.assertEqual(rows["Gamma (Europe).z64"]["verdicts"]["next"], report.NOT_RUN)
        self.assertEqual({r["status"] for r in report.compare(self.columns(self.ours))}, {"single"})

    def test_rebuilt_core_is_its_own_column_and_latest_run_counts(self):
        a = next(iter(ROMS))
        write_run(self.ours, a, "crash", core="3" * 64)
        write_run(self.ours, a, "hang", ended="2026-11-20T11:00:00Z", config="4" * 64)
        write_run(self.ours, a, "error", suffix=".incomplete-1")
        columns = self.columns(self.ours)
        self.assertEqual(sorted(c.label for c in columns),
                         ["ours@11111111.22222222", "ours@11111111.44444444", "ours@33333333.22222222"])
        same = report.columns_for("ours", [
            {**json.loads((self.agree[0] / "record.json").read_text()), "_dir": "x", "verdict": "pass"},
            {**json.loads((self.agree[0] / "record.json").read_text()), "_dir": "y", "verdict": "frozen",
             "ended": "2026-11-21T00:00:00Z"}])
        self.assertEqual(same[0].runs[a]["verdict"], "frozen")

    def test_outputs_and_shed(self):
        out = self.root / "report"
        status, output = self.main(f"ours={self.ours}", f"next={self.next}", "--out", out, "--shed")
        self.assertEqual(status, 0, output)
        with (out / "verdicts.csv").open() as handle:
            rows = {r["rom_name"]: r for r in csv.DictReader(handle)}
        beta = rows["Beta (Japan).z64"]
        self.assertEqual((beta["ours"], beta["next"], beta["status"]), ("frozen", "pass", "disagree"))
        self.assertTrue(beta["ours_sheet"].endswith("contact.jpg"))
        comparison = Path(beta["comparison_sheet"])
        from PIL import Image
        with Image.open(comparison) as image:
            self.assertGreater(image.height, 2 * 218)  # both cores, one above the other
        with (out / "disagreements.csv").open() as handle:
            self.assertEqual([r["rom_name"] for r in csv.DictReader(handle)], ["Beta (Japan).z64"])
        self.assertTrue((out / "sheets/ours/page-001.jpg").is_file())
        self.assertIn("| Beta (Japan).z64 | frozen | pass |", (out / "summary.md").read_text())
        # Only the agreeing ROM loses its bundles; records and sheets stay everywhere.
        for run_dir in self.agree:
            self.assertFalse((run_dir / "bundles").exists())
        for run_dir in [*self.agree, *self.differ, self.only_ours]:
            self.assertTrue((run_dir / "record.json").is_file() and (run_dir / "contact.jpg").is_file())
        for run_dir in [*self.differ, self.only_ours]:
            self.assertTrue((run_dir / "bundles").is_dir())
        self.assertIn("shed 2 agreeing runs", output)

    def test_shed_and_labels_need_distinct_columns(self):
        self.assertEqual(self.main(self.ours, "--out", self.root / "r1", "--shed")[0], 2)
        self.assertTrue((self.agree[0] / "bundles").is_dir())
        self.assertEqual(self.main(f"x={self.ours}", f"x={self.next}", "--out", self.root / "r2")[0], 2)


if __name__ == "__main__":
    unittest.main()

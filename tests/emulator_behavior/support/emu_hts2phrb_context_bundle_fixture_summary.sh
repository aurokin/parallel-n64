#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
TMP_DIR="$(mktemp -d /tmp/parallel-n64-hts2phrb-context-fixtures-XXXXXX)"
REPO_RELATIVE_DIR="$ROOT_DIR/artifacts/hts2phrb-context-fixture-summary-contract-$$"
trap 'rm -rf "$TMP_DIR" "$REPO_RELATIVE_DIR"' EXIT

CACHE_PATH="$TMP_DIR/sample.htc"
BUNDLE_A_DIR="$TMP_DIR/bundle-a"
BUNDLE_B_DIR="$TMP_DIR/bundle-b"
REPO_BUNDLE_A_DIR="$REPO_RELATIVE_DIR/bundle-a"
REPO_BUNDLE_B_DIR="$REPO_RELATIVE_DIR/bundle-b"
SUMMARY_DIR="$TMP_DIR/summary"
SUMMARY_REPO_DIR="$TMP_DIR/summary-repo"
SUMMARY_JSON="$SUMMARY_DIR/validation-summary.json"
SUMMARY_MD="$SUMMARY_DIR/validation-summary.md"
SUMMARY_REPO_JSON="$SUMMARY_REPO_DIR/validation-summary.json"
SUMMARY_REPO_MD="$SUMMARY_REPO_DIR/validation-summary.md"
OUT_JSON="$TMP_DIR/out-json"
OUT_MD="$TMP_DIR/out-md"
OUT_DIR="$TMP_DIR/out-dir"
OUT_REPO_JSON="$TMP_DIR/out-repo-json"
OUT_REPO_MD="$TMP_DIR/out-repo-md"

mkdir -p "$BUNDLE_A_DIR/traces" "$BUNDLE_B_DIR/traces" "$REPO_BUNDLE_A_DIR/traces" "$REPO_BUNDLE_B_DIR/traces" "$SUMMARY_DIR" "$SUMMARY_REPO_DIR"

python3 - "$ROOT_DIR" "$CACHE_PATH" "$BUNDLE_A_DIR/traces/hires-evidence.json" "$BUNDLE_B_DIR/traces/hires-evidence.json" "$REPO_BUNDLE_A_DIR/traces/hires-evidence.json" "$REPO_BUNDLE_B_DIR/traces/hires-evidence.json" "$SUMMARY_JSON" "$SUMMARY_MD" "$SUMMARY_REPO_JSON" "$SUMMARY_REPO_MD" <<'PY'
import gzip
import hashlib
import json
import os
import struct
import sys
from pathlib import Path

root_dir = Path(sys.argv[1])
cache_path = Path(sys.argv[2])
bundle_a = Path(sys.argv[3])
bundle_b = Path(sys.argv[4])
repo_bundle_a = Path(sys.argv[5])
repo_bundle_b = Path(sys.argv[6])
summary_json = Path(sys.argv[7])
summary_md = Path(sys.argv[8])
summary_repo_json = Path(sys.argv[9])
summary_repo_md = Path(sys.argv[10])

TXCACHE_FORMAT_VERSION = 0x08000000
GL_RGBA = 0x1908
GL_UNSIGNED_BYTE = 0x1401
GL_RGBA8 = 0x8058

records = [
    (0x11111111, 0xAAAABBBB, 258),
    (0x22222222, 0xCCCCDDDD, 514),
]

with gzip.open(cache_path, "wb") as fp:
    fp.write(struct.pack("<i", TXCACHE_FORMAT_VERSION))
    fp.write(struct.pack("<i", 0))
    for texture_crc, palette_crc, formatsize in records:
        checksum64 = (palette_crc << 32) | texture_crc
        payload = bytes([
            0x10, 0x20, 0x30, 0xFF,
            0x40, 0x50, 0x60, 0xFF,
            0x70, 0x80, 0x90, 0xFF,
            0xA0, 0xB0, 0xC0, 0xFF,
        ])
        fp.write(struct.pack("<Q", checksum64))
        fp.write(struct.pack("<IIIHHB", 2, 2, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 1))
        fp.write(struct.pack("<H", formatsize))
        fp.write(struct.pack("<I", len(payload)))
        fp.write(payload)


cache_sha = hashlib.sha256(cache_path.read_bytes()).hexdigest()
rom_path = cache_path.with_name("synthetic-rom.z64")
core_path = cache_path.with_name("synthetic-core.so")
rom_path.write_bytes(b"synthetic-rom")
core_path.write_bytes(b"synthetic-core")
rom_sha = hashlib.sha256(rom_path.read_bytes()).hexdigest()
core_sha = hashlib.sha256(core_path.read_bytes()).hexdigest()
commands = ["WAIT_COMMAND_READY 120", "SCREENSHOT", "QUIT"]
commands_text = "\n".join(commands) + "\n"
command_signature = hashlib.sha256(commands_text.encode()).hexdigest()


def write_bundle(path, texture_crc, palette_crc, formatsize, fixture_id):
    bundle_dir = path.parent.parent
    (bundle_dir / "bundle.json").write_text(json.dumps({
        "fixture_id": fixture_id,
        "inputs": {
            "rom_path": str(rom_path),
            "rom_sha256": rom_sha,
            "hires_pack_path": str(cache_path),
            "hires_pack_sha256": cache_sha,
        },
        "status": {
            "runtime_executed": True,
        },
    }, indent=2) + "\n")
    (bundle_dir / "traces" / "fixture-verification.json").write_text(json.dumps({
        "fixture_id": fixture_id,
        "passed": True,
        "failures": [],
    }, indent=2) + "\n")
    (bundle_dir / "logs").mkdir(exist_ok=True)
    (bundle_dir / "config.env").write_text("synthetic=1\n")
    (bundle_dir / "retroarch.expected.commands.log").write_text(commands_text)
    (bundle_dir / "retroarch.planned.commands.log").write_text(commands_text)
    (bundle_dir / "retroarch.executed.commands.log").write_text(commands_text)
    (bundle_dir / "logs" / "retroarch.commands.log").write_text(commands_text)
    (bundle_dir / "retroarch.session.env").write_text(
        f"MODE=on\nROM_PATH={rom_path}\nROM_SHA256={rom_sha}\nCORE_PATH={core_path}\n"
        f"CORE_SHA256={core_sha}\nHIRES_CACHE_PATH={cache_path}\nHIRES_CACHE_SHA256={cache_sha}\n"
        f"COMMAND_SIGNATURE={command_signature}\n"
    )
    (bundle_dir / "retroarch.run.env").write_text(
        "RUNTIME_EXECUTED=1\nRETROARCH_EXIT_STATUS=0\nFORCED_TERMINATION=0\n"
    )
    path.write_text(json.dumps({
        "available": True,
        "cache_loaded": True,
        "cache_path": str(cache_path),
        "cache_sha256": cache_sha,
        "summary": {
            "provider": "on",
            "source_mode": "phrb-only",
            "entry_count": 1,
            "native_sampled_entry_count": 1,
            "source_counts": {"phrb": 1},
            "descriptor_path_counts": {
                "sampled": 1,
                "native_checksum": 0,
                "generic": 0,
                "compat": 0
            },
        },
        "ci_palette_probe": {"families": [], "usages": [], "emulated_tmem": []},
        "sampled_object_probe": {
            "exact_hit_count": 1,
            "exact_miss_count": 0,
            "exact_conflict_miss_count": 0,
            "exact_unresolved_miss_count": 0,
            "top_groups": [
                {
                    "fields": {
                        "draw_class": "texrect",
                        "cycle": "1cycle",
                        "fmt": "2",
                        "siz": "1",
                        "off": "0",
                        "stride": "2",
                        "wh": "2x2",
                        "fs": str(formatsize),
                        "sampled_low32": f"{texture_crc:08x}",
                        "sampled_entry_pcrc": f"{palette_crc:08x}",
                        "sampled_sparse_pcrc": f"{palette_crc:08x}",
                        "sampled_entry_count": "1",
                        "sampled_used_count": "1"
                    },
                    "upload_low32s": [{"value": f"{texture_crc:08x}"}],
                    "upload_pcrcs": [{"value": f"{palette_crc:08x}"}]
                }
            ]
        }
    }, indent=2) + "\n")


write_bundle(bundle_a, 0x11111111, 0xAAAABBBB, 258, "paper-mario-title-screen")
write_bundle(bundle_b, 0x22222222, 0xCCCCDDDD, 514, "paper-mario-file-select")
write_bundle(repo_bundle_a, 0x11111111, 0xAAAABBBB, 258, "paper-mario-title-screen")
write_bundle(repo_bundle_b, 0x22222222, 0xCCCCDDDD, 514, "paper-mario-file-select")

summary = {
    "summary_title": "fixture summary",
    "all_passed": True,
    "fixtures": [
        {
            "label": "title-screen",
            "fixture_id": "paper-mario-title-screen",
            "bundle_dir": "../bundle-a",
            "passed": True,
        },
        {
            "label": "file-select",
            "fixture_id": "paper-mario-file-select",
            "bundle_dir": "../bundle-b",
            "passed": True,
        }
    ]
}
summary_json.write_text(json.dumps(summary, indent=2) + "\n")
summary_md.write_text("# validation summary\n")

summary_repo = {
    "summary_title": "fixture summary repo-root",
    "all_passed": True,
    "fixtures": [
        {
            "label": "title-screen",
            "fixture_id": "paper-mario-title-screen",
            "bundle_dir": os.path.relpath(repo_bundle_a.parent.parent, root_dir),
            "passed": True,
        },
        {
            "label": "file-select",
            "fixture_id": "paper-mario-file-select",
            "bundle_dir": os.path.relpath(repo_bundle_b.parent.parent, root_dir),
            "passed": True,
        }
    ]
}
summary_repo_json.write_text(json.dumps(summary_repo, indent=2) + "\n")
summary_repo_md.write_text("# validation summary repo-root\n")
PY

python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$OUT_JSON" \
  > "$TMP_DIR/report-json.json"

python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_MD" \
  --stdout-format json \
  --output-dir "$OUT_MD" \
  > "$TMP_DIR/report-md.json"

python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_DIR" \
  --stdout-format json \
  --output-dir "$OUT_DIR" \
  > "$TMP_DIR/report-dir.json"

if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_REPO_JSON" \
  --stdout-format json \
  --output-dir "$OUT_REPO_JSON" \
  > "$TMP_DIR/report-repo-json.json" 2>"$TMP_DIR/report-repo-json.err"; then
  echo "expected repo-root-relative fixture summary to fail without summary-relative bundle paths" >&2
  exit 1
fi
if ! rg -q "Resolved fixture bundle has no hires-evidence.json" "$TMP_DIR/report-repo-json.err"; then
  echo "expected repo-root-relative JSON summary to fail on deterministic summary-relative lookup" >&2
  cat "$TMP_DIR/report-repo-json.err" >&2
  exit 1
fi

if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_REPO_MD" \
  --stdout-format json \
  --output-dir "$OUT_REPO_MD" \
  > "$TMP_DIR/report-repo-md.json" 2>"$TMP_DIR/report-repo-md.err"; then
  echo "expected repo-root-relative fixture summary markdown to fail without summary-relative bundle paths" >&2
  exit 1
fi
if ! rg -q "Resolved fixture bundle has no hires-evidence.json" "$TMP_DIR/report-repo-md.err"; then
  echo "expected repo-root-relative markdown summary to fail on deterministic summary-relative lookup" >&2
  cat "$TMP_DIR/report-repo-md.err" >&2
  exit 1
fi

cp "$SUMMARY_JSON" "$TMP_DIR/fixture-summary.good.json"
python3 - "$SUMMARY_JSON" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["fixtures"][0].pop("fixture_id", None)
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-missing-fixture-summary-id" \
  > "$TMP_DIR/report-missing-fixture-summary-id.json" 2>"$TMP_DIR/report-missing-fixture-summary-id.err"; then
  echo "expected fixture summary to fail when a fixture entry has no fixture_id" >&2
  exit 1
fi
if ! rg -q "Fixture validation summary fixture entry has no fixture_id" "$TMP_DIR/report-missing-fixture-summary-id.err"; then
  echo "expected missing fixture-summary fixture_id rejection" >&2
  cat "$TMP_DIR/report-missing-fixture-summary-id.err" >&2
  exit 1
fi
cp "$TMP_DIR/fixture-summary.good.json" "$SUMMARY_JSON"

mv "$BUNDLE_A_DIR/retroarch.executed.commands.log" "$TMP_DIR/executed.commands.log"
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-missing-executed" \
  > "$TMP_DIR/report-missing-executed.json" 2>"$TMP_DIR/report-missing-executed.err"; then
  echo "expected fixture summary to fail without executed command provenance" >&2
  exit 1
fi
if ! rg -q "missing executed command provenance" "$TMP_DIR/report-missing-executed.err"; then
  echo "expected missing executed command rejection" >&2
  cat "$TMP_DIR/report-missing-executed.err" >&2
  exit 1
fi
mv "$TMP_DIR/executed.commands.log" "$BUNDLE_A_DIR/retroarch.executed.commands.log"

cp "$BUNDLE_A_DIR/retroarch.executed.commands.log" "$TMP_DIR/executed.commands.good.log"
printf '%s\n' "WAIT_COMMAND_READY 120" "SCREENSHOT" > "$BUNDLE_A_DIR/retroarch.executed.commands.log"
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-divergent-executed" \
  > "$TMP_DIR/report-divergent-executed.json" 2>"$TMP_DIR/report-divergent-executed.err"; then
  echo "expected fixture summary to fail with divergent executed command provenance" >&2
  exit 1
fi
if ! rg -q "executed command log does not match expected commands" "$TMP_DIR/report-divergent-executed.err"; then
  echo "expected divergent executed command rejection" >&2
  cat "$TMP_DIR/report-divergent-executed.err" >&2
  exit 1
fi
cp "$TMP_DIR/executed.commands.good.log" "$BUNDLE_A_DIR/retroarch.executed.commands.log"

cp "$BUNDLE_A_DIR/bundle.json" "$TMP_DIR/bundle-a.good.json"
python3 - "$BUNDLE_A_DIR/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
inputs = data.get("inputs") or {}
inputs.pop("rom_path", None)
inputs.pop("rom_sha256", None)
data["inputs"] = inputs
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-missing-rom" \
  > "$TMP_DIR/report-missing-rom.json" 2>"$TMP_DIR/report-missing-rom.err"; then
  echo "expected fixture summary to fail without manifest ROM provenance" >&2
  exit 1
fi
if ! rg -q "missing ROM path/SHA provenance" "$TMP_DIR/report-missing-rom.err"; then
  echo "expected missing manifest ROM provenance rejection" >&2
  cat "$TMP_DIR/report-missing-rom.err" >&2
  exit 1
fi
cp "$TMP_DIR/bundle-a.good.json" "$BUNDLE_A_DIR/bundle.json"

cp "$BUNDLE_A_DIR/traces/hires-evidence.json" "$TMP_DIR/bundle-a-hires.good.json"
python3 - "$BUNDLE_A_DIR/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["summary"]["source_counts"] = {"phrb": 0}
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-zero-phrb-source" \
  > "$TMP_DIR/report-zero-phrb-source.json" 2>"$TMP_DIR/report-zero-phrb-source.err"; then
  echo "expected fixture summary to fail without positive PHRB source ownership" >&2
  exit 1
fi
if ! rg -q "no explicit PHRB source ownership" "$TMP_DIR/report-zero-phrb-source.err"; then
  echo "expected missing PHRB source ownership rejection" >&2
  cat "$TMP_DIR/report-zero-phrb-source.err" >&2
  exit 1
fi
cp "$TMP_DIR/bundle-a-hires.good.json" "$BUNDLE_A_DIR/traces/hires-evidence.json"

python3 - "$TMP_DIR/report-json.json" "$TMP_DIR/report-md.json" "$TMP_DIR/report-dir.json" <<'PY'
import json
import sys
from pathlib import Path

reports = [json.loads(Path(path).read_text()) for path in sys.argv[1:]]

for report in reports:
    if report["conversion_outcome"] != "promotable-runtime-package":
        raise SystemExit(f"unexpected conversion outcome: {report['conversion_outcome']!r}")
    if report["requested_family_count"] != 2:
        raise SystemExit(f"unexpected requested family count: {report['requested_family_count']!r}")
    if not report["runtime_overlay_built"]:
        raise SystemExit(f"expected runtime overlay build from fixture-summary context bundles: {report!r}")
    if report["runtime_overlay_reason"] != "runtime-context-available":
        raise SystemExit(f"unexpected runtime overlay reason: {report['runtime_overlay_reason']!r}")
    if report.get("binding_count") != 2 or report.get("unresolved_count") != 0:
        raise SystemExit(f"expected two deterministic bindings with no unresolved transport cases, got {report!r}")
    if len(report.get("context_bundle_resolutions") or []) != 2:
        raise SystemExit(f"unexpected context bundle resolutions: {report.get('context_bundle_resolutions')!r}")
    if report.get("context_bundle_input_count") != 1:
        raise SystemExit(f"unexpected context bundle input count: {report.get('context_bundle_input_count')!r}")
    if report.get("context_bundle_resolution_count") != 2:
        raise SystemExit(f"unexpected expanded context bundle count: {report.get('context_bundle_resolution_count')!r}")
    if report.get("runtime_state_counts") != {"runtime-bound": 2}:
        raise SystemExit(f"unexpected runtime state counts: {report.get('runtime_state_counts')!r}")
    imported = report.get("imported_index_summary") or {}
    if imported.get("canonical_sampled_record_count") != 2:
        raise SystemExit(f"unexpected canonical sampled count: {imported!r}")
    if sorted(item.get("selection_reason") for item in report["context_bundle_resolutions"]) != [
        "validation-summary-fixtures",
        "validation-summary-fixtures",
    ]:
        raise SystemExit(f"unexpected context bundle resolution reasons: {report['context_bundle_resolutions']!r}")
    if sorted(item.get("input_kind") for item in report["context_bundle_resolutions"]) != [
        "fixture-validation-summary",
        "fixture-validation-summary",
    ]:
        raise SystemExit(f"unexpected context bundle input kinds: {report['context_bundle_resolutions']!r}")

for report in reports:
    modes = sorted(item.get("bundle_reference_mode") for item in report["context_bundle_resolutions"])
    if modes != ["summary-relative", "summary-relative"]:
        raise SystemExit(f"unexpected fixture summary bundle reference modes: {report['context_bundle_resolutions']!r}")
PY

echo "emu_hts2phrb_context_bundle_fixture_summary: PASS"

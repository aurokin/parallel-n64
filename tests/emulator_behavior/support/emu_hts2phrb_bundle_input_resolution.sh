#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
TMP_DIR="$(mktemp -d /tmp/parallel-n64-hts2phrb-bundle-input-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

CACHE_PATH="$TMP_DIR/sample.htc"
BUNDLE_DIR="$TMP_DIR/bundle"
TRACE_DIR="$BUNDLE_DIR/traces"
SUMMARY_DIR="$TMP_DIR/summary-root"
SUMMARY_CWD_DIR="$TMP_DIR/summary-root-cwd"
SUMMARY_JSON="$SUMMARY_DIR/validation-summary.json"
SUMMARY_MD="$SUMMARY_DIR/validation-summary.md"
SUMMARY_CWD_JSON="$SUMMARY_CWD_DIR/validation-summary.json"
SUMMARY_CWD_MD="$SUMMARY_CWD_DIR/validation-summary.md"
RAW_EVIDENCE="$TMP_DIR/raw/hires-evidence.json"
UNPROVEN_RAW_EVIDENCE="$TMP_DIR/unproven-raw/hires-evidence.json"
OUT_EVIDENCE="$TMP_DIR/out-evidence"
OUT_RAW_EVIDENCE="$TMP_DIR/out-raw-evidence"
OUT_RAW_CONTEXT="$TMP_DIR/out-raw-context"
OUT_SUMMARY_JSON="$TMP_DIR/out-summary-json"
OUT_SUMMARY_MD="$TMP_DIR/out-summary-md"
OUT_SUMMARY_CWD_JSON="$TMP_DIR/out-summary-cwd-json"
OUT_SUMMARY_CWD_MD="$TMP_DIR/out-summary-cwd-md"

mkdir -p "$TRACE_DIR" "$SUMMARY_DIR" "$SUMMARY_CWD_DIR" "$(dirname "$RAW_EVIDENCE")" "$(dirname "$UNPROVEN_RAW_EVIDENCE")"

python3 - "$CACHE_PATH" "$TRACE_DIR/hires-evidence.json" "$RAW_EVIDENCE" "$UNPROVEN_RAW_EVIDENCE" "$SUMMARY_JSON" "$SUMMARY_MD" "$SUMMARY_CWD_JSON" "$SUMMARY_CWD_MD" <<'PY'
import gzip
import json
import struct
import sys
from pathlib import Path

cache_path = Path(sys.argv[1])
evidence_path = Path(sys.argv[2])
raw_evidence_path = Path(sys.argv[3])
unproven_raw_evidence_path = Path(sys.argv[4])
summary_json = Path(sys.argv[5])
summary_md = Path(sys.argv[6])
summary_cwd_json = Path(sys.argv[7])
summary_cwd_md = Path(sys.argv[8])

TXCACHE_FORMAT_VERSION = 0x08000000
GL_RGBA = 0x1908
GL_UNSIGNED_BYTE = 0x1401
GL_RGBA8 = 0x8058

texture_crc = 0x11111111
palette_crc = 0xAAAABBBB
checksum64 = (palette_crc << 32) | texture_crc
formatsize = 258
payload = bytes([
    0x10, 0x20, 0x30, 0xFF,
    0x40, 0x50, 0x60, 0xFF,
    0x70, 0x80, 0x90, 0xFF,
    0xA0, 0xB0, 0xC0, 0xFF,
])

with gzip.open(cache_path, "wb") as fp:
    fp.write(struct.pack("<i", TXCACHE_FORMAT_VERSION))
    fp.write(struct.pack("<i", 0))
    fp.write(struct.pack("<Q", checksum64))
    fp.write(struct.pack("<IIIHHB", 2, 2, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 1))
    fp.write(struct.pack("<H", formatsize))
    fp.write(struct.pack("<I", len(payload)))
    fp.write(payload)

cache_sha = __import__("hashlib").sha256(cache_path.read_bytes()).hexdigest()
rom_path = cache_path.with_name("synthetic-rom.z64")
core_path = cache_path.with_name("synthetic-core.so")
base_config_path = cache_path.with_name("retroarch.cfg")
append_config_path = cache_path.with_name("retroarch.append.cfg")
core_options_path = cache_path.with_name("core-options.opt")
rom_path.write_bytes(b"synthetic-rom")
core_path.write_bytes(b"synthetic-core")
base_config_path.write_text('video_driver = "vulkan"\n')
append_config_path.write_text(f'core_options_path = "{core_options_path}"\n')
core_options_path.write_text('parallel-n64-parallel-rdp-hirestex = "enabled"\n')
rom_sha = __import__("hashlib").sha256(rom_path.read_bytes()).hexdigest()
core_sha = __import__("hashlib").sha256(core_path.read_bytes()).hexdigest()
base_config_sha = __import__("hashlib").sha256(base_config_path.read_bytes()).hexdigest()
append_config_sha = __import__("hashlib").sha256(append_config_path.read_bytes()).hexdigest()
core_options_sha = __import__("hashlib").sha256(core_options_path.read_bytes()).hexdigest()
commands = ["WAIT_COMMAND_READY 120", "SCREENSHOT", "QUIT"]
commands_text = "\n".join(commands) + "\n"
command_signature = __import__("hashlib").sha256(commands_text.encode()).hexdigest()

evidence = {
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
            "compat": 0,
        },
    },
    "ci_palette_probe": {
        "families": [
            {
                "low32": f"{texture_crc:08x}",
                "fs": str(formatsize),
                "mode": "loadtile",
                "wh": "2x2",
                "pcrc": f"{palette_crc:08x}",
                "active_pool": "compatibility"
            }
        ],
        "usages": [],
        "emulated_tmem": []
    },
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
                "upload_low32s": [
                    {"value": f"{texture_crc:08x}"}
                ],
                "upload_pcrcs": [
                    {"value": f"{palette_crc:08x}"}
                ]
            }
        ]
    }
}
evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
bundle_dir = evidence_path.parent.parent
(bundle_dir / "bundle.json").write_text(json.dumps({
    "fixture_id": "synthetic-bundle-input",
    "inputs": {
        "rom_path": str(rom_path),
        "rom_sha256": rom_sha,
        "hires_pack_path": str(cache_path),
        "hires_pack_sha256": cache_sha,
    },
    "status": {"runtime_executed": True},
}, indent=2) + "\n")
(bundle_dir / "traces" / "fixture-verification.json").write_text(json.dumps({
    "fixture_id": "synthetic-bundle-input",
    "passed": True,
    "failures": [],
}, indent=2) + "\n")
(bundle_dir / "logs").mkdir(exist_ok=True)
(bundle_dir / "config.env").write_text("synthetic=1\n")
(bundle_dir / "retroarch.expected.commands.log").write_text(commands_text)
(bundle_dir / "retroarch.planned.commands.log").write_text(commands_text)
(bundle_dir / "retroarch.executed.commands.log").write_text(commands_text)
(bundle_dir / "logs" / "retroarch.commands.log").write_text(commands_text)
(bundle_dir / "logs" / "retroarch.log").write_text(
    "[parallel-rdp-hires] replacement provenance outcome=hit key=0000000000000000 pcrc=00000000 fmt=2 siz=1 tmem=0x0 line=2 width=2 height=2 formatsize=4 cycle=1cycle\n"
)
(bundle_dir / "retroarch.session.env").write_text(
    f"MODE=on\nROM_PATH={rom_path}\nROM_SHA256={rom_sha}\nCORE_PATH={core_path}\n"
    f"CORE_SHA256={core_sha}\nBASE_CONFIG={base_config_path}\nBASE_CONFIG_SHA256={base_config_sha}\n"
    f"APPEND_CONFIG={append_config_path}\nAPPEND_CONFIG_SHA256={append_config_sha}\n"
    f"CORE_OPTIONS_FILE={core_options_path}\nCORE_OPTIONS_FILE_SHA256={core_options_sha}\n"
    f"HIRES_CACHE_PATH={cache_path}\nHIRES_CACHE_SHA256={cache_sha}\n"
    f"COMMAND_SIGNATURE={command_signature}\n"
)
(bundle_dir / "retroarch.run.env").write_text(
    "RUNTIME_EXECUTED=1\nRETROARCH_EXIT_STATUS=0\nFORCED_TERMINATION=0\n"
)
raw_evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
unproven = dict(evidence)
unproven.pop("available", None)
unproven.pop("cache_loaded", None)
unproven.pop("summary", None)
unproven_raw_evidence_path.write_text(json.dumps(unproven, indent=2) + "\n")

summary = {
    "cache_path": "dummy.phrb",
    "passed": True,
    "steps": [
        {
            "step_frames": 960,
            "passed": True,
            "fixture_id": "synthetic-bundle-input",
            "off_bundle": "../bundle",
            "on_bundle": "../bundle"
        }
    ]
}
summary_json.write_text(json.dumps(summary, indent=2) + "\n")
summary_md.write_text("# validation summary\n")

summary_cwd = {
    "cache_path": "dummy.phrb",
    "passed": True,
    "steps": [
        {
            "step_frames": 960,
            "passed": True,
            "fixture_id": "synthetic-bundle-input",
            "off_bundle": "bundle",
            "on_bundle": "bundle"
        }
    ]
}
summary_cwd_json.write_text(json.dumps(summary_cwd, indent=2) + "\n")
summary_cwd_md.write_text("# validation summary cwd\n")
(cache_path.parent / "validation-summary.json").write_text(json.dumps(summary_cwd, indent=2) + "\n")
(cache_path.parent / "validation-summary.md").write_text("# validation summary parent\n")
PY

python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$TRACE_DIR/hires-evidence.json" \
  --stdout-format json \
  --output-dir "$OUT_EVIDENCE" \
  > "$TMP_DIR/evidence.json"

cp "$BUNDLE_DIR/bundle.json" "$TMP_DIR/direct-bundle.good.json"
python3 - "$BUNDLE_DIR/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data.pop("fixture_id", None)
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$TRACE_DIR/hires-evidence.json" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-direct-missing-fixture" \
  > "$TMP_DIR/direct-missing-fixture.json" \
  2> "$TMP_DIR/direct-missing-fixture.stderr"; then
  echo "expected direct bundle input without fixture_id to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "fixture_id mismatch" "$TMP_DIR/direct-missing-fixture.stderr"; then
  echo "expected direct bundle fixture_id mismatch rejection" >&2
  cat "$TMP_DIR/direct-missing-fixture.stderr" >&2
  exit 1
fi
cp "$TMP_DIR/direct-bundle.good.json" "$BUNDLE_DIR/bundle.json"

cp "$TRACE_DIR/fixture-verification.json" "$TMP_DIR/direct-verification.good.json"
python3 - "$TRACE_DIR/fixture-verification.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["fixture_id"] = "wrong-fixture"
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$TRACE_DIR/hires-evidence.json" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-direct-wrong-verification" \
  > "$TMP_DIR/direct-wrong-verification.json" \
  2> "$TMP_DIR/direct-wrong-verification.stderr"; then
  echo "expected direct bundle input with mismatched verification fixture_id to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "fixture_id mismatch" "$TMP_DIR/direct-wrong-verification.stderr"; then
  echo "expected direct bundle verification fixture_id mismatch rejection" >&2
  cat "$TMP_DIR/direct-wrong-verification.stderr" >&2
  exit 1
fi
cp "$TMP_DIR/direct-verification.good.json" "$TRACE_DIR/fixture-verification.json"

cp "$BUNDLE_DIR/bundle.json" "$TMP_DIR/cache-bound-bundle.good.json"
cp "$TRACE_DIR/hires-evidence.json" "$TMP_DIR/cache-bound-evidence.good.json"
cp "$BUNDLE_DIR/retroarch.session.env" "$TMP_DIR/cache-bound-session.good.env"
OTHER_CACHE="$TMP_DIR/other.htc"
printf 'other-cache' > "$OTHER_CACHE"
python3 - "$BUNDLE_DIR/bundle.json" "$TRACE_DIR/hires-evidence.json" "$BUNDLE_DIR/retroarch.session.env" "$OTHER_CACHE" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

bundle_path = Path(sys.argv[1])
evidence_path = Path(sys.argv[2])
session_path = Path(sys.argv[3])
other_cache = Path(sys.argv[4])
other_sha = hashlib.sha256(other_cache.read_bytes()).hexdigest()

bundle = json.loads(bundle_path.read_text())
bundle["inputs"]["hires_pack_path"] = str(other_cache)
bundle["inputs"]["hires_pack_sha256"] = other_sha
bundle_path.write_text(json.dumps(bundle, indent=2) + "\n")

evidence = json.loads(evidence_path.read_text())
evidence["cache_path"] = str(other_cache)
evidence["cache_sha256"] = other_sha
evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")

lines = []
for line in session_path.read_text().splitlines():
    if line.startswith("HIRES_CACHE_PATH="):
        lines.append(f"HIRES_CACHE_PATH={other_cache}")
    elif line.startswith("HIRES_CACHE_SHA256="):
        lines.append(f"HIRES_CACHE_SHA256={other_sha}")
    else:
        lines.append(line)
session_path.write_text("\n".join(lines) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$TRACE_DIR/hires-evidence.json" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-wrong-cache-binding" \
  > "$TMP_DIR/wrong-cache-binding.json" \
  2> "$TMP_DIR/wrong-cache-binding.stderr"; then
  echo "expected direct bundle from a different cache artifact to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "cache path does not match requested --cache" "$TMP_DIR/wrong-cache-binding.stderr"; then
  echo "expected wrong-cache direct bundle rejection" >&2
  cat "$TMP_DIR/wrong-cache-binding.stderr" >&2
  exit 1
fi
cp "$TMP_DIR/cache-bound-bundle.good.json" "$BUNDLE_DIR/bundle.json"
cp "$TMP_DIR/cache-bound-evidence.good.json" "$TRACE_DIR/hires-evidence.json"
cp "$TMP_DIR/cache-bound-session.good.env" "$BUNDLE_DIR/retroarch.session.env"

if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$RAW_EVIDENCE" \
  --stdout-format json \
  --output-dir "$OUT_RAW_EVIDENCE" \
  > "$TMP_DIR/raw-evidence.json" \
  2> "$TMP_DIR/raw-evidence.stderr"; then
  echo "expected raw hires-evidence bundle input without runtime provenance to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "Direct enrichment bundle inputs require a passed validation-summary.json" "$TMP_DIR/raw-evidence.stderr"; then
  echo "expected raw bundle rejection to mention validation-summary requirement" >&2
  cat "$TMP_DIR/raw-evidence.stderr" >&2
  exit 1
fi

if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$RAW_EVIDENCE" \
  --stdout-format json \
  --output-dir "$OUT_RAW_CONTEXT" \
  > "$TMP_DIR/raw-context.json" \
  2> "$TMP_DIR/raw-context.stderr"; then
  echo "expected direct hires-evidence context to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "Direct enrichment bundle inputs require a passed validation-summary.json" "$TMP_DIR/raw-context.stderr"; then
  echo "expected direct context rejection to mention validation-summary requirement" >&2
  cat "$TMP_DIR/raw-context.stderr" >&2
  exit 1
fi

if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$UNPROVEN_RAW_EVIDENCE" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-unproven-raw-context" \
  > "$TMP_DIR/unproven-raw-context.json" \
  2> "$TMP_DIR/unproven-raw-context.stderr"; then
  echo "expected unproven direct hires-evidence context to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "Direct enrichment bundle inputs require a passed validation-summary.json" "$TMP_DIR/unproven-raw-context.stderr"; then
  echo "expected unproven context rejection to mention validation-summary requirement" >&2
  cat "$TMP_DIR/unproven-raw-context.stderr" >&2
  exit 1
fi

python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$OUT_SUMMARY_JSON" \
  > "$TMP_DIR/summary.json"

python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$SUMMARY_MD" \
  --stdout-format json \
  --output-dir "$OUT_SUMMARY_MD" \
  > "$TMP_DIR/summary-md.json"

cp "$SUMMARY_JSON" "$TMP_DIR/summary.good.json"
python3 - "$SUMMARY_JSON" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data.pop("passed", None)
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-summary-missing-passed" \
  > "$TMP_DIR/summary-missing-passed.json" \
  2> "$TMP_DIR/summary-missing-passed.stderr"; then
  echo "expected validation summary without top-level pass state to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "Step validation summary is not marked passed" "$TMP_DIR/summary-missing-passed.stderr"; then
  echo "expected missing top-level pass rejection" >&2
  cat "$TMP_DIR/summary-missing-passed.stderr" >&2
  exit 1
fi
cp "$TMP_DIR/summary.good.json" "$SUMMARY_JSON"

python3 - "$SUMMARY_JSON" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["steps"][0]["passed"] = False
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-summary-failed-step" \
  > "$TMP_DIR/summary-failed-step.json" \
  2> "$TMP_DIR/summary-failed-step.stderr"; then
  echo "expected validation summary failed selected step to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "Validation summary selected step is not marked passed" "$TMP_DIR/summary-failed-step.stderr"; then
  echo "expected failed selected step rejection" >&2
  cat "$TMP_DIR/summary-failed-step.stderr" >&2
  exit 1
fi
cp "$TMP_DIR/summary.good.json" "$SUMMARY_JSON"

python3 - "$SUMMARY_JSON" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["steps"][0].pop("fixture_id", None)
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-summary-missing-fixture-id" \
  > "$TMP_DIR/summary-missing-fixture-id.json" \
  2> "$TMP_DIR/summary-missing-fixture-id.stderr"; then
  echo "expected validation summary selected step without fixture_id to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "Validation summary selected step has no fixture_id" "$TMP_DIR/summary-missing-fixture-id.stderr"; then
  echo "expected missing selected-step fixture_id rejection" >&2
  cat "$TMP_DIR/summary-missing-fixture-id.stderr" >&2
  exit 1
fi
cp "$TMP_DIR/summary.good.json" "$SUMMARY_JSON"

pushd "$TMP_DIR" >/dev/null
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$SUMMARY_CWD_JSON" \
  --stdout-format json \
  --output-dir "$OUT_SUMMARY_CWD_JSON" \
  > "$TMP_DIR/summary-cwd.json" \
  2> "$TMP_DIR/summary-cwd.stderr"; then
  echo "expected cwd-relative validation-summary bundle reference to be rejected" >&2
  exit 1
fi

if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --bundle "$SUMMARY_CWD_MD" \
  --stdout-format json \
  --output-dir "$OUT_SUMMARY_CWD_MD" \
  > "$TMP_DIR/summary-cwd-md.json" \
  2> "$TMP_DIR/summary-cwd-md.stderr"; then
  echo "expected cwd-relative markdown validation-summary bundle reference to be rejected" >&2
  exit 1
fi
popd >/dev/null
if ! rg -q --fixed-strings -- "Resolved validation bundle has no hires-evidence.json" "$TMP_DIR/summary-cwd.stderr"; then
  echo "expected cwd-relative JSON rejection to mention unresolved validation bundle" >&2
  cat "$TMP_DIR/summary-cwd.stderr" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "Resolved validation bundle has no hires-evidence.json" "$TMP_DIR/summary-cwd-md.stderr"; then
  echo "expected cwd-relative markdown rejection to mention unresolved validation bundle" >&2
  cat "$TMP_DIR/summary-cwd-md.stderr" >&2
  exit 1
fi

python3 - "$TMP_DIR" "$BUNDLE_DIR" "$TRACE_DIR/hires-evidence.json" "$RAW_EVIDENCE" "$SUMMARY_JSON" "$SUMMARY_MD" <<'PY'
import json
import sys
from pathlib import Path

tmp_dir = Path(sys.argv[1])
bundle_dir = Path(sys.argv[2])
hires_path = Path(sys.argv[3])
raw_hires_path = Path(sys.argv[4])
summary_json = Path(sys.argv[5])
summary_md = Path(sys.argv[6])

evidence = json.loads((tmp_dir / "evidence.json").read_text())
summary = json.loads((tmp_dir / "summary.json").read_text())
summary_md_report = json.loads((tmp_dir / "summary-md.json").read_text())

for report in (evidence, summary, summary_md_report):
    if report["binding_count"] != 1 or report["unresolved_count"] != 0:
        raise SystemExit(f"unexpected binding state: {report!r}")
for report in (evidence, summary, summary_md_report):
    if report["resolved_bundle_path"] != str(bundle_dir):
        raise SystemExit(f"unexpected resolved bundle path: {report['resolved_bundle_path']!r}")

if evidence["bundle_resolution"]["input_kind"] != "hires-evidence-json":
    raise SystemExit(f"unexpected evidence bundle resolution: {evidence['bundle_resolution']!r}")
if evidence["bundle_resolution"]["resolved_hires_path"] != str(hires_path):
    raise SystemExit(f"unexpected evidence hires path: {evidence['bundle_resolution']!r}")
if summary["bundle_resolution"]["input_kind"] != "validation-summary":
    raise SystemExit(f"unexpected summary bundle resolution: {summary['bundle_resolution']!r}")
if summary["bundle_resolution"]["selection_reason"] != "validation-summary-single-step":
    raise SystemExit(f"unexpected summary selection reason: {summary['bundle_resolution']!r}")
if summary["bundle_resolution"]["selected_step_frames"] != 960:
    raise SystemExit(f"unexpected summary step selection: {summary['bundle_resolution']!r}")
if summary["bundle_path"] != str(summary_json):
    raise SystemExit(f"unexpected raw bundle path: {summary['bundle_path']!r}")
if summary["bundle_resolution"].get("bundle_reference_mode") != "summary-relative":
    raise SystemExit(f"unexpected summary bundle reference mode: {summary['bundle_resolution']!r}")

if summary_md_report["bundle_resolution"]["selection_reason"] != "validation-summary-single-step":
    raise SystemExit(f"unexpected markdown summary selection: {summary_md_report['bundle_resolution']!r}")
if summary_md_report["bundle_path"] != str(summary_md):
    raise SystemExit(f"unexpected markdown raw bundle path: {summary_md_report['bundle_path']!r}")
if summary_md_report["bundle_resolution"].get("bundle_reference_mode") != "summary-relative":
    raise SystemExit(f"unexpected markdown summary bundle reference mode: {summary_md_report['bundle_resolution']!r}")
PY

echo "emu_hts2phrb_bundle_input_resolution: PASS"

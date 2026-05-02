#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
TMP_DIR="$(mktemp -d /tmp/parallel-n64-hts2phrb-context-dir-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

CACHE_PATH="$TMP_DIR/sample.htc"
CONTEXT_TREE="$TMP_DIR/validation-tree"
EXTERNAL_CONTEXT="$TMP_DIR/external-context"
STEP_CONTEXT_TREE="$TMP_DIR/step-mismatch-tree"
OUT_DIR_SCAN="$TMP_DIR/out-dir-scan"
OUT_BUNDLE_EXPLICIT="$TMP_DIR/out-bundle-explicit"

mkdir -p \
  "$CONTEXT_TREE/run-a/bundle-a/traces" \
  "$CONTEXT_TREE/run-b/bundle-b/traces" \
  "$CONTEXT_TREE/run-c" \
  "$CONTEXT_TREE/run-d/direct-bundle/traces" \
  "$CONTEXT_TREE/run-e/raw" \
  "$CONTEXT_TREE/run-f/direct-compat/traces" \
  "$CONTEXT_TREE/run-g/bundle-g/traces" \
  "$CONTEXT_TREE/run-h/step-bundle/traces" \
  "$CONTEXT_TREE/run-i/mixed-valid/traces" \
  "$CONTEXT_TREE/run-i/mixed-invalid/traces" \
  "$CONTEXT_TREE/run-j/raw-sampled/traces" \
  "$CONTEXT_TREE/run-k/mixed-invalid/traces" \
  "$EXTERNAL_CONTEXT/mixed-external-valid/traces" \
  "$STEP_CONTEXT_TREE/run-l/step-mismatch-bundle/traces" \
  "$STEP_CONTEXT_TREE/run-m/invalid-selected/traces" \
  "$STEP_CONTEXT_TREE/run-m/valid-unselected/traces" \
  "$STEP_CONTEXT_TREE/run-n" \
  "$STEP_CONTEXT_TREE/run-p" \
  "$STEP_CONTEXT_TREE/run-o/failed-fixture/traces"

python3 - "$CACHE_PATH" \
  "$CONTEXT_TREE/run-a/bundle-a/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-b/bundle-b/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-a/validation-summary.json" \
  "$CONTEXT_TREE/run-b/validation-summary.json" \
  "$CONTEXT_TREE/run-c/validation-summary.json" \
  "$CONTEXT_TREE/run-d/direct-bundle/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-e/raw/hires-evidence.json" \
  "$CONTEXT_TREE/run-f/direct-compat/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-g/bundle-g/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-g/validation-summary.json" \
  "$CONTEXT_TREE/run-h/step-bundle/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-h/validation-summary.json" \
  "$CONTEXT_TREE/run-i/mixed-valid/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-i/mixed-invalid/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-i/validation-summary.json" \
  "$CONTEXT_TREE/run-j/raw-sampled/traces/hires-evidence.json" \
  "$EXTERNAL_CONTEXT/mixed-external-valid/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-k/mixed-invalid/traces/hires-evidence.json" \
  "$CONTEXT_TREE/run-k/validation-summary.json" \
  "$STEP_CONTEXT_TREE/run-l/step-mismatch-bundle/traces/hires-evidence.json" \
  "$STEP_CONTEXT_TREE/run-l/validation-summary.json" \
  "$STEP_CONTEXT_TREE/run-m/invalid-selected/traces/hires-evidence.json" \
  "$STEP_CONTEXT_TREE/run-m/valid-unselected/traces/hires-evidence.json" \
  "$STEP_CONTEXT_TREE/run-m/validation-summary.json" \
  "$STEP_CONTEXT_TREE/run-n/validation-summary.json" \
  "$STEP_CONTEXT_TREE/run-o/failed-fixture/traces/hires-evidence.json" \
  "$STEP_CONTEXT_TREE/run-o/validation-summary.json" \
  "$STEP_CONTEXT_TREE/run-p/validation-summary.json" \
  <<'PY'
import gzip
import hashlib
import json
import struct
import sys
from pathlib import Path

cache_path = Path(sys.argv[1])
evidence_a = Path(sys.argv[2])
evidence_b = Path(sys.argv[3])
summary_a = Path(sys.argv[4])
summary_b = Path(sys.argv[5])
summary_c_bad = Path(sys.argv[6])
evidence_d_direct = Path(sys.argv[7])
evidence_e_raw = Path(sys.argv[8])
evidence_f_compat = Path(sys.argv[9])
evidence_g_off = Path(sys.argv[10])
summary_g_off = Path(sys.argv[11])
evidence_h_step_off = Path(sys.argv[12])
summary_h_step_off = Path(sys.argv[13])
evidence_i_mixed_valid = Path(sys.argv[14])
evidence_i_mixed_invalid = Path(sys.argv[15])
summary_i_mixed = Path(sys.argv[16])
evidence_j_raw_sampled = Path(sys.argv[17])
evidence_k_external_valid = Path(sys.argv[18])
evidence_k_mixed_invalid = Path(sys.argv[19])
summary_k_mixed_external = Path(sys.argv[20])
evidence_l_step_mismatch = Path(sys.argv[21])
summary_l_step_mismatch = Path(sys.argv[22])
evidence_m_invalid_selected = Path(sys.argv[23])
evidence_m_valid_unselected = Path(sys.argv[24])
summary_m_unselected_leak = Path(sys.argv[25])
summary_n_malformed = Path(sys.argv[26])
evidence_o_failed_fixture = Path(sys.argv[27])
summary_o_failed_fixture = Path(sys.argv[28])
summary_p_malformed_step = Path(sys.argv[29])

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


def write_bundle_provenance(evidence_path, fixture_id, passed=True):
    bundle_dir = evidence_path.parent.parent
    (bundle_dir / "bundle.json").write_text(json.dumps({
        "fixture_id": fixture_id,
        "inputs": {
            "rom_path": str(rom_path),
            "rom_sha256": rom_sha,
            "hires_pack_path": str(cache_path),
            "hires_pack_sha256": cache_sha,
        },
        "status": {"runtime_executed": True},
    }, indent=2) + "\n")
    (bundle_dir / "traces" / "fixture-verification.json").write_text(json.dumps({
        "fixture_id": fixture_id,
        "passed": passed,
        "failures": [] if passed else ["synthetic failed fixture"],
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


def write_evidence(path, texture_crc, palette_crc, formatsize):
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


def write_invalid_evidence(path, *, provider="on", entry_class=None, native_sampled_entry_count=1):
    summary = {
        "provider": provider,
        "source_mode": "phrb-only",
        "entry_count": 1,
        "native_sampled_entry_count": native_sampled_entry_count,
        "source_counts": {"phrb": 1},
    }
    if entry_class:
        summary["entry_class"] = entry_class
    path.write_text(json.dumps({
        "available": True,
        "cache_loaded": True,
        "cache_path": str(cache_path),
        "cache_sha256": cache_sha,
        "summary": summary,
        "sampled_object_probe": {"top_groups": [{
            "fields": {
                "draw_class": "texrect",
                "cycle": "1cycle",
                "fmt": "2",
                "siz": "1",
                "off": "0",
                "stride": "2",
                "wh": "2x2",
                "fs": "258",
                "sampled_low32": "11111111",
                "sampled_entry_pcrc": "aaaabbbb",
                "sampled_sparse_pcrc": "aaaabbbb",
                "sampled_entry_count": "1",
                "sampled_used_count": "1"
            },
            "upload_low32s": [{"value": "11111111"}],
            "upload_pcrcs": [{"value": "aaaabbbb"}]
        }]}
    }, indent=2) + "\n")


def write_raw_sampled_evidence(path, texture_crc, palette_crc, formatsize):
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


write_evidence(evidence_a, 0x11111111, 0xAAAABBBB, 258)
write_evidence(evidence_b, 0x22222222, 0xCCCCDDDD, 514)
write_evidence(evidence_d_direct, 0x11111111, 0xAAAABBBB, 258)
write_evidence(evidence_e_raw, 0x22222222, 0xCCCCDDDD, 514)
write_invalid_evidence(evidence_f_compat, entry_class="compat-only", native_sampled_entry_count=0)
write_invalid_evidence(evidence_g_off, provider="off")
write_invalid_evidence(evidence_h_step_off, provider="off")
write_evidence(evidence_i_mixed_valid, 0x22222222, 0xCCCCDDDD, 514)
write_invalid_evidence(evidence_i_mixed_invalid, provider="off")
write_raw_sampled_evidence(evidence_j_raw_sampled, 0x11111111, 0xAAAABBBB, 258)
write_evidence(evidence_k_external_valid, 0x22222222, 0xCCCCDDDD, 514)
write_invalid_evidence(evidence_k_mixed_invalid, provider="off")
write_evidence(evidence_l_step_mismatch, 0x11111111, 0xAAAABBBB, 258)
write_invalid_evidence(evidence_m_invalid_selected, provider="off")
write_evidence(evidence_m_valid_unselected, 0x22222222, 0xCCCCDDDD, 514)
write_evidence(evidence_o_failed_fixture, 0x11111111, 0xAAAABBBB, 258)
write_bundle_provenance(evidence_a, "fixture-a")
write_bundle_provenance(evidence_b, "fixture-b")
write_bundle_provenance(evidence_g_off, "fixture-g")
write_bundle_provenance(evidence_i_mixed_valid, "fixture-i-valid")
write_bundle_provenance(evidence_i_mixed_invalid, "fixture-i-invalid")
write_bundle_provenance(evidence_k_external_valid, "fixture-k-valid-external")
write_bundle_provenance(evidence_k_mixed_invalid, "fixture-k-invalid")
write_bundle_provenance(evidence_o_failed_fixture, "failed-fixture", passed=False)

# Summary A: points to bundle-a (sibling-relative)
summary_a.write_text(json.dumps({
    "summary_title": "run-a",
    "all_passed": True,
    "fixtures": [{
        "label": "fixture-a",
        "fixture_id": "fixture-a",
        "bundle_dir": "bundle-a",
        "passed": True,
    }]
}, indent=2) + "\n")

# Summary B: points to bundle-b (sibling-relative)
summary_b.write_text(json.dumps({
    "summary_title": "run-b",
    "all_passed": True,
    "fixtures": [{
        "label": "fixture-b",
        "fixture_id": "fixture-b",
        "bundle_dir": "bundle-b",
        "passed": True,
    }]
}, indent=2) + "\n")

# Summary C: intentionally broken (no bundle_dir) — should be skipped
summary_c_bad.write_text(json.dumps({
    "summary_title": "run-c-broken",
    "all_passed": True,
    "fixtures": [{
        "label": "fixture-c",
        "fixture_id": "fixture-c",
        "passed": True,
    }]
}, indent=2) + "\n")

# Summary G: resolves to invalid provider=off evidence — should be skipped and not used for enrichment
summary_g_off.write_text(json.dumps({
    "summary_title": "run-g-off",
    "all_passed": True,
    "fixtures": [{
        "label": "fixture-g",
        "fixture_id": "fixture-g",
        "bundle_dir": "bundle-g",
        "passed": True,
    }]
}, indent=2) + "\n")

# Summary H: step summary resolves to invalid provider=off evidence — should be skipped once
summary_h_step_off.write_text(json.dumps({
    "cache_path": "dummy.phrb",
    "steps": [{
        "step_frames": 960,
        "off_bundle": "step-bundle",
        "on_bundle": "step-bundle",
    }]
}, indent=2) + "\n")

# Summary I: one valid fixture plus one provider-off fixture. The rejected summary
# must not seed partial enrichment and must hide referenced raw traces from fallback scans.
summary_i_mixed.write_text(json.dumps({
    "summary_title": "run-i-mixed-valid-invalid",
    "all_passed": True,
    "fixtures": [
        {
            "label": "fixture-i-valid",
            "fixture_id": "fixture-i-valid",
            "bundle_dir": "mixed-valid",
            "passed": True,
        },
        {
            "label": "fixture-i-invalid",
            "fixture_id": "fixture-i-invalid",
            "bundle_dir": "mixed-invalid",
            "passed": True,
        },
    ]
}, indent=2) + "\n")

# Summary K: one valid external fixture plus one invalid in-tree fixture. Partial
# summary salvage is forbidden; the mixed summary must not seed enrichment.
summary_k_mixed_external.write_text(json.dumps({
    "summary_title": "run-k-mixed-external-valid-invalid",
    "all_passed": True,
    "fixtures": [
        {
            "label": "fixture-k-valid-external",
            "fixture_id": "fixture-k-valid-external",
            "bundle_dir": str(evidence_k_external_valid.parent.parent),
            "passed": True,
        },
        {
            "label": "fixture-k-invalid",
            "fixture_id": "fixture-k-invalid",
            "bundle_dir": "mixed-invalid",
            "passed": True,
        },
    ]
}, indent=2) + "\n")

# Summary L: requested --bundle-step must not leak this referenced trace through
# the direct fallback scan when the summary has no matching step.
summary_l_step_mismatch.write_text(json.dumps({
    "cache_path": "dummy.phrb",
    "steps": [
        {
            "step_frames": 960,
            "off_bundle": "step-mismatch-bundle",
            "on_bundle": "step-mismatch-bundle",
        },
        {
            "step_frames": 1920,
            "off_bundle": "step-mismatch-bundle",
            "on_bundle": "step-mismatch-bundle",
        },
    ]
}, indent=2) + "\n")

# Summary M: selected first step is invalid; a valid unselected sibling must not
# leak through the direct fallback scan.
summary_m_unselected_leak.write_text(json.dumps({
    "cache_path": "dummy.phrb",
    "steps": [
        {
            "step_frames": 960,
            "off_bundle": "invalid-selected",
            "on_bundle": "invalid-selected",
        },
        {
            "step_frames": 1920,
            "off_bundle": "valid-unselected",
            "on_bundle": "valid-unselected",
        },
    ]
}, indent=2) + "\n")

# Summary N: malformed fixture entries should be skipped, not crash context-dir.
summary_n_malformed.write_text(json.dumps({
    "summary_title": "malformed-fixtures",
    "all_passed": True,
    "fixtures": ["not-an-object"],
}, indent=2) + "\n")

# Summary O: failed fixture summaries must not seed enrichment, even when the
# referenced evidence itself is syntactically valid.
summary_o_failed_fixture.write_text(json.dumps({
    "summary_title": "failed-fixture",
    "all_passed": False,
    "fixtures": [{
        "label": "failed-fixture",
        "fixture_id": "failed-fixture",
        "bundle_dir": "failed-fixture",
        "passed": False,
    }],
}, indent=2) + "\n")

# Summary P: malformed step entries should be skipped, not crash context-dir.
summary_p_malformed_step.write_text(json.dumps({
    "cache_path": "dummy.phrb",
    "steps": ["not-an-object"],
}, indent=2) + "\n")
PY

# Run with --context-dir. Context enrichment must come from fully passed
# validation summaries only; mixed or malformed summaries must not seed partial
# enrichment, and direct/raw hires-evidence context must be rejected even when
# raw scanning is explicitly enabled.
python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-dir "$CONTEXT_TREE" \
  --context-dir-include-raw-hires \
  --stdout-format json \
  --output-dir "$OUT_DIR_SCAN" \
  > "$TMP_DIR/report-dir-scan.json" \
  2> "$TMP_DIR/stderr-dir-scan.txt"

# Run with explicit --context-bundle for each good summary (baseline)
python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$CONTEXT_TREE/run-a/validation-summary.json" \
  --context-bundle "$CONTEXT_TREE/run-b/validation-summary.json" \
  --stdout-format json \
  --output-dir "$OUT_BUNDLE_EXPLICIT" \
  > "$TMP_DIR/report-bundle-explicit.json"

python3 - \
  "$TMP_DIR/report-dir-scan.json" \
  "$TMP_DIR/report-bundle-explicit.json" \
  "$TMP_DIR/stderr-dir-scan.txt" \
  <<'PY'
import json
import sys
from pathlib import Path

dir_scan = json.loads(Path(sys.argv[1]).read_text())
explicit = json.loads(Path(sys.argv[2]).read_text())
stderr = Path(sys.argv[3]).read_text()

if dir_scan["conversion_outcome"] != "promotable-runtime-package":
    raise SystemExit(f"unexpected dir-scan outcome: {dir_scan['conversion_outcome']!r}")

if explicit["conversion_outcome"] != "promotable-runtime-package":
    raise SystemExit(f"unexpected explicit outcome: {explicit['conversion_outcome']!r}")

if dir_scan.get("context_bundle_resolution_count") != 2:
    raise SystemExit(f"dir-scan: expected 2 context resolutions, got {dir_scan.get('context_bundle_resolution_count')}")
if explicit.get("context_bundle_resolution_count") != 2:
    raise SystemExit(f"explicit: expected 2 context resolutions, got {explicit.get('context_bundle_resolution_count')}")

# dir-scan should report context_bundle_input_count=1 (one --context-dir)
if dir_scan.get("context_bundle_input_count") != 1:
    raise SystemExit(
        f"dir-scan: expected input_count=1, got {dir_scan.get('context_bundle_input_count')}"
    )

# explicit should report context_bundle_input_count=2 (two --context-bundle inputs)
if explicit.get("context_bundle_input_count") != 2:
    raise SystemExit(
        f"explicit: expected input_count=2, got {explicit.get('context_bundle_input_count')}"
    )

# dir-scan should have context_dir_paths in report
if not dir_scan.get("context_dir_paths"):
    raise SystemExit("dir-scan: missing context_dir_paths in report")

# dir-scan should match the explicit A/B baseline exactly; rejected mixed
# summaries and raw traces must not add enrichment.
dir_sampled = dir_scan.get("package_manifest_runtime_ready_native_sampled_record_count")
exp_sampled = explicit.get("package_manifest_runtime_ready_native_sampled_record_count")
if dir_sampled != exp_sampled:
    raise SystemExit(
        f"native sampled regression: dir_scan={dir_sampled}, explicit={exp_sampled}"
    )

# Stderr should mention the skipped summary
if "context-dir: skipping" not in stderr:
    raise SystemExit(f"expected skip warning in stderr, got: {stderr!r}")
if "context-dir: discovered 2, skipped 8" not in stderr:
    raise SystemExit(f"expected discovery stats in stderr, got: {stderr!r}")
if "Direct enrichment bundle inputs require a passed validation-summary.json" not in stderr:
    raise SystemExit(f"expected direct evidence validation-summary rejection in stderr, got: {stderr!r}")
if "is not a hi-res-on enrichment source" not in stderr:
    raise SystemExit(f"expected provider-off summary evidence skip in stderr, got: {stderr!r}")

# Runtime overlay should be built for both
for label, report in [("dir-scan", dir_scan), ("explicit", explicit)]:
    if not report.get("runtime_overlay_built"):
        raise SystemExit(f"{label}: expected runtime overlay to be built")
PY

STEP_MISMATCH_OUT="$TMP_DIR/out-step-mismatch"
STEP_UNPASSED_OUT="$TMP_DIR/out-step-unpassed"
python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-dir "$STEP_CONTEXT_TREE/run-l" \
  --bundle-step 960 \
  --stdout-format json \
  --output-dir "$STEP_UNPASSED_OUT" \
  > "$TMP_DIR/report-step-unpassed.json" \
  2> "$TMP_DIR/stderr-step-unpassed.txt"

python3 - "$TMP_DIR/report-step-unpassed.json" "$TMP_DIR/stderr-step-unpassed.txt" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())
stderr = Path(sys.argv[2]).read_text()
if report.get("context_bundle_resolution_count") != 0:
    raise SystemExit(f"expected no context resolutions for unpassed step summary, got {report.get('context_bundle_resolution_count')}")
if "Step validation summary is not marked passed" not in stderr:
    raise SystemExit(f"expected unpassed step-summary skip reason in stderr, got: {stderr!r}")
if "context-dir: discovered 0, skipped 1" not in stderr:
    raise SystemExit(f"expected unpassed step-summary skip stats in stderr, got: {stderr!r}")
PY

python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-dir "$STEP_CONTEXT_TREE/run-l" \
  --context-dir-include-raw-hires \
  --bundle-step 777 \
  --stdout-format json \
  --output-dir "$STEP_MISMATCH_OUT" \
  > "$TMP_DIR/report-step-mismatch.json" \
  2> "$TMP_DIR/stderr-step-mismatch.txt"

python3 - "$TMP_DIR/report-step-mismatch.json" "$TMP_DIR/stderr-step-mismatch.txt" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())
stderr = Path(sys.argv[2]).read_text()
if report.get("context_bundle_resolution_count") != 0:
    raise SystemExit(f"expected no context resolutions for missing bundle step, got {report.get('context_bundle_resolution_count')}")
if "context-dir: discovered 0, skipped 1" not in stderr:
    raise SystemExit(f"expected step-mismatch skip stats in stderr, got: {stderr!r}")
PY

UNSELECTED_OUT="$TMP_DIR/out-unselected-step"
python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-dir "$STEP_CONTEXT_TREE/run-m" \
  --context-dir-include-raw-hires \
  --stdout-format json \
  --output-dir "$UNSELECTED_OUT" \
  > "$TMP_DIR/report-unselected-step.json" \
  2> "$TMP_DIR/stderr-unselected-step.txt"

python3 - "$TMP_DIR/report-unselected-step.json" "$TMP_DIR/stderr-unselected-step.txt" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())
stderr = Path(sys.argv[2]).read_text()
if report.get("context_bundle_resolution_count") != 0:
    raise SystemExit(f"expected no context resolutions from unselected step evidence, got {report.get('context_bundle_resolution_count')}")
if "context-dir: discovered 0, skipped 1" not in stderr:
    raise SystemExit(f"expected unselected-step skip stats in stderr, got: {stderr!r}")
PY

MALFORMED_OUT="$TMP_DIR/out-malformed-summary"
python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-dir "$STEP_CONTEXT_TREE/run-n" \
  --stdout-format json \
  --output-dir "$MALFORMED_OUT" \
  > "$TMP_DIR/report-malformed-summary.json" \
  2> "$TMP_DIR/stderr-malformed-summary.txt"

python3 - "$TMP_DIR/report-malformed-summary.json" "$TMP_DIR/stderr-malformed-summary.txt" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())
stderr = Path(sys.argv[2]).read_text()
if report.get("context_bundle_resolution_count") != 0:
    raise SystemExit(f"expected no context resolutions from malformed fixture summary, got {report.get('context_bundle_resolution_count')}")
if "context-dir: discovered 0, skipped 1" not in stderr:
    raise SystemExit(f"expected malformed-summary skip stats in stderr, got: {stderr!r}")
PY

FAILED_FIXTURE_OUT="$TMP_DIR/out-failed-fixture-summary"
python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-dir "$STEP_CONTEXT_TREE/run-o" \
  --stdout-format json \
  --output-dir "$FAILED_FIXTURE_OUT" \
  > "$TMP_DIR/report-failed-fixture-summary.json" \
  2> "$TMP_DIR/stderr-failed-fixture-summary.txt"

python3 - "$TMP_DIR/report-failed-fixture-summary.json" "$TMP_DIR/stderr-failed-fixture-summary.txt" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())
stderr = Path(sys.argv[2]).read_text()
if report.get("context_bundle_resolution_count") != 0:
    raise SystemExit(f"expected no context resolutions from failed fixture summary, got {report.get('context_bundle_resolution_count')}")
if "context-dir: discovered 0, skipped 1" not in stderr:
    raise SystemExit(f"expected failed-fixture skip stats in stderr, got: {stderr!r}")
PY

MALFORMED_STEP_OUT="$TMP_DIR/out-malformed-step-summary"
python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-dir "$STEP_CONTEXT_TREE/run-p" \
  --stdout-format json \
  --output-dir "$MALFORMED_STEP_OUT" \
  > "$TMP_DIR/report-malformed-step-summary.json" \
  2> "$TMP_DIR/stderr-malformed-step-summary.txt"

python3 - "$TMP_DIR/report-malformed-step-summary.json" "$TMP_DIR/stderr-malformed-step-summary.txt" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())
stderr = Path(sys.argv[2]).read_text()
if report.get("context_bundle_resolution_count") != 0:
    raise SystemExit(f"expected no context resolutions from malformed step summary, got {report.get('context_bundle_resolution_count')}")
if "context-dir: discovered 0, skipped 1" not in stderr:
    raise SystemExit(f"expected malformed-step skip stats in stderr, got: {stderr!r}")
PY

echo "emu_hts2phrb_context_dir_discovery: PASS"

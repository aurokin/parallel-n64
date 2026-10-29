#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/emulator_behavior/support/hts2phrb_synthetic_bundle_provenance.sh"
TMP_DIR="$(mktemp -d /tmp/parallel-n64-hts2phrb-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

CACHE_PATH="$TMP_DIR/sample.htc"
BUNDLE_DIR="$TMP_DIR/bundle"
TRACE_DIR="$BUNDLE_DIR/traces"
OUTPUT_DIR="$TMP_DIR/out"

mkdir -p "$TRACE_DIR"

python3 - "$CACHE_PATH" "$TRACE_DIR/hires-evidence.json" <<'PY'
import gzip
import json
import struct
import sys
from pathlib import Path

cache_path = Path(sys.argv[1])
evidence_path = Path(sys.argv[2])

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

evidence = {
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
PY

hts2phrb_write_synthetic_runtime_provenance "$BUNDLE_DIR" "$CACHE_PATH" "synthetic-hts2phrb-smoke"

python3 - "$ROOT_DIR" \
  --cache "$CACHE_PATH" \
  --bundle "$BUNDLE_DIR" \
  --stdout-format json \
  --output-dir "$OUTPUT_DIR" <<'PY'
import sys
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(sys.argv[1]) / "tools"))
import hts2phrb

sys.argv = sys.argv[1:]
# Fresh conversion must derive the package summary once while still refreshing
# review, overlay and gate fields at all three synchronization points.
with patch.object(hts2phrb, "summarize_package_manifest", wraps=hts2phrb.summarize_package_manifest) as summarize, \
     patch.object(hts2phrb, "_load_runtime_manifest_stats", wraps=hts2phrb._load_runtime_manifest_stats) as load, \
     patch.object(hts2phrb, "synchronize_report_summary_fields", wraps=hts2phrb.synchronize_report_summary_fields) as synchronize:
    hts2phrb.main()
    assert summarize.call_count == 1, summarize.call_count
    load.assert_not_called()
    assert synchronize.call_count == 3, synchronize.call_count
PY

python3 - "$OUTPUT_DIR" <<'PY'
import json
import sys
from pathlib import Path

out_dir = Path(sys.argv[1])
report = json.loads((out_dir / "hts2phrb-report.json").read_text())
progress = json.loads((out_dir / "hts2phrb-progress.json").read_text())
loader_manifest = json.loads((out_dir / "loader-manifest.json").read_text())
runtime_loader_manifest = json.loads((out_dir / "runtime-loader-manifest.json").read_text())
package_manifest = json.loads((out_dir / "package" / "package-manifest.json").read_text())
bindings = json.loads((out_dir / "bindings.json").read_text())
plan = json.loads((out_dir / "migration-plan.json").read_text())
summary_text = (out_dir / "hts2phrb-summary.md").read_text()
family_inventory_json = out_dir / "hts2phrb-family-inventory.json"
family_inventory_md = out_dir / "hts2phrb-family-inventory.md"

if report["binding_count"] != 1:
    raise SystemExit(f"expected one binding, got {report['binding_count']}")
if report["runtime_overlay_mode"] != "auto" or not report["runtime_overlay_built"]:
    raise SystemExit(f"expected runtime overlay to build in bundle mode, got mode={report['runtime_overlay_mode']!r} built={report['runtime_overlay_built']!r}")
if report["runtime_overlay_reason"] != "runtime-context-available":
    raise SystemExit(f"unexpected runtime overlay reason: {report['runtime_overlay_reason']!r}")
if report["unresolved_count"] != 0:
    raise SystemExit(f"expected zero unresolved cases, got {report['unresolved_count']}")
if report["package_manifest_record_count"] != 1:
    raise SystemExit(f"expected one package record, got {report['package_manifest_record_count']}")
if report["package_manifest_runtime_ready_record_count"] != 1 or report["package_manifest_runtime_deferred_record_count"] != 0:
    raise SystemExit(f"unexpected package runtime-ready counts: {report!r}")
if report["package_manifest_runtime_ready_native_sampled_record_count"] != 1:
    raise SystemExit(
        f"expected one runtime-ready native-sampled record, got {report['package_manifest_runtime_ready_native_sampled_record_count']!r}"
    )
if report["package_manifest_runtime_ready_compat_record_count"] != 0:
    raise SystemExit(
        f"expected zero runtime-ready compat records, got {report['package_manifest_runtime_ready_compat_record_count']!r}"
    )
if report["package_manifest_runtime_deferred_native_sampled_record_count"] != 0 or report["package_manifest_runtime_deferred_compat_record_count"] != 0:
    raise SystemExit(
        "expected zero runtime-deferred native/compat records, "
        f"got native={report['package_manifest_runtime_deferred_native_sampled_record_count']!r} "
        f"compat={report['package_manifest_runtime_deferred_compat_record_count']!r}"
    )
if loader_manifest["runtime_ready_record_count"] != 1 or loader_manifest["runtime_deferred_record_count"] != 0:
    raise SystemExit(f"unexpected canonical loader runtime counts: {loader_manifest!r}")
if loader_manifest["runtime_ready_native_sampled_record_count"] != 1 or loader_manifest["runtime_ready_compat_record_count"] != 0:
    raise SystemExit(f"unexpected canonical loader runtime-ready class counts: {loader_manifest!r}")
if loader_manifest["runtime_ready_record_class"] != "native-sampled-only":
    raise SystemExit(f"unexpected canonical loader runtime-ready class: {loader_manifest['runtime_ready_record_class']!r}")
if runtime_loader_manifest["runtime_ready_record_count"] != 1 or runtime_loader_manifest["runtime_deferred_record_count"] != 0:
    raise SystemExit(f"unexpected runtime loader runtime counts: {runtime_loader_manifest!r}")
if runtime_loader_manifest["runtime_ready_native_sampled_record_count"] != 0 or runtime_loader_manifest["runtime_ready_compat_record_count"] != 1:
    raise SystemExit(f"unexpected runtime loader runtime-ready class counts: {runtime_loader_manifest!r}")
if runtime_loader_manifest["runtime_ready_record_class"] != "compat-only":
    raise SystemExit(f"unexpected runtime loader runtime-ready class: {runtime_loader_manifest['runtime_ready_record_class']!r}")
if package_manifest["runtime_ready_record_count"] != 1 or package_manifest["runtime_deferred_record_count"] != 0:
    raise SystemExit(f"unexpected package-manifest runtime counts: {package_manifest!r}")
if package_manifest["runtime_ready_native_sampled_record_count"] != 1 or package_manifest["runtime_ready_compat_record_count"] != 0:
    raise SystemExit(f"unexpected package-manifest runtime-ready class counts: {package_manifest!r}")
if package_manifest["runtime_ready_record_class"] != "native-sampled-only":
    raise SystemExit(f"unexpected package-manifest runtime-ready class: {package_manifest['runtime_ready_record_class']!r}")
if report["package_asset_storage"] != "legacy-blobs":
    raise SystemExit(f"unexpected package asset storage: {report['package_asset_storage']!r}")
if report["conversion_outcome"] != "promotable-runtime-package":
    raise SystemExit(f"unexpected conversion outcome: {report['conversion_outcome']!r}")
if report.get("minimum_outcome") is not None or report.get("require_promotable"):
    raise SystemExit(f"expected no explicit outcome gate metadata, got {report!r}")
if report["output_dir_was_default"]:
    raise SystemExit(f"did not expect default output dir for explicit output path: {report!r}")
if not report.get("gate_success", False) or report.get("gate_failures"):
    raise SystemExit(f"expected no gate failures without explicit gates, got {report!r}")
if report["warnings"]:
    raise SystemExit(f"expected no warnings, got {report['warnings']}")
if report["binding_policy_keys"] != ["sampled-fmt2-siz1-off0-stride2-wh2x2-fs258-low3211111111"]:
    raise SystemExit(f"unexpected binding policy keys: {report['binding_policy_keys']!r}")
if report["binding_sampled_low32s"] != ["11111111"]:
    raise SystemExit(f"unexpected binding sampled_low32s: {report['binding_sampled_low32s']!r}")
if report["unresolved_policy_keys"] or report["unresolved_sampled_low32s"]:
    raise SystemExit(
        f"expected no unresolved summaries, got keys={report['unresolved_policy_keys']!r} low32s={report['unresolved_sampled_low32s']!r}"
    )
if report["input_cache_bytes"] <= 0:
    raise SystemExit(f"expected positive input_cache_bytes, got {report['input_cache_bytes']}")
if report["binary_package_bytes"] <= 0:
    raise SystemExit(f"expected positive binary_package_bytes, got {report['binary_package_bytes']}")
if report["progress_path"] != str(out_dir / "hts2phrb-progress.json"):
    raise SystemExit(f"unexpected progress path: {report['progress_path']!r}")
if report.get("family_inventory_json_path") != str(family_inventory_json):
    raise SystemExit(f"unexpected family inventory json path: {report.get('family_inventory_json_path')!r}")
if report.get("family_inventory_markdown_path") != str(family_inventory_md):
    raise SystemExit(f"unexpected family inventory markdown path: {report.get('family_inventory_markdown_path')!r}")
if not family_inventory_json.exists() or not family_inventory_md.exists():
    raise SystemExit("missing family inventory artifacts")
if progress.get("status") != "complete":
    raise SystemExit(f"expected complete progress report, got {progress!r}")
if progress.get("binary_package", {}).get("record_count") != 1:
    raise SystemExit(f"unexpected progress binary summary: {progress!r}")
if progress.get("package_manifest_runtime_ready_native_sampled_record_count") != 1:
    raise SystemExit(f"unexpected progress native-sampled ready count: {progress!r}")
if progress.get("package_manifest_runtime_ready_compat_record_count") != 0:
    raise SystemExit(f"unexpected progress compat ready count: {progress!r}")
if progress.get("package_manifest_runtime_ready_record_class") != "native-sampled-only":
    raise SystemExit(f"unexpected progress runtime-ready class: {progress!r}")
if progress.get("package_manifest_runtime_ready_record_kind_counts") != {"canonical-sampled": 1}:
    raise SystemExit(f"unexpected progress runtime-ready kind counts: {progress!r}")
migration_summary = report.get("migration_plan_summary") or {}
if migration_summary.get("family_count") != 1:
    raise SystemExit(f"unexpected migration family count: {migration_summary!r}")
if migration_summary.get("tier_counts") != {"exact-authoritative": 1}:
    raise SystemExit(f"unexpected migration tier counts: {migration_summary!r}")
index_summary = report.get("imported_index_summary") or {}
if index_summary.get("exact_authority_count") != 1 or index_summary.get("canonical_record_count") != 1:
    raise SystemExit(f"unexpected imported index summary: {index_summary!r}")
family_states = report.get("requested_family_states") or {}
if family_states.get("runtime_state_counts") != {"runtime-bound": 1}:
    raise SystemExit(f"unexpected runtime state counts: {family_states!r}")
if family_states.get("import_state_counts") != {"exact-authority": 1}:
    raise SystemExit(f"unexpected import state counts: {family_states!r}")
if report.get("runtime_state_counts") != {"runtime-bound": 1}:
    raise SystemExit(f"unexpected top-level runtime state counts: {report.get('runtime_state_counts')!r}")
if report.get("import_state_counts") != {"exact-authority": 1}:
    raise SystemExit(f"unexpected top-level import state counts: {report.get('import_state_counts')!r}")
if float(report.get("total_runtime_ms") or 0.0) <= 0.0:
    raise SystemExit(f"expected positive total_runtime_ms, got {report.get('total_runtime_ms')!r}")
families = family_states.get("families") or []
if len(families) != 1:
    raise SystemExit(f"expected one family state, got {families!r}")
family = families[0]
if family["family_key"] != "11111111:fs258" or family["runtime_state"] != "runtime-bound" or family["import_state"] != "exact-authority":
    raise SystemExit(f"unexpected family state: {family!r}")
if report.get("promotion_blockers"):
    raise SystemExit(f"expected no promotion blockers, got {report['promotion_blockers']!r}")
if "Outcome: `promotable-runtime-package`" not in summary_text:
    raise SystemExit(f"expected summary markdown to include outcome, got {summary_text!r}")
if "Family inventory:" not in summary_text:
    raise SystemExit(f"expected summary markdown to link family inventory, got {summary_text!r}")
stage_timings = report.get("stage_timings_ms") or {}
required_stages = {
    "parse_cache",
    "resolve_requested_pairs",
    "build_migration_plan",
    "build_canonical_loader_manifest",
    "build_bindings",
    "build_runtime_loader_manifest",
    "materialize_package",
    "emit_binary_package",
    "total",
}
missing_stages = sorted(required_stages.difference(stage_timings))
if missing_stages:
    raise SystemExit(f"missing stage timings: {missing_stages}")
for stage_name in required_stages:
    if float(stage_timings[stage_name]) < 0:
        raise SystemExit(f"expected non-negative stage timing for {stage_name}, got {stage_timings[stage_name]!r}")

binding = bindings["bindings"][0]
if binding.get("selection_reason") != "deterministic-singleton-source-policy":
    raise SystemExit(f"unexpected selection reason: {binding.get('selection_reason')}")

records = plan["imported_index"]["canonical_records"]
if len(records) != 1:
    raise SystemExit(f"expected one canonical record, got {len(records)}")

package_path = Path(report["binary_package"]["output_path"])
if not package_path.exists():
    raise SystemExit(f"missing emitted package: {package_path}")
PY

python3 - "$ROOT_DIR" "$CACHE_PATH" "$BUNDLE_DIR" "$OUTPUT_DIR" <<'PY'
import copy
import json
import sys
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(sys.argv[1]) / "tools"))
import hts2phrb

cache, bundle, out_dir = map(Path, sys.argv[2:])
report_path = out_dir / "hts2phrb-report.json"
original = json.loads(report_path.read_text())
manifest_path = out_dir / "package/package-manifest.json"
manifest = json.loads(manifest_path.read_text())

# A persisted report's missing or stale summary must never act as a cache.
# Exercise both report reuse entrypoints, with missing manifest fields backfilled.
for pre_signature, stale_summary in ((True, None), (False, {"record_count": 999})):
    report = copy.deepcopy(original)
    if stale_summary is None:
        report.pop("package_manifest_summary")
    else:
        report["package_manifest_summary"] = stale_summary
    if not pre_signature:
        report.pop("pre_request_signature")
    report_path.write_text(json.dumps(report))
    partial_manifest = copy.deepcopy(manifest)
    partial_manifest.pop("runtime_ready_native_sampled_record_count")
    partial_manifest["runtime_ready_record_class"] = None
    manifest_path.write_text(json.dumps(partial_manifest))
    sys.argv = ["hts2phrb", "--cache", str(cache), "--bundle", str(bundle),
                "--output-dir", str(out_dir), "--reuse-existing", "--stdout-format", "json"]
    with patch.object(hts2phrb, "summarize_package_manifest", wraps=hts2phrb.summarize_package_manifest) as summarize, \
         patch.object(hts2phrb, "_load_runtime_manifest_stats", wraps=hts2phrb._load_runtime_manifest_stats) as load:
        hts2phrb.main()
        assert summarize.call_count > 0
        assert load.call_count > 0
    reused = json.loads(report_path.read_text())
    assert reused["reused_existing"]
    assert reused["package_manifest_summary"] == original["package_manifest_summary"]
    assert json.loads(manifest_path.read_text()) == manifest
    for key in ("gate_success", "gate_failures", "runtime_state_counts", "import_state_counts",
                "unresolved_family_review_summary", "runtime_overlay_blockers"):
        assert reused[key] == original[key], key

# External manifests still derive mixed ready/deferred counts from records.
external_dir = out_dir / "external"
external_dir.mkdir()
external = {"records": [
    {"record_kind": kind, "runtime_ready": ready}
    for ready in (True, False) for kind in ("canonical-sampled", "legacy-exact")
]}
(external_dir / "package-manifest.json").write_text(json.dumps(external))
report = {"package_dir": str(external_dir), "package_manifest_summary": {"record_count": 999},
          "stage_timings_ms": {"total": 123.0}}
hts2phrb.synchronize_report_summary_fields(report)
assert report["package_manifest_record_count"] == 4
for state in ("ready", "deferred"):
    assert report[f"package_manifest_runtime_{state}_record_count"] == 2
    assert report[f"package_manifest_runtime_{state}_native_sampled_record_count"] == 1
    assert report[f"package_manifest_runtime_{state}_compat_record_count"] == 1
    assert report[f"package_manifest_runtime_{state}_record_class"] == "mixed-native-and-compat"
assert report["total_runtime_ms"] == report["total_ms"] == 123.0

# An invalid transport selection retains the package and emits an overlay review.
# Its nonzero review fields must reach the final report without package rescans.
bindings = json.loads((out_dir / "bindings.json").read_text())
proxy_key = bindings["bindings"][0]["policy_key"]
policy_path = out_dir / "invalid-transport-policy.json"
policy_path.write_text(json.dumps({"transport_proxies": {
    proxy_key: {"selected_replacement_id": "missing-selection"}
}}))
overlay_dir = out_dir / "unresolved-overlay"
sys.argv = ["hts2phrb", "--cache", str(cache), "--bundle", str(bundle),
            "--output-dir", str(overlay_dir), "--transport-policy", str(policy_path),
            "--stdout-format", "json"]
with patch.object(hts2phrb, "summarize_package_manifest", wraps=hts2phrb.summarize_package_manifest) as summarize, \
     patch.object(hts2phrb, "_load_runtime_manifest_stats", side_effect=AssertionError("unexpected package reload")):
    hts2phrb.main()
    assert summarize.call_count == 1, summarize.call_count
overlay = json.loads((overlay_dir / "hts2phrb-report.json").read_text())
assert overlay["package_manifest_summary"] == original["package_manifest_summary"]
assert overlay["unresolved_count"] == overlay["runtime_overlay_unresolved_count"] == 1
assert overlay["runtime_overlay_direct_unresolved_count"] == 1
assert overlay["runtime_overlay_reason_counts"] == {"selected-replacement-id-not-found": 1}
assert Path(overlay["runtime_overlay_review_json_path"]).is_file()
PY

echo "emu_hts2phrb_smoke: PASS"

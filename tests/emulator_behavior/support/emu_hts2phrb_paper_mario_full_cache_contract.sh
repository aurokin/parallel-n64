#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"

CACHE_PATH="${EMU_HTS2PHRB_PM64_CACHE_PATH:-$REPO_ROOT/assets/PAPER MARIO_HIRESTEXTURES.hts}"

if [[ ! -f "$CACHE_PATH" ]]; then
  echo "SKIP: Paper Mario legacy cache not found at $CACHE_PATH."
  exit 77
fi

TMPDIR_RUN="$(mktemp -d)"
cleanup() {
  rm -rf "$TMPDIR_RUN"
}
trap cleanup EXIT

OUT_DIR="$TMPDIR_RUN/zero"
MAX_TOTAL_MS="10000"
MAX_BINARY_PACKAGE_BYTES="2100000000"

# Budget contract: the zero-config full-cache conversion must finish inside
# the time/size gates, both fresh and via --reuse-existing.
python3 "$REPO_ROOT/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --output-dir "$OUT_DIR" \
  --minimum-outcome partial-runtime-package \
  --expect-context-class zero-context \
  --max-total-ms "$MAX_TOTAL_MS" \
  --max-binary-package-bytes "$MAX_BINARY_PACKAGE_BYTES" \
  --stdout-format json >/dev/null

python3 "$REPO_ROOT/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --output-dir "$OUT_DIR" \
  --minimum-outcome partial-runtime-package \
  --expect-context-class zero-context \
  --max-total-ms "$MAX_TOTAL_MS" \
  --max-binary-package-bytes "$MAX_BINARY_PACKAGE_BYTES" \
  --reuse-existing \
  --stdout-format json >/dev/null

# Class-level assertions only: the conversion must succeed inside its
# budgets, produce a non-empty runtime-ready package, and report explicit
# reasons for anything deferred. No exact record/family counts.
python3 - "$OUT_DIR/hts2phrb-report.json" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())

def to_int(value, default=0):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

if report.get("gate_failures"):
    raise SystemExit(f"FAIL: report recorded gate failures: {report.get('gate_failures')!r}.")
if not report.get("gate_success"):
    raise SystemExit("FAIL: report did not record gate_success.")
if report.get("conversion_outcome") not in ("partial-runtime-package", "full-runtime-package"):
    raise SystemExit(f"FAIL: unexpected conversion outcome: {report.get('conversion_outcome')!r}.")
if report.get("context_bundle_class") != "zero-context":
    raise SystemExit(f"FAIL: expected zero-context conversion, got {report.get('context_bundle_class')!r}.")
if not report.get("reused_existing"):
    raise SystemExit("FAIL: --reuse-existing run did not report reused_existing.")
if to_int(report.get("requested_family_count")) < 1:
    raise SystemExit(f"FAIL: no families requested: {report.get('requested_family_count')!r}.")
if to_int(report.get("package_manifest_record_count")) < 1:
    raise SystemExit(f"FAIL: empty package manifest: {report.get('package_manifest_record_count')!r}.")
if to_int(report.get("package_manifest_runtime_ready_record_count")) < 1:
    raise SystemExit(
        f"FAIL: no runtime-ready records: {report.get('package_manifest_runtime_ready_record_count')!r}."
    )
if float(report.get("total_runtime_ms") or 0.0) <= 0.0:
    raise SystemExit("FAIL: report did not record total_runtime_ms.")
if int(report.get("binary_package_bytes") or 0) <= 0:
    raise SystemExit("FAIL: report did not record binary_package_bytes.")

# Deferred records are acceptable only with explicit reasons.
deferred = to_int(report.get("package_manifest_runtime_deferred_record_count"))
if deferred > 0:
    reason_counts = report.get("promotion_blocker_reason_counts") or {}
    explained = sum(to_int(v) for v in reason_counts.values())
    if explained < 1:
        raise SystemExit(
            f"FAIL: {deferred} deferred records without explicit promotion-blocker reasons."
        )
    if to_int(report.get("promotion_blocker_reason_unclassified_family_count")) != 0:
        raise SystemExit(
            "FAIL: deferred records include unclassified promotion-blocker families: "
            f"{report.get('promotion_blocker_reason_unclassified_family_count')!r}."
        )
PY

echo "emu_hts2phrb_paper_mario_full_cache_contract: PASS"

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"

LEGACY_CACHE_PATH="${EMU_RUNTIME_PM64_FULL_CACHE_LEGACY_CACHE:-$REPO_ROOT/assets/PAPER MARIO_HIRESTEXTURES.hts}"
CONTEXT_DIR="${EMU_RUNTIME_PM64_FULL_CACHE_CONTEXT_DIR:-}"
CONTEXT_SUMMARY="${EMU_RUNTIME_PM64_FULL_CACHE_CONTEXT_SUMMARY:-}"
OUTPUT_DIR="${EMU_RUNTIME_PM64_FULL_CACHE_REFRESH_OUTPUT_DIR:-}"
BUNDLE_ROOT="${EMU_RUNTIME_PM64_FULL_CACHE_REFRESH_BUNDLE_ROOT:-}"
PROMOTED_CONTEXT_ROOT="${EMU_RUNTIME_PM64_PROMOTED_CONTEXT_ROOT:-}"

if [[ "${EMU_ENABLE_RUNTIME_CONFORMANCE:-0}" != "1" ]]; then
  echo "SKIP: set EMU_ENABLE_RUNTIME_CONFORMANCE=1 to run Paper Mario full-cache PHRB authority refresh conformance."
  exit 77
fi

if [[ ! -f "$LEGACY_CACHE_PATH" ]]; then
  echo "SKIP: Paper Mario legacy cache not found at $LEGACY_CACHE_PATH."
  exit 77
fi

REFRESH_SCENARIO="$REPO_ROOT/tools/scenarios/paper-mario-full-cache-phrb-authority-refresh.sh"

if [[ ! -f "$REFRESH_SCENARIO" ]]; then
  echo "FAIL: full-cache authority refresh scenario is missing at $REFRESH_SCENARIO." >&2
  exit 1
fi

cleanup_output_dir=0
cleanup_bundle_root=0
cleanup_context_dir=0
default_context=0
normalize_repo_path() {
  local path="$1"
  if [[ -z "$path" || "$path" = /* ]]; then
    printf '%s\n' "$path"
  else
    printf '%s\n' "$REPO_ROOT/$path"
  fi
}
LEGACY_CACHE_PATH="$(normalize_repo_path "$LEGACY_CACHE_PATH")"
CONTEXT_DIR="$(normalize_repo_path "$CONTEXT_DIR")"
CONTEXT_SUMMARY="$(normalize_repo_path "$CONTEXT_SUMMARY")"
OUTPUT_DIR="$(normalize_repo_path "$OUTPUT_DIR")"
BUNDLE_ROOT="$(normalize_repo_path "$BUNDLE_ROOT")"
if [[ -z "$OUTPUT_DIR" ]]; then
  OUTPUT_DIR="$(mktemp -d)"
  cleanup_output_dir=1
fi
if [[ -z "$BUNDLE_ROOT" ]]; then
  BUNDLE_ROOT="$(mktemp -d)"
  cleanup_bundle_root=1
fi
if [[ -z "$CONTEXT_DIR" && -z "$CONTEXT_SUMMARY" ]]; then
  if [[ -n "$PROMOTED_CONTEXT_ROOT" ]]; then
    case "$PROMOTED_CONTEXT_ROOT" in
      /*)
        ;;
      artifacts/*)
        PROMOTED_CONTEXT_ROOT="$REPO_ROOT/$PROMOTED_CONTEXT_ROOT"
        ;;
      *)
        PROMOTED_CONTEXT_ROOT="$REPO_ROOT/artifacts/paper-mario-probes/validation/$PROMOTED_CONTEXT_ROOT"
        ;;
    esac
    if [[ ! -d "$PROMOTED_CONTEXT_ROOT" ]]; then
      echo "SKIP: promoted Paper Mario sampled-probe context root not found at $PROMOTED_CONTEXT_ROOT." >&2
      exit 77
    fi
  else
    PROMOTED_CONTEXT_ROOT="$REPO_ROOT/artifacts/paper-mario-probes/validation/promoted-selected-package-authorities"
  fi
  if [[ ! -d "$PROMOTED_CONTEXT_ROOT" ]]; then
    echo "SKIP: promoted Paper Mario sampled-probe context root not found at $PROMOTED_CONTEXT_ROOT." >&2
    exit 77
  fi
  TITLE_CONTEXT_BUNDLE_DEFAULT="$PROMOTED_CONTEXT_ROOT/title-screen"
  FILE_CONTEXT_BUNDLE_DEFAULT="$PROMOTED_CONTEXT_ROOT/file-select"
  WORLD_CONTEXT_BUNDLE_DEFAULT="$PROMOTED_CONTEXT_ROOT/kmr-03-entry-5"
  PROMOTED_SUMMARY_PATH="$PROMOTED_CONTEXT_ROOT/validation-summary.json"
  if [[ ! -f "$PROMOTED_SUMMARY_PATH" ]]; then
    echo "SKIP: promoted Paper Mario authority summary not found at $PROMOTED_SUMMARY_PATH." >&2
    exit 77
  fi
  for context_bundle in "$TITLE_CONTEXT_BUNDLE_DEFAULT" "$FILE_CONTEXT_BUNDLE_DEFAULT" "$WORLD_CONTEXT_BUNDLE_DEFAULT"; do
    if [[ ! -f "$context_bundle/traces/hires-evidence.json" ]]; then
      echo "SKIP: Paper Mario sampled-probe context bundle not found at $context_bundle." >&2
      exit 77
    fi
  done
  python3 - "$PROMOTED_SUMMARY_PATH" "$TITLE_CONTEXT_BUNDLE_DEFAULT" "$FILE_CONTEXT_BUNDLE_DEFAULT" "$WORLD_CONTEXT_BUNDLE_DEFAULT" <<'PY'
import json
import sys
from pathlib import Path

summary_path = Path(sys.argv[1])
expected_bundles = {
    "title-screen": Path(sys.argv[2]).resolve(),
    "file-select": Path(sys.argv[3]).resolve(),
    "kmr-03-entry-5": Path(sys.argv[4]).resolve(),
}
def skip(message):
    print(message, file=sys.stderr)
    sys.exit(77)

summary = json.loads(summary_path.read_text())
if summary.get("all_passed") is not True:
    skip(f"SKIP: promoted authority summary is not all_passed: {summary_path}")
fixtures = summary.get("fixtures") or []
by_label = {fixture.get("label"): fixture for fixture in fixtures if isinstance(fixture, dict)}
if set(by_label) != set(expected_bundles):
    skip(
        f"SKIP: promoted authority summary fixture labels mismatch: "
        f"expected {sorted(expected_bundles)!r}, got {sorted(by_label)!r}"
    )
for label, expected_bundle in expected_bundles.items():
    fixture = by_label[label]
    if fixture.get("passed") is not True:
        skip(f"SKIP: promoted authority fixture {label} did not pass")
    bundle_ref = fixture.get("bundle_dir")
    if not bundle_ref:
        skip(f"SKIP: promoted authority fixture {label} has no bundle_dir")
    bundle_path = Path(bundle_ref)
    if not bundle_path.is_absolute():
        bundle_path = (summary_path.parent / bundle_path).resolve()
    else:
        bundle_path = bundle_path.resolve()
    if bundle_path != expected_bundle:
        skip(
            f"SKIP: promoted authority fixture {label} points to {bundle_path}, "
            f"expected {expected_bundle}"
        )
PY
  CONTEXT_SUMMARY="$PROMOTED_SUMMARY_PATH"
  default_context=1
elif [[ -n "$CONTEXT_SUMMARY" && ! -f "$CONTEXT_SUMMARY" ]]; then
  echo "SKIP: Paper Mario authority context summary not found at $CONTEXT_SUMMARY."
  exit 77
elif [[ -z "$CONTEXT_SUMMARY" && ! -d "$CONTEXT_DIR" ]]; then
  echo "SKIP: Paper Mario authority context directory not found at $CONTEXT_DIR."
  exit 77
fi

cleanup() {
  local rc=$?
  if (( cleanup_output_dir )) && [[ $rc -eq 0 ]]; then
    rm -rf "$OUTPUT_DIR"
  else
    echo "[conformance-refresh] output dir: $OUTPUT_DIR"
  fi
  if (( cleanup_bundle_root )) && [[ $rc -eq 0 ]]; then
    rm -rf "$BUNDLE_ROOT"
  else
    echo "[conformance-refresh] bundle root: $BUNDLE_ROOT"
  fi
  if (( cleanup_context_dir )) && [[ $rc -eq 0 ]]; then
    rm -rf "$CONTEXT_DIR"
  elif [[ -n "$CONTEXT_SUMMARY" ]]; then
    echo "[conformance-refresh] context summary: $CONTEXT_SUMMARY"
  else
    echo "[conformance-refresh] context dir: $CONTEXT_DIR"
  fi
  exit "$rc"
}
trap cleanup EXIT

REPORT_PATH="$OUTPUT_DIR/hts2phrb-report.json"
PACKAGE_PATH="$OUTPUT_DIR/package.phrb"

declare -a refresh_args=(
  --legacy-cache "$LEGACY_CACHE_PATH"
  --output-dir "$OUTPUT_DIR"
  --bundle-root "$BUNDLE_ROOT"
)
if [[ -n "$CONTEXT_SUMMARY" ]]; then
  refresh_args+=(--context-summary "$CONTEXT_SUMMARY")
else
  refresh_args+=(--context-dir "$CONTEXT_DIR")
fi

bash "$REFRESH_SCENARIO" "${refresh_args[@]}"

if [[ ! -f "$REPORT_PATH" ]]; then
  echo "FAIL: refresh scenario did not produce $REPORT_PATH." >&2
  exit 1
fi
if [[ ! -f "$PACKAGE_PATH" ]]; then
  echo "FAIL: refresh scenario did not produce $PACKAGE_PATH." >&2
  exit 1
fi

python3 - "$REPORT_PATH" "$default_context" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())
default_context = bool(int(sys.argv[2]))

exact = {
    "conversion_outcome": "partial-runtime-package",
    "requested_family_count": 8992,
    "package_manifest_runtime_deferred_record_count": 368,
    "context_bundle_class": "context-enriched",
    "package_manifest_runtime_ready_record_class": "mixed-native-and-compat",
}
minimums = {
    "context_bundle_input_count": 1,
}
if default_context:
    exact.update({
        "package_manifest_record_count": 8960,
        "package_manifest_runtime_ready_record_count": 8592,
        "package_manifest_runtime_ready_native_sampled_record_count": 5,
        "package_manifest_runtime_ready_compat_record_count": 8587,
        "context_bundle_resolution_count": 3,
    })
else:
    minimums.update({
        "package_manifest_record_count": 8960,
        "package_manifest_runtime_ready_record_count": 8592,
        "package_manifest_runtime_ready_native_sampled_record_count": 5,
        "package_manifest_runtime_ready_compat_record_count": 8587,
        "context_bundle_resolution_count": 3,
    })
for key, expected_value in exact.items():
    actual = report.get(key)
    if actual != expected_value:
        raise SystemExit(
            f"FAIL: refresh report expected {key}={expected_value!r}, got {actual!r}."
        )
for key, min_value in minimums.items():
    actual = report.get(key)
    if actual is None or actual < min_value:
        raise SystemExit(
            f"FAIL: refresh report expected {key}>={min_value!r}, got {actual!r}."
        )

if not report.get("runtime_overlay_built"):
    reason = report.get("runtime_overlay_reason")
    blockers = report.get("runtime_overlay_blockers") or []
    if reason != "no-deterministic-bindings" or not blockers:
        raise SystemExit(
            "FAIL: refresh report skipped runtime overlay without explicit deterministic-binding blockers: "
            f"reason={reason!r} blockers={blockers!r}."
        )
PY

SUMMARY_PATH="$BUNDLE_ROOT/validation-summary.json"
if [[ ! -f "$SUMMARY_PATH" ]]; then
  echo "FAIL: refresh scenario did not produce $SUMMARY_PATH." >&2
  exit 1
fi

python3 - "$SUMMARY_PATH" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text())

fixtures = summary.get("fixtures") or []
if not summary.get("all_passed"):
    raise SystemExit("FAIL: refresh validation summary is not all_passed.")
if len(fixtures) != 3:
    raise SystemExit(f"FAIL: expected 3 refresh fixtures, found {len(fixtures)}.")
expected_labels = {"title-screen", "file-select", "kmr-03-entry-5"}
actual_labels = {fixture.get("label") for fixture in fixtures}
if actual_labels != expected_labels:
    raise SystemExit(f"FAIL: expected refresh fixture labels {sorted(expected_labels)!r}, got {sorted(actual_labels)!r}.")
expected_fixture_contract = {
    "title-screen": {
        "native_sampled_entry_count": 196,
        "entry_class": "mixed-native-and-compat",
        "descriptor_path_class": "mixed-sampled-compat",
        "descriptor_path_counts": {"sampled": 68, "native_checksum": 0, "generic": 0, "compat": 124},
    },
    "file-select": {
        "native_sampled_entry_count": 196,
        "entry_class": "mixed-native-and-compat",
        "descriptor_path_class": "mixed-sampled-compat",
        "descriptor_path_counts": {"sampled": 94, "native_checksum": 0, "generic": 0, "compat": 55},
    },
    "kmr-03-entry-5": {
        "native_sampled_entry_count": 196,
        "entry_class": "mixed-native-and-compat",
        "descriptor_path_class": "mixed-sampled-compat",
        "descriptor_path_counts": {"sampled": 4, "native_checksum": 0, "generic": 0, "compat": 184},
    },
}
for fixture in fixtures:
    label = fixture.get("label")
    expected = expected_fixture_contract[label]
    if not fixture.get("passed"):
        raise SystemExit(f"FAIL: refresh fixture {label} did not pass.")
    hires = fixture.get("hires_summary") or {}
    if hires.get("source_mode") != "phrb-only":
        raise SystemExit(
            f"FAIL: refresh fixture {label} expected source_mode=phrb-only, got {hires.get('source_mode')!r}."
        )
    native_sampled = int(hires.get("native_sampled_entry_count") or 0)
    if native_sampled != expected["native_sampled_entry_count"]:
        raise SystemExit(
            f"FAIL: refresh fixture {label} expected native_sampled_entry_count="
            f"{expected['native_sampled_entry_count']}, got {native_sampled}."
        )
    if hires.get("entry_class") != expected["entry_class"]:
        raise SystemExit(
            f"FAIL: refresh fixture {label} expected entry_class={expected['entry_class']!r}, "
            f"got {hires.get('entry_class')!r}."
        )
    if hires.get("descriptor_path_class") != expected["descriptor_path_class"]:
        raise SystemExit(
            f"FAIL: refresh fixture {label} expected descriptor_path_class="
            f"{expected['descriptor_path_class']!r}, got {hires.get('descriptor_path_class')!r}."
        )
    descriptor_paths = hires.get("descriptor_path_counts") or {}
    if descriptor_paths != expected["descriptor_path_counts"]:
        raise SystemExit(
            f"FAIL: refresh fixture {label} expected descriptor_path_counts="
            f"{expected['descriptor_path_counts']!r}, got {descriptor_paths!r}."
        )
PY

echo "emu_conformance_paper_mario_full_cache_phrb_authorities_refresh: PASS ($PACKAGE_PATH)"

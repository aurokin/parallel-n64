#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"

LEGACY_CACHE_PATH="${REPO_ROOT}/assets/PAPER MARIO_HIRESTEXTURES.hts"
PROMOTED_CONTEXT_ROOT="${REPO_ROOT}/artifacts/paper-mario-probes/validation/promoted-selected-package-authorities"
CONTEXT_SUMMARY_PATH=""
CONTEXT_DIR=""
OUTPUT_DIR=""
BUNDLE_ROOT=""
REUSE_EXISTING=0
CONTEXT_SUMMARY_EXPLICIT=0
CONTEXT_DIR_EXPLICIT=0

usage() {
  cat <<'USAGE'
Usage:
  tools/scenarios/paper-mario-full-cache-phrb-authority-refresh.sh [options]

Options:
  --legacy-cache PATH      Legacy `.hts` cache to convert
  --context-summary PATH   Single validation summary as context
                           (default: promoted-selected-package-authorities/validation-summary.json)
  --context-dir PATH       Explicit review-only directory scan for validation summaries
  --output-dir PATH        Converter output directory
  --bundle-root PATH       Validation bundle root
  --reuse-existing         Reuse a matching existing converter artifact when possible
  -h, --help               Show this help
USAGE
}

while (($#)); do
  case "$1" in
    --legacy-cache)
      shift
      LEGACY_CACHE_PATH="${1:-}"
      ;;
    --context-summary)
      shift
      CONTEXT_SUMMARY_PATH="${1:-}"
      CONTEXT_SUMMARY_EXPLICIT=1
      ;;
    --context-dir)
      shift
      CONTEXT_DIR="${1:-}"
      CONTEXT_DIR_EXPLICIT=1
      ;;
    --output-dir)
      shift
      OUTPUT_DIR="${1:-}"
      ;;
    --bundle-root)
      shift
      BUNDLE_ROOT="${1:-}"
      ;;
    --reuse-existing)
      REUSE_EXISTING=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

normalize_repo_path() {
  local path="$1"
  if [[ -z "$path" || "$path" = /* ]]; then
    printf '%s\n' "$path"
  else
    printf '%s\n' "$REPO_ROOT/$path"
  fi
}

LEGACY_CACHE_PATH="$(normalize_repo_path "$LEGACY_CACHE_PATH")"
CONTEXT_SUMMARY_PATH="$(normalize_repo_path "$CONTEXT_SUMMARY_PATH")"
CONTEXT_DIR="$(normalize_repo_path "$CONTEXT_DIR")"
OUTPUT_DIR="$(normalize_repo_path "$OUTPUT_DIR")"
BUNDLE_ROOT="$(normalize_repo_path "$BUNDLE_ROOT")"
PROMOTED_CONTEXT_ROOT="$(normalize_repo_path "$PROMOTED_CONTEXT_ROOT")"

if (( CONTEXT_SUMMARY_EXPLICIT && CONTEXT_DIR_EXPLICIT )); then
  echo "--context-summary and --context-dir are mutually exclusive." >&2
  exit 2
fi

if (( ! CONTEXT_SUMMARY_EXPLICIT && ! CONTEXT_DIR_EXPLICIT )); then
  CONTEXT_SUMMARY_PATH="$PROMOTED_CONTEXT_ROOT/validation-summary.json"
fi

if [[ ! -f "$LEGACY_CACHE_PATH" ]]; then
  echo "Legacy cache not found: $LEGACY_CACHE_PATH" >&2
  exit 2
fi

if [[ -n "$CONTEXT_SUMMARY_PATH" && ! -f "$CONTEXT_SUMMARY_PATH" ]]; then
  echo "Context summary not found: $CONTEXT_SUMMARY_PATH" >&2
  exit 2
fi

if [[ -z "$CONTEXT_SUMMARY_PATH" && ! -d "$CONTEXT_DIR" ]]; then
  echo "Context directory not found: $CONTEXT_DIR" >&2
  exit 2
fi

if [[ -n "$CONTEXT_SUMMARY_PATH" && "$CONTEXT_SUMMARY_PATH" == "$PROMOTED_CONTEXT_ROOT/validation-summary.json" ]]; then
  python3 - "$CONTEXT_SUMMARY_PATH" "$PROMOTED_CONTEXT_ROOT/title-screen" "$PROMOTED_CONTEXT_ROOT/file-select" "$PROMOTED_CONTEXT_ROOT/kmr-03-entry-5" <<'PY'
import json
import sys
from pathlib import Path

summary_path = Path(sys.argv[1])
expected_bundles = {
    "title-screen": Path(sys.argv[2]).resolve(),
    "file-select": Path(sys.argv[3]).resolve(),
    "kmr-03-entry-5": Path(sys.argv[4]).resolve(),
}

summary = json.loads(summary_path.read_text())
if summary.get("all_passed") is not True:
    raise SystemExit(f"Promoted authority summary is not all_passed: {summary_path}")
fixtures = summary.get("fixtures") or []
by_label = {fixture.get("label"): fixture for fixture in fixtures if isinstance(fixture, dict)}
if set(by_label) != set(expected_bundles):
    raise SystemExit(
        f"Promoted authority summary fixture labels mismatch: expected {sorted(expected_bundles)!r}, got {sorted(by_label)!r}"
    )
for label, expected_bundle in expected_bundles.items():
    fixture = by_label[label]
    if fixture.get("passed") is not True:
        raise SystemExit(f"Promoted authority fixture {label} did not pass")
    bundle_ref = fixture.get("bundle_dir")
    if not bundle_ref:
        raise SystemExit(f"Promoted authority fixture {label} has no bundle_dir")
    bundle_path = Path(bundle_ref)
    if not bundle_path.is_absolute():
        bundle_path = (summary_path.parent / bundle_path).resolve()
    else:
        bundle_path = bundle_path.resolve()
    if bundle_path != expected_bundle:
        raise SystemExit(
            f"Promoted authority fixture {label} points to {bundle_path}, expected {expected_bundle}"
        )
    if not (bundle_path / "traces" / "hires-evidence.json").is_file():
        raise SystemExit(f"Promoted authority fixture {label} has no hires-evidence.json at {bundle_path}")
PY
fi

if [[ -z "$OUTPUT_DIR" || -z "$BUNDLE_ROOT" ]]; then
  timestamp="$(date +"%Y%m%d-%H%M%S")"
  if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="$REPO_ROOT/artifacts/hts2phrb-review/${timestamp}-pm64-all-families-authority-context-refresh"
  fi
  if [[ -z "$BUNDLE_ROOT" ]]; then
    BUNDLE_ROOT="$REPO_ROOT/artifacts/paper-mario-probes/validation/${timestamp}-full-cache-phrb-authorities-authority-context-refresh"
  fi
fi

declare -a converter_args
converter_args=(
  --cache "$LEGACY_CACHE_PATH"
  --minimum-outcome partial-runtime-package
  --expect-context-class context-enriched
  --expect-runtime-ready-class mixed-native-and-compat
  --output-dir "$OUTPUT_DIR"
  --stdout-format json
)

if [[ -n "$CONTEXT_SUMMARY_PATH" ]]; then
  converter_args+=(--context-bundle "$CONTEXT_SUMMARY_PATH")
else
  converter_args+=(--context-dir "$CONTEXT_DIR")
fi

if (( REUSE_EXISTING )); then
  converter_args+=(--reuse-existing)
fi

python3 "$REPO_ROOT/tools/hts2phrb.py" "${converter_args[@]}" >/dev/null

PACKAGE_PATH="$OUTPUT_DIR/package.phrb"
REPORT_PATH="$OUTPUT_DIR/hts2phrb-report.json"
if [[ ! -f "$PACKAGE_PATH" ]]; then
  echo "Missing refreshed package: $PACKAGE_PATH" >&2
  exit 1
fi
if [[ ! -f "$REPORT_PATH" ]]; then
  echo "Missing converter report: $REPORT_PATH" >&2
  exit 1
fi

"$SCRIPT_DIR/paper-mario-full-cache-phrb-authority-validation.sh" \
  --cache-path "$PACKAGE_PATH" \
  --bundle-root "$BUNDLE_ROOT"

SUMMARY_PATH="$BUNDLE_ROOT/validation-summary.json"
if [[ ! -f "$SUMMARY_PATH" ]]; then
  echo "Missing authority validation summary: $SUMMARY_PATH" >&2
  exit 1
fi

echo "[refresh] converter report: $REPORT_PATH"
echo "[refresh] validation summary: $SUMMARY_PATH"

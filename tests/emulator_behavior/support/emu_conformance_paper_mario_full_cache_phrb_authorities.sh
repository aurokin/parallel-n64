#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"

DEFAULT_CACHE_PATH="$REPO_ROOT/artifacts/hts2phrb-review/local-pm64-exact-variant-set/package.phrb"
CACHE_PATH="${EMU_RUNTIME_PM64_FULL_CACHE_PHRB:-$DEFAULT_CACHE_PATH}"
BUNDLE_ROOT="${EMU_RUNTIME_PM64_FULL_CACHE_BUNDLE_ROOT:-}"

if [[ "${EMU_ENABLE_RUNTIME_CONFORMANCE:-0}" != "1" ]]; then
  echo "SKIP: set EMU_ENABLE_RUNTIME_CONFORMANCE=1 to run Paper Mario full-cache PHRB authority conformance."
  exit 77
fi

fail_missing() {
  local what="$1"
  local detail="$2"
  local staging="$3"
  echo "==================================================================" >&2
  echo "FAIL: missing prerequisite for the Paper Mario full-cache PHRB lane" >&2
  echo "  what:  $what" >&2
  echo "  where: $detail" >&2
  echo "  stage: $staging" >&2
  echo "==================================================================" >&2
  exit 1
}

if [[ ! -x "$REPO_ROOT/tools/scenarios/paper-mario-full-cache-phrb-authority-validation.sh" ]]; then
  echo "FAIL: full-cache PHRB authority validation wrapper is missing or not executable." >&2
  exit 1
fi

if [[ ! -f "$CACHE_PATH" ]]; then
  fail_missing "Paper Mario PHRB package" "$CACHE_PATH" \
    "run tools/hts2phrb.py against 'assets/PAPER MARIO_HIRESTEXTURES.hts' (or set EMU_RUNTIME_PM64_FULL_CACHE_PHRB)."
fi

require_runtime_env_prereqs() {
  local env_path="$1"
  local label="$2"
  if [[ ! -f "$env_path" ]]; then
    fail_missing "runtime env for $label" "$env_path" \
      "restore the fixture runtime env under tools/scenarios/."
  fi

  local bin_path base_cfg core_path rom_path authoritative_state_path
  bin_path="$(
    ENV_PATH="$env_path" python3 - <<'PY'
import os
from pathlib import Path

env_path = Path(os.environ["ENV_PATH"])
values = {}
for raw in env_path.read_text().splitlines():
    line = raw.strip()
    if not line or line.startswith("#") or "=" not in line:
        continue
    key, value = line.split("=", 1)
    values[key] = value.strip().strip('"')
for key in ("RETROARCH_BIN", "RETROARCH_BASE_CONFIG", "CORE_PATH", "ROM_PATH", "AUTHORITATIVE_STATE_PATH"):
    print(values.get(key, ""))
PY
  )"

  mapfile -t prereq_paths <<<"$bin_path"
  bin_path="${prereq_paths[0]:-}"
  base_cfg="${prereq_paths[1]:-}"
  core_path="${prereq_paths[2]:-}"
  rom_path="${prereq_paths[3]:-}"
  authoritative_state_path="${prereq_paths[4]:-}"

  if [[ -z "$bin_path" || ! -x "$bin_path" ]]; then
    fail_missing "RetroArch binary for $label" "$bin_path" \
      "build RetroArch (see RETROARCH_BIN in $env_path)."
  fi
  if [[ -z "$base_cfg" || ! -f "$base_cfg" ]]; then
    fail_missing "RetroArch base config for $label" "$base_cfg" \
      "stage the deterministic RetroArch config referenced by $env_path."
  fi
  if [[ -z "$core_path" || ! -f "$core_path" ]]; then
    fail_missing "libretro core for $label" "$core_path" \
      "build it with: make -j4 HAVE_PARALLEL=1 parallel_n64_libretro.so"
  fi
  if [[ -z "$rom_path" || ! -f "$rom_path" ]]; then
    fail_missing "Paper Mario ROM for $label" "$rom_path" \
      "stage 'Paper Mario (USA).zip' under assets/."
  fi
  if [[ -z "$authoritative_state_path" || ! -f "$authoritative_state_path" ]]; then
    fail_missing "authoritative savestate for $label" "$authoritative_state_path" \
      "remint it with tools/scenarios/remint-paper-mario-${label#paper-mario-}-authority.sh (savestates under assets/states/ are not checked in)."
  fi
}

require_runtime_env_prereqs "$REPO_ROOT/tools/scenarios/paper-mario-title-screen.runtime.env" "paper-mario-title-screen"
require_runtime_env_prereqs "$REPO_ROOT/tools/scenarios/paper-mario-file-select.runtime.env" "paper-mario-file-select"
require_runtime_env_prereqs "$REPO_ROOT/tools/scenarios/paper-mario-kmr-03-entry-5.runtime.env" "paper-mario-kmr-03-entry-5"

cleanup_bundle_root=0
if [[ -z "$BUNDLE_ROOT" ]]; then
  BUNDLE_ROOT="$(mktemp -d)"
  cleanup_bundle_root=1
fi

cleanup() {
  local rc=$?
  if (( cleanup_bundle_root )) && [[ $rc -eq 0 ]]; then
    rm -rf "$BUNDLE_ROOT"
  else
    echo "[conformance] bundle root: $BUNDLE_ROOT"
  fi
  exit "$rc"
}
trap cleanup EXIT

set +e
timeout --signal=INT --kill-after=15 600s \
  "$REPO_ROOT/tools/scenarios/paper-mario-full-cache-phrb-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$BUNDLE_ROOT"
rc=$?
set -e

if [[ $rc -ne 0 ]]; then
  echo "FAIL: full-cache PHRB authority validation exited with status $rc." >&2
  exit 1
fi

SUMMARY_PATH="$BUNDLE_ROOT/validation-summary.json"
if [[ ! -f "$SUMMARY_PATH" ]]; then
  echo "FAIL: full-cache PHRB authority validation did not produce $SUMMARY_PATH." >&2
  exit 1
fi

# Class-level semantics only: provider on, phrb-only sourcing, non-empty
# entry set, live draw hits, and explicit fallback reasons. No exact
# descriptor-path counts and no hi-res-on screenshot digests.
python3 - "$SUMMARY_PATH" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text())
fixtures = summary.get("fixtures") or []

def to_int(value, default=0):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default

if not summary.get("all_passed"):
    raise SystemExit("FAIL: full-cache PHRB authority summary is not all_passed.")
if len(fixtures) != 3:
    raise SystemExit(f"FAIL: expected 3 fixtures, found {len(fixtures)}.")
for fixture in fixtures:
    label = fixture.get("label")
    if not fixture.get("passed"):
        raise SystemExit(f"FAIL: fixture {label} did not pass.")
    hires = fixture.get("hires_summary") or {}
    if hires.get("source_mode") != "phrb-only":
        raise SystemExit(
            f"FAIL: fixture {label} expected source_mode=phrb-only, "
            f"got {hires.get('source_mode')!r}."
        )
    if to_int(hires.get("entry_count")) < 1:
        raise SystemExit(f"FAIL: fixture {label} has no hi-res entries.")
    if to_int(hires.get("source_phrb_count")) < 1:
        raise SystemExit(f"FAIL: fixture {label} has no phrb-backed hi-res entries.")
    if to_int(hires.get("draw_hits")) < 1:
        raise SystemExit(f"FAIL: fixture {label} reported no hi-res draw hits.")
PY

echo "emu_conformance_paper_mario_full_cache_phrb_authorities: PASS ($CACHE_PATH)"

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
source "$REPO_ROOT/tools/scenarios/lib/common.sh"
BOOT_VALIDATOR="$REPO_ROOT/tools/scenarios/cross-game-hires-boot-validation.sh"
FIXTURE_VALIDATOR="$REPO_ROOT/tools/scenarios/cross-game-hires-savestate-fixture-validation.sh"
REMINT_SCENARIO="$REPO_ROOT/tools/scenarios/remint-cross-game-boot-state.sh"
RUNTIME_ADAPTER="$REPO_ROOT/tools/adapters/retroarch_stdin_session.sh"

if [[ ! -x "$BOOT_VALIDATOR" ]]; then
  echo "FAIL: missing executable boot validator at $BOOT_VALIDATOR" >&2
  exit 1
fi
if [[ ! -x "$FIXTURE_VALIDATOR" ]]; then
  echo "FAIL: missing executable savestate fixture validator at $FIXTURE_VALIDATOR" >&2
  exit 1
fi
if [[ ! -x "$REMINT_SCENARIO" ]]; then
  echo "FAIL: missing executable remint scenario at $REMINT_SCENARIO" >&2
  exit 1
fi
if [[ ! -x "$RUNTIME_ADAPTER" ]]; then
  echo "FAIL: missing executable runtime adapter at $RUNTIME_ADAPTER" >&2
  exit 1
fi
for pattern in ROM_SHA256 CORE_SHA256 HIRES_CACHE_SHA256 COMMAND_SIGNATURE; do
  if ! rg -q --fixed-strings -- "$pattern=" "$RUNTIME_ADAPTER"; then
    echo "FAIL: runtime adapter must persist $pattern in retroarch.session.env for reuse identity checks." >&2
    exit 1
  fi
done
if ! rg -q --fixed-strings 'rm -rf "$BUNDLE_DIR"/captures "$BUNDLE_DIR"/logs "$BUNDLE_DIR"/traces' "$BOOT_VALIDATOR"; then
  echo "FAIL: boot validator must clear fresh-run bundle evidence subdirectories before launch." >&2
  exit 1
fi
if ! rg -q --fixed-strings '"$BUNDLE_DIR"/savefiles' "$BOOT_VALIDATOR"; then
  echo "FAIL: boot validator must clear bundle-local savefiles before fresh launch." >&2
  exit 1
fi
if ! rg -q --fixed-strings 'rm -rf "$BUNDLE_DIR"/captures "$BUNDLE_DIR"/logs "$BUNDLE_DIR"/traces "$BUNDLE_DIR"/savefiles "$BUNDLE_DIR/states/ParaLLEl N64"' "$FIXTURE_VALIDATOR"; then
  echo "FAIL: savestate fixture validator must clear fresh-run bundle evidence subdirectories before launch." >&2
  exit 1
fi
if ! rg -q --fixed-strings '"$BUNDLE_DIR"/savefiles' "$FIXTURE_VALIDATOR"; then
  echo "FAIL: savestate fixture validator must clear bundle-local savefiles before fresh launch." >&2
  exit 1
fi
if ! rg -q --fixed-strings 'rm -rf "$BOOTSTRAP_BUNDLE" "$VERIFY_BUNDLE" "$BUNDLE_ROOT/candidate"' "$REMINT_SCENARIO"; then
  echo "FAIL: remint scenario must clear bootstrap, verify, and candidate artifacts before early failures or fresh launch." >&2
  exit 1
fi
for name in PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE; do
  if [[ "$(rg -c --fixed-strings -- "-u $name" "$REMINT_SCENARIO")" -lt 2 ]]; then
    echo "FAIL: remint scenario must scrub ambient $name for both baseline-off bootstrap and verify runs." >&2
    exit 1
  fi
done

TMPDIR="$(mktemp -d /tmp/parallel-n64-cross-game-validator-contract-XXXXXX)"
cleanup() {
  rm -rf "$TMPDIR"
}
trap cleanup EXIT

sha256_file() {
  scenario_sha256_file "$1"
}

write_hires_log() {
  local path="$1"
  local entries="$2"
  local native_entries="$3"
  local compat_entries="$4"
  local compat_hits="$5"
  local ci_hits="$6"
  local ci_attempts="$7"
  local descriptor_sampled="${8:-0}"
  local descriptor_native_checksum="${9:-0}"
  local descriptor_generic="${10:-0}"
  local descriptor_compat="${11:-$compat_hits}"
  local source_hts="${12:-0}"
  local source_htc="${13:-0}"
  mkdir -p "$(dirname -- "$path")"
  {
    printf 'Hi-res replacement cache loaded: %s entries from %s/package.phrb\n' "$entries" "$TMPDIR"
    printf 'Hi-res keying summary: lookups=10 hits=2 misses=8 filtered=0 block_probe_hits=0 compat_draw_hits=%s compat_draw_ci_hits=%s compat_draw_ci_attempts=%s provider=on entries=%s native_sampled=%s compat=%s sampled_index=0 sampled_dupe_keys=0 sampled_dupe_entries=0 sampled_families=0 compat_low32_families=%s sources(phrb=%s hts=%s htc=%s) descriptor_paths(sampled=%s native_checksum=%s generic=%s compat=%s) sampled_detail(family_singleton=0 ordered_surface_singleton=0 exact_selector=0) generic_detail(identity_assisted=0 plain=0 native=0 compat=0 unknown=0).\n' \
      "$compat_hits" "$ci_hits" "$ci_attempts" "$entries" "$native_entries" "$compat_entries" "$compat_entries" "$entries" "$source_hts" "$source_htc" "$descriptor_sampled" "$descriptor_native_checksum" "$descriptor_generic" "$descriptor_compat"
  } >"$path"
}

write_fixture_runtime_log() {
  local path="$1"
  local entries="$2"
  local native_entries="$3"
  local compat_entries="$4"
  local compat_hits="$5"
  local ci_hits="$6"
  local ci_attempts="$7"
  local state_path="$8"
  local frame="$9"
  local descriptor_sampled="${10:-0}"
  local descriptor_native_checksum="${11:-0}"
  local descriptor_generic="${12:-0}"
  local descriptor_compat="${13:-$compat_hits}"
  local source_hts="${14:-0}"
  local source_htc="${15:-0}"
  mkdir -p "$(dirname -- "$path")"
  {
    printf '[INFO] [State] Loading state "%s", 10 bytes.\n' "$state_path"
    printf 'GET_STATUS PAUSED ParaLLEl N64,Contract Game,crc32=00000000,frame=%s\n' "$frame"
    printf 'Hi-res replacement cache loaded: %s entries from %s/package.phrb\n' "$entries" "$TMPDIR"
    printf 'Hi-res keying summary: lookups=10 hits=2 misses=8 filtered=0 block_probe_hits=0 compat_draw_hits=%s compat_draw_ci_hits=%s compat_draw_ci_attempts=%s provider=on entries=%s native_sampled=%s compat=%s sampled_index=0 sampled_dupe_keys=0 sampled_dupe_entries=0 sampled_families=0 compat_low32_families=%s sources(phrb=%s hts=%s htc=%s) descriptor_paths(sampled=%s native_checksum=%s generic=%s compat=%s) sampled_detail(family_singleton=0 ordered_surface_singleton=0 exact_selector=0) generic_detail(identity_assisted=0 plain=0 native=0 compat=0 unknown=0).\n' \
      "$compat_hits" "$ci_hits" "$ci_attempts" "$entries" "$native_entries" "$compat_entries" "$compat_entries" "$entries" "$source_hts" "$source_htc" "$descriptor_sampled" "$descriptor_native_checksum" "$descriptor_generic" "$descriptor_compat"
  } >"$path"
}

assert_json_field() {
  local json_path="$1"
  local expr="$2"
  local message="$3"
  python3 - "$json_path" "$expr" "$message" <<'PY'
import json
import sys
from pathlib import Path

path, expr, message = sys.argv[1:]
data = json.loads(Path(path).read_text())
if not eval(expr, {"__builtins__": {}}, {"data": data}):
    print(f"FAIL: {message}", file=sys.stderr)
    print(path, file=sys.stderr)
    raise SystemExit(1)
PY
}

write_session_env() {
  local bundle_dir="$1"
  local rom_path="$2"
  local core_path="$3"
  local cache_path="${4:-$CACHE_PATH}"
  local command_signature="${5:-}"
  local -a commands=()
  if [[ -n "${BOOT_COMMAND_SIGNATURE:-}" && "$command_signature" == "$BOOT_COMMAND_SIGNATURE" ]]; then
    commands=("WAIT_COMMAND_READY 120" "WAIT 30" "SCREENSHOT" "WAIT_NEW_CAPTURE 15" "QUIT")
  elif [[ -n "${FIXTURE_COMMAND_SIGNATURE:-}" && "$command_signature" == "$FIXTURE_COMMAND_SIGNATURE" ]]; then
    commands=("WAIT_COMMAND_READY 120" "LOAD_STATE_SLOT_PAUSED 0" "STEP_FRAME 3" "WAIT_STATUS_FRAME PAUSED 3 15" "SCREENSHOT" "WAIT_NEW_CAPTURE 15" "QUIT")
  fi
  if ((${#commands[@]})); then
    printf '%s\n' "${commands[@]}" > "$bundle_dir/retroarch.expected.commands.log"
    printf '%s\n' "${commands[@]}" > "$bundle_dir/retroarch.executed.commands.log"
    printf '%s\n' "${commands[@]}" > "$bundle_dir/logs/retroarch.commands.log"
    : > "$bundle_dir/retroarch.command-proofs.log"
    for command in "${commands[@]}"; do
      printf '%s\tproof=synthetic-contract\n' "$command" >> "$bundle_dir/retroarch.command-proofs.log"
    done
  fi
  cat > "$bundle_dir/retroarch.session.env" <<EOF
ROM_PATH=$rom_path
CORE_PATH=$core_path
ROM_SHA256=$(sha256_file "$rom_path")
CORE_SHA256=$(sha256_file "$core_path")
HIRES_CACHE_PATH=$cache_path
HIRES_CACHE_SHA256=$(sha256_file "$cache_path")
MODE=on
COMMAND_SIGNATURE=$command_signature
EOF
}

write_run_env() {
  local bundle_dir="$1"
  local exit_status="${2:-0}"
  local forced_termination="${3:-0}"
  cat > "$bundle_dir/retroarch.run.env" <<EOF
RUNTIME_EXECUTED=1
RETROARCH_EXIT_STATUS=$exit_status
FORCED_TERMINATION=$forced_termination
EOF
}

ROM_PATH="$TMPDIR/test-rom.z64"
CACHE_PATH="$TMPDIR/package.phrb"
BAD_CACHE_PATH="$TMPDIR/package.hts"
OTHER_CACHE_PATH="$TMPDIR/other-package.phrb"
STATE_PATH="$TMPDIR/test.state"
CORE_PATH_CONTRACT="$TMPDIR/parallel_n64_libretro.so"
RETROARCH_BIN_CONTRACT="$TMPDIR/retroarch"
RETROARCH_CONFIG_CONTRACT="$TMPDIR/retroarch.cfg"
CAPTURE_PAYLOAD="$TMPDIR/capture.bin"

printf 'rom\n' >"$ROM_PATH"
printf 'phrb\n' >"$CACHE_PATH"
printf 'hts\n' >"$BAD_CACHE_PATH"
printf 'other-phrb\n' >"$OTHER_CACHE_PATH"
printf 'state\n' >"$STATE_PATH"
printf 'core\n' >"$CORE_PATH_CONTRACT"
printf '#!/usr/bin/env bash\nexit 0\n' >"$RETROARCH_BIN_CONTRACT"
chmod +x "$RETROARCH_BIN_CONTRACT"
printf 'config\n' >"$RETROARCH_CONFIG_CONTRACT"
printf 'capture\n' >"$CAPTURE_PAYLOAD"

STATE_SHA256="$(sha256_file "$STATE_PATH")"
CAPTURE_SHA256="$(sha256_file "$CAPTURE_PAYLOAD")"
FAKE_REMINT_ADAPTER="$TMPDIR/fake-remint-adapter.sh"
cat > "$FAKE_REMINT_ADAPTER" <<'SH'
#!/usr/bin/env bash
set -euo pipefail

bundle_dir=""
mode=""
core_path=""
rom_path=""
declare -a commands=()
while (($#)); do
  case "$1" in
    --bundle-dir)
      shift
      bundle_dir="${1:-}"
      ;;
    --mode)
      shift
      mode="${1:-}"
      ;;
    --core)
      shift
      core_path="${1:-}"
      ;;
    --rom)
      shift
      rom_path="${1:-}"
      ;;
    --command)
      shift
      commands+=("${1:-}")
      ;;
    --retroarch-bin|--base-config|--startup-wait)
      shift
      ;;
  esac
  shift
done

sha256_file() {
  sha256sum "$1" | awk '{print $1}'
}

rom_file="$(basename -- "$rom_path")"
rom_stem="${rom_file%.*}"
command_signature="$(printf '%s\n' "${commands[@]}" | sha256sum | awk '{print $1}')"
mkdir -p "$bundle_dir/captures" "$bundle_dir/logs" "$bundle_dir/states/ParaLLEl N64"
printf '%s\n' "${commands[@]}" > "$bundle_dir/retroarch.expected.commands.log"
printf '%s\n' "${commands[@]}" > "$bundle_dir/retroarch.executed.commands.log"
printf '%s\n' "${commands[@]}" > "$bundle_dir/logs/retroarch.commands.log"
: > "$bundle_dir/retroarch.command-proofs.log"
for command in "${commands[@]}"; do
  printf '%s\tproof=synthetic-adapter\n' "$command" >> "$bundle_dir/retroarch.command-proofs.log"
done
cat > "$bundle_dir/retroarch.session.env" <<EOF
ROM_PATH=$rom_path
CORE_PATH=$core_path
ROM_SHA256=$(sha256_file "$rom_path")
CORE_SHA256=$(sha256_file "$core_path")
HIRES_CACHE_PATH=
HIRES_CACHE_SHA256=
MODE=$mode
COMMAND_SIGNATURE=$command_signature
EOF
cat > "$bundle_dir/retroarch.run.env" <<'EOF'
RUNTIME_EXECUTED=1
RETROARCH_EXIT_STATUS=0
FORCED_TERMINATION=0
EOF
if printf '%s\n' "${commands[@]}" | rg -q '^SAVE_STATE$'; then
  printf 'state\n' > "$bundle_dir/states/ParaLLEl N64/${rom_stem}.state"
fi
if printf '%s\n' "${commands[@]}" | rg -q '^SCREENSHOT$'; then
  printf 'capture\n' > "$bundle_dir/captures/verify.png"
fi
if printf '%s\n' "${commands[@]}" | rg -q '^LOAD_STATE_SLOT_PAUSED 0$'; then
  staged_state="$bundle_dir/states/ParaLLEl N64/${rom_stem}.state"
  {
    printf '[INFO] [State] Loading state "%s", 6 bytes.\n' "$staged_state"
    printf 'GET_STATUS PAUSED ParaLLEl N64,Contract Game,crc32=00000000,frame=3\n'
  } > "$bundle_dir/logs/retroarch.log"
else
  : > "$bundle_dir/logs/retroarch.log"
fi
SH
chmod +x "$FAKE_REMINT_ADAPTER"
BOOT_COMMAND_SIGNATURE="$(printf '%s\n' "WAIT_COMMAND_READY 120" "WAIT 30" "SCREENSHOT" "WAIT_NEW_CAPTURE 15" "QUIT" | sha256sum | awk '{print $1}')"
FIXTURE_COMMAND_SIGNATURE="$(printf '%s\n' "WAIT_COMMAND_READY 120" "LOAD_STATE_SLOT_PAUSED 0" "STEP_FRAME 3" "WAIT_STATUS_FRAME PAUSED 3 15" "SCREENSHOT" "WAIT_NEW_CAPTURE 15" "QUIT" | sha256sum | awk '{print $1}')"

REMINT_FAILURE_BUNDLE="$TMPDIR/remint-missing-hash"
if CORE_PATH="$CORE_PATH_CONTRACT" RETROARCH_BIN="$RETROARCH_BIN_CONTRACT" RETROARCH_BASE_CONFIG="$RETROARCH_CONFIG_CONTRACT" \
  "$REMINT_SCENARIO" \
  --game-id contract-remint-missing-hash \
  --rom "$ROM_PATH" \
  --output-path "$TMPDIR/output.state" \
  --bundle-root "$REMINT_FAILURE_BUNDLE" >/dev/null 2>&1; then
  echo "FAIL: remint scenario accepted a missing expected verification hash without --allow-unverified." >&2
  exit 1
fi
assert_json_field "$REMINT_FAILURE_BUNDLE/remint-summary.json" 'data["passed"] is False' \
  "remint failure summary should mark the run failed"
assert_json_field "$REMINT_FAILURE_BUNDLE/remint-summary.json" 'data["verification_status"] == "verification-failed"' \
  "remint failure summary should include verification_status"
assert_json_field "$REMINT_FAILURE_BUNDLE/remint-summary.json" 'data["rom_sha256"] is not None and data["core_sha256"] is not None' \
  "remint failure summary should include ROM/core provenance hashes"
assert_json_field "$REMINT_FAILURE_BUNDLE/remint-summary.json" 'data["output_path"] is None and data["output_written"] is False' \
  "remint failure summary should not advertise a written output"
if ! rg -q 'Verification status: `verification-failed`' "$REMINT_FAILURE_BUNDLE/remint-summary.md"; then
  echo "FAIL: remint failure markdown missing verification status." >&2
  exit 1
fi

REMINT_UNVERIFIED_BUNDLE="$TMPDIR/remint-allow-unverified"
REMINT_UNVERIFIED_OUTPUT="$TMPDIR/unverified-output.state"
if CORE_PATH="$CORE_PATH_CONTRACT" RETROARCH_BIN="$RETROARCH_BIN_CONTRACT" RETROARCH_BASE_CONFIG="$RETROARCH_CONFIG_CONTRACT" RUNTIME_ADAPTER="$FAKE_REMINT_ADAPTER" \
  "$REMINT_SCENARIO" \
  --game-id contract-remint-allow-unverified \
  --rom "$ROM_PATH" \
  --output-path "$REMINT_UNVERIFIED_OUTPUT" \
  --bundle-root "$REMINT_UNVERIFIED_BUNDLE" \
  --allow-unverified >/dev/null 2>&1; then
  echo "FAIL: remint scenario promoted an unverified state." >&2
  exit 1
fi
assert_json_field "$REMINT_UNVERIFIED_BUNDLE/remint-summary.json" 'data["passed"] is False' \
  "allow-unverified remint should remain non-authoritative"
assert_json_field "$REMINT_UNVERIFIED_BUNDLE/remint-summary.json" 'data["verification_status"] == "explicitly-unverified"' \
  "allow-unverified remint should be marked explicitly unverified when hard checks pass"
assert_json_field "$REMINT_UNVERIFIED_BUNDLE/remint-summary.json" 'data["review_only_hard_checks_passed"] is True' \
  "allow-unverified remint should preserve hard-check success as review-only"
assert_json_field "$REMINT_UNVERIFIED_BUNDLE/remint-summary.json" 'data["output_path"] is None and data["output_written"] is False and data["promoted"] is False' \
  "allow-unverified remint must not advertise promotion or final output"
if [[ -f "$REMINT_UNVERIFIED_OUTPUT" ]]; then
  echo "FAIL: allow-unverified remint wrote the final output path." >&2
  exit 1
fi
if [[ ! -f "$REMINT_UNVERIFIED_BUNDLE/candidate/test-rom.state" ]]; then
  echo "FAIL: allow-unverified remint did not retain the review-only candidate state." >&2
  exit 1
fi
if ! rg -q 'Verification status: `explicitly-unverified`' "$REMINT_UNVERIFIED_BUNDLE/remint-summary.md"; then
  echo "FAIL: allow-unverified remint markdown missing explicitly-unverified status." >&2
  exit 1
fi

if ! python3 - "$REMINT_SCENARIO" <<'PY'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text()
if not re.search(r"authoritative\s*=\s*capture_hash_matches\s+and\s+hard_checks_pass", text):
    raise SystemExit(1)
if not re.search(r"passed\s*=\s*authoritative", text):
    raise SystemExit(1)
if '"review_only_hard_checks_passed": unverified_review_only and hard_checks_pass' not in text:
    raise SystemExit(1)
if '"output_path": None' not in text or '"output_written": False' not in text:
    raise SystemExit(1)
if 'summary["output_written"] = True' not in text:
    raise SystemExit(1)
PY
then
  echo "FAIL: remint summary must keep explicit unverified hard-check success review-only and non-authoritative." >&2
  exit 1
fi

if ! python3 - "$REPO_ROOT/tools/hts2phrb.py" <<'PY'
import ast
import sys
from pathlib import Path

tree = ast.parse(Path(sys.argv[1]).read_text())
for node in ast.walk(tree):
    if isinstance(node, ast.FunctionDef) and node.name == "tool_fingerprint":
        constants = {child.value for child in ast.walk(node) if isinstance(child, ast.Constant) and isinstance(child.value, str)}
        if "hires_pack*.py" not in constants:
            raise SystemExit(1)
        break
else:
    raise SystemExit(1)
PY
then
  echo "FAIL: hts2phrb reuse signature must fingerprint all hires_pack*.py converter helpers." >&2
  exit 1
fi

BOOT_BUNDLE="$TMPDIR/boot-bundle"
mkdir -p "$BOOT_BUNDLE/captures" "$BOOT_BUNDLE/logs"
cp "$CAPTURE_PAYLOAD" "$BOOT_BUNDLE/captures/frame.png"
cat > "$BOOT_BUNDLE/bundle.json" <<EOF
{
  "scenario": "cross-game-hires-boot-validation",
  "mode": "on",
  "game_id": "contract-boot",
  "rom_path": "$ROM_PATH",
  "cache_path": "$CACHE_PATH",
  "hires_pack_sha256": "$(sha256_file "$CACHE_PATH")",
  "core_path": "$CORE_PATH_CONTRACT"
}
EOF
write_hires_log "$BOOT_BUNDLE/logs/retroarch.log" 3 0 3 5 2 3
write_session_env "$BOOT_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$BOOT_COMMAND_SIGNATURE"
write_run_env "$BOOT_BUNDLE"

CORE_PATH="$CORE_PATH_CONTRACT" "$BOOT_VALIDATOR" \
  --reuse \
  --game-id contract-boot \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --bundle-dir "$BOOT_BUNDLE" \
  --min-entries 3 \
  --min-compat-draw-hits 5 \
  --min-ci-attempts 3 \
  --min-ci-hits 2 \
  --expected-entry-class compat-only >/dev/null

assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["passed"] is True' \
  "boot validator should pass the semantic evidence bundle"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["source_mode_match"] is True' \
  "boot validator should enforce source_mode"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["entry_class_match"] is True' \
  "boot validator should enforce entry_class"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["compat_descriptor_paths_only"] is True' \
  "boot validator should enforce compat-only descriptor paths"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["session_rom_path_match"] is True' \
  "boot validator should enforce reused session ROM identity"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["session_core_path_match"] is True' \
  "boot validator should enforce reused session core identity"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["session_rom_sha256_match"] is True' \
  "boot validator should enforce reused session ROM hash identity"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["session_core_sha256_match"] is True' \
  "boot validator should enforce reused session core hash identity"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["session_cache_sha256_match"] is True' \
  "boot validator should enforce reused session cache hash identity"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["cache_sha256_match"] is True' \
  "boot validator should enforce extracted evidence cache hash identity"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["session_command_signature_match"] is True' \
  "boot validator should enforce reused boot command provenance"
assert_json_field "$BOOT_BUNDLE/bundle.json" 'data["scenario"] == "cross-game-hires-boot-validation" and data["mode"] == "on"' \
  "boot validator should emit a bundle manifest"
assert_json_field "$BOOT_BUNDLE/validation-summary.json" 'data["checks"]["manifest_scenario_match"] is True and data["checks"]["manifest_cache_path_match"] is True and data["checks"]["manifest_cache_sha256_match"] is True' \
  "boot validator should enforce reused bundle manifest identity"
if ! rg -q --fixed-strings 'Capture SHA-256 (artifact identity only)' "$BOOT_BUNDLE/validation-summary.md"; then
  echo "FAIL: boot validator markdown should label capture hash as artifact identity only." >&2
  exit 1
fi

cp "$BOOT_BUNDLE/bundle.json" "$TMPDIR/boot-bundle.json.good"
python3 - "$BOOT_BUNDLE/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["scenario"] = "wrong-scenario"
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if CORE_PATH="$CORE_PATH_CONTRACT" "$BOOT_VALIDATOR" \
  --reuse \
  --game-id contract-boot \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --bundle-dir "$BOOT_BUNDLE" >/dev/null 2>&1; then
  echo "FAIL: boot validator accepted reused evidence with mismatched bundle manifest scenario." >&2
  exit 1
fi
cp "$TMPDIR/boot-bundle.json.good" "$BOOT_BUNDLE/bundle.json"
python3 - "$BOOT_BUNDLE/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["hires_pack_sha256"] = "0" * 64
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if CORE_PATH="$CORE_PATH_CONTRACT" "$BOOT_VALIDATOR" \
  --reuse \
  --game-id contract-boot \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --bundle-dir "$BOOT_BUNDLE" >/dev/null 2>&1; then
  echo "FAIL: boot validator accepted reused evidence with mismatched bundle manifest cache SHA-256." >&2
  exit 1
fi
cp "$TMPDIR/boot-bundle.json.good" "$BOOT_BUNDLE/bundle.json"

printf 'other-rom\n' >"$TMPDIR/other-rom.z64"
write_session_env "$BOOT_BUNDLE" "$TMPDIR/other-rom.z64" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$BOOT_COMMAND_SIGNATURE"
if CORE_PATH="$CORE_PATH_CONTRACT" "$BOOT_VALIDATOR" \
  --reuse \
  --game-id contract-boot-wrong-session-rom \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --bundle-dir "$BOOT_BUNDLE" >/dev/null 2>&1; then
  echo "FAIL: boot validator accepted reused evidence with mismatched session ROM path." >&2
  exit 1
fi
write_session_env "$BOOT_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$BOOT_COMMAND_SIGNATURE"
cat > "$BOOT_BUNDLE/retroarch.session.env" <<EOF
ROM_PATH=$ROM_PATH
CORE_PATH=$CORE_PATH_CONTRACT
ROM_SHA256=0000000000000000000000000000000000000000000000000000000000000000
CORE_SHA256=$(sha256_file "$CORE_PATH_CONTRACT")
HIRES_CACHE_PATH=$CACHE_PATH
HIRES_CACHE_SHA256=$(sha256_file "$CACHE_PATH")
MODE=on
COMMAND_SIGNATURE=$BOOT_COMMAND_SIGNATURE
EOF
if CORE_PATH="$CORE_PATH_CONTRACT" "$BOOT_VALIDATOR" \
  --reuse \
  --game-id contract-boot-wrong-session-rom-hash \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --bundle-dir "$BOOT_BUNDLE" >/dev/null 2>&1; then
  echo "FAIL: boot validator accepted reused evidence with mismatched session ROM SHA-256." >&2
  exit 1
fi
write_session_env "$BOOT_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$BOOT_COMMAND_SIGNATURE"

write_session_env "$BOOT_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$OTHER_CACHE_PATH" "$BOOT_COMMAND_SIGNATURE"
if CORE_PATH="$CORE_PATH_CONTRACT" "$BOOT_VALIDATOR" \
  --reuse \
  --game-id contract-boot-wrong-session-cache \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --bundle-dir "$BOOT_BUNDLE" >/dev/null 2>&1; then
  echo "FAIL: boot validator accepted reused evidence with mismatched session cache path." >&2
  exit 1
fi
cat > "$BOOT_BUNDLE/retroarch.session.env" <<EOF
ROM_PATH=$ROM_PATH
CORE_PATH=$CORE_PATH_CONTRACT
ROM_SHA256=$(sha256_file "$ROM_PATH")
CORE_SHA256=$(sha256_file "$CORE_PATH_CONTRACT")
HIRES_CACHE_PATH=$CACHE_PATH
HIRES_CACHE_SHA256=0000000000000000000000000000000000000000000000000000000000000000
MODE=on
COMMAND_SIGNATURE=$BOOT_COMMAND_SIGNATURE
EOF
if CORE_PATH="$CORE_PATH_CONTRACT" "$BOOT_VALIDATOR" \
  --reuse \
  --game-id contract-boot-wrong-session-cache-hash \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --bundle-dir "$BOOT_BUNDLE" >/dev/null 2>&1; then
  echo "FAIL: boot validator accepted reused evidence with mismatched session cache SHA-256." >&2
  exit 1
fi
write_session_env "$BOOT_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$BOOT_COMMAND_SIGNATURE"

MIXED_SOURCE_BUNDLE="$TMPDIR/mixed-source-boot-bundle"
mkdir -p "$MIXED_SOURCE_BUNDLE/captures" "$MIXED_SOURCE_BUNDLE/logs"
cp "$CAPTURE_PAYLOAD" "$MIXED_SOURCE_BUNDLE/captures/frame.png"
cat > "$MIXED_SOURCE_BUNDLE/bundle.json" <<EOF
{
  "scenario": "cross-game-hires-boot-validation",
  "mode": "on",
  "game_id": "contract-mixed-source",
  "rom_path": "$ROM_PATH",
  "cache_path": "$CACHE_PATH",
  "hires_pack_sha256": "$(sha256_file "$CACHE_PATH")",
  "core_path": "$CORE_PATH_CONTRACT"
}
EOF
write_hires_log "$MIXED_SOURCE_BUNDLE/logs/retroarch.log" 3 0 3 5 2 3 0 0 0 5 1 0
write_session_env "$MIXED_SOURCE_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$BOOT_COMMAND_SIGNATURE"
write_run_env "$MIXED_SOURCE_BUNDLE"

if CORE_PATH="$CORE_PATH_CONTRACT" "$BOOT_VALIDATOR" \
  --reuse \
  --game-id contract-mixed-source \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --bundle-dir "$MIXED_SOURCE_BUNDLE" \
  --min-entries 3 \
  --min-compat-draw-hits 5 \
  --min-ci-attempts 3 \
  --min-ci-hits 2 \
  --expected-entry-class compat-only >/dev/null 2>&1; then
  echo "FAIL: boot validator accepted a mixed PHRB/HTS source log as phrb-only." >&2
  exit 1
fi

if CORE_PATH="$CORE_PATH_CONTRACT" "$BOOT_VALIDATOR" \
  --reuse \
  --game-id contract-bad-cache \
  --rom "$ROM_PATH" \
  --cache-path "$BAD_CACHE_PATH" \
  --bundle-dir "$BOOT_BUNDLE" >/dev/null 2>&1; then
  echo "FAIL: boot validator accepted a non-PHRB runtime cache." >&2
  exit 1
fi

FIXTURE_BUNDLE="$TMPDIR/fixture-bundle"
mkdir -p "$FIXTURE_BUNDLE/captures" "$FIXTURE_BUNDLE/logs" "$FIXTURE_BUNDLE/states/ParaLLEl N64"
cp "$CAPTURE_PAYLOAD" "$FIXTURE_BUNDLE/captures/frame.png"
cp "$STATE_PATH" "$FIXTURE_BUNDLE/states/ParaLLEl N64/test-rom.state"
cat > "$FIXTURE_BUNDLE/bundle.json" <<EOF
{
  "scenario": "cross-game-hires-savestate-fixture-validation",
  "mode": "on",
  "game_id": "contract-fixture",
  "rom_path": "$ROM_PATH",
  "cache_path": "$CACHE_PATH",
  "hires_pack_sha256": "$(sha256_file "$CACHE_PATH")",
  "state_path": "$STATE_PATH",
  "core_path": "$CORE_PATH_CONTRACT"
}
EOF
write_fixture_runtime_log "$FIXTURE_BUNDLE/logs/retroarch.log" 4 0 4 7 1 2 "$FIXTURE_BUNDLE/states/ParaLLEl N64/test-rom.state" 3
write_session_env "$FIXTURE_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$FIXTURE_COMMAND_SIGNATURE"
write_run_env "$FIXTURE_BUNDLE"

CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-fixture \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$FIXTURE_BUNDLE" \
  --expected-state-sha256 "$STATE_SHA256" \
  --min-entries 4 \
  --min-compat-draw-hits 7 \
  --min-ci-attempts 2 \
  --min-ci-hits 1 \
  --expected-entry-class compat-only >/dev/null

assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["passed"] is True' \
  "fixture validator should pass the semantic evidence bundle"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["state_sha256_match"] is True' \
  "fixture validator should enforce state identity"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["staged_state_sha256_match"] is True' \
  "fixture validator should enforce staged state identity"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["load_ack_uses_staged_state"] is True' \
  "fixture validator should enforce staged state load acknowledgement"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["post_load_paused_frame_reached"] is True' \
  "fixture validator should enforce post-load paused frame acknowledgement"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["compat_descriptor_paths_only"] is True' \
  "fixture validator should enforce compat-only descriptor paths"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["session_rom_path_match"] is True' \
  "fixture validator should enforce reused session ROM identity"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["session_core_path_match"] is True' \
  "fixture validator should enforce reused session core identity"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["session_rom_sha256_match"] is True' \
  "fixture validator should enforce reused session ROM hash identity"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["session_core_sha256_match"] is True' \
  "fixture validator should enforce reused session core hash identity"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["session_cache_sha256_match"] is True' \
  "fixture validator should enforce reused session cache hash identity"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["cache_sha256_match"] is True' \
  "fixture validator should enforce extracted evidence cache hash identity"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["session_mode_on"] is True' \
  "fixture validator should enforce reused session mode"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["session_command_signature_match"] is True' \
  "fixture validator should enforce reused savestate command provenance"
assert_json_field "$FIXTURE_BUNDLE/bundle.json" 'data["scenario"] == "cross-game-hires-savestate-fixture-validation" and data["mode"] == "on"' \
  "fixture validator should emit a bundle manifest"
assert_json_field "$FIXTURE_BUNDLE/validation-summary.json" 'data["checks"]["manifest_scenario_match"] is True and data["checks"]["manifest_state_path_match"] is True and data["checks"]["manifest_cache_sha256_match"] is True' \
  "fixture validator should enforce reused bundle manifest identity"
if ! rg -q --fixed-strings 'Capture SHA-256 (artifact identity only)' "$FIXTURE_BUNDLE/validation-summary.md"; then
  echo "FAIL: fixture validator markdown should label capture hash as artifact identity only." >&2
  exit 1
fi

cp "$FIXTURE_BUNDLE/bundle.json" "$TMPDIR/fixture-bundle.json.good"
python3 - "$FIXTURE_BUNDLE/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["state_path"] = "/tmp/wrong.state"
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-fixture \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$FIXTURE_BUNDLE" \
  --expected-state-sha256 "$STATE_SHA256" >/dev/null 2>&1; then
  echo "FAIL: fixture validator accepted reused evidence with mismatched bundle manifest state path." >&2
  exit 1
fi
cp "$TMPDIR/fixture-bundle.json.good" "$FIXTURE_BUNDLE/bundle.json"
python3 - "$FIXTURE_BUNDLE/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["hires_pack_sha256"] = "0" * 64
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-fixture \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$FIXTURE_BUNDLE" \
  --expected-state-sha256 "$STATE_SHA256" >/dev/null 2>&1; then
  echo "FAIL: fixture validator accepted reused evidence with mismatched bundle manifest cache SHA-256." >&2
  exit 1
fi
cp "$TMPDIR/fixture-bundle.json.good" "$FIXTURE_BUNDLE/bundle.json"

write_session_env "$FIXTURE_BUNDLE" "$ROM_PATH" "$TMPDIR/other-core.so" "$CACHE_PATH" "$FIXTURE_COMMAND_SIGNATURE"
printf 'other-core\n' >"$TMPDIR/other-core.so"
if CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-fixture-wrong-session-core \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$FIXTURE_BUNDLE" \
  --expected-state-sha256 "$STATE_SHA256" >/dev/null 2>&1; then
  echo "FAIL: fixture validator accepted reused evidence with mismatched session core path." >&2
  exit 1
fi
write_session_env "$FIXTURE_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$FIXTURE_COMMAND_SIGNATURE"

write_session_env "$FIXTURE_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$OTHER_CACHE_PATH" "$FIXTURE_COMMAND_SIGNATURE"
if CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-fixture-wrong-session-cache \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$FIXTURE_BUNDLE" \
  --expected-state-sha256 "$STATE_SHA256" >/dev/null 2>&1; then
  echo "FAIL: fixture validator accepted reused evidence with mismatched session cache path." >&2
  exit 1
fi
cat > "$FIXTURE_BUNDLE/retroarch.session.env" <<EOF
ROM_PATH=$ROM_PATH
CORE_PATH=$CORE_PATH_CONTRACT
ROM_SHA256=$(sha256_file "$ROM_PATH")
CORE_SHA256=$(sha256_file "$CORE_PATH_CONTRACT")
HIRES_CACHE_PATH=$CACHE_PATH
HIRES_CACHE_SHA256=0000000000000000000000000000000000000000000000000000000000000000
MODE=on
COMMAND_SIGNATURE=$FIXTURE_COMMAND_SIGNATURE
EOF
if CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-fixture-wrong-session-cache-hash \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$FIXTURE_BUNDLE" \
  --expected-state-sha256 "$STATE_SHA256" >/dev/null 2>&1; then
  echo "FAIL: fixture validator accepted reused evidence with mismatched session cache SHA-256." >&2
  exit 1
fi
write_session_env "$FIXTURE_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$FIXTURE_COMMAND_SIGNATURE"

BAD_ACK_BUNDLE="$TMPDIR/bad-ack-fixture-bundle"
mkdir -p "$BAD_ACK_BUNDLE/captures" "$BAD_ACK_BUNDLE/logs" "$BAD_ACK_BUNDLE/states/ParaLLEl N64"
cp "$CAPTURE_PAYLOAD" "$BAD_ACK_BUNDLE/captures/frame.png"
cp "$STATE_PATH" "$BAD_ACK_BUNDLE/states/ParaLLEl N64/test-rom.state"
cp "$FIXTURE_BUNDLE/bundle.json" "$BAD_ACK_BUNDLE/bundle.json"
write_fixture_runtime_log "$BAD_ACK_BUNDLE/logs/retroarch.log" 4 0 4 7 1 2 "$TMPDIR/other/ParaLLEl N64/test-rom.state" 3
write_session_env "$BAD_ACK_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$FIXTURE_COMMAND_SIGNATURE"
write_run_env "$BAD_ACK_BUNDLE"

if CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-bad-load-ack \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$BAD_ACK_BUNDLE" \
  --expected-state-sha256 "$STATE_SHA256" \
  --min-entries 4 \
  --min-compat-draw-hits 7 \
  --expected-entry-class compat-only >/dev/null 2>&1; then
  echo "FAIL: fixture validator accepted a same-basename load acknowledgement from another directory." >&2
  exit 1
fi

STALE_STATUS_BUNDLE="$TMPDIR/stale-status-fixture-bundle"
mkdir -p "$STALE_STATUS_BUNDLE/captures" "$STALE_STATUS_BUNDLE/logs" "$STALE_STATUS_BUNDLE/states/ParaLLEl N64"
cp "$CAPTURE_PAYLOAD" "$STALE_STATUS_BUNDLE/captures/frame.png"
cp "$STATE_PATH" "$STALE_STATUS_BUNDLE/states/ParaLLEl N64/test-rom.state"
cp "$FIXTURE_BUNDLE/bundle.json" "$STALE_STATUS_BUNDLE/bundle.json"
{
  printf 'GET_STATUS PAUSED ParaLLEl N64,Contract Game,crc32=00000000,frame=99\n'
  printf '[INFO] [State] Loading state "%s", 10 bytes.\n' "$STALE_STATUS_BUNDLE/states/ParaLLEl N64/test-rom.state"
  printf 'Hi-res replacement cache loaded: 4 entries from %s/package.phrb\n' "$TMPDIR"
  printf 'Hi-res keying summary: lookups=10 hits=2 misses=8 filtered=0 block_probe_hits=0 compat_draw_hits=7 compat_draw_ci_hits=1 compat_draw_ci_attempts=2 provider=on entries=4 native_sampled=0 compat=4 sampled_index=0 sampled_dupe_keys=0 sampled_dupe_entries=0 sampled_families=0 compat_low32_families=4 sources(phrb=4) descriptor_paths(sampled=0 native_checksum=0 generic=0 compat=7) sampled_detail(family_singleton=0 ordered_surface_singleton=0 exact_selector=0) generic_detail(identity_assisted=0 plain=0 native=0 compat=0 unknown=0).\n'
} >"$STALE_STATUS_BUNDLE/logs/retroarch.log"
write_session_env "$STALE_STATUS_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$FIXTURE_COMMAND_SIGNATURE"
write_run_env "$STALE_STATUS_BUNDLE"

if CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-stale-status \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$STALE_STATUS_BUNDLE" \
  --expected-state-sha256 "$STATE_SHA256" \
  --min-entries 4 \
  --min-compat-draw-hits 7 \
  --expected-entry-class compat-only >/dev/null 2>&1; then
  echo "FAIL: fixture validator accepted a stale pre-load paused status as post-load evidence." >&2
  exit 1
fi

BAD_DESCRIPTOR_BUNDLE="$TMPDIR/bad-descriptor-fixture-bundle"
mkdir -p "$BAD_DESCRIPTOR_BUNDLE/captures" "$BAD_DESCRIPTOR_BUNDLE/logs" "$BAD_DESCRIPTOR_BUNDLE/states/ParaLLEl N64"
cp "$CAPTURE_PAYLOAD" "$BAD_DESCRIPTOR_BUNDLE/captures/frame.png"
cp "$STATE_PATH" "$BAD_DESCRIPTOR_BUNDLE/states/ParaLLEl N64/test-rom.state"
cp "$FIXTURE_BUNDLE/bundle.json" "$BAD_DESCRIPTOR_BUNDLE/bundle.json"
write_fixture_runtime_log "$BAD_DESCRIPTOR_BUNDLE/logs/retroarch.log" 4 0 4 7 1 2 "$BAD_DESCRIPTOR_BUNDLE/states/ParaLLEl N64/test-rom.state" 3 1 0 0 6
write_session_env "$BAD_DESCRIPTOR_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$FIXTURE_COMMAND_SIGNATURE"
write_run_env "$BAD_DESCRIPTOR_BUNDLE"

if CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-bad-descriptor \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$BAD_DESCRIPTOR_BUNDLE" \
  --expected-state-sha256 "$STATE_SHA256" \
  --min-entries 4 \
  --min-compat-draw-hits 7 \
  --expected-entry-class compat-only >/dev/null 2>&1; then
  echo "FAIL: fixture validator accepted non-compat descriptor paths." >&2
  exit 1
fi

BAD_STATE_BUNDLE="$TMPDIR/bad-state-fixture-bundle"
mkdir -p "$BAD_STATE_BUNDLE/captures" "$BAD_STATE_BUNDLE/logs" "$BAD_STATE_BUNDLE/states/ParaLLEl N64"
cp "$CAPTURE_PAYLOAD" "$BAD_STATE_BUNDLE/captures/frame.png"
cp "$STATE_PATH" "$BAD_STATE_BUNDLE/states/ParaLLEl N64/test-rom.state"
cp "$FIXTURE_BUNDLE/bundle.json" "$BAD_STATE_BUNDLE/bundle.json"
write_fixture_runtime_log "$BAD_STATE_BUNDLE/logs/retroarch.log" 4 0 4 7 1 2 "$BAD_STATE_BUNDLE/states/ParaLLEl N64/test-rom.state" 3
write_session_env "$BAD_STATE_BUNDLE" "$ROM_PATH" "$CORE_PATH_CONTRACT" "$CACHE_PATH" "$FIXTURE_COMMAND_SIGNATURE"
write_run_env "$BAD_STATE_BUNDLE"

if CORE_PATH="$CORE_PATH_CONTRACT" "$FIXTURE_VALIDATOR" \
  --reuse \
  --game-id contract-bad-state \
  --rom "$ROM_PATH" \
  --cache-path "$CACHE_PATH" \
  --state-path "$STATE_PATH" \
  --bundle-dir "$BAD_STATE_BUNDLE" \
  --expected-state-sha256 "0000000000000000000000000000000000000000000000000000000000000000" \
  --min-entries 4 \
  --min-compat-draw-hits 7 \
  --expected-entry-class compat-only >/dev/null 2>&1; then
  echo "FAIL: fixture validator accepted a mismatched state hash." >&2
  exit 1
fi

echo "emu_cross_game_hires_validators_contract: PASS"

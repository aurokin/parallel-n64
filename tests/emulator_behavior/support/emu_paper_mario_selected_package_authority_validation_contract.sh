#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

CACHE_PATH="$TMP_DIR/package.PHRB"
printf 'phrb' > "$CACHE_PATH"
ROM_PATH="$TMP_DIR/paper-mario.z64"
CORE_PATH="$TMP_DIR/parallel_n64_libretro.so"
BASE_CONFIG_PATH="$TMP_DIR/retroarch.cfg"
printf 'rom' > "$ROM_PATH"
printf 'core' > "$CORE_PATH"
printf 'video_driver = "vulkan"\n' > "$BASE_CONFIG_PATH"
ROM_SHA="$(sha256sum "$ROM_PATH" | awk '{print $1}')"
CORE_SHA="$(sha256sum "$CORE_PATH" | awk '{print $1}')"
BASE_CONFIG_SHA="$(sha256sum "$BASE_CONFIG_PATH" | awk '{print $1}')"
COMMAND_SIGNATURE="$(printf '%s\n' "WAIT_COMMAND_READY 120" "SCREENSHOT" "QUIT" | sha256sum | awk '{print $1}')"
export PAPER_MARIO_EXPECTED_ROM_PATH="$ROM_PATH"

write_fixture_bundle() {
  local root="$1"
  local label="$2"
  local fixture_id="$3"
  local screenshot_sha="$4"
  local init_symbol="$5"
  local step_symbol="$6"
  local entry_count="$7"
  local native_count="$8"
  local phrb_count="$9"

  local bundle_dir="$root/$label"
  mkdir -p "$bundle_dir/traces" "$bundle_dir/logs"
  local cache_sha
  cache_sha="$(sha256sum "$CACHE_PATH" | awk '{print $1}')"
  local append_config="$bundle_dir/retroarch.append.cfg"
  local core_options="$bundle_dir/core-options.opt"
  printf 'core_options_path = "%s"\n' "$core_options" > "$append_config"
  printf 'parallel-n64-parallel-rdp-hirestex = "enabled"\n' > "$core_options"
  local append_config_sha
  local core_options_sha
  append_config_sha="$(sha256sum "$append_config" | awk '{print $1}')"
  core_options_sha="$(sha256sum "$core_options" | awk '{print $1}')"
  cat > "$bundle_dir/bundle.json" <<EOF
{
  "fixture_id": "$fixture_id",
  "mode": "on",
  "hires_pack_path": "$CACHE_PATH",
  "hires_pack_sha256": "$cache_sha",
  "inputs": {
    "rom_path": "$ROM_PATH",
    "rom_sha256": "$ROM_SHA",
    "hires_pack_path": "$CACHE_PATH",
    "hires_pack_sha256": "$cache_sha"
  },
  "fixture_authority": {
    "authority_mode_used": "authoritative"
  },
  "status": {
    "runtime_executed": true
  }
}
EOF
  printf '%s\n' "WAIT_COMMAND_READY 120" "SCREENSHOT" "QUIT" > "$bundle_dir/logs/retroarch.commands.log"
  printf '%s\n' "WAIT_COMMAND_READY 120" "SCREENSHOT" "QUIT" > "$bundle_dir/retroarch.expected.commands.log"
  printf '%s\n' "WAIT_COMMAND_READY 120" "SCREENSHOT" "QUIT" > "$bundle_dir/retroarch.planned.commands.log"
  printf '%s\n' "WAIT_COMMAND_READY 120" "SCREENSHOT" "QUIT" > "$bundle_dir/retroarch.executed.commands.log"
  : > "$bundle_dir/retroarch.command-proofs.log"
  printf '%s\tproof=synthetic-contract\n' "WAIT_COMMAND_READY 120" "SCREENSHOT" "QUIT" > "$bundle_dir/retroarch.command-proofs.log"
  cat > "$bundle_dir/retroarch.session.env" <<EOF
ROM_PATH=$ROM_PATH
CORE_PATH=$CORE_PATH
ROM_SHA256=$ROM_SHA
CORE_SHA256=$CORE_SHA
BASE_CONFIG=$BASE_CONFIG_PATH
BASE_CONFIG_SHA256=$BASE_CONFIG_SHA
APPEND_CONFIG=$append_config
APPEND_CONFIG_SHA256=$append_config_sha
CORE_OPTIONS_FILE=$core_options
CORE_OPTIONS_FILE_SHA256=$core_options_sha
HIRES_CACHE_PATH=$CACHE_PATH
HIRES_CACHE_SHA256=$cache_sha
COMMAND_SIGNATURE=$COMMAND_SIGNATURE
MODE=on
EOF
  cat > "$bundle_dir/retroarch.run.env" <<'EOF'
RUNTIME_EXECUTED=1
RETROARCH_EXIT_STATUS=0
FORCED_TERMINATION=0
EOF
  cat > "$bundle_dir/traces/fixture-verification.json" <<EOF
{
  "fixture_id": "$fixture_id",
  "passed": true,
  "checks": {
    "screenshot_sha256": "$screenshot_sha"
  },
  "actual": {
    "capture_path": "$bundle_dir/captures/capture.png",
    "init_symbol": "$init_symbol",
    "step_symbol": "$step_symbol",
    "hires_summary_provider": "on",
    "hires_summary_source_mode": "phrb-only",
    "hires_summary_entry_count": $entry_count,
    "hires_summary_native_sampled_entry_count": $native_count,
    "hires_summary_source_phrb_count": $phrb_count,
    "hires_exact_hit_count": 12,
    "hires_exact_conflict_miss_count": 3,
    "hires_exact_unresolved_miss_count": 4
  },
  "failures": []
}
EOF
  cat > "$bundle_dir/traces/hires-evidence.json" <<EOF
{
  "cache_path": "$CACHE_PATH",
  "cache_sha256": "$cache_sha",
  "available": true,
  "cache_loaded": true,
  "summary": {
    "provider": "on",
    "source_mode": "phrb-only",
    "entry_count": $entry_count,
    "native_sampled_entry_count": $native_count,
    "compat_entry_count": 0,
    "source_counts": {
      "phrb": $phrb_count,
      "hts": 0,
      "htc": 0
    },
    "descriptor_path_counts": {
      "sampled": 7,
      "native_checksum": 0,
      "generic": 0,
      "compat": 0
    }
  },
  "sampled_object_probe": {
    "available": true,
    "line_count": 3,
    "exact_hit_count": 12,
    "exact_miss_count": 7,
    "exact_conflict_miss_count": 3,
    "exact_unresolved_miss_count": 4
  }
}
EOF
}

write_fixture_bundle "$TMP_DIR/bundles" "title-screen" "paper-mario-title-screen" "titlehash" "state_init_title_screen" "state_step_title_screen" 195 195 10
write_fixture_bundle "$TMP_DIR/bundles" "file-select" "paper-mario-file-select" "filehash" "state_init_file_select" "state_step_file_select" 205 205 20
write_fixture_bundle "$TMP_DIR/bundles" "kmr-03-entry-5" "paper-mario-kmr-03-entry-5" "kmrhash" "state_init_world" "state_step_world" 215 215 30

if bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --reuse \
  --min-native-sampled-count 0 >"$TMP_DIR/selected-package-downgrade.out" 2>"$TMP_DIR/selected-package-downgrade.err"; then
  echo "expected selected-package authority wrapper to reject native-floor override" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "cannot be overridden" "$TMP_DIR/selected-package-downgrade.err"; then
  echo "expected selected-package override rejection" >&2
  cat "$TMP_DIR/selected-package-downgrade.err" >&2
  exit 1
fi

bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --reuse

python3 - "$TMP_DIR/bundles/validation-summary.json" "$TMP_DIR/bundles/validation-summary.md" <<'PY'
import json
import sys
from pathlib import Path

summary_path = Path(sys.argv[1])
md_path = Path(sys.argv[2])
summary = json.loads(summary_path.read_text())
markdown = md_path.read_text()

def check(condition, message):
    if not condition:
        raise SystemExit(message)

check(summary.get("all_passed") is True, f"unexpected pass state: {summary}")
fixtures = summary.get("fixtures") or []
check(len(fixtures) == 3, f"unexpected fixture count: {fixtures}")
check(fixtures[0]["hires_summary"]["source_mode"] == "phrb-only", f"unexpected source mode: {fixtures[0]}")
check(fixtures[2]["sampled_object_probe"]["exact_unresolved_miss_count"] == 4, f"unexpected sampled probe summary: {fixtures[2]}")
check("All passed: `true`" in markdown, f"missing all-passed markdown: {markdown}")
check("source mode `phrb-only`" in markdown, f"missing source mode markdown: {markdown}")
PY

for manifest_case in bad-mode bad-runtime bad-authority bad-probe-available bad-probe-line-count; do
  case_root="$TMP_DIR/$manifest_case-bundles"
  write_fixture_bundle "$case_root" "title-screen" "paper-mario-title-screen" "titlehash" "state_init_title_screen" "state_step_title_screen" 195 195 10
  write_fixture_bundle "$case_root" "file-select" "paper-mario-file-select" "filehash" "state_init_file_select" "state_step_file_select" 205 205 20
  write_fixture_bundle "$case_root" "kmr-03-entry-5" "paper-mario-kmr-03-entry-5" "kmrhash" "state_init_world" "state_step_world" 215 215 30
  python3 - "$case_root/title-screen/bundle.json" "$case_root/title-screen/traces/hires-evidence.json" "$manifest_case" <<'PY'
import json
import sys
from pathlib import Path

bundle_path = Path(sys.argv[1])
evidence_path = Path(sys.argv[2])
case = sys.argv[3]
bundle = json.loads(bundle_path.read_text())
evidence = json.loads(evidence_path.read_text())
if case == "bad-mode":
    bundle["mode"] = "off"
elif case == "bad-runtime":
    bundle["status"]["runtime_executed"] = False
elif case == "bad-authority":
    bundle["fixture_authority"]["authority_mode_used"] = "bootstrap"
elif case == "bad-probe-available":
    evidence["sampled_object_probe"]["available"] = False
elif case == "bad-probe-line-count":
    evidence["sampled_object_probe"]["line_count"] = 0
else:
    raise SystemExit(f"unknown case {case}")
bundle_path.write_text(json.dumps(bundle, indent=2) + "\n")
evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
PY
  if bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
    --cache-path "$CACHE_PATH" \
    --bundle-root "$case_root" \
    --reuse >/dev/null 2>&1; then
    echo "expected selected-package authority reuse to fail for $manifest_case" >&2
    exit 1
  fi
done

write_fixture_bundle "$TMP_DIR/low-native-bundles" "title-screen" "paper-mario-title-screen" "titlehash" "state_init_title_screen" "state_step_title_screen" 195 194 10
write_fixture_bundle "$TMP_DIR/low-native-bundles" "file-select" "paper-mario-file-select" "filehash" "state_init_file_select" "state_step_file_select" 205 205 20
write_fixture_bundle "$TMP_DIR/low-native-bundles" "kmr-03-entry-5" "paper-mario-kmr-03-entry-5" "kmrhash" "state_init_world" "state_step_world" 215 215 30

if bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/low-native-bundles" \
  --reuse; then
  echo "expected selected-package authority reuse to fail below the native sampled floor" >&2
  exit 1
fi

python3 - "$TMP_DIR/low-native-bundles/validation-summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text())
if summary.get("all_passed") is not False:
    raise SystemExit(f"expected failed all_passed summary: {summary}")
failures = summary["fixtures"][0].get("failures") or []
if not any("native sampled" in failure for failure in failures):
    raise SystemExit(f"expected native sampled floor failure, got: {failures}")
PY

write_fixture_bundle "$TMP_DIR/missing-evidence-bundles" "title-screen" "paper-mario-title-screen" "titlehash" "state_init_title_screen" "state_step_title_screen" 195 195 10
write_fixture_bundle "$TMP_DIR/missing-evidence-bundles" "file-select" "paper-mario-file-select" "filehash" "state_init_file_select" "state_step_file_select" 205 205 20
write_fixture_bundle "$TMP_DIR/missing-evidence-bundles" "kmr-03-entry-5" "paper-mario-kmr-03-entry-5" "kmrhash" "state_init_world" "state_step_world" 215 215 30
rm "$TMP_DIR/missing-evidence-bundles/title-screen/traces/hires-evidence.json"

if bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/missing-evidence-bundles" \
  --reuse; then
  echo "expected selected-package authority reuse to fail without provider-owned hi-res evidence" >&2
  exit 1
fi

python3 - "$TMP_DIR/missing-evidence-bundles/validation-summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text())
if summary.get("all_passed") is not False:
    raise SystemExit(f"expected missing-evidence all_passed=false summary: {summary}")
failures = summary["fixtures"][0].get("failures") or []
if not any("provider-owned hi-res evidence" in failure for failure in failures):
    raise SystemExit(f"expected missing evidence failure, got: {failures}")
PY

write_fixture_bundle "$TMP_DIR/bad-descriptor-bundles" "title-screen" "paper-mario-title-screen" "titlehash" "state_init_title_screen" "state_step_title_screen" 195 195 10
write_fixture_bundle "$TMP_DIR/bad-descriptor-bundles" "file-select" "paper-mario-file-select" "filehash" "state_init_file_select" "state_step_file_select" 205 205 20
write_fixture_bundle "$TMP_DIR/bad-descriptor-bundles" "kmr-03-entry-5" "paper-mario-kmr-03-entry-5" "kmrhash" "state_init_world" "state_step_world" 215 215 30
python3 - "$TMP_DIR/bad-descriptor-bundles/title-screen/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["summary"]["descriptor_path_counts"]["native_checksum"] = 1
path.write_text(json.dumps(data, indent=2) + "\n")
PY

if bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bad-descriptor-bundles" \
  --reuse; then
  echo "expected selected-package authority reuse to fail with checksum descriptor traffic" >&2
  exit 1
fi

python3 - "$TMP_DIR/bad-descriptor-bundles/validation-summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text())
failures = summary["fixtures"][0].get("failures") or []
if not any("native_checksum descriptor traffic" in failure for failure in failures):
    raise SystemExit(f"expected native checksum descriptor failure, got: {failures}")
PY

write_fixture_bundle "$TMP_DIR/wrong-cache-bundles" "title-screen" "paper-mario-title-screen" "titlehash" "state_init_title_screen" "state_step_title_screen" 195 195 10
write_fixture_bundle "$TMP_DIR/wrong-cache-bundles" "file-select" "paper-mario-file-select" "filehash" "state_init_file_select" "state_step_file_select" 205 205 20
write_fixture_bundle "$TMP_DIR/wrong-cache-bundles" "kmr-03-entry-5" "paper-mario-kmr-03-entry-5" "kmrhash" "state_init_world" "state_step_world" 215 215 30
cp "$TMP_DIR/wrong-cache-bundles/title-screen/bundle.json" "$TMP_DIR/wrong-cache-title-bundle.good.json"
python3 - "$TMP_DIR/wrong-cache-bundles/title-screen/bundle.json" "$TMP_DIR/wrong-cache-bundles/title-screen/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

bundle_path = Path(sys.argv[1])
evidence_path = Path(sys.argv[2])
bundle = json.loads(bundle_path.read_text())
bundle["hires_pack_sha256"] = "0" * 64
bundle_path.write_text(json.dumps(bundle, indent=2) + "\n")
evidence = json.loads(evidence_path.read_text())
evidence["cache_sha256"] = "0" * 64
evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
PY

if bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/wrong-cache-bundles" \
  --reuse; then
  echo "expected selected-package authority reuse to fail on stale cache provenance" >&2
  exit 1
fi

python3 - "$TMP_DIR/wrong-cache-bundles/validation-summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text())
failures = summary["fixtures"][0].get("failures") or []
if not any("hires_pack_sha256" in failure or "cache_sha256" in failure for failure in failures):
    raise SystemExit(f"expected cache provenance failure, got: {failures}")
PY
cp "$TMP_DIR/wrong-cache-title-bundle.good.json" "$TMP_DIR/wrong-cache-bundles/title-screen/bundle.json"

python3 - "$TMP_DIR/wrong-cache-bundles/title-screen/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

bundle_path = Path(sys.argv[1])
bundle = json.loads(bundle_path.read_text())
bundle["inputs"]["hires_pack_sha256"] = "0" * 64
bundle_path.write_text(json.dumps(bundle, indent=2) + "\n")
PY
if bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/wrong-cache-bundles" \
  --reuse >/dev/null 2>&1; then
  echo "expected selected-package authority reuse to fail on stale input cache provenance" >&2
  exit 1
fi

write_fixture_bundle "$TMP_DIR/wrong-fixture-bundles" "title-screen" "paper-mario-file-select" "titlehash" "state_init_title_screen" "state_step_title_screen" 195 195 10
write_fixture_bundle "$TMP_DIR/wrong-fixture-bundles" "file-select" "paper-mario-file-select" "filehash" "state_init_file_select" "state_step_file_select" 205 205 20
write_fixture_bundle "$TMP_DIR/wrong-fixture-bundles" "kmr-03-entry-5" "paper-mario-kmr-03-entry-5" "kmrhash" "state_init_world" "state_step_world" 215 215 30

if bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/wrong-fixture-bundles" \
  --reuse; then
  echo "expected selected-package authority reuse to fail on swapped fixture provenance" >&2
  exit 1
fi

python3 - "$TMP_DIR/wrong-fixture-bundles/validation-summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text())
failures = summary["fixtures"][0].get("failures") or []
if not any("fixture_id" in failure for failure in failures):
    raise SystemExit(f"expected fixture provenance failure, got: {failures}")
PY

write_fixture_bundle "$TMP_DIR/wrong-rom-bundles" "title-screen" "paper-mario-title-screen" "titlehash" "state_init_title_screen" "state_step_title_screen" 195 195 10
write_fixture_bundle "$TMP_DIR/wrong-rom-bundles" "file-select" "paper-mario-file-select" "filehash" "state_init_file_select" "state_step_file_select" 205 205 20
write_fixture_bundle "$TMP_DIR/wrong-rom-bundles" "kmr-03-entry-5" "paper-mario-kmr-03-entry-5" "kmrhash" "state_init_world" "state_step_world" 215 215 30
OTHER_ROM="$TMP_DIR/other-paper-mario.z64"
printf 'other-rom' > "$OTHER_ROM"
OTHER_ROM_SHA="$(sha256sum "$OTHER_ROM" | awk '{print $1}')"
python3 - "$TMP_DIR/wrong-rom-bundles/title-screen/bundle.json" "$TMP_DIR/wrong-rom-bundles/title-screen/retroarch.session.env" "$OTHER_ROM" "$OTHER_ROM_SHA" <<'PY'
import json
import sys
from pathlib import Path

bundle_path = Path(sys.argv[1])
session_path = Path(sys.argv[2])
other_rom = sys.argv[3]
other_sha = sys.argv[4]
bundle = json.loads(bundle_path.read_text())
bundle["inputs"]["rom_path"] = other_rom
bundle["inputs"]["rom_sha256"] = other_sha
bundle_path.write_text(json.dumps(bundle, indent=2) + "\n")
lines = []
for line in session_path.read_text().splitlines():
    if line.startswith("ROM_PATH="):
        lines.append(f"ROM_PATH={other_rom}")
    elif line.startswith("ROM_SHA256="):
        lines.append(f"ROM_SHA256={other_sha}")
    else:
        lines.append(line)
session_path.write_text("\n".join(lines) + "\n")
PY
if bash "$REPO_ROOT/tools/scenarios/paper-mario-selected-package-authority-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/wrong-rom-bundles" \
  --reuse; then
  echo "expected selected-package authority reuse to fail on wrong ROM provenance" >&2
  exit 1
fi
python3 - "$TMP_DIR/wrong-rom-bundles/validation-summary.json" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text())
failures = summary["fixtures"][0].get("failures") or []
if not any("ROM path" in failure or "ROM SHA" in failure for failure in failures):
    raise SystemExit(f"expected ROM identity failure, got: {failures}")
PY

echo "emu_paper_mario_selected_package_authority_validation_contract: PASS"

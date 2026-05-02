#!/usr/bin/env bash

hts2phrb_write_synthetic_runtime_provenance() {
  local bundle_dir="$1"
  local cache_path="$2"
  local fixture_id="${3:-synthetic-hts2phrb-fixture}"
  local evidence_path="$bundle_dir/traces/hires-evidence.json"
  local rom_path="$bundle_dir/synthetic-rom.z64"
  local core_path="$bundle_dir/synthetic-core.so"
  local base_config_path="$bundle_dir/synthetic-retroarch.cfg"
  local append_config_path="$bundle_dir/retroarch.append.cfg"
  local core_options_path="$bundle_dir/core-options.opt"
  local commands_text=$'WAIT_COMMAND_READY 120\nSCREENSHOT\nQUIT\n'
  local validation_summary_path="$bundle_dir/../validation-summary.json"
  local validation_summary_md_path="$bundle_dir/../validation-summary.md"
  local cache_sha=""
  local rom_sha=""
  local core_sha=""
  local base_config_sha=""
  local append_config_sha=""
  local core_options_sha=""
  local command_signature=""

  mkdir -p "$bundle_dir/traces" "$bundle_dir/logs"
  if [[ ! -f "$rom_path" ]]; then
    printf '%s\n' "synthetic-rom:$fixture_id" > "$rom_path"
  fi
  if [[ ! -f "$core_path" ]]; then
    printf '%s\n' "synthetic-core:$fixture_id" > "$core_path"
  fi
  printf '%s\n' "video_driver = \"vulkan\"" > "$base_config_path"
  printf '%s\n' "core_options_path = \"$core_options_path\"" > "$append_config_path"
  printf '%s\n' 'parallel-n64-parallel-rdp-hirestex = "enabled"' > "$core_options_path"

  cache_sha="$(sha256sum "$cache_path" | awk '{print $1}')"
  rom_sha="$(sha256sum "$rom_path" | awk '{print $1}')"
  core_sha="$(sha256sum "$core_path" | awk '{print $1}')"
  base_config_sha="$(sha256sum "$base_config_path" | awk '{print $1}')"
  append_config_sha="$(sha256sum "$append_config_path" | awk '{print $1}')"
  core_options_sha="$(sha256sum "$core_options_path" | awk '{print $1}')"
  command_signature="$(printf '%s' "$commands_text" | sha256sum | awk '{print $1}')"

  cat > "$bundle_dir/bundle.json" <<EOF
{
  "fixture_id": "$fixture_id",
  "inputs": {
    "rom_path": "$rom_path",
    "rom_sha256": "$rom_sha",
    "hires_pack_path": "$cache_path",
    "hires_pack_sha256": "$cache_sha"
  },
  "status": {
    "runtime_executed": true
  }
}
EOF

  cat > "$bundle_dir/config.env" <<EOF
FIXTURE_ID=$fixture_id
MODE=on
ROM_PATH=$rom_path
HIRES_PACK_PATH=$cache_path
EOF

  cat > "$bundle_dir/traces/fixture-verification.json" <<EOF
{
  "fixture_id": "$fixture_id",
  "passed": true,
  "failures": []
}
EOF

  printf '%s' "$commands_text" > "$bundle_dir/retroarch.expected.commands.log"
  printf '%s' "$commands_text" > "$bundle_dir/retroarch.planned.commands.log"
  printf '%s' "$commands_text" > "$bundle_dir/retroarch.executed.commands.log"
  printf '%s' "$commands_text" > "$bundle_dir/logs/retroarch.commands.log"
  printf '%s\n' "[parallel-rdp-hires] replacement provenance outcome=hit key=0000000000000000 pcrc=00000000 fmt=2 siz=1 tmem=0x0 line=2 width=2 height=2 formatsize=4 cycle=1cycle" > "$bundle_dir/logs/retroarch.log"
  : > "$bundle_dir/retroarch.command-proofs.log"
  while IFS= read -r command; do
    [[ -z "$command" ]] && continue
    printf '%s\tproof=synthetic-runtime\n' "$command" >> "$bundle_dir/retroarch.command-proofs.log"
  done <<< "$commands_text"

  cat > "$bundle_dir/retroarch.session.env" <<EOF
MODE=on
ROM_PATH=$rom_path
ROM_SHA256=$rom_sha
CORE_PATH=$core_path
CORE_SHA256=$core_sha
BASE_CONFIG=$base_config_path
BASE_CONFIG_SHA256=$base_config_sha
APPEND_CONFIG=$append_config_path
APPEND_CONFIG_SHA256=$append_config_sha
CORE_OPTIONS_FILE=$core_options_path
CORE_OPTIONS_FILE_SHA256=$core_options_sha
HIRES_CACHE_PATH=$cache_path
HIRES_CACHE_SHA256=$cache_sha
COMMAND_SIGNATURE=$command_signature
EOF

  cat > "$bundle_dir/retroarch.run.env" <<'EOF'
RUNTIME_EXECUTED=1
RETROARCH_EXIT_STATUS=0
FORCED_TERMINATION=0
EOF

  python3 - "$evidence_path" "$cache_path" "$cache_sha" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
cache_path = sys.argv[2]
cache_sha = sys.argv[3]
data = json.loads(path.read_text())
data["available"] = True
data["cache_loaded"] = True
data["cache_path"] = cache_path
data["cache_sha256"] = cache_sha
data["log_path"] = str(path.parents[1] / "logs" / "retroarch.log")

summary = data.setdefault("summary", {})
summary.setdefault("provider", "on")
summary.setdefault("source_mode", "phrb-only")
summary.setdefault("entry_count", 1)
summary.setdefault("native_sampled_entry_count", 1)
summary.setdefault("compat_entry_count", 0)
summary.setdefault("source_counts", {"phrb": 1})
summary.setdefault("descriptor_path_counts", {"sampled": 1, "native_checksum": 0, "generic": 0, "compat": 0})

sampled = data.setdefault("sampled_object_probe", {})
sampled.setdefault("available", True)
sampled.setdefault("line_count", 1)
sampled.setdefault("exact_hit_count", 1)
sampled.setdefault("exact_miss_count", 0)
sampled.setdefault("exact_conflict_miss_count", 0)
sampled.setdefault("exact_unresolved_miss_count", 0)
if not sampled.get("top_groups") and not sampled.get("groups"):
    family = {}
    families = (data.get("ci_palette_probe") or {}).get("families") or []
    if families:
        family = families[0]
    low32 = str(family.get("low32") or "00000000")
    fs = str(family.get("fs") or family.get("formatsize") or "0")
    pcrc = str(family.get("pcrc") or "00000000")
    sampled["top_groups"] = [
        {
            "fields": {
                "draw_class": "texrect",
                "cycle": "1cycle",
                "fmt": "2",
                "siz": "1",
                "off": "0",
                "stride": "2",
                "wh": str(family.get("wh") or "2x2"),
                "fs": fs,
                "sampled_low32": low32,
                "sampled_entry_pcrc": pcrc,
                "sampled_sparse_pcrc": pcrc,
                "sampled_entry_count": "1",
                "sampled_used_count": "1",
            },
            "upload_low32s": [{"value": low32}],
            "upload_pcrcs": [{"value": pcrc}],
        }
    ]

path.write_text(json.dumps(data, indent=2) + "\n")
PY

  python3 - "$validation_summary_path" "$validation_summary_md_path" "$bundle_dir" "$fixture_id" "$cache_path" "$cache_sha" <<'PY'
import json
import sys
from pathlib import Path

summary_path = Path(sys.argv[1]).resolve()
summary_md_path = Path(sys.argv[2]).resolve()
bundle_dir = Path(sys.argv[3]).resolve()
fixture_id = sys.argv[4]
cache_path = sys.argv[5]
cache_sha = sys.argv[6]
try:
    bundle_ref = str(bundle_dir.relative_to(summary_path.parent))
except ValueError:
    bundle_ref = str(bundle_dir)
summary = {
    "cache_path": cache_path,
    "cache_sha256": cache_sha,
    "passed": True,
    "all_passed": True,
    "steps": [
        {
            "step_frames": 0,
            "passed": True,
            "fixture_id": fixture_id,
            "on_bundle": bundle_ref,
        }
    ],
}
summary_path.write_text(json.dumps(summary, indent=2) + "\n")
summary_md_path.write_text(f"# synthetic validation summary\n\n- fixture: `{fixture_id}`\n")
PY
}

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
DUMP_DIR="${RDP_DUMP_CORPUS_DIR:-$REPO_ROOT/tests/rdp_dumps}"
MANIFEST_PATH="${RDP_DUMP_MANIFEST:-$DUMP_DIR/MANIFEST.txt}"

trim() {
  local value="$1"
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  printf '%s' "$value"
}

if [[ ! -f "$MANIFEST_PATH" ]]; then
  echo "SKIP: dump manifest not found: $MANIFEST_PATH"
  exit 77
fi

required_tags_default="smoke,sync"
if [[ "${RDP_DUMP_STRICT_COMPOSITION:-0}" == "1" ]]; then
  required_tags_default="smoke,sync,tmem,tlut,depth,coverage,rect"
fi
required_tags_csv="${RDP_DUMP_REQUIRED_TAGS:-$required_tags_default}"

entries=0
declare -A seen_tags=()

while IFS='|' read -r raw_path raw_tags raw_modes _; do
  line_path="$(trim "$raw_path")"
  [[ -z "$line_path" || "$line_path" == \#* ]] && continue

  tags_csv="$(trim "${raw_tags:-}")"
  modes_csv="$(trim "${raw_modes:-}")"

  if [[ -z "$tags_csv" || -z "$modes_csv" ]]; then
    echo "FAIL: malformed manifest entry (expected path|tags|modes): $line_path" >&2
    exit 2
  fi

  entries=$((entries + 1))
  dump_file="$DUMP_DIR/$line_path"
  if [[ ! -f "$dump_file" ]]; then
    echo "FAIL: manifest entry references missing dump: $dump_file" >&2
    exit 2
  fi

  has_normal=0
  has_sync_only=0
  IFS=',' read -r -a mode_items <<< "$modes_csv"
  for raw_mode in "${mode_items[@]}"; do
    mode="$(trim "$raw_mode")"
    [[ -z "$mode" ]] && continue
    if [[ "$mode" == "normal" ]]; then
      has_normal=1
    elif [[ "$mode" == "sync-only" ]]; then
      has_sync_only=1
    fi
  done

  if (( has_normal == 0 || has_sync_only == 0 )); then
    echo "FAIL: manifest entry must include both normal and sync-only modes: $line_path" >&2
    exit 2
  fi

  IFS=',' read -r -a tag_items <<< "$tags_csv"
  any_tag=0
  for raw_tag in "${tag_items[@]}"; do
    tag="$(trim "$raw_tag")"
    [[ -z "$tag" ]] && continue
    seen_tags["$tag"]=1
    any_tag=1
  done

  if (( any_tag == 0 )); then
    echo "FAIL: manifest entry must include at least one tag: $line_path" >&2
    exit 2
  fi
done < "$MANIFEST_PATH"

if (( entries == 0 )); then
  echo "FAIL: dump manifest has no entries: $MANIFEST_PATH" >&2
  exit 2
fi

IFS=',' read -r -a required_tag_items <<< "$required_tags_csv"
for raw_required in "${required_tag_items[@]}"; do
  required="$(trim "$raw_required")"
  [[ -z "$required" ]] && continue
  if [[ -z "${seen_tags[$required]:-}" ]]; then
    echo "FAIL: required dump tag missing from manifest coverage: $required" >&2
    exit 2
  fi
done

echo "emu_dump_manifest_check: PASS (entries=$entries, required_tags=$required_tags_csv)"

#!/usr/bin/env bash
# Render BSV2 TAS segments to one continuous video (the "crash-sewing"
# stitcher). Each segment carries its own anchor state (full checkpoint
# embedded at record time), so segments recorded across different — even
# crashed — sessions replay deterministically: each is rendered in a
# fresh session (replay-start restores the anchor, RECORDING_TOGGLE
# captures h264 while the recorded inputs drive the core), then the
# per-segment MKVs are concatenated.
#
# Requires a RetroArch agent-control build with HAVE_FFMPEG=1 (see
# tools/retroarch-patches/README.md).
#
# Usage:
#   render_segments.sh --rom ROM --core CORE --out OUT.mkv \
#       [--retroarch-bin BIN] [--mode off|on] [--keep-work] \
#       SEGMENT.replay [SEGMENT2.replay ...]
set -euo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ADAPTER="$HERE/../adapters/retroarch_interactive_session.sh"

ROM="" CORE="" OUT="" MODE="off" KEEP_WORK=0
SEGMENTS=()
while (($#)); do
  case "$1" in
    --rom) shift; ROM="${1:?}" ;;
    --core) shift; CORE="${1:?}" ;;
    --out) shift; OUT="${1:?}" ;;
    --retroarch-bin) shift; export RETROARCH_BIN="${1:?}" ;;
    --mode) shift; MODE="${1:?}" ;;
    --keep-work) KEEP_WORK=1 ;;
    -*) echo "Unknown option: $1" >&2; exit 2 ;;
    *) SEGMENTS+=("$1") ;;
  esac
  shift
done
[[ -n "$ROM" && -n "$CORE" && -n "$OUT" && ${#SEGMENTS[@]} -ge 1 ]] \
  || { echo "render_segments.sh --rom ROM --core CORE --out OUT.mkv SEG.replay..." >&2; exit 2; }

segment_frames() {
  python3 -c '
import struct, sys
with open(sys.argv[1], "rb") as f:
    words = struct.unpack("<10I", f.read(40))
assert words[0] == 0x42535632, "not a BSV2 replay"
print(words[6])' "$1"
}

WORK="$(mktemp -d "${TMPDIR:-/tmp}/render-segments.XXXXXX")"
cleanup() { (( KEEP_WORK )) || rm -rf "$WORK"; }
trap cleanup EXIT

CONCAT_LIST="$WORK/concat.txt"
: > "$CONCAT_LIST"

i=0
for SEG in "${SEGMENTS[@]}"; do
  i=$((i + 1))
  FRAMES="$(segment_frames "$SEG")"
  BUNDLE="$WORK/bundle-$i"
  mkdir -p "$BUNDLE"
  echo "[render] segment $i/${#SEGMENTS[@]}: $SEG ($FRAMES frames)"

  "$ADAPTER" start --bundle-dir "$BUNDLE" --rom "$ROM" --core "$CORE" \
      --mode "$MODE" --ttl-seconds $(( FRAMES / 30 + 300 )) --start-paused >/dev/null
  # The core's savestate machinery initializes on the first frame; the
  # anchor restore needs it (see patch 0009).
  "$ADAPTER" input --bundle-dir "$BUNDLE" --mask 0x0 --frames 1 >/dev/null
  "$ADAPTER" replay-start --bundle-dir "$BUNDLE" --path "$SEG" >/dev/null
  "$ADAPTER" send --bundle-dir "$BUNDLE" --command "RECORDING_TOGGLE" >/dev/null
  "$ADAPTER" input --bundle-dir "$BUNDLE" --mask 0x0 --frames "$FRAMES" >/dev/null
  "$ADAPTER" send --bundle-dir "$BUNDLE" --command "RECORDING_TOGGLE" >/dev/null
  sleep 2
  "$ADAPTER" stop --bundle-dir "$BUNDLE" >/dev/null

  MKV="$(ls -t "$BUNDLE"/records/*.mkv 2>/dev/null | head -1)"
  [[ -n "$MKV" ]] || { echo "[render] segment $i produced no video" >&2; exit 1; }
  mv "$MKV" "$WORK/seg-$i.mkv"
  printf "file '%s'\n" "$WORK/seg-$i.mkv" >> "$CONCAT_LIST"
done

ffmpeg -v error -f concat -safe 0 -i "$CONCAT_LIST" -c copy "$OUT" -y
echo "[render] wrote $OUT ($(ffprobe -v error -select_streams v -count_frames \
    -show_entries stream=nb_read_frames -of csv=p=0 "$OUT") frames)"

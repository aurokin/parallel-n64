#!/usr/bin/env bash
set -euo pipefail

# Plays the hi-res-ON game intros live on the display, one game at a time,
# via cross-game-hires-boot-validation.sh. Meant for watching: each game
# boots, runs its intro for the listed seconds, screenshots, and quits.
# Evidence bundles land under one grouped directory per invocation.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"

# game_id|rom|pack|seconds
GAMES=(
  "pm64|assets/Paper Mario (USA).zip|artifacts/hts2phrb-review/local-pm64-exact-variant-set/package.phrb|40"
  "sm64|assets/Super Mario 64 (USA).zip|artifacts/hts2phrb-review/local-sm64-zero-config/package.phrb|30"
  "oot|assets/Legend of Zelda, The - Ocarina of Time (USA).zip|artifacts/hts2phrb-review/local-oot-zero-config/package.phrb|40"
  "mk64|assets/Mario Kart 64 (USA).zip|artifacts/hts2phrb-review/local-mk64-zero-config/package.phrb|30"
)

usage() {
  cat <<'USAGE'
Usage:
  tools/scenarios/watch-hires-intros.sh [game-id ...]

Runs the hi-res-ON boot intros live on the display, serially. With no
arguments plays all of: pm64 sm64 oot mk64. Pass game ids to play a subset.

Options:
  --pause N    Seconds to wait between games (default: 3)
  -h, --help   Show this help
USAGE
}

PAUSE=3
SELECTED=()
while (($#)); do
  case "$1" in
    --pause)
      shift
      PAUSE="${1:-3}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      SELECTED+=("$1")
      ;;
  esac
  shift
done

RUN_DIR="$REPO_ROOT/artifacts/experiments/watch-intros-$(date +%y%m%d-%H%M%S)"
declare -a RESULTS

run_game() {
  local game_id="$1" rom="$2" pack="$3" seconds="$4"
  echo
  echo "[watch-intros] === $game_id ($seconds seconds) ==="
  if "$SCRIPT_DIR/cross-game-hires-boot-validation.sh" \
      --game-id "$game_id" \
      --rom "$REPO_ROOT/$rom" \
      --cache-path "$REPO_ROOT/$pack" \
      --seconds "$seconds" \
      --bundle-dir "$RUN_DIR/$game_id"; then
    RESULTS+=("$game_id: PASS")
  else
    RESULTS+=("$game_id: FAIL (bundle: $RUN_DIR/$game_id)")
  fi
}

PLAYED=0
for entry in "${GAMES[@]}"; do
  IFS='|' read -r game_id rom pack seconds <<< "$entry"
  if ((${#SELECTED[@]})); then
    case " ${SELECTED[*]} " in
      *" $game_id "*) ;;
      *) continue ;;
    esac
  fi
  if ((PLAYED)); then
    echo "[watch-intros] next game in ${PAUSE}s..."
    sleep "$PAUSE"
  fi
  run_game "$game_id" "$rom" "$pack" "$seconds"
  PLAYED=1
done

if ((!PLAYED)); then
  echo "No matching game ids. Known: pm64 sm64 oot mk64." >&2
  exit 2
fi

echo
echo "[watch-intros] summary:"
for line in "${RESULTS[@]}"; do
  echo "  $line"
done
echo "[watch-intros] bundles: $RUN_DIR"

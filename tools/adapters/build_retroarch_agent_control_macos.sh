#!/usr/bin/env bash
# Build the agent-control RetroArch binary on macOS (metapod) and stage it
# into the two app bundles the runtime adapters use.
#
# This is the recovered June 2026 build recipe (task #44). The original
# binary (Git 00dc81041d) was built from a dirty tree whose Makefile and
# header fixes were only partially captured by the "capture metapod macOS
# runtime setup" commit; the missing pieces are now committed on the
# agent-control branch, and this script pins the configure environment
# that non-interactive SSH shells do not provide on their own.
#
# Recipe facts recovered from the working June binary (--features) and its
# failure modes:
#   - metal + vulkan + coreaudio3 explicitly enabled (configure does not
#     auto-detect them); metal selects HAVE_COCOA_METAL, which is the only
#     cocoa UI variant that builds outside the Xcode/griffin path
#   - sdl2 and microphone disabled (the June binary has no SDL feature;
#     the SDL mic driver does not compile on macOS)
#   - builtinzlib disabled (deps/libz clashes with the modern macOS SDK)
#   - accessibility and translate disabled (matches the June binary and
#     the HAVE_ACCESSIBILITY gates captured in d8a560738c)
#   - homebrew paths must be pinned: the vulkan loader lives in
#     /opt/homebrew/lib and configure's -lvulkan check fails without it
#   - ffmpeg and freetype disabled so the feature set does not drift with
#     whatever homebrew happens to have installed (the adapters use
#     neither). Known accepted drift vs the June binary: OpenAL is gone
#     (deprecated framework; adapters use CoreAudio).
set -euo pipefail

RETROARCH_SRC="${RETROARCH_SRC:-$HOME/code/RetroArch}"
JOBS="${JOBS:-8}"
DEPLOY="${1:-}"

usage() {
  cat <<'EOF'
Usage:
  build_retroarch_agent_control_macos.sh            # configure + build only
  build_retroarch_agent_control_macos.sh deploy     # build, then stage into
                                                    # /Applications/RetroArch.app and
                                                    # refresh the MVK141 app copy

Environment:
  RETROARCH_SRC   RetroArch checkout (agent-control branch), default ~/code/RetroArch
  JOBS            parallel make jobs, default 8
EOF
}

if [[ "$DEPLOY" == "-h" || "$DEPLOY" == "--help" ]]; then
  usage; exit 0
fi
if [[ -n "$DEPLOY" && "$DEPLOY" != "deploy" ]]; then
  usage >&2; exit 2
fi
if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This build recipe is macOS-only." >&2
  exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"

export PATH="/opt/homebrew/bin:$PATH"
export LDFLAGS="-L/opt/homebrew/lib"
export CPPFLAGS="-I/opt/homebrew/include"
export CFLAGS="-I/opt/homebrew/include"
export CXXFLAGS="-I/opt/homebrew/include"

cd "$RETROARCH_SRC"
echo "[build] $(git rev-parse --short HEAD) on $(git branch --show-current)"

./configure \
  --enable-metal \
  --enable-vulkan \
  --enable-coreaudio3 \
  --disable-vulkan_display \
  --disable-sdl2 \
  --disable-microphone \
  --disable-builtinzlib \
  --disable-accessibility \
  --disable-translate \
  --disable-ffmpeg \
  --disable-freetype

make -j"$JOBS"

# The app bundles carry a stripped binary (June deployment style).
strip -S -o retroarch.stripped retroarch
codesign --force --sign - retroarch.stripped
echo "[build] built $(./retroarch --version 2>&1 | head -1)"
ls -l retroarch retroarch.stripped

if [[ "$DEPLOY" != "deploy" ]]; then
  echo "[build] done (no deploy; pass 'deploy' to stage the app bundles)"
  exit 0
fi

APP_BIN="/Applications/RetroArch.app/Contents/MacOS/RetroArch"
if [[ ! -f "$APP_BIN" ]]; then
  echo "No /Applications/RetroArch.app to stage into." >&2
  exit 1
fi
cp -f retroarch.stripped "$APP_BIN"
codesign --force --deep --sign - /Applications/RetroArch.app
echo "[deploy] staged $APP_BIN"

# Refresh the MoltenVK 1.4.1 app copy from the updated /Applications app.
# Preserve the provenance backup binary across the refresh (the prepare
# script recreates the whole bundle).
MVK_APP="$REPO_ROOT/artifacts/external/RetroArch-MVK141.app"
MVK_BAK=""
if compgen -G "$MVK_APP/Contents/MacOS/RetroArch.bak-*" >/dev/null; then
  MVK_BAK="$(mktemp -d)/baks"
  mkdir -p "$MVK_BAK"
  cp -p "$MVK_APP"/Contents/MacOS/RetroArch.bak-* "$MVK_BAK/"
fi
"$SCRIPT_DIR/prepare_retroarch_mvk141_app.sh" --force
if [[ -n "$MVK_BAK" ]]; then
  cp -p "$MVK_BAK"/RetroArch.bak-* "$MVK_APP/Contents/MacOS/"
fi
echo "[deploy] refreshed $MVK_APP"
"$MVK_APP/Contents/MacOS/RetroArch" --version 2>&1 | head -1

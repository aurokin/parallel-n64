#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  prepare_retroarch_mvk141_app.sh [options]

Creates an isolated RetroArch.app copy with MoltenVK 1.4.1 for macOS Vulkan
runtime testing. The source app is never modified.

Options:
  --source-app PATH       Source RetroArch.app (default: /Applications/RetroArch.app)
  --output-app PATH       Output app copy
                          (default: artifacts/external/RetroArch-MVK141.app)
  --moltenvk-tar PATH     MoltenVK macOS release tar
                          (default: artifacts/external/moltenvk-v1.4.1/MoltenVK-macos.tar)
  --extract-dir PATH      MoltenVK extraction directory
                          (default: artifacts/external/moltenvk-v1.4.1)
  --version VERSION       MoltenVK version to require (default: 1.4.1)
  --download-url URL      Release tar URL
  --no-download           Fail if --moltenvk-tar is missing
  --force                 Replace an existing output app
  -h, --help              Show this help
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"

MOLTENVK_VERSION="1.4.1"
SOURCE_APP="/Applications/RetroArch.app"
OUTPUT_APP="$REPO_ROOT/artifacts/external/RetroArch-MVK141.app"
EXTRACT_DIR="$REPO_ROOT/artifacts/external/moltenvk-v1.4.1"
MOLTENVK_TAR="$EXTRACT_DIR/MoltenVK-macos.tar"
DOWNLOAD_URL="https://github.com/KhronosGroup/MoltenVK/releases/download/v1.4.1/MoltenVK-macos.tar"
ALLOW_DOWNLOAD=1
FORCE=0

while (($#)); do
  case "$1" in
    --source-app)
      shift
      SOURCE_APP="${1:-}"
      ;;
    --output-app)
      shift
      OUTPUT_APP="${1:-}"
      ;;
    --moltenvk-tar)
      shift
      MOLTENVK_TAR="${1:-}"
      ;;
    --extract-dir)
      shift
      EXTRACT_DIR="${1:-}"
      ;;
    --version)
      shift
      MOLTENVK_VERSION="${1:-}"
      DOWNLOAD_URL="https://github.com/KhronosGroup/MoltenVK/releases/download/v${MOLTENVK_VERSION}/MoltenVK-macos.tar"
      ;;
    --download-url)
      shift
      DOWNLOAD_URL="${1:-}"
      ;;
    --no-download)
      ALLOW_DOWNLOAD=0
      ;;
    --force)
      FORCE=1
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

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This setup script is macOS-only." >&2
  exit 1
fi

if [[ ! -d "$SOURCE_APP" || ! -x "$SOURCE_APP/Contents/MacOS/RetroArch" ]]; then
  echo "Source RetroArch app not usable: $SOURCE_APP" >&2
  exit 1
fi

mkdir -p "$(dirname "$MOLTENVK_TAR")" "$EXTRACT_DIR" "$(dirname "$OUTPUT_APP")"

if [[ ! -f "$MOLTENVK_TAR" ]]; then
  if [[ "$ALLOW_DOWNLOAD" != "1" ]]; then
    echo "MoltenVK release tar not found: $MOLTENVK_TAR" >&2
    exit 1
  fi
  echo "[mvk141] downloading $DOWNLOAD_URL"
  curl --fail --location --output "$MOLTENVK_TAR" "$DOWNLOAD_URL"
fi

framework_path() {
  local direct="$EXTRACT_DIR/MoltenVK/MoltenVK/dynamic/MoltenVK.xcframework/macos-arm64_x86_64/MoltenVK.framework"
  if [[ -d "$direct" ]]; then
    echo "$direct"
    return
  fi
  find "$EXTRACT_DIR" -path '*/MoltenVK.xcframework/macos-arm64_x86_64/MoltenVK.framework' -type d -print -quit
}

FRAMEWORK_SRC="$(framework_path)"
if [[ -z "$FRAMEWORK_SRC" ]]; then
  echo "[mvk141] extracting $MOLTENVK_TAR"
  tar -xf "$MOLTENVK_TAR" -C "$EXTRACT_DIR"
  FRAMEWORK_SRC="$(framework_path)"
fi

if [[ -z "$FRAMEWORK_SRC" || ! -d "$FRAMEWORK_SRC" ]]; then
  echo "MoltenVK.framework not found under: $EXTRACT_DIR" >&2
  exit 1
fi

INFO_PLIST="$FRAMEWORK_SRC/Resources/Info.plist"
if [[ ! -f "$INFO_PLIST" ]]; then
  INFO_PLIST="$FRAMEWORK_SRC/Versions/A/Resources/Info.plist"
fi
FRAMEWORK_VERSION="$(plutil -extract CFBundleShortVersionString raw -o - "$INFO_PLIST")"
if [[ "$FRAMEWORK_VERSION" != "$MOLTENVK_VERSION" ]]; then
  echo "MoltenVK version mismatch: expected $MOLTENVK_VERSION, got $FRAMEWORK_VERSION from $INFO_PLIST" >&2
  exit 1
fi

if [[ -e "$OUTPUT_APP" ]]; then
  if [[ "$FORCE" != "1" ]]; then
    echo "Output app already exists: $OUTPUT_APP (use --force to replace it)" >&2
    exit 1
  fi
  rm -rf "$OUTPUT_APP"
fi

echo "[mvk141] copying $SOURCE_APP -> $OUTPUT_APP"
ditto "$SOURCE_APP" "$OUTPUT_APP"

FRAMEWORK_DST="$OUTPUT_APP/Contents/Frameworks/MoltenVK.framework"
rm -rf "$FRAMEWORK_DST"
echo "[mvk141] installing MoltenVK $MOLTENVK_VERSION framework"
ditto "$FRAMEWORK_SRC" "$FRAMEWORK_DST"

OUTPUT_BIN="$OUTPUT_APP/Contents/MacOS/RetroArch"
FRAMEWORK_SHA256="$(shasum -a 256 "$FRAMEWORK_DST/Versions/A/MoltenVK" | awk '{print $1}')"
OUTPUT_BIN_SHA256="$(shasum -a 256 "$OUTPUT_BIN" | awk '{print $1}')"
MANIFEST="$OUTPUT_APP/Contents/Resources/parallel-n64-mvk141-manifest.env"
cat > "$MANIFEST" <<EOF
SOURCE_APP=$SOURCE_APP
OUTPUT_APP=$OUTPUT_APP
MOLTENVK_VERSION=$MOLTENVK_VERSION
MOLTENVK_TAR=$MOLTENVK_TAR
MOLTENVK_FRAMEWORK_SOURCE=$FRAMEWORK_SRC
MOLTENVK_FRAMEWORK_SHA256=$FRAMEWORK_SHA256
RETROARCH_BIN=$OUTPUT_BIN
RETROARCH_BIN_SHA256=$OUTPUT_BIN_SHA256
EOF

echo "[mvk141] ad-hoc signing app copy"
codesign --force --deep --sign - "$OUTPUT_APP" >/dev/null
codesign --verify --deep --strict "$OUTPUT_APP"

if ! "$OUTPUT_BIN" --version >/dev/null 2>&1; then
  echo "Prepared RetroArch binary did not launch with --version: $OUTPUT_BIN" >&2
  exit 1
fi

echo "[mvk141] ready"
echo "RETROARCH_BIN=$OUTPUT_BIN"
echo "MOLTENVK_FRAMEWORK_SHA256=$FRAMEWORK_SHA256"

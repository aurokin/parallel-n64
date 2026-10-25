#!/usr/bin/env bash
# Build the pinned n64-systemtest ROM (thelemmy/nemu64-test, base features) and
# stage it as <dest>/systemtest/n64-systemtest.z64 for run_test_roms.py.
#
# The upstream project publishes no release binaries, so the ROM is built from
# the pinned commit with the pinned toolchain. Source paths embedded in panic
# messages are remapped to fixed prefixes, which makes the output independent
# of the checkout, cargo home and toolchain locations; the result must match
# the sha256 recorded in test_roms.json.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
MANIFEST="$SCRIPT_DIR/test_roms.json"
SOURCE_URL="https://github.com/thelemmy/nemu64-test.git"
COMMIT="9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c"
TOOLCHAIN="nightly-2026-07-16"
NUST64_VERSION="0.4.1"

DEST="$REPO_ROOT/assets/conformance-roms"
WORK=""
INSTALL_TOOLCHAIN=0

usage() {
  cat <<EOF
Usage: $(basename "$0") [--dest DIR] [--work DIR] [--install-toolchain]

  --dest DIR            staging directory (default: assets/conformance-roms)
  --work DIR            build directory to keep (default: a temporary directory, removed afterwards)
  --install-toolchain   install Rust $TOOLCHAIN (minimal profile + rust-src, about 700 MB)
                        and stable if missing
EOF
}

while (($#)); do
  case "$1" in
    --dest) DEST="$2"; shift 2 ;;
    --work) WORK="$2"; shift 2 ;;
    --install-toolchain) INSTALL_TOOLCHAIN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

for tool in git rustup cargo python3 sha256sum; do
  command -v "$tool" >/dev/null || { echo "build_n64_systemtest: $tool is required." >&2; exit 1; }
done

EXPECTED="$(python3 -c 'import json,sys; m=json.load(open(sys.argv[1])); print(next(t["sha256"] for t in m["tests"] if t["suite"]=="systemtest"))' "$MANIFEST")"

# The ROM builds with the pinned nightly; nust64 (which bundles the IPL3) needs
# a stable toolchain, as in the upstream CI.
for toolchain in "$TOOLCHAIN" stable; do
  if ! rustup run "$toolchain" rustc --version >/dev/null 2>&1; then
    install=(rustup toolchain install "$toolchain" --profile minimal)
    [[ "$toolchain" == "$TOOLCHAIN" ]] && install+=(--component rust-src)
    if ((INSTALL_TOOLCHAIN)); then
      "${install[@]}"
    else
      echo "build_n64_systemtest: Rust $toolchain is not installed. Install it with" >&2
      echo "  ${install[*]}" >&2
      echo "or rerun with --install-toolchain." >&2
      exit 1
    fi
  fi
done

if [[ -z "$WORK" ]]; then
  WORK="$(mktemp -d)"
  trap 'rm -rf "$WORK"' EXIT
fi
mkdir -p "$WORK"
# The build below runs from inside the source tree; keep every derived path absolute.
WORK="$(cd -- "$WORK" && pwd)"

if [[ ! -x "$WORK/nust64/bin/nust64" ]]; then
  cargo +stable install nust64 --version "$NUST64_VERSION" --locked --root "$WORK/nust64"
fi

SOURCE="$WORK/nemu64-test"
if [[ ! -d "$SOURCE/.git" ]]; then
  git init -q "$SOURCE"
  git -C "$SOURCE" fetch -q --depth 1 "$SOURCE_URL" "$COMMIT"
fi
git -C "$SOURCE" checkout -q --detach "$COMMIT"

SYSROOT="$(rustup run "$TOOLCHAIN" rustc --print sysroot)"
CARGO_HOME_DIR="${CARGO_HOME:-$HOME/.cargo}"
(
  cd "$SOURCE"
  PATH="$WORK/nust64/bin:$PATH" \
  CARGO_TARGET_DIR="$WORK/target" \
  RUSTFLAGS="-Clinker-plugin-lto -D warnings --remap-path-prefix=$SYSROOT=/rust-sysroot --remap-path-prefix=$CARGO_HOME_DIR=/cargo-home --remap-path-prefix=$SOURCE=/nemu64-test" \
    cargo "+$TOOLCHAIN" run --release
)

ROM="$WORK/target/mips-nintendo64-none/release/n64-systemtest.z64"
ACTUAL="$(sha256sum "$ROM" | cut -d' ' -f1)"
if [[ "$ACTUAL" != "$EXPECTED" ]]; then
  echo "build_n64_systemtest: built ROM sha256 $ACTUAL does not match the pinned $EXPECTED." >&2
  exit 1
fi
mkdir -p "$DEST/systemtest"
cp "$ROM" "$DEST/systemtest/n64-systemtest.z64"
echo "build_n64_systemtest: staged $DEST/systemtest/n64-systemtest.z64 ($ACTUAL)"

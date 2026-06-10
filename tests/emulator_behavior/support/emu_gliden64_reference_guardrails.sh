#!/usr/bin/env bash
# Guardrail: GlideN64 reference captures are for visual content review only.
#
# Attempt A died chasing GlideN64 pixel accuracy. The reference rig is allowed
# back ONLY as a semantic/content oracle, so this lint fails the required gate
# if numeric image comparison creeps into the gate surface:
#   - no image-similarity metric terms (ssim/psnr/mse/phash/...) anywhere in
#     scenario, adapter, or test scripts (the project bans them in gates
#     outright, GlideN64-related or not)
#   - no digest (sha256) logic tied to gliden64 on the same line: pack/ROM
#     identity hashing is fine, digest-gating GlideN64 captures is not
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"

SCAN_PATHS=(
  "tools/scenarios"
  "tools/adapters"
  "tests"
  "run-tests.sh"
  "run-dump-tests.sh"
)

# The lint itself names the banned terms.
ALLOWLIST=(
  "tests/emulator_behavior/support/emu_gliden64_reference_guardrails.sh"
)

METRIC_TERMS='ssim|psnr|\bmse\b|rmse|phash|perceptual_hash|hamming_distance|image_similarity|pixel_match_percent'

is_allowlisted() {
  local rel="$1"
  for allowed in "${ALLOWLIST[@]}"; do
    if [[ "$rel" == "$allowed" ]]; then
      return 0
    fi
  done
  return 1
}

fail=0
while IFS= read -r file; do
  rel="${file#"$REPO_ROOT"/}"
  if is_allowlisted "$rel"; then
    continue
  fi
  if grep -niqE "$METRIC_TERMS" "$file"; then
    echo "FAIL: $rel uses an image-similarity metric term (banned in the gate surface)." >&2
    grep -niE "$METRIC_TERMS" "$file" | head -5 >&2
    fail=1
  fi
  if grep -niqE "gliden64.*sha256|sha256.*gliden64" "$file"; then
    echo "FAIL: $rel couples gliden64 and sha256 on the same line (no digest-gating GlideN64 captures)." >&2
    grep -niE "gliden64.*sha256|sha256.*gliden64" "$file" | head -5 >&2
    fail=1
  fi
done < <(
  for path in "${SCAN_PATHS[@]}"; do
    target="$REPO_ROOT/$path"
    if [[ -d "$target" ]]; then
      find "$target" -type f \( -name '*.sh' -o -name '*.py' -o -name '*.cmake' -o -name 'CMakeLists.txt' \)
    elif [[ -f "$target" ]]; then
      printf '%s\n' "$target"
    fi
  done
)

if (( fail )); then
  echo "GlideN64 references are a visual content oracle, not a numeric target." >&2
  exit 1
fi

echo "OK: gate surface is free of image-similarity metrics and GlideN64 digest gating."

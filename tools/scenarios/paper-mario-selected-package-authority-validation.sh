#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

for arg in "$@"; do
  case "$arg" in
    --expected-source-mode|--min-native-sampled-count|--allow-compat-descriptor-traffic)
      echo "Selected-package authority policy is fixed; $arg cannot be overridden." >&2
      exit 2
      ;;
  esac
done

exec "$SCRIPT_DIR/paper-mario-phrb-authority-validation.sh" \
  "$@" \
  --summary-title "Selected-Package Authority Validation" \
  --expected-source-mode "phrb-only" \
  --min-native-sampled-count 195

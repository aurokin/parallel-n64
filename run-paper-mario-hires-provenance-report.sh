#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPORTER="$SCRIPT_DIR/tools/hires_draw_provenance_report.py"

exec python3 "$REPORTER" "$@"

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
TOOL="$REPO_ROOT/tools/virtual_gamepad.py"

if [[ ! -f "$TOOL" ]]; then
  echo "FAIL: missing virtual gamepad tool at $TOOL" >&2
  exit 1
fi

if [[ ! -x "$TOOL" ]]; then
  echo "FAIL: tool is not executable: $TOOL" >&2
  exit 1
fi

if ! python3 -m py_compile "$TOOL"; then
  echo "FAIL: python syntax validation failed for $TOOL" >&2
  exit 1
fi

require_pattern() {
  local pattern="$1"
  local message="$2"
  if ! rg -n --fixed-strings -- "$pattern" "$TOOL" >/dev/null; then
    echo "FAIL: $message" >&2
    exit 1
  fi
}

require_pattern "def build_parser()" "missing argument parser"
require_pattern "sub.add_parser(\"daemon\"" "missing daemon mode"
require_pattern "sub.add_parser(\"send\"" "missing send mode"
require_pattern "sub.add_parser(\"stop\"" "missing stop mode"
require_pattern "DEFAULT_SOCKET = \"/tmp/parallel-n64-vpad.sock\"" "missing default socket contract"
require_pattern "commands: tap <btn> [hold_ms], down <btn>, up <btn>, " "missing daemon command contract header"
require_pattern "pulse <btn> <count> <interval_ms> [hold_ms], quit" "missing daemon command contract footer"
require_pattern "parallel-n64 Virtual Pad" "missing virtual device name contract"

echo "emu_virtual_gamepad_tool_contract: PASS"

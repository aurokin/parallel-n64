#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd)"
RUNNER="$REPO_ROOT/run-paper-mario-hires-capture.sh"

if [[ ! -f "$RUNNER" ]]; then
  echo "FAIL: missing run-paper-mario-hires-capture.sh at $RUNNER" >&2
  exit 1
fi

require_pattern() {
  local pattern="$1"
  local message="$2"
  if ! rg -n --fixed-strings -- "$pattern" "$RUNNER" >/dev/null; then
    echo "FAIL: $message" >&2
    exit 1
  fi
}

require_pattern "run-paper-mario-hires-capture.sh [options] [-- RUN_N64_ARGS...]" \
  "usage text missing paper mario capture invocation"
require_pattern "--smoke-mode MODE       Capture path: buttons|state|timed (default: buttons)" \
  "usage text missing --smoke-mode"
require_pattern "--screenshot-at SEC     Seconds after launch to send SCREENSHOT (default: 27)" \
  "usage text missing --screenshot-at"
require_pattern "--state-load-delay SEC  Delay before sending state command in state mode (default: 4.0)" \
  "usage text missing --state-load-delay"
require_pattern "--state-pause-delay SEC Delay after state load before PAUSE_TOGGLE (default: 0.2)" \
  "usage text missing --state-pause-delay"
require_pattern "--state-shot-delay SEC  Delay after state load/pause before SCREENSHOT (default: 1.2)" \
  "usage text missing --state-shot-delay"
require_pattern "--state-close-delay SEC Delay after SCREENSHOT before close in state mode (default: 1.0)" \
  "usage text missing --state-close-delay"
require_pattern "--timed-close-delay SEC Delay after SCREENSHOT before close in timed mode (default: 1.0)" \
  "usage text missing --timed-close-delay"
require_pattern "--timed-save-state-at SEC" \
  "usage text missing --timed-save-state-at"
require_pattern "--timed-save-state-cmd CMD" \
  "usage text missing --timed-save-state-cmd"
require_pattern "--state-cmd CMD         Command to send for state load in state mode (default: LOAD_STATE)" \
  "usage text missing --state-cmd"
require_pattern "--state-pause           Send PAUSE_TOGGLE before screenshot in state mode" \
  "usage text missing --state-pause"
require_pattern "--no-state-pause        Skip PAUSE_TOGGLE in state mode (default)" \
  "usage text missing --no-state-pause"
require_pattern "--savestate-dir PATH    Override RetroArch savestate directory for timed/state workflows" \
  "usage text missing --savestate-dir"
require_pattern "--core-option K=V       Override a ParaLLEl core option in the temp options file" \
  "usage text missing --core-option"
require_pattern "--dump-vi-stages CSV    Dump VI stages once under capture_dir/vi-stages" \
  "usage text missing --dump-vi-stages"
require_pattern "--debug-hires           Enable PARALLEL_RDP_HIRES_DEBUG=1 for the run" \
  "usage text missing --debug-hires"
require_pattern "--require-hires         Fail unless the log proves HIRES replacement was active" \
  "usage text missing --require-hires"
require_pattern 'SMOKE_START_RUNNER="$SCRIPT_DIR/run-n64-smoke-start.sh"' \
  "missing button smoke runner path"
require_pattern 'SMOKE_STATE_RUNNER="$SCRIPT_DIR/run-n64-smoke-state.sh"' \
  "missing state smoke runner path"
require_pattern 'RUNNER="$SCRIPT_DIR/run-n64.sh"' \
  "missing direct runner path"
require_pattern 'smoke_mode="buttons"' "default smoke mode missing"
require_pattern 'DEFAULT_CORE_OPTIONS_FILE="$HOME/.config/retroarch/config/ParaLLEl N64/ParaLLEl N64.opt"' \
  "default core options path missing"
require_pattern 'DEFAULT_RETROARCH_CFG="$HOME/.config/retroarch/retroarch.cfg"' \
  "default RetroArch config path missing"
require_pattern 'DEFAULT_SCREENSHOT_DIR="$HOME/.config/retroarch/screenshots"' \
  "default screenshot path missing"
require_pattern 'force_fullscreen="${RUN_N64_FULLSCREEN:-0}"' \
  "default windowed fullscreen policy missing"
require_pattern 'mode="$(xrandr 2>/dev/null | awk ' \
  "desktop resolution helper missing"
require_pattern 'buttons_csv="start"' "default Paper Mario button sequence missing"
require_pattern 'max_presses=2' "default max presses missing"
require_pattern 'screenshot_at=27' "default screenshot timing missing"
require_pattern 'timed_close_delay="1.0"' "default timed close delay missing"
require_pattern 'xdg_root="$capture_dir/xdg"' "temp XDG root missing"
require_pattern 'retroarch_cfg="$xdg_root/retroarch/retroarch.cfg"' \
  "temp RetroArch config path missing"
require_pattern 'core_options_file="$xdg_root/retroarch/core-options.cfg"' \
  "temp core options path missing"
require_pattern 'cp "$DEFAULT_CORE_OPTIONS_FILE" "$core_options_file"' \
  "temp core options copy missing"
require_pattern 'apply_default_hires_core_options' "default hires core option bootstrap missing"
require_pattern 'parallel-n64-parallel-rdp-hirestex" "enabled"' "hires helper must force replacement on by default"
require_pattern 'apply_core_option_overrides' "core option override application missing"
require_pattern 'write_temp_retroarch_cfg()' "temp RetroArch config writer missing"
require_pattern 'global_core_options" "true"' "RetroArch config must force explicit global core options mode"
require_pattern 'core_options_path" "$core_options_file"' "RetroArch config must point at the temp core options file"
require_pattern 'screenshot_directory" "$capture_dir"' "RetroArch config missing screenshot directory override"
require_pattern 'network_cmd_enable" "true"' "RetroArch config missing network command enable"
require_pattern 'savestate_directory" "$save_state_dir"' "savestate directory override missing"
require_pattern 'savestate_auto_index" "false"' "savestate auto index override missing"
require_pattern 'state_slot" "0"' "state slot override missing"
require_pattern 'video_window_custom_size_enable" "true"' "RetroArch config missing window size override"
require_pattern 'video_windowed_position_width" "$window_override_width"' "window width override missing"
require_pattern 'video_windowed_position_height" "$window_override_height"' "window height override missing"
require_pattern 'send_netcmd "SCREENSHOT"' "RetroArch screenshot command missing"
require_pattern 'export RUN_N64_CORE_OPTIONS_FILE="$core_options_file"' \
  "temp core options env export missing"
require_pattern 'export XDG_CONFIG_HOME="$xdg_root"' "temp XDG config export missing"
require_pattern 'export PARALLEL_VI_DUMP_STAGES="$dump_vi_stages"' "VI stage dump env export missing"
require_pattern 'export PARALLEL_VI_DUMP_DIR="$capture_dir/vi-stages"' "VI stage dump directory export missing"
require_pattern 'export PARALLEL_VI_DUMP_TRIGGER_FILE="$dump_vi_trigger_file"' "VI stage dump trigger export missing"
require_pattern 'export PARALLEL_RDP_HIRES_DEBUG=1' "hires debug export missing"
require_pattern 'require_hires="1"' "require-hires option should enable hires validation mode"
require_pattern 'validate_hires_log()' "hires log validation helper missing"
require_pattern "Hi-res keying summary: .*provider=on" "hires validation must require provider=on summary"
require_pattern 'draw_with_replacement' "hires validation must check replacement-bound draws"
require_pattern 'dump_vi_stages="${1:-}"' "VI stage dump option parsing missing"
require_pattern 'timed_save_state_at="${1:-}"' "timed save-state timing parsing missing"
require_pattern 'timed_save_state_cmd="${1:-}"' "timed save-state command parsing missing"
require_pattern 'save_state_dir="${1:-}"' "savestate dir parsing missing"
require_pattern 'core_option_overrides+=("${1:-}")' "core option override parsing missing"
require_pattern 'smoke_cmd+=("--buttons" "$buttons_csv")' "button forwarding missing"
require_pattern 'smoke_cmd+=("--state-cmd" "$state_cmd")' "state command forwarding missing"
require_pattern 'smoke_cmd+=("--load-delay" "$state_load_delay")' "state load delay forwarding missing"
require_pattern 'smoke_cmd+=("--shot-delay" "$state_shot_delay")' "state shot delay forwarding missing"
require_pattern 'smoke_cmd+=("--dump-trigger-file" "$dump_vi_trigger_file")' "state dump trigger forwarding missing"
require_pattern 'smoke_cmd+=("$RUNNER")' "timed mode must launch run-n64.sh directly"
require_pattern 'echo "Smoke mode: timed"' "timed mode logging missing"
require_pattern 'echo "Timed screenshot: +${screenshot_at}s, close +${timed_close_delay}s after shot"' "timed mode timing log missing"
require_pattern 'echo "Smoke mode: state"' "state mode logging missing"
require_pattern 'echo "Smoke mode: buttons"' "buttons mode logging missing"
require_pattern 'smoke_cmd+=(-- --config "$retroarch_cfg")' "RetroArch config forwarding missing"
require_pattern 'if [[ "$smoke_mode" == "buttons" || "$smoke_mode" == "timed" ]]; then' "timed mode must use screenshot timer path"
require_pattern 'float_delay_from_to()' "timed save-state delay helper missing"
require_pattern 'send_netcmd "$timed_save_state_cmd"' "timed save-state netcmd missing"
require_pattern 'sleep "$timed_close_delay"' "timed mode close delay missing"
require_pattern 'find "$DEFAULT_SCREENSHOT_DIR" -maxdepth 1 -type f -name '\''*.png'\'' -newer "$stamp_file"' \
  "default screenshot fallback missing"

echo "emu_run_paper_mario_hires_capture_contract: PASS"

#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

CACHE_PATH="$TMP_DIR/package.PHRB"
printf 'phrb' > "$CACHE_PATH"
ALT_SOURCE_CACHE_PATH="$REPO_ROOT/assets/PAPER MARIO_HIRESTEXTURES.hts"
if [[ ! -f "$ALT_SOURCE_CACHE_PATH" ]]; then
  echo "SKIP: alternate-source cache not found at $ALT_SOURCE_CACHE_PATH"
  exit 77
fi
for wrapper in \
  "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  "$REPO_ROOT/tools/scenarios/paper-mario-phrb-authority-validation.sh" \
  "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  "$REPO_ROOT/tests/emulator_behavior/support/emu_conformance_paper_mario_selected_package_timeout_lookup_without_probe.sh"; do
  for name in \
    PARALLEL_RDP_HIRES_BLOCK_SHAPE_PROBE \
    PARALLEL_RDP_HIRES_CI_COMPAT \
    PARALLEL_RDP_HIRES_CI_LOW32_FALLBACK \
    PARALLEL_RDP_HIRES_CI_PALETTE_PROBE \
    PARALLEL_RDP_HIRES_CI_SELECT \
    PARALLEL_RDP_HIRES_DEBUG \
    PARALLEL_RDP_HIRES_FILTER_ALLOW_BLOCK \
    PARALLEL_RDP_HIRES_FILTER_ALLOW_TILE \
    PARALLEL_RDP_HIRES_FILTER_SIGNATURES \
    PARALLEL_RDP_HIRES_GLIDEN64_COMPAT_CRC \
    PARALLEL_RDP_HIRES_GPU_BUDGET_MB \
    PARALLEL_RDP_HIRES_PHRB_DEBUG \
    PARALLEL_RDP_HIRES_SAMPLED_OBJECT_LOOKUP \
    PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE \
    HIRES_FILTER_ALLOW_TILE \
    HIRES_FILTER_ALLOW_BLOCK \
    HIRES_FILTER_SIGNATURES; do
    if ! rg -q --fixed-strings -- "-u $name" "$wrapper"; then
      echo "FAIL: $wrapper must scrub ambient $name fallback filters." >&2
      exit 1
    fi
  done
done

if ! rg -n --fixed-strings -- 'PARALLEL_RDP_HIRES_SAMPLED_OBJECT_PROBE=1' \
  "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" >/dev/null; then
  echo "FAIL: file-select selected-package validation must enable sampled-object probe for selected runs." >&2
  exit 1
fi

mkdir -p "$TMP_DIR/bundles/on/timeout-960/captures" "$TMP_DIR/bundles/on/timeout-960/traces"
mkdir -p "$TMP_DIR/bundles/off/timeout-960/captures" "$TMP_DIR/bundles/off/timeout-960/traces"
mkdir -p "$TMP_DIR/guards/title/traces" "$TMP_DIR/guards/file/traces"
mkdir -p "$TMP_DIR/history"

python3 - "$TMP_DIR" <<'PY'
import hashlib
import json
import sys
from pathlib import Path
from PIL import Image

tmp_dir = Path(sys.argv[1])
cache_path = tmp_dir / "package.PHRB"
cache_sha256 = hashlib.sha256(cache_path.read_bytes()).hexdigest()
rom_path = tmp_dir / "paper-mario.z64"
core_path = tmp_dir / "parallel_n64_libretro.so"
base_config_path = tmp_dir / "retroarch.cfg"
rom_path.write_bytes(b"rom")
core_path.write_bytes(b"core")
base_config_path.write_text('video_driver = "vulkan"\n')
rom_sha256 = hashlib.sha256(rom_path.read_bytes()).hexdigest()
core_sha256 = hashlib.sha256(core_path.read_bytes()).hexdigest()
base_config_sha256 = hashlib.sha256(base_config_path.read_bytes()).hexdigest()
commands = ["WAIT_COMMAND_READY 120", "SCREENSHOT", "QUIT"]
command_signature = hashlib.sha256(("\n".join(commands) + "\n").encode()).hexdigest()

def write_adapter_provenance(bundle_dir: Path, mode: str, cache_enabled: bool):
    (bundle_dir / "logs").mkdir(parents=True, exist_ok=True)
    append_config = bundle_dir / "retroarch.append.cfg"
    core_options = bundle_dir / "core-options.opt"
    append_config.write_text(f'core_options_path = "{core_options}"\n')
    core_options.write_text(f'parallel-n64-parallel-rdp-hirestex = "{"enabled" if cache_enabled else "disabled"}"\n')
    append_config_sha = hashlib.sha256(append_config.read_bytes()).hexdigest()
    core_options_sha = hashlib.sha256(core_options.read_bytes()).hexdigest()
    (bundle_dir / "logs" / "retroarch.commands.log").write_text("\n".join(commands) + "\n")
    (bundle_dir / "retroarch.expected.commands.log").write_text("\n".join(commands) + "\n")
    (bundle_dir / "retroarch.planned.commands.log").write_text("\n".join(commands) + "\n")
    (bundle_dir / "retroarch.executed.commands.log").write_text("\n".join(commands) + "\n")
    session_cache_path = str(cache_path) if cache_enabled else ""
    session_cache_sha = cache_sha256 if cache_enabled else ""
    (bundle_dir / "retroarch.session.env").write_text(
        "\n".join([
            f"ROM_PATH={rom_path}",
            f"CORE_PATH={core_path}",
            f"ROM_SHA256={rom_sha256}",
            f"CORE_SHA256={core_sha256}",
            f"BASE_CONFIG={base_config_path}",
            f"BASE_CONFIG_SHA256={base_config_sha256}",
            f"APPEND_CONFIG={append_config}",
            f"APPEND_CONFIG_SHA256={append_config_sha}",
            f"CORE_OPTIONS_FILE={core_options}",
            f"CORE_OPTIONS_FILE_SHA256={core_options_sha}",
            f"HIRES_CACHE_PATH={session_cache_path}",
            f"HIRES_CACHE_SHA256={session_cache_sha}",
            f"COMMAND_SIGNATURE={command_signature}",
            f"MODE={mode}",
            "",
        ])
    )
    (bundle_dir / "retroarch.run.env").write_text(
        "RUNTIME_EXECUTED=1\nRETROARCH_EXIT_STATUS=0\nFORCED_TERMINATION=0\n"
    )

for mode in ("on", "off"):
    capture_dir = tmp_dir / "bundles" / mode / "timeout-960" / "captures"
    Image.new("RGBA", (1, 1), (255, 255, 255, 255)).save(capture_dir / f"{mode}.png")

semantic = {
    "paper_mario_us": {
        "game_status": {
            "map_name_candidate": "kmr_03",
            "entry_id": 5,
        },
        "cur_game_mode": {
            "init_symbol": "state_init_world",
            "step_symbol": "state_step_world",
        },
    },
}
(tmp_dir / "bundles" / "on" / "timeout-960" / "traces" / "paper-mario-game-status.json").write_text(
    json.dumps(semantic, indent=2) + "\n"
)
(tmp_dir / "bundles" / "off" / "timeout-960" / "traces" / "paper-mario-game-status.json").write_text(
    json.dumps(semantic, indent=2) + "\n"
)

hires = {
    "cache_path": str(cache_path),
    "cache_sha256": cache_sha256,
    "available": True,
    "cache_loaded": True,
    "summary": {
        "provider": "on",
        "source_mode": "phrb-only",
        "entry_count": 195,
        "native_sampled_entry_count": 195,
        "compat_entry_count": 0,
        "sampled_index_count": 194,
        "sampled_duplicate_key_count": 1,
        "sampled_duplicate_entry_count": 1,
        "sampled_family_count": 10,
        "source_counts": {
            "phrb": 195,
            "hts": 0,
            "htc": 0,
        },
        "descriptor_path_counts": {
            "sampled": 66,
            "native_checksum": 0,
            "generic": 0,
            "compat": 0,
        },
    },
    "sampled_object_probe": {
        "available": True,
        "line_count": 1,
        "groups": [
            {
                "draw_class": "triangle",
                "cycle": "2cycle",
                "tile": "0",
                "fmt": "4",
                "siz": "0",
                "pal": "0",
                "off": "0",
                "stride": "8",
                "wh": "16x32",
                "upload_low32": "de3dac2a",
                "upload_pcrc": "00000000",
                "sampled_low32": "91887078",
                "sampled_entry_pcrc": "00000000",
                "sampled_sparse_pcrc": "00000000",
                "fs": "4",
            }
        ],
        "exact_hit_count": 99,
        "exact_miss_count": 657,
        "exact_conflict_miss_count": 66,
        "exact_unresolved_miss_count": 591,
        "top_exact_hit_buckets": [],
        "top_exact_conflict_miss_buckets": [],
        "top_exact_unresolved_miss_buckets": [
            {
                "count": 12,
                "fields": {
                    "reason": "lookup",
                    "draw_class": "triangle",
                    "cycle": "2cycle",
                    "tile": "0",
                    "sampled_low32": "91887078",
                    "palette_crc": "00000000",
                    "fs": "4",
                    "selector": "00000000de3dac2a",
                    "provider_enabled": "1",
                    "provider_entries": "195"
                },
                "sample_detail": "synthetic"
            }
        ],
    },
    "sampled_duplicate_probe": {
        "available": True,
        "line_count": 1,
        "unique_bucket_count": 1,
        "top_buckets": [
            {
                "fields": {
                    "sampled_low32": "7701ac09",
                    "palette_crc": "00000000",
                    "fs": "768",
                    "selector": "0000000071c71cdd",
                    "total_entries": "2",
                    "policy": "surface-7701ac09",
                    "replacement_id": "legacy-844144ad-00000000-fs0-1600x16"
                }
            }
        ]
    },
    "sampled_pool_stream_probe": {
        "available": True,
        "line_count": 1,
        "family_count": 1,
        "top_families": [
            {
                "fields": {
                    "sampled_low32": "91887078",
                    "palette_crc": "00000000",
                    "fs": "4",
                    "observed_count": "3",
                    "unique_observed_selectors": "1",
                    "transition_count": "0",
                    "max_run": "3",
                    "observed_selector": "00000000de3dac2a",
                    "observed_selector_source": "synthetic"
                }
            }
        ]
    }
}
(tmp_dir / "bundles" / "on" / "timeout-960" / "traces" / "hires-evidence.json").write_text(
    json.dumps(hires, indent=2) + "\n"
)
(tmp_dir / "bundles" / "on" / "timeout-960" / "bundle.json").write_text(json.dumps({
    "fixture_id": "paper-mario-title-timeout-probe",
    "mode": "on",
    "hires_pack_path": str(cache_path),
    "hires_pack_sha256": cache_sha256,
    "probe": {
        "step_frames": 960,
        "step_chunk_frames": 960,
        "authority_fixture_id": "paper-mario-title-screen",
    },
    "inputs": {
        "rom_path": str(rom_path),
        "rom_sha256": rom_sha256,
        "hires_pack_path": str(cache_path),
        "hires_pack_sha256": cache_sha256,
    },
    "status": {"runtime_executed": True},
}, indent=2) + "\n")
off_hires = {
    "available": False,
    "cache_loaded": False,
    "summary": {
        "provider": "off",
        "entry_count": 0,
        "native_sampled_entry_count": 0,
        "compat_entry_count": 0,
    }
}
(tmp_dir / "bundles" / "off" / "timeout-960" / "traces" / "hires-evidence.json").write_text(
    json.dumps(off_hires, indent=2) + "\n"
)
(tmp_dir / "bundles" / "off" / "timeout-960" / "bundle.json").write_text(json.dumps({
    "fixture_id": "paper-mario-title-timeout-probe",
    "mode": "off",
    "hires_pack_path": "",
    "hires_pack_sha256": "missing",
    "probe": {
        "step_frames": 960,
        "step_chunk_frames": 960,
        "authority_fixture_id": "paper-mario-title-screen",
    },
    "inputs": {
        "rom_path": str(rom_path),
        "rom_sha256": rom_sha256,
        "hires_pack_path": "",
        "hires_pack_sha256": "missing",
    },
    "status": {"runtime_executed": True},
}, indent=2) + "\n")
write_adapter_provenance(tmp_dir / "bundles" / "on" / "timeout-960", "on", True)
write_adapter_provenance(tmp_dir / "bundles" / "off" / "timeout-960", "off", False)
(tmp_dir / "bundles" / "on" / "timeout-960" / "traces" / "hires-runtime-seam-register.json").write_text(
    json.dumps({"summary": {"registered_family_count": 1}}, indent=2) + "\n"
)
(tmp_dir / "bundles" / "on" / "timeout-960" / "traces" / "hires-runtime-seam-register.md").write_text(
    "# seam register\n"
)

transport_review = {
    "groups": [
        {
            "signature": {
                "draw_class": "triangle",
                "cycle": "2cycle",
                "sampled_low32": "91887078",
                "sampled_entry_pcrc": "00000000",
                "sampled_sparse_pcrc": "00000000",
                "formatsize": 4,
                "replacement_dims": "0x0"
            },
            "canonical_identity": {
                "draw_class": "triangle",
                "cycle": "2cycle",
                "wh": "16x32",
                "formatsize": 4,
                "sampled_low32": "91887078",
                "sampled_entry_pcrc": "00000000",
                "sampled_sparse_pcrc": "00000000"
            },
            "probe_event_count": 1,
            "transport_candidates": []
        }
    ]
}
(tmp_dir / "transport-review.json").write_text(json.dumps(transport_review, indent=2) + "\n")

guard_hires = {
    "sampled_object_probe": {
        "groups": [
            {
                "draw_class": "triangle",
                "cycle": "2cycle",
                "tile": "0",
                "fmt": "4",
                "siz": "0",
                "pal": "0",
                "off": "0",
                "stride": "8",
                "wh": "16x32",
                "upload_low32": "de3dac2a",
                "upload_pcrc": "00000000",
                "sampled_low32": "91887078",
                "sampled_entry_pcrc": "00000000",
                "sampled_sparse_pcrc": "00000000",
                "fs": "4",
            }
        ],
        "top_exact_family_buckets": [],
        "top_exact_hit_buckets": [],
        "top_exact_unresolved_miss_buckets": [],
    }
}
(tmp_dir / "guards" / "title" / "traces" / "hires-evidence.json").write_text(
    json.dumps(guard_hires, indent=2) + "\n"
)
(tmp_dir / "guards" / "file" / "traces" / "hires-evidence.json").write_text(
    json.dumps(guard_hires, indent=2) + "\n"
)

def write_history(path, *, ae, rmse, hit_rows):
    path.write_text(json.dumps({
        "cache_path": str(path.with_suffix(".phrb")),
        "cache_sha256": path.stem,
        "steps": [
            {
                "step_frames": 960,
                "ae": ae,
                "rmse": rmse,
                "sampled_object_probe": {
                    "exact_hit_count": 1,
                    "exact_conflict_miss_count": 0,
                    "exact_unresolved_miss_count": 0,
                    "top_exact_hit_buckets": [
                        {
                            "count": count,
                            "fields": {
                                "sampled_low32": "1b8530fb",
                                "reason": reason,
                                "key": "52e0d2531b8530fb",
                                "repl": "1184x24",
                            },
                            "sample_detail": f"{reason} x {count}",
                        }
                        for reason, count in hit_rows
                    ],
                },
            }
        ],
    }, indent=2) + "\n")

write_history(tmp_dir / "history" / "flat-summary.json", ae=1659865, rmse=1.3326554039, hit_rows=[("sampled-sparse-exact", 1056)])
write_history(tmp_dir / "history" / "dual-summary.json", ae=34094281, rmse=10.8172496800, hit_rows=[("sampled-sparse-exact", 1056), ("sampled-sparse-ordered-surface", 1056)])
write_history(tmp_dir / "history" / "ordered-summary.json", ae=126937490, rmse=19.8116065828, hit_rows=[("sampled-sparse-ordered-surface", 2112)])
(tmp_dir / "history" / "surface-package.json").write_text(json.dumps({
    "surface_count": 1,
    "surfaces": [
        {
            "canonical_identity": {
                "sampled_low32": "1b8530fb",
            },
            "surface": {
                "sampled_low32": "1b8530fb",
                "shape_hint": "rotating-stream-edge-dwell",
                "slot_count": 34,
                "replacement_ids": ["r0", "r1", "r2"],
                "unresolved_sequences": [
                    {
                        "sequence_index": 33,
                        "upload_key": "77e5f3760b110a9b",
                    }
                ],
            },
        }
    ],
}, indent=2) + "\n")
(tmp_dir / "history" / "package-manifest.json").write_text(json.dumps({
    "records": [
        {
            "policy_key": "surface-7701ac09",
            "canonical_identity": {
                "sampled_low32": "7701ac09",
            },
            "asset_candidates": [
                {
                    "replacement_id": "legacy-844144ad-00000000-fs0-1600x16",
                    "legacy_texture_crc": "844144ad",
                    "selector_checksum64": "0000000071c71cdd",
                    "variant_group_id": "surface-7701ac09-0000000071c71cdd",
                    "materialized_path": "assets/legacy-844144ad-00000000-fs0-1600x16.png",
                    "pixel_sha256": "61827be38596dd7941e48ca97760fb7a3602c704f4dbfab44d1444d06db267a4",
                    "alpha_normalized_pixel_sha256": "f627ca4c2c322f15db26152df306bd4f983f0146409b81a4341b9b340c365a16",
                },
                {
                    "replacement_id": "legacy-e0dc03d0-00000000-fs0-1600x16",
                    "legacy_texture_crc": "e0dc03d0",
                    "selector_checksum64": "0000000071c71cdd",
                    "variant_group_id": "surface-7701ac09-0000000071c71cdd",
                    "materialized_path": "assets/legacy-e0dc03d0-00000000-fs0-1600x16.png",
                    "pixel_sha256": "61827be38596dd7941e48ca97760fb7a3602c704f4dbfab44d1444d06db267a4",
                    "alpha_normalized_pixel_sha256": "f627ca4c2c322f15db26152df306bd4f983f0146409b81a4341b9b340c365a16",
                },
            ],
            "duplicate_pixel_groups": [
                {
                    "alpha_normalized_pixel_sha256": "f627ca4c2c322f15db26152df306bd4f983f0146409b81a4341b9b340c365a16",
                    "replacement_ids": [
                        "legacy-844144ad-00000000-fs0-1600x16",
                        "legacy-e0dc03d0-00000000-fs0-1600x16",
                    ],
                }
            ],
        }
    ]
}, indent=2) + "\n")
PY

PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --transport-review "$TMP_DIR/transport-review.json" \
  --alternate-source-cache "$ALT_SOURCE_CACHE_PATH" \
  --package-manifest "$TMP_DIR/history/package-manifest.json" \
  --pool-regression-flat-summary "$TMP_DIR/history/flat-summary.json" \
  --pool-regression-dual-summary "$TMP_DIR/history/dual-summary.json" \
  --pool-regression-ordered-summary "$TMP_DIR/history/ordered-summary.json" \
  --pool-regression-surface-package "$TMP_DIR/history/surface-package.json" \
  --cross-scene-guard-evidence "title=$TMP_DIR/guards/title/traces/hires-evidence.json" \
  --cross-scene-guard-evidence "file=$TMP_DIR/guards/file/traces/hires-evidence.json" \
  --reuse

python3 - "$TMP_DIR/bundles/validation-summary.json" "$TMP_DIR/bundles/validation-summary.md" <<'PY'
import json
import sys
from pathlib import Path

summary = json.loads(Path(sys.argv[1]).read_text())
markdown = Path(sys.argv[2]).read_text()

steps = summary.get("steps") or []
if len(steps) != 1:
    raise SystemExit(f"FAIL: expected 1 step, found {len(steps)}.")

step = steps[0]
if step.get("passed") is not True or summary.get("passed") is not True:
    raise SystemExit(f"FAIL: expected passed markers on summary and selected step: {summary!r}.")
hires = step.get("hires_summary") or {}
if hires.get("source_mode") != "phrb-only":
    raise SystemExit(f"FAIL: unexpected source mode {hires.get('source_mode')!r}.")
if hires.get("native_sampled_entry_count") != 195:
    raise SystemExit(f"FAIL: unexpected native sampled count {hires.get('native_sampled_entry_count')!r}.")
descriptor_path_counts = step.get("descriptor_path_counts") or {}
if descriptor_path_counts != {"sampled": 66, "native_checksum": 0, "generic": 0, "compat": 0}:
    raise SystemExit(f"FAIL: unexpected descriptor path counts {descriptor_path_counts!r}.")
if step.get("sampled_object_probe", {}).get("exact_conflict_miss_count") != 66:
    raise SystemExit(f"FAIL: unexpected sampled conflict misses in {step!r}.")
if step.get("sampled_duplicate_probe", {}).get("unique_bucket_count") != 1:
    raise SystemExit(f"FAIL: unexpected sampled duplicate bucket count in {step!r}.")
pool_reviews = step.get("sampled_pool_reviews") or []
if pool_reviews:
    if pool_reviews[0].get("runtime_sample_replacement_id") != "legacy-038a968c-9afc43ab-fs0-1184x24":
        raise SystemExit(f"FAIL: unexpected pool runtime replacement id in {pool_reviews[0]!r}.")
alt_review = step.get("alternate_source_review") or {}
if alt_review.get("available_group_count") != 1:
    raise SystemExit(f"FAIL: unexpected alternate-source available count in {alt_review!r}.")
if alt_review.get("total_candidate_count") != 1:
    raise SystemExit(f"FAIL: unexpected alternate-source candidate count in {alt_review!r}.")
activation_review = step.get("alternate_source_activation_review") or {}
activation_summary = activation_review.get("summary") or {}
if activation_summary.get("review_bounded_probe_count") != 0:
    raise SystemExit(f"FAIL: unexpected activation review bounded-probe count in {activation_review!r}.")
if activation_summary.get("shared_scene_blocked_count") != 1:
    raise SystemExit(f"FAIL: unexpected activation review shared-scene blocked count in {activation_review!r}.")
activation_families = activation_review.get("families") or []
if len(activation_families) != 1:
    raise SystemExit(f"FAIL: expected one activation review family, found {activation_families!r}.")
if activation_families[0].get("sampled_low32") != "91887078":
    raise SystemExit(f"FAIL: unexpected activation review family payload {activation_families!r}.")
if activation_families[0].get("activation_status") != "shared-scene-source-backed-candidates":
    raise SystemExit(f"FAIL: unexpected activation review status in {activation_families!r}.")
if activation_families[0].get("activation_recommendation") != "keep-review-only-until-new-runtime-discriminator":
    raise SystemExit(f"FAIL: unexpected activation review recommendation in {activation_families!r}.")
cross_scene_review = step.get("sampled_cross_scene_review") or {}
families = cross_scene_review.get("families") or []
if len(families) != 1:
    raise SystemExit(f"FAIL: expected one cross-scene family, found {families!r}.")
if families[0].get("sampled_low32") != "91887078":
    raise SystemExit(f"FAIL: unexpected cross-scene family payload {families!r}.")
if families[0].get("promotion_status") != "no-runtime-discriminator-observed":
    raise SystemExit(f"FAIL: unexpected cross-scene promotion status in {families!r}.")
seam_register = step.get("runtime_seam_register") or {}
duplicate_families = seam_register.get("sampled_duplicate_families") or []
if duplicate_families:
    if duplicate_families[0].get("replacement_id") != "legacy-844144ad-00000000-fs0-1600x16":
        raise SystemExit(f"FAIL: unexpected duplicate replacement id in seam register {duplicate_families[0]!r}.")
duplicate_reviews = step.get("sampled_duplicate_reviews") or []
if len(duplicate_reviews) != 1:
    raise SystemExit(f"FAIL: expected one sampled duplicate review, found {duplicate_reviews!r}.")
if duplicate_reviews[0].get("recommendation") != "keep-runtime-winner-rule-and-defer-offline-dedupe":
    raise SystemExit(f"FAIL: unexpected sampled duplicate review {duplicate_reviews[0]!r}.")
pool_regression = step.get("sampled_pool_regression_review") or {}
if pool_regression.get("json_path"):
    if pool_regression.get("recommendation") != "keep-flat-runtime-binding":
        raise SystemExit(f"FAIL: unexpected pool regression recommendation in {pool_regression!r}.")
    case_metrics = pool_regression.get("case_metrics") or []
    if [case.get("label") for case in case_metrics] != ["flat", "dual", "ordered-only"]:
        raise SystemExit(f"FAIL: unexpected pool regression cases in {case_metrics!r}.")
if "source mode `phrb-only`" not in markdown:
    raise SystemExit("FAIL: markdown summary missing source mode line.")
if "descriptor paths sampled `66` / native checksum `0` / generic `0` / compat `0`" not in markdown:
    raise SystemExit("FAIL: markdown summary missing descriptor path counts.")
if "Sampled duplicate keys: `1` buckets, `1` log lines" not in markdown:
    raise SystemExit("FAIL: markdown summary missing sampled duplicate line.")
if "Alternate-source review:" not in markdown:
    raise SystemExit("FAIL: markdown summary missing alternate-source review line.")
if "Alternate-source family `91887078`" not in markdown:
    raise SystemExit("FAIL: markdown summary missing alternate-source family detail.")
if "Alternate-source activation review:" not in markdown:
    raise SystemExit("FAIL: markdown summary missing alternate-source activation review line.")
if "Alternate-source activation family `91887078`" not in markdown:
    raise SystemExit("FAIL: markdown summary missing alternate-source activation family detail.")
if "Cross-scene review:" not in markdown:
    raise SystemExit("FAIL: markdown summary missing cross-scene review line.")
if "Cross-scene family `91887078`" not in markdown:
    raise SystemExit("FAIL: markdown summary missing cross-scene family detail.")
if "Sampled duplicate review `7701ac09`" not in markdown:
    raise SystemExit("FAIL: markdown summary missing sampled duplicate review line.")
PY

cp "$TMP_DIR/bundles/on/timeout-960/retroarch.executed.commands.log" "$TMP_DIR/executed.commands.good"
rm "$TMP_DIR/bundles/on/timeout-960/retroarch.executed.commands.log"
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted missing executed command provenance." >&2
  exit 1
fi
cp "$TMP_DIR/executed.commands.good" "$TMP_DIR/bundles/on/timeout-960/retroarch.executed.commands.log"

printf '%s\n' "WAIT_COMMAND_READY 120" "SCREENSHOT" > "$TMP_DIR/bundles/on/timeout-960/retroarch.executed.commands.log"
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted mismatched executed command provenance." >&2
  exit 1
fi
cp "$TMP_DIR/executed.commands.good" "$TMP_DIR/bundles/on/timeout-960/retroarch.executed.commands.log"

cp "$TMP_DIR/bundles/on/timeout-960/traces/paper-mario-game-status.json" "$TMP_DIR/on-semantic.good.json"
python3 - "$TMP_DIR/bundles/on/timeout-960/traces/paper-mario-game-status.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["paper_mario_us"]["game_status"]["semantic_drift"] = True
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted semantic drift on reuse." >&2
  exit 1
fi
cp "$TMP_DIR/on-semantic.good.json" "$TMP_DIR/bundles/on/timeout-960/traces/paper-mario-game-status.json"

cp "$TMP_DIR/bundles/off/timeout-960/bundle.json" "$TMP_DIR/off-bundle.good.json"
python3 - "$TMP_DIR/bundles/off/timeout-960/bundle.json" "$CACHE_PATH" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
cache_path = Path(sys.argv[2])
data = json.loads(path.read_text())
data["inputs"]["hires_pack_path"] = str(cache_path)
data["inputs"]["hires_pack_sha256"] = hashlib.sha256(cache_path.read_bytes()).hexdigest()
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted stale off-bundle cache provenance." >&2
  exit 1
fi
cp "$TMP_DIR/off-bundle.good.json" "$TMP_DIR/bundles/off/timeout-960/bundle.json"

python3 - "$TMP_DIR/bundles/off/timeout-960/bundle.json" "$CACHE_PATH" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
cache_path = Path(sys.argv[2])
data = json.loads(path.read_text())
data["hires_pack_path"] = str(cache_path)
data["hires_pack_sha256"] = hashlib.sha256(cache_path.read_bytes()).hexdigest()
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted top-level stale off-bundle cache provenance." >&2
  exit 1
fi
cp "$TMP_DIR/off-bundle.good.json" "$TMP_DIR/bundles/off/timeout-960/bundle.json"

cp "$TMP_DIR/bundles/on/timeout-960/bundle.json" "$TMP_DIR/on-bundle.probe-good.json"
python3 - "$TMP_DIR/bundles/on/timeout-960/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["probe"]["step_chunk_frames"] = 1
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted mismatched step_chunk_frames." >&2
  exit 1
fi
cp "$TMP_DIR/on-bundle.probe-good.json" "$TMP_DIR/bundles/on/timeout-960/bundle.json"

cp "$TMP_DIR/bundles/on/timeout-960/bundle.json" "$TMP_DIR/on-bundle.rom-good.json"
python3 - "$TMP_DIR/bundles/on/timeout-960/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["inputs"].pop("rom_sha256", None)
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted missing ROM SHA provenance." >&2
  exit 1
fi
cp "$TMP_DIR/on-bundle.rom-good.json" "$TMP_DIR/bundles/on/timeout-960/bundle.json"

python3 - "$TMP_DIR/bundles/on/timeout-960/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["inputs"]["rom_path"] = str(path.parent / "missing-rom.z64")
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted missing ROM artifact." >&2
  exit 1
fi
cp "$TMP_DIR/on-bundle.rom-good.json" "$TMP_DIR/bundles/on/timeout-960/bundle.json"

cp "$TMP_DIR/bundles/on/timeout-960/bundle.json" "$TMP_DIR/on-bundle.cache-good.json"
python3 - "$TMP_DIR/bundles/on/timeout-960/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["inputs"]["hires_pack_sha256"] = "0" * 64
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted stale input cache provenance." >&2
  exit 1
fi
cp "$TMP_DIR/on-bundle.cache-good.json" "$TMP_DIR/bundles/on/timeout-960/bundle.json"

cp "$TMP_DIR/bundles/on/timeout-960/traces/hires-evidence.json" "$TMP_DIR/timeout-hires.good.json"
python3 - "$TMP_DIR/bundles/on/timeout-960/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["summary"]["source_counts"] = {"phrb": 0}
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted missing PHRB source ownership." >&2
  exit 1
fi
cp "$TMP_DIR/timeout-hires.good.json" "$TMP_DIR/bundles/on/timeout-960/traces/hires-evidence.json"

python3 - "$TMP_DIR/bundles/on/timeout-960/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["summary"]["source_counts"] = {"phrb": 1, "hts": 1}
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted mixed source ownership." >&2
  exit 1
fi
cp "$TMP_DIR/timeout-hires.good.json" "$TMP_DIR/bundles/on/timeout-960/traces/hires-evidence.json"

python3 - "$TMP_DIR/bundles/on/timeout-960/bundle.json" "$TMP_DIR/bundles/on/timeout-960/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

bundle_path = Path(sys.argv[1])
evidence_path = Path(sys.argv[2])
bundle = json.loads(bundle_path.read_text())
bundle["hires_pack_sha256"] = "0" * 64
bundle_path.write_text(json.dumps(bundle, indent=2) + "\n")
evidence = json.loads(evidence_path.read_text())
evidence["cache_sha256"] = "0" * 64
evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
PY

if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-title-timeout-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$TMP_DIR/bundles" \
  --steps "960" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: selected-package timeout validation accepted stale cache provenance on reuse." >&2
  exit 1
fi

FS_ROOT="$TMP_DIR/file-select-bundles"
mkdir -p "$FS_ROOT/legacy/captures" "$FS_ROOT/legacy/traces" "$FS_ROOT/legacy/logs"
mkdir -p "$FS_ROOT/selected/captures" "$FS_ROOT/selected/traces" "$FS_ROOT/selected/logs"

python3 - "$TMP_DIR" "$FS_ROOT" <<'PY'
import hashlib
import json
import sys
from pathlib import Path
from PIL import Image

tmp_dir = Path(sys.argv[1])
root = Path(sys.argv[2])
cache_path = tmp_dir / "package.PHRB"
cache_sha = hashlib.sha256(cache_path.read_bytes()).hexdigest()
rom_path = tmp_dir / "paper-mario.z64"
core_path = tmp_dir / "parallel_n64_libretro.so"
rom_sha = hashlib.sha256(rom_path.read_bytes()).hexdigest()
core_sha = hashlib.sha256(core_path.read_bytes()).hexdigest()
commands = ["WAIT_COMMAND_READY 120", "SCREENSHOT", "QUIT"]
commands_text = "\n".join(commands) + "\n"
command_signature = hashlib.sha256(commands_text.encode()).hexdigest()
semantic = {
    "paper_mario_us": {
        "game_status": {"map_name_candidate": "file_select", "entry_id": 0},
        "cur_game_mode": {"init_symbol": "state_init_file_select", "step_symbol": "state_step_file_select"},
    }
}
probe = {
    "label": "selected-package-validation",
    "input_mask": "0x01",
    "input_hold_frames": 1,
    "input_repeat_count": 1,
    "inter_pulse_settle_frames": 5,
    "input_sequence": "",
    "post_input_settle_frames": 20,
    "step_chunk_frames": 1,
}

def write_bundle(bundle_dir: Path, mode: str):
    cache_enabled = mode == "on"
    base_config = bundle_dir / "retroarch.cfg"
    append_config = bundle_dir / "retroarch.append.cfg"
    core_options = bundle_dir / "core-options.opt"
    base_config.write_text('video_driver = "vulkan"\n')
    append_config.write_text(f'core_options_path = "{core_options}"\n')
    core_options.write_text(f'parallel-n64-parallel-rdp-hirestex = "{"enabled" if cache_enabled else "disabled"}"\n')
    base_config_sha = hashlib.sha256(base_config.read_bytes()).hexdigest()
    append_config_sha = hashlib.sha256(append_config.read_bytes()).hexdigest()
    core_options_sha = hashlib.sha256(core_options.read_bytes()).hexdigest()
    Image.new("RGBA", (1, 1), (64, 64, 64, 255)).save(bundle_dir / "captures" / f"{mode}.png")
    (bundle_dir / "traces" / "paper-mario-game-status.json").write_text(json.dumps(semantic, indent=2) + "\n")
    (bundle_dir / "bundle.json").write_text(json.dumps({
        "fixture_id": "paper-mario-file-select-input-probe",
        "mode": mode,
        "probe": probe,
        "inputs": {
            "rom_path": str(rom_path),
            "rom_sha256": rom_sha,
            "hires_pack_path": str(cache_path) if cache_enabled else "",
            "hires_pack_sha256": cache_sha if cache_enabled else "missing",
        },
        "status": {"runtime_executed": True},
    }, indent=2) + "\n")
    for rel in ("logs/retroarch.commands.log", "retroarch.expected.commands.log", "retroarch.planned.commands.log", "retroarch.executed.commands.log"):
        (bundle_dir / rel).write_text(commands_text)
    (bundle_dir / "retroarch.session.env").write_text("\n".join([
        f"ROM_PATH={rom_path}",
        f"CORE_PATH={core_path}",
        f"ROM_SHA256={rom_sha}",
        f"CORE_SHA256={core_sha}",
        f"BASE_CONFIG={base_config}",
        f"BASE_CONFIG_SHA256={base_config_sha}",
        f"APPEND_CONFIG={append_config}",
        f"APPEND_CONFIG_SHA256={append_config_sha}",
        f"CORE_OPTIONS_FILE={core_options}",
        f"CORE_OPTIONS_FILE_SHA256={core_options_sha}",
        f"HIRES_CACHE_PATH={cache_path if cache_enabled else ''}",
        f"HIRES_CACHE_SHA256={cache_sha if cache_enabled else ''}",
        f"COMMAND_SIGNATURE={command_signature}",
        f"MODE={mode}",
        "",
    ]))
    (bundle_dir / "retroarch.run.env").write_text("RUNTIME_EXECUTED=1\nRETROARCH_EXIT_STATUS=0\nFORCED_TERMINATION=0\n")
    if cache_enabled:
        hires = {
            "available": True,
            "cache_loaded": True,
            "cache_path": str(cache_path),
            "cache_sha256": cache_sha,
            "summary": {
                "provider": "on",
                "source_mode": "phrb-only",
                "entry_count": 1,
                "native_sampled_entry_count": 1,
                "source_counts": {"phrb": 1},
                "descriptor_path_counts": {"sampled": 1, "native_checksum": 0, "generic": 0, "compat": 0},
            },
            "sampled_object_probe": {
                "available": True,
                "line_count": 1,
                "exact_hit_count": 1,
                "exact_miss_count": 0,
                "exact_conflict_miss_count": 0,
                "exact_unresolved_miss_count": 0,
            },
            "sampled_duplicate_probe": {
                "available": True,
                "line_count": 1,
                "unique_bucket_count": 1,
            },
            "sampled_pool_stream_probe": {
                "available": True,
                "line_count": 1,
                "family_count": 1,
            },
        }
    else:
        hires = {
            "available": False,
            "cache_loaded": False,
            "cache_path": "",
            "cache_sha256": "",
            "summary": {
                "provider": "off",
                "entry_count": 0,
                "native_sampled_entry_count": 0,
                "compat_entry_count": 0,
            },
        }
    (bundle_dir / "traces" / "hires-evidence.json").write_text(json.dumps(hires, indent=2) + "\n")

write_bundle(root / "legacy", "off")
write_bundle(root / "selected", "on")
PY

PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse

cp "$FS_ROOT/selected/retroarch.executed.commands.log" "$TMP_DIR/file-select-executed.good"
rm "$FS_ROOT/selected/retroarch.executed.commands.log"
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted missing executed command provenance." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-executed.good" "$FS_ROOT/selected/retroarch.executed.commands.log"

printf '%s\n' "WAIT_COMMAND_READY 120" "SCREENSHOT" > "$FS_ROOT/selected/retroarch.executed.commands.log"
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted mismatched executed command provenance." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-executed.good" "$FS_ROOT/selected/retroarch.executed.commands.log"

cp "$FS_ROOT/selected/traces/paper-mario-game-status.json" "$TMP_DIR/file-select-semantic.good.json"
python3 - "$FS_ROOT/selected/traces/paper-mario-game-status.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["paper_mario_us"]["game_status"]["semantic_drift"] = True
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted semantic drift." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-semantic.good.json" "$FS_ROOT/selected/traces/paper-mario-game-status.json"

cp "$FS_ROOT/selected/bundle.json" "$TMP_DIR/file-select-bundle.good.json"
python3 - "$FS_ROOT/selected/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["fixture_id"] = "wrong-fixture"
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted wrong fixture manifest." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-bundle.good.json" "$FS_ROOT/selected/bundle.json"

python3 - "$FS_ROOT/selected/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["inputs"].pop("rom_sha256", None)
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted missing ROM SHA provenance." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-bundle.good.json" "$FS_ROOT/selected/bundle.json"

python3 - "$FS_ROOT/selected/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["inputs"]["rom_path"] = str(path.parent / "missing-rom.z64")
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted missing ROM artifact." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-bundle.good.json" "$FS_ROOT/selected/bundle.json"

cp "$FS_ROOT/legacy/bundle.json" "$TMP_DIR/file-select-legacy-bundle.good.json"
python3 - "$FS_ROOT/legacy/bundle.json" "$CACHE_PATH" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
cache_path = Path(sys.argv[2])
data = json.loads(path.read_text())
data["inputs"]["hires_pack_path"] = str(cache_path)
data["inputs"]["hires_pack_sha256"] = hashlib.sha256(cache_path.read_bytes()).hexdigest()
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted stale legacy cache provenance." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-legacy-bundle.good.json" "$FS_ROOT/legacy/bundle.json"

python3 - "$FS_ROOT/legacy/bundle.json" "$CACHE_PATH" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
cache_path = Path(sys.argv[2])
data = json.loads(path.read_text())
data["hires_pack_path"] = str(cache_path)
data["hires_pack_sha256"] = hashlib.sha256(cache_path.read_bytes()).hexdigest()
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted top-level stale legacy cache provenance." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-legacy-bundle.good.json" "$FS_ROOT/legacy/bundle.json"

if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x02" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted mismatched input contract." >&2
  exit 1
fi

cp "$FS_ROOT/selected/traces/hires-evidence.json" "$TMP_DIR/file-select-hires.good.json"
python3 - "$FS_ROOT/selected/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["summary"]["descriptor_path_counts"]["sampled"] = 0
data["sampled_object_probe"].pop("exact_hit_count", None)
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted fake sampled evidence." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-hires.good.json" "$FS_ROOT/selected/traces/hires-evidence.json"

python3 - "$FS_ROOT/selected/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["summary"]["source_counts"] = {"phrb": 0}
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted missing PHRB source ownership." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-hires.good.json" "$FS_ROOT/selected/traces/hires-evidence.json"

python3 - "$FS_ROOT/selected/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text())
data["summary"]["source_counts"] = {"phrb": 1, "htc": 1}
path.write_text(json.dumps(data, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted mixed source ownership." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-hires.good.json" "$FS_ROOT/selected/traces/hires-evidence.json"

python3 - "$FS_ROOT/selected/bundle.json" "$FS_ROOT/selected/traces/hires-evidence.json" <<'PY'
import json
import sys
from pathlib import Path

bundle_path = Path(sys.argv[1])
evidence_path = Path(sys.argv[2])
bundle = json.loads(bundle_path.read_text())
bundle["inputs"]["hires_pack_sha256"] = "0" * 64
bundle_path.write_text(json.dumps(bundle, indent=2) + "\n")
evidence = json.loads(evidence_path.read_text())
evidence["cache_sha256"] = "0" * 64
evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted stale cache provenance." >&2
  exit 1
fi
cp "$TMP_DIR/file-select-bundle.good.json" "$FS_ROOT/selected/bundle.json"
cp "$TMP_DIR/file-select-hires.good.json" "$FS_ROOT/selected/traces/hires-evidence.json"

python3 - "$FS_ROOT/selected/bundle.json" <<'PY'
import json
import sys
from pathlib import Path

bundle_path = Path(sys.argv[1])
bundle = json.loads(bundle_path.read_text())
bundle["hires_pack_sha256"] = "0" * 64
bundle_path.write_text(json.dumps(bundle, indent=2) + "\n")
PY
if PAPER_MARIO_EXPECTED_ROM_PATH="$TMP_DIR/paper-mario.z64" bash "$REPO_ROOT/tools/scenarios/paper-mario-file-select-selected-package-validation.sh" \
  --cache-path "$CACHE_PATH" \
  --bundle-root "$FS_ROOT" \
  --input-mask "0x01" \
  --reuse >/dev/null 2>&1; then
  echo "FAIL: file-select validation accepted stale top-level cache provenance." >&2
  exit 1
fi

echo "emu_paper_mario_title_timeout_selected_package_validation_contract: PASS"

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
TMP_DIR="$(mktemp -d /tmp/parallel-n64-hts2phrb-context-provenance-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

CACHE_PATH="$TMP_DIR/sample.htc"
BUNDLE_DIR="$TMP_DIR/bundle"
SUMMARY_DIR="$TMP_DIR/summary"
SUMMARY_JSON="$SUMMARY_DIR/validation-summary.json"
OUT_DIR="$TMP_DIR/out"

mkdir -p "$BUNDLE_DIR/traces" "$BUNDLE_DIR/logs" "$SUMMARY_DIR"

python3 - "$CACHE_PATH" "$BUNDLE_DIR/traces/hires-evidence.json" "$BUNDLE_DIR/logs/retroarch.log" "$SUMMARY_JSON" <<'PY'
import gzip
import hashlib
import json
import struct
import sys
from pathlib import Path

cache_path = Path(sys.argv[1])
evidence_path = Path(sys.argv[2])
log_path = Path(sys.argv[3])
summary_path = Path(sys.argv[4])

TXCACHE_FORMAT_VERSION = 0x08000000
GL_RGBA = 0x1908
GL_UNSIGNED_BYTE = 0x1401
GL_RGBA8 = 0x8058

texture_crc = 0x44444444
palette_crc = 0x00000000
checksum64 = (palette_crc << 32) | texture_crc
formatsize = 259
payload = bytes([
    0x10, 0x20, 0x30, 0xFF,
    0x40, 0x50, 0x60, 0xFF,
    0x70, 0x80, 0x90, 0xFF,
    0xA0, 0xB0, 0xC0, 0xFF,
])

with gzip.open(cache_path, "wb") as fp:
    fp.write(struct.pack("<i", TXCACHE_FORMAT_VERSION))
    fp.write(struct.pack("<i", 0))
    fp.write(struct.pack("<Q", checksum64))
    fp.write(struct.pack("<IIIHHB", 16, 8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 1))
    fp.write(struct.pack("<H", formatsize))
    fp.write(struct.pack("<I", len(payload)))
    fp.write(payload)

cache_sha = hashlib.sha256(cache_path.read_bytes()).hexdigest()
rom_path = cache_path.with_name("synthetic-rom.z64")
core_path = cache_path.with_name("synthetic-core.so")
base_config_path = cache_path.with_name("retroarch.cfg")
append_config_path = cache_path.with_name("retroarch.append.cfg")
core_options_path = cache_path.with_name("core-options.opt")
rom_path.write_bytes(b"synthetic-rom")
core_path.write_bytes(b"synthetic-core")
base_config_path.write_text('video_driver = "vulkan"\n')
append_config_path.write_text(f'core_options_path = "{core_options_path}"\n')
core_options_path.write_text('parallel-n64-parallel-rdp-hirestex = "enabled"\n')
rom_sha = hashlib.sha256(rom_path.read_bytes()).hexdigest()
core_sha = hashlib.sha256(core_path.read_bytes()).hexdigest()
base_config_sha = hashlib.sha256(base_config_path.read_bytes()).hexdigest()
append_config_sha = hashlib.sha256(append_config_path.read_bytes()).hexdigest()
core_options_sha = hashlib.sha256(core_options_path.read_bytes()).hexdigest()
commands = ["WAIT_COMMAND_READY 120", "SCREENSHOT", "QUIT"]
commands_text = "\n".join(commands) + "\n"
command_signature = hashlib.sha256(commands_text.encode()).hexdigest()
bundle_dir = evidence_path.parent.parent
(bundle_dir / "bundle.json").write_text(json.dumps({
    "fixture_id": "synthetic-provenance",
    "inputs": {
        "rom_path": str(rom_path),
        "rom_sha256": rom_sha,
        "hires_pack_path": str(cache_path),
        "hires_pack_sha256": cache_sha,
    },
    "status": {"runtime_executed": True},
}, indent=2) + "\n")
(bundle_dir / "traces" / "fixture-verification.json").write_text(json.dumps({
    "fixture_id": "synthetic-provenance",
    "passed": True,
    "failures": [],
}, indent=2) + "\n")
(bundle_dir / "logs").mkdir(exist_ok=True)
(bundle_dir / "config.env").write_text("synthetic=1\n")
(bundle_dir / "retroarch.expected.commands.log").write_text(commands_text)
(bundle_dir / "retroarch.planned.commands.log").write_text(commands_text)
(bundle_dir / "retroarch.executed.commands.log").write_text(commands_text)
(bundle_dir / "logs" / "retroarch.commands.log").write_text(commands_text)
(bundle_dir / "retroarch.session.env").write_text(
    f"MODE=on\nROM_PATH={rom_path}\nROM_SHA256={rom_sha}\nCORE_PATH={core_path}\n"
    f"CORE_SHA256={core_sha}\nBASE_CONFIG={base_config_path}\nBASE_CONFIG_SHA256={base_config_sha}\n"
    f"APPEND_CONFIG={append_config_path}\nAPPEND_CONFIG_SHA256={append_config_sha}\n"
    f"CORE_OPTIONS_FILE={core_options_path}\nCORE_OPTIONS_FILE_SHA256={core_options_sha}\n"
    f"HIRES_CACHE_PATH={cache_path}\nHIRES_CACHE_SHA256={cache_sha}\n"
    f"COMMAND_SIGNATURE={command_signature}\n"
)
(bundle_dir / "retroarch.run.env").write_text(
    "RUNTIME_EXECUTED=1\nRETROARCH_EXIT_STATUS=0\nFORCED_TERMINATION=0\n"
)
evidence = {
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
        "descriptor_path_counts": {
            "sampled": 1,
            "native_checksum": 0,
            "generic": 0,
            "compat": 0,
        },
    },
    "log_path": str(log_path),
    "ci_palette_probe": {
        "families": [
            {
                "low32": f"{texture_crc:08x}",
                "fs": str(formatsize),
                "mode": "loadtile",
                "wh": "16x8",
                "pcrc": "00000000",
                "active_pool": "exact",
            }
        ],
        "usages": [],
        "emulated_tmem": [],
    },
    "sampled_object_probe": {
        "exact_hit_count": 1,
        "exact_miss_count": 0,
        "exact_conflict_miss_count": 0,
        "exact_unresolved_miss_count": 0,
        "top_groups": [
            {
                "fields": {
                    "draw_class": "triangle",
                    "cycle": "1cycle",
                    "fmt": "3",
                    "siz": "1",
                    "off": "2048",
                    "stride": "16",
                    "wh": "16x8",
                    "fs": str(formatsize),
                    "sampled_low32": f"{texture_crc:08x}",
                    "sampled_entry_pcrc": "00000000",
                    "sampled_sparse_pcrc": "00000000",
                    "sampled_entry_count": "1",
                    "sampled_used_count": "1"
                },
                "upload_low32s": [{"value": f"{texture_crc:08x}"}],
                "upload_pcrcs": [{"value": "00000000"}]
            }
        ]
    },
    "provenance": {
        "top_buckets": []
    }
}
evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
summary_path.write_text(json.dumps({
    "summary_title": "provenance context bundle",
    "all_passed": True,
    "fixtures": [
        {"label": "provenance", "fixture_id": "synthetic-provenance", "bundle_dir": "../bundle", "passed": True},
    ],
}, indent=2) + "\n")
log_path.write_text(
    "Hi-res keying provenance: "
    "outcome=hit source_class=authored-rdram provenance_class=loadtile "
    "mode=tile addr=0x24b400 tile=7 fmt=3 siz=1 pal=0 wh=16x8 "
    "key=0000000044444444 pcrc=00000000 fs=259 upload=tile cycle=2cycle copy=0 "
    "tlut=0 tlut_type=0 framebuffer=0 color_fb=0 depth_fb=0 tmem=0x800 line=16 key_xy=0x0\n"
)
PY

python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$OUT_DIR" \
  > "$TMP_DIR/report.json"

python3 - "$TMP_DIR/report.json" "$OUT_DIR/package/package-manifest.json" <<'PY'
import json
import sys
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())
package_manifest = json.loads(Path(sys.argv[2]).read_text())

if report["conversion_outcome"] != "promotable-runtime-package":
    raise SystemExit(f"unexpected conversion outcome: {report['conversion_outcome']!r}")
if not report["runtime_overlay_built"]:
    raise SystemExit(f"expected runtime overlay build from provenance-backed context bundle: {report!r}")
if report["runtime_overlay_reason"] != "runtime-context-available":
    raise SystemExit(f"unexpected runtime overlay reason: {report['runtime_overlay_reason']!r}")
if report.get("binding_count") != 1 or report.get("unresolved_count") != 0:
    raise SystemExit(f"expected one deterministic binding with no unresolved transport cases, got {report!r}")

imported = report.get("imported_index_summary") or {}
if imported.get("exact_authority_count") != 1:
    raise SystemExit(f"unexpected imported exact-authority count: {imported!r}")
if imported.get("canonical_sampled_record_count") != 1:
    raise SystemExit(f"expected one canonical sampled record, got {imported!r}")
if imported.get("canonical_runtime_ready_count") != 1:
    raise SystemExit(f"expected one runtime-ready canonical record, got {imported!r}")

if report["package_manifest_runtime_ready_native_sampled_record_count"] != 1:
    raise SystemExit(
        f"expected one runtime-ready native-sampled record, got {report['package_manifest_runtime_ready_native_sampled_record_count']!r}"
    )
if report["package_manifest_runtime_ready_compat_record_count"] != 0:
    raise SystemExit(
        f"expected zero runtime-ready compat records, got {report['package_manifest_runtime_ready_compat_record_count']!r}"
    )

records = package_manifest.get("records", [])
if len(records) != 1:
    raise SystemExit(f"unexpected package records: {records!r}")
record = records[0]
if record.get("record_kind") != "canonical-sampled":
    raise SystemExit(f"expected canonical-sampled record, got {record.get('record_kind')!r}")
identity = record.get("canonical_identity") or {}
if int(identity.get("fmt") or -1) != 3 or int(identity.get("siz") or -1) != 1:
    raise SystemExit(f"unexpected canonical fmt/siz: {identity!r}")
if int(identity.get("off") or -1) != 2048 or int(identity.get("stride") or -1) != 16:
    raise SystemExit(f"unexpected canonical off/stride: {identity!r}")
if str(identity.get("wh") or "") != "16x8":
    raise SystemExit(f"unexpected canonical wh: {identity!r}")
if int(identity.get("formatsize") or -1) != 259:
    raise SystemExit(f"unexpected canonical formatsize: {identity!r}")
if str(identity.get("sampled_low32") or "").lower() != "44444444":
    raise SystemExit(f"unexpected sampled_low32: {identity!r}")
if not record.get("runtime_ready"):
    raise SystemExit(f"expected runtime_ready canonical record: {record!r}")
PY

cp "$BUNDLE_DIR/traces/hires-evidence.json" "$TMP_DIR/hires-evidence.good.json"
python3 - "$BUNDLE_DIR/traces/hires-evidence.json" "$TMP_DIR/external-retroarch.log" <<'PY'
import json
import sys
from pathlib import Path

evidence_path = Path(sys.argv[1])
external_log = Path(sys.argv[2])
data = json.loads(evidence_path.read_text())
external_log.write_text("Hi-res keying provenance: outcome=hit key=0000000044444444 pcrc=00000000 fmt=3 siz=1 width=16 height=8 formatsize=259 tmem=0x800 line=16 cycle=2cycle\n")
data["log_path"] = str(external_log)
evidence_path.write_text(json.dumps(data, indent=2) + "\n")
PY
if python3 "$ROOT_DIR/tools/hts2phrb.py" \
  --cache "$CACHE_PATH" \
  --context-bundle "$SUMMARY_JSON" \
  --stdout-format json \
  --output-dir "$TMP_DIR/out-external-log" \
  > "$TMP_DIR/external-log.json" \
  2> "$TMP_DIR/external-log.stderr"; then
  echo "expected external provenance log_path to be rejected" >&2
  exit 1
fi
if ! rg -q --fixed-strings -- "log_path must resolve inside the validated bundle" "$TMP_DIR/external-log.stderr"; then
  echo "expected external provenance log rejection" >&2
  cat "$TMP_DIR/external-log.stderr" >&2
  exit 1
fi
cp "$TMP_DIR/hires-evidence.good.json" "$BUNDLE_DIR/traces/hires-evidence.json"

echo "emu_hts2phrb_context_bundle_provenance_hits: PASS"

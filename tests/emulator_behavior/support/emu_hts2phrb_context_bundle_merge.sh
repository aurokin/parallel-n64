#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
TMP_DIR="$(mktemp -d /tmp/parallel-n64-hts2phrb-context-bundles-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

CACHE_PATH="$TMP_DIR/sample.htc"
BUNDLE_A_DIR="$TMP_DIR/bundle-a"
BUNDLE_B_DIR="$TMP_DIR/bundle-b"
SUMMARY_DIR="$TMP_DIR/summary"
SUMMARY_JSON="$SUMMARY_DIR/validation-summary.json"
OUT_DIR="$TMP_DIR/out"

mkdir -p "$BUNDLE_A_DIR/traces" "$BUNDLE_B_DIR/traces" "$SUMMARY_DIR"

python3 - "$CACHE_PATH" "$BUNDLE_A_DIR/traces/hires-evidence.json" "$BUNDLE_B_DIR/traces/hires-evidence.json" "$SUMMARY_JSON" <<'PY'
import gzip
import hashlib
import json
import struct
import sys
from pathlib import Path

cache_path = Path(sys.argv[1])
bundle_a = Path(sys.argv[2])
bundle_b = Path(sys.argv[3])
summary_path = Path(sys.argv[4])

TXCACHE_FORMAT_VERSION = 0x08000000
GL_RGBA = 0x1908
GL_UNSIGNED_BYTE = 0x1401
GL_RGBA8 = 0x8058

records = [
    (0x11111111, 0xAAAABBBB, 258),
    (0x22222222, 0xCCCCDDDD, 514),
]

with gzip.open(cache_path, "wb") as fp:
    fp.write(struct.pack("<i", TXCACHE_FORMAT_VERSION))
    fp.write(struct.pack("<i", 0))
    for texture_crc, palette_crc, formatsize in records:
        checksum64 = (palette_crc << 32) | texture_crc
        payload = bytes([
            0x10, 0x20, 0x30, 0xFF,
            0x40, 0x50, 0x60, 0xFF,
            0x70, 0x80, 0x90, 0xFF,
            0xA0, 0xB0, 0xC0, 0xFF,
        ])
        fp.write(struct.pack("<Q", checksum64))
        fp.write(struct.pack("<IIIHHB", 2, 2, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 1))
        fp.write(struct.pack("<H", formatsize))
        fp.write(struct.pack("<I", len(payload)))
        fp.write(payload)

cache_sha = hashlib.sha256(cache_path.read_bytes()).hexdigest()
rom_path = cache_path.with_name("synthetic-rom.z64")
core_path = cache_path.with_name("synthetic-core.so")
rom_path.write_bytes(b"synthetic-rom")
core_path.write_bytes(b"synthetic-core")
rom_sha = hashlib.sha256(rom_path.read_bytes()).hexdigest()
core_sha = hashlib.sha256(core_path.read_bytes()).hexdigest()
commands = ["WAIT_COMMAND_READY 120", "SCREENSHOT", "QUIT"]
commands_text = "\n".join(commands) + "\n"
command_signature = hashlib.sha256(commands_text.encode()).hexdigest()

def write_bundle_provenance(evidence_path, fixture_id):
    bundle_dir = evidence_path.parent.parent
    (bundle_dir / "bundle.json").write_text(json.dumps({
        "fixture_id": fixture_id,
        "inputs": {
            "rom_path": str(rom_path),
            "rom_sha256": rom_sha,
            "hires_pack_path": str(cache_path),
            "hires_pack_sha256": cache_sha,
        },
        "status": {"runtime_executed": True},
    }, indent=2) + "\n")
    (bundle_dir / "traces" / "fixture-verification.json").write_text(json.dumps({
        "fixture_id": fixture_id,
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
        f"CORE_SHA256={core_sha}\nHIRES_CACHE_PATH={cache_path}\nHIRES_CACHE_SHA256={cache_sha}\n"
        f"COMMAND_SIGNATURE={command_signature}\n"
    )
    (bundle_dir / "retroarch.run.env").write_text(
        "RUNTIME_EXECUTED=1\nRETROARCH_EXIT_STATUS=0\nFORCED_TERMINATION=0\n"
    )


bundle_a.write_text(json.dumps({
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
    "ci_palette_probe": {"families": [], "usages": [], "emulated_tmem": []},
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
                    "fmt": "2",
                    "siz": "1",
                    "off": "0",
                    "stride": "2",
                    "wh": "2x2",
                    "fs": "258",
                    "sampled_low32": "11111111",
                    "sampled_entry_pcrc": "aaaabbbb",
                    "sampled_sparse_pcrc": "aaaabbbb",
                    "sampled_entry_count": "1",
                    "sampled_used_count": "1"
                },
                "upload_low32s": [{"value": "11111111"}],
                "upload_pcrcs": [{"value": "aaaabbbb"}]
            }
        ]
    }
}, indent=2) + "\n")
write_bundle_provenance(bundle_a, "synthetic-a")

bundle_b.write_text(json.dumps({
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
    "ci_palette_probe": {"families": [], "usages": [], "emulated_tmem": []},
    "sampled_object_probe": {
        "exact_hit_count": 1,
        "exact_miss_count": 0,
        "exact_conflict_miss_count": 0,
        "exact_unresolved_miss_count": 0,
        "top_groups": [
            {
                "fields": {
                    "draw_class": "texrect",
                    "cycle": "copy",
                    "fmt": "0",
                    "siz": "3",
                    "off": "0",
                    "stride": "2",
                    "wh": "2x2",
                    "fs": "514",
                    "sampled_low32": "22222222",
                    "sampled_entry_pcrc": "ccccdddd",
                    "sampled_sparse_pcrc": "ccccdddd",
                    "sampled_entry_count": "1",
                    "sampled_used_count": "1"
                },
                "upload_low32s": [{"value": "22222222"}],
                "upload_pcrcs": [{"value": "ccccdddd"}]
            }
        ]
    }
}, indent=2) + "\n")
write_bundle_provenance(bundle_b, "synthetic-b")

summary_path.write_text(json.dumps({
    "summary_title": "merged context bundles",
    "all_passed": True,
    "fixtures": [
        {"label": "bundle-a", "fixture_id": "synthetic-a", "bundle_dir": "../bundle-a", "passed": True},
        {"label": "bundle-b", "fixture_id": "synthetic-b", "bundle_dir": "../bundle-b", "passed": True},
    ],
}, indent=2) + "\n")
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
from collections import Counter
from pathlib import Path

report = json.loads(Path(sys.argv[1]).read_text())
package_manifest = json.loads(Path(sys.argv[2]).read_text())

if report["requested_family_count"] != 2:
    raise SystemExit(f"unexpected requested family count: {report['requested_family_count']!r}")
if report["conversion_outcome"] != "promotable-runtime-package":
    raise SystemExit(f"unexpected conversion outcome: {report['conversion_outcome']!r}")
if not report["runtime_overlay_built"]:
    raise SystemExit(f"expected runtime overlay build from merged context bundles: {report!r}")
if report["runtime_overlay_reason"] != "runtime-context-available":
    raise SystemExit(f"unexpected runtime overlay reason: {report['runtime_overlay_reason']!r}")
if report["binding_count"] != 1 or report["unresolved_count"] != 0:
    raise SystemExit(f"expected one deterministic binding with no unresolved transport cases, got {report!r}")
if len(report.get("context_bundle_resolutions") or []) != 2:
    raise SystemExit(f"unexpected context bundle resolutions: {report.get('context_bundle_resolutions')!r}")
if report.get("runtime_state_counts") != {"runtime-bound": 1, "runtime-ready-package": 1}:
    raise SystemExit(f"unexpected runtime state counts: {report.get('runtime_state_counts')!r}")
if report.get("import_state_counts") != {"exact-authority": 2}:
    raise SystemExit(f"unexpected import state counts: {report.get('import_state_counts')!r}")

records = package_manifest.get("records", [])
if len(records) != 2:
    raise SystemExit(f"unexpected package record count: {len(records)}")
kinds = Counter(record.get("record_kind") for record in records)
if set(kinds) - {"canonical-sampled", "exact-authority-family"}:
    raise SystemExit(f"unexpected record kinds: {kinds!r}")
if package_manifest.get("runtime_ready_record_count") != 2:
    raise SystemExit(f"unexpected runtime ready count: {package_manifest.get('runtime_ready_record_count')!r}")
if any(not bool(record.get("runtime_ready")) for record in records):
    raise SystemExit(f"expected all merged context records to be runtime-ready, got {records!r}")

sampled_low32s = sorted(
    str((record.get("canonical_identity") or {}).get("sampled_low32") or "").lower()
    for record in records
)
if sampled_low32s != ["11111111", "22222222"]:
    raise SystemExit(f"unexpected sampled_low32s: {sampled_low32s!r}")
PY

echo "emu_hts2phrb_context_bundle_merge: PASS"

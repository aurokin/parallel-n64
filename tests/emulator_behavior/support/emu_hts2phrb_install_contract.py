"""Synthetic, offline contracts for ROM pairing and converter installation."""
import gzip
import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools"))

from hires_pack_install import (
    check_pairing,
    hires_game_ident,
    parse_rdb_keys,
    read_rom_header,
    read_source_allowlist,
)

KEY = "12345678-90ABCDEF-C:45"
MD5 = "0123456789abcdef0123456789abcdef"


def rom_header(name=b"MARIOKART64", code=b"NKTE"):
    header = bytearray(64)
    header[:4] = bytes.fromhex("80371240")
    header[0x10:0x18] = bytes.fromhex("1234567890abcdef")
    header[0x20:0x34] = name.ljust(20, b" ")
    header[0x3B:0x3F] = code
    header[0x3F] = 2
    return bytes(header)


def cache_fixture(path):
    payload = bytes((10, 20, 30, 255)) * 4
    with gzip.open(path, "wb") as fp:
        fp.write(struct.pack("<iiQIIIHHBHI", 0x08000000, 0, 0x12345678, 2, 2,
                             0x8058, 0x1908, 0x1401, 1, 0x200, len(payload)))
        fp.write(payload)


def image(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.new("RGBA", (2, 2), (10, 20, 30, 255)).save(path)


class Identity(unittest.TestCase):
    def test_shared_core_vectors(self):
        vectors = json.loads((REPO / "tests/emulator_behavior/fixtures/hires_game_ident.json").read_text())
        for vector in vectors:
            with self.subTest(vector=vector["label"]):
                self.assertEqual(hires_game_ident(bytes.fromhex(vector["header_hex"])), vector["ident"])

    def test_all_byte_orders_plain_and_zip_first_rom_member(self):
        canonical = rom_header()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for ext, swap in (("z64", 0), ("v64", 1), ("n64", 3)):
                data = bytes(canonical[i ^ swap] for i in range(64))
                path = root / f"game.{ext}"
                path.write_bytes(data + b"unused payload")
                self.assertEqual(read_rom_header(path), canonical)
                archive = root / f"{ext}.zip"
                with zipfile.ZipFile(archive, "w") as z:
                    z.writestr("readme.txt", "ignored")
                    z.writestr(f"nested/game.{ext.upper()}", data)
                    z.writestr("other.z64", rom_header(b"OTHER"))
                self.assertEqual(read_rom_header(archive), canonical)

    def test_bad_rom_inputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "game.z64"
            for data, message in ((b"short", "truncated"), (bytes(64), "byte order")):
                path.write_bytes(data)
                with self.assertRaisesRegex(ValueError, message):
                    read_rom_header(path)
            path = Path(tmp) / "game.zip"
            with zipfile.ZipFile(path, "w") as z:
                z.writestr("readme.txt", "no ROM")
            with self.assertRaisesRegex(ValueError, "No .z64/.v64/.n64"):
                read_rom_header(path)


class Pairing(unittest.TestCase):
    def test_exact_and_case_only(self):
        for label in ("MARIOKART64", "mariokart64"):
            self.assertEqual(check_pairing(label, [], rom_header()), "exact")
        header = rom_header(b"Turok 3: Shadow of O")
        self.assertEqual(check_pairing("Turok 3- Shadow of O", [], header), "exact")
        header = rom_header(b"WCW / nWo  REVENGE")
        self.assertEqual(check_pairing("WCW - nWo  REVENGE", [], header), "exact")

    def test_near_misses_and_shared_stadium_names_refuse_unless_forced(self):
        cases = [("Mario Kart 64", rom_header(), "MARIOKART64"),
                 ("FZEROX", rom_header(b"F-ZERO X"), "F-ZERO X"),
                 ("POKEMON STADIUM 2", rom_header(b"POKEMON STADIUM 2", b"CP2J"), "CP2J"),
                 ("POKEMON STADIUM", rom_header(b"POKEMON STADIUM", b"CPSJ"), "CPSJ")]
        for label, header, ident in cases:
            with self.subTest(label=label, ident=ident):
                with self.assertRaises(ValueError) as caught:
                    check_pairing(label, [], header)
                self.assertIn(f'"{label}"', str(caught.exception))
                self.assertIn(f'"{ident}"', str(caught.exception))
                self.assertIn("--force-target", str(caught.exception))
                self.assertEqual(check_pairing(label, [], header, force=True), "forced")

    def test_allowlist_takes_priority_over_matching_label_and_checks_country(self):
        self.assertEqual(check_pairing("arbitrary", [KEY], rom_header()), "allowlist")
        for key in ("00000000-90ABCDEF-C:45", "12345678-00000000-C:45", "12345678-90ABCDEF-C:4A"):
            with self.assertRaisesRegex(ValueError, "not in its supported ROM list"):
                check_pairing("MARIOKART64", [key], rom_header())
            self.assertEqual(check_pairing("MARIOKART64", [key], rom_header(), force=True), "forced")
        self.assertEqual(check_pairing("MARIOKART64", [], rom_header(), force=True), "exact")

    def test_no_identity_cannot_be_forced(self):
        header = rom_header(b"", bytes(4))
        with self.assertRaisesRegex(ValueError, "no usable name or game code"):
            check_pairing("anything", [], header, force=True)

    def test_xml_and_ini_supported_lists_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            xml = root / "PACK.XML"
            xml.write_text(f"<pack><supported><rdb>{KEY.lower()}</rdb></supported>"
                           "<description>AAAAAAAA-BBBBBBBB-C:4A</description></pack>")
            ini = root / "roms.ini"
            ini.write_text('[Supported]\nCRC="12345678-90abcdef-C:45","AAAAAAAA-BBBBBBBB-C:a"\n'
                           '[Unrelated]\nCRC="CCCCCCCC-DDDDDDDD-C:45"\n')
            self.assertEqual(read_source_allowlist(root), [KEY, "AAAAAAAA-BBBBBBBB-C:0A"])
            self.assertEqual(parse_rdb_keys("012345678-90ABCDEF-C:45 12345678-90ABCDEF-C:456"), [])
            xml.write_text("<pack><supported><rdb>invalid</rdb></supported></pack>")
            with self.assertRaisesRegex(ValueError, "no valid RDB keys"):
                read_source_allowlist(root)
            xml.write_text("<broken")
            with self.assertRaisesRegex(ValueError, "Cannot read supported ROM list"):
                read_source_allowlist(root)


class InstallCLI(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.cache = self.root / "MARIOKART64_HIRESTEXTURES.htc"
        cache_fixture(self.cache)
        self.rom = self.root / "game.z64"
        self.rom.write_bytes(rom_header())
        self.system = self.root / "system"
        self.output = self.root / "out"

    def run_converter(self, *options, cache=None, install=True, success=True):
        args = [sys.executable, str(REPO / "tools/hts2phrb.py"), "--cache", str((cache or self.cache).relative_to(self.root)),
                "--output-dir", str(self.output), "--loose-png-workers", "1", "--stdout-format", "json"]
        if install:
            args += ["--rom", str(self.rom), "--install", str(self.system)]
        result = subprocess.run(args + list(options), cwd=self.root, text=True, capture_output=True, check=False)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        return result

    def test_install_report_default_name_and_no_install_package_parity(self):
        raw = json.loads(self.run_converter(install=False).stdout)
        binary = Path(raw["binary_package"]["output_path"]).read_bytes()
        self.assertNotIn("install", raw)
        report = json.loads(self.run_converter().stdout)
        target = self.system / "parallel-n64/hires/MARIOKART64/MARIOKART64_HIRESTEXTURES.phrb"
        self.assertEqual(target.read_bytes(), binary)
        info = report["install"]
        self.assertEqual(info["target_path"], str(target))
        self.assertEqual(info["source_label"], "MARIOKART64")
        self.assertEqual(info["source_path"], str(self.cache))
        self.assertEqual(info["source_allowlist"], [])
        self.assertEqual(info["ident"], "MARIOKART64")
        self.assertEqual(info["target_check"], "exact")
        self.assertFalse(info["forced"])
        self.assertEqual(info["rom"], {"path": str(self.rom), "name": "MARIOKART64", "code": "NKTE",
                                       "rev": 2, "crc1": "12345678", "crc2": "90ABCDEF"})
        self.assertEqual(json.loads(Path(report["report_path"]).read_text())["install"], info)
        # Phase-1 output for cache_fixture(), independently checked at 40e98f87.
        self.assertEqual(hashlib.sha256(binary).hexdigest(), "a2ccb61b2f73164be5bd7650945a63ece0da09defc734fd85ebfc57ec3471e90")

    def test_refusal_precedes_conversion_and_force_is_recorded(self):
        renamed = self.cache.with_name("Mario Kart 64.htc")
        self.cache.rename(renamed)
        refusal = self.run_converter(cache=renamed, success=False)
        self.assertIn('labelled "Mario Kart 64"', refusal.stderr)
        self.assertIn('ROM is "MARIOKART64"', refusal.stderr)
        self.assertFalse(self.output.exists())
        report = json.loads(self.run_converter("--force-target", cache=renamed).stdout)
        self.assertEqual(report["install"]["target_check"], "forced")
        self.assertTrue(report["install"]["forced"])

    def test_install_stdout_and_reuse_without_install_do_not_retain_stale_provenance(self):
        installed = self.run_converter("--stdout-format", "summary")
        self.assertIn("installed: ", installed.stdout)
        self.assertNotIn("suggested_install_path:", installed.stdout)
        report = json.loads(self.run_converter("--reuse-existing", install=False).stdout)
        self.assertTrue(report["reused_existing"])
        self.assertNotIn("install", report)
        self.assertNotIn("install", json.loads(Path(report["report_path"]).read_text()))
        manual = self.run_converter("--reuse-existing", "--stdout-format", "summary", install=False)
        self.assertIn("suggested_install_path:", manual.stdout)

    def test_replace_and_layering_note(self):
        target_dir = self.system / "parallel-n64/hires/MARIOKART64"
        target_dir.mkdir(parents=True)
        target = target_dir / "skin.phrb"
        target.write_bytes(b"preserve me")
        (target_dir / "addon.phrb").write_bytes(b"other package")
        result = self.run_converter("--name", "skin", success=False)
        self.assertIn("--replace", result.stderr)
        self.assertEqual(target.read_bytes(), b"preserve me")
        self.assertFalse(self.output.exists())
        result = self.run_converter("--name", "skin.phrb", "--replace")
        self.assertTrue(target.read_bytes().startswith(b"PHRB"))
        self.assertIn("addon.phrb", result.stderr)
        self.assertIn("later names win", result.stderr)
        self.assertEqual(json.loads(result.stdout)["install"]["other_packages"], ["addon.phrb"])

    def test_jabo_allowlist_and_no_list_folder_label(self):
        pack = self.root / "MARIOKART64"
        image(pack / f"{MD5}.png")
        report = json.loads(self.run_converter(cache=pack).stdout)
        self.assertEqual(report["install"]["target_check"], "exact")
        renamed = self.root / "Free-form Jabo label"
        pack.rename(renamed)
        pack = renamed
        (pack / "pack.xml").write_text(f"<pack><supported><rdb>{KEY}</rdb></supported></pack>")
        report = json.loads(self.run_converter("--name", "allowlist", cache=pack).stdout)
        self.assertEqual(report["install"]["source_allowlist"], [KEY])
        self.assertEqual(report["install"]["target_check"], "allowlist")
        (pack / "pack.xml").write_text('<pack><supported><rdb>00000000-00000000-C:45</rdb></supported></pack>')
        self.assertIn("not in its supported ROM list", self.run_converter(cache=pack, success=False).stderr)

    def test_directory_allowlist_is_checked_when_input_resolves_to_a_cache_file(self):
        pack = self.root / "cache-directory"
        pack.mkdir()
        self.cache.rename(pack / self.cache.name)
        metadata = pack / "pack.xml"
        metadata.write_text('<pack><supported><rdb>00000000-00000000-C:45</rdb></supported></pack>')
        self.assertIn("not in its supported ROM list", self.run_converter(cache=pack, success=False).stderr)
        self.assertFalse(self.output.exists())
        metadata.write_text(f"<pack><supported><rdb>{KEY}</rdb></supported></pack>")
        report = json.loads(self.run_converter(cache=pack).stdout)
        self.assertEqual(report["install"]["target_check"], "allowlist")

    def test_stadium_cache_refuses_before_conversion_and_loose_case_match_installs(self):
        self.rom.write_bytes(rom_header(b"POKEMON STADIUM 2", b"CP2J"))
        pack = self.root / "POKEMON STADIUM 2_HIRESTEXTURES.hts"
        pack.write_bytes(b"conversion must not run")
        refusal = self.run_converter(cache=pack, success=False)
        self.assertIn('ROM is "CP2J"', refusal.stderr)
        self.assertFalse(self.output.exists())
        self.rom.write_bytes(rom_header())
        pack = self.root / "artist-folder"
        image(pack / "mariokart64#12345678#0#2_all.png")
        report = json.loads(self.run_converter(cache=pack).stdout)
        self.assertEqual(report["install"]["target_check"], "exact")
        self.assertEqual(report["install"]["source_label"], "mariokart64")

    def test_loose_label_is_dominant_prefix_not_folder_or_override(self):
        pack = self.root / "MARIOKART64"
        image(pack / "Mario Kart 64#12345678#0#2_all.png")
        refusal = self.run_converter("--rom-name", "MARIOKART64", cache=pack, success=False)
        self.assertIn('labelled "Mario Kart 64"', refusal.stderr)
        result = self.run_converter("--force-target", cache=pack)
        self.assertEqual(json.loads(result.stdout)["install"]["source_label"], "Mario Kart 64")

    def test_memorycache_refusal_and_port_summary(self):
        for suffix in ("hts", "htc", "dat"):
            path = self.root / f"MARIOKART64_MEMORYCACHE.{suffix}"
            path.write_bytes(b"not a pack")
            result = self.run_converter(cache=path, install=False, success=False)
            self.assertIn("texture-enhancement cache, not a hi-res pack", result.stderr)
        pack = self.root / "loose"
        image(pack / "MARIOKART64#12345678#0#2_all.png")
        image(pack / "tex1_32x32_0123456789abcdef_3.png")
        image(pack / "rt64/00ab6f00#0#2.png")
        result = self.run_converter(cache=pack, install=False)
        self.assertIn("Skipped port-specific textures: Dolphin tex1_*, rt64", result.stderr)
        (pack / "MARIOKART64#12345678#0#2_all.png").unlink()
        result = self.run_converter(cache=pack, install=False, success=False)
        self.assertIn("Dolphin tex1_*, rt64", result.stderr)
        dds_only = self.root / "dds-only"
        dds_only.mkdir()
        (dds_only / "00ab6f00#0#2.dds").write_bytes(b"not convertible")
        self.assertIn("rt64", self.run_converter(cache=dds_only, install=False, success=False).stderr)

    def test_install_argument_requirements_and_unsafe_names(self):
        for options, message in ((["--install", str(self.system)], "--install requires --rom"),
                                 (["--force-target"], "require --install")):
            result = self.run_converter(*options, install=False, success=False)
            self.assertIn(message, result.stderr)
        for name in ("../escape", "/escape", ".hidden", "bad\\name"):
            result = self.run_converter("--name", name, success=False)
            self.assertIn("usable install filename", result.stderr)
        self.assertFalse(self.output.exists())

    def test_gate_failure_does_not_install(self):
        self.run_converter("--max-binary-package-bytes", "1", success=False)
        self.assertFalse(self.system.exists())

    def test_header_ident_with_path_like_fallback_code_cannot_escape_install_root(self):
        self.rom.write_bytes(rom_header(b"", b"../X"))
        result = self.run_converter("--force-target", success=False)
        self.assertIn("ROM identity is not a usable install filename", result.stderr)
        self.assertFalse(self.system.exists())


if __name__ == "__main__":
    unittest.main()

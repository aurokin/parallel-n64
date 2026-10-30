# Hi-res pack converter

[`hts2phrb.py`](hts2phrb.py) converts legacy caches, loose Rice PNG packs and
Project64/Jabo packs into PHRB packages. See the
[pack format and conversion guide](../assets/TEXTURE_PACKS.md) and run
`python3 tools/hts2phrb.py --help` for options.

To check a pack against your ROM and install it:

```sh
python3 tools/hts2phrb.py --cache "extracted pack" --rom "game.z64" \
  --install "RetroArch system directory"
```

The ROM may use z64/v64/n64 byte order or be the first N64 member in a ZIP.
The converter checks a supported-ROM list when present, otherwise an exact
case-insensitive label, then writes
`<system>/parallel-n64/hires/<ident>/<source stem>.phrb`. `--name` changes the
installed filename, `--replace` permits overwriting it, and `--force-target`
explicitly overrides a pairing mismatch. The report's `install` object records
the source, ROM header, target and check result. See the guide for label rules
and layering order. Without `--install`, conversion behavior is unchanged.

Raw RGBA compression is opt-in: `--compress-raw-rgba 1` or `6` uses zlib at
that level only for exact, nonzero RGBA/unsigned-byte blobs, retaining the
compressed bytes only when smaller. The default is `off`. Other formats,
already compressed blobs and deferred assets retain their storage behavior.
PHRB v7/v8 and texture identity are unchanged. Output storage counts appear in
the report's `binary_package.raw_rgba_compression`; source manifest fields
remain provenance. Smaller output adds conversion and first-use decode CPU
cost; this option makes no speed guarantee.

## License

The converter, its helpers listed below, `hires_pack_transport_policy.json`,
the `hires_pack_curation_*.json` example policies, and the PHRB format and its
converter documentation are MIT under [LICENSE-hires-converter](LICENSE-hires-converter).
They may be copied and used independently of the GPLv2 ParaLLEish core. This
license covers the tools and format; input texture artwork retains its own
rights.

Every Python file below carries `SPDX-License-Identifier: MIT`. Each was
introduced in this repository by the listed local addition commit.

| MIT Python file | Local addition commit |
| --- | --- |
| [`hts2phrb.py`](hts2phrb.py) | `0311d9f2` |
| [`hires_pack_apply_alias_group_review.py`](hires_pack_apply_alias_group_review.py) | `95433aa0` |
| [`hires_pack_apply_duplicate_review.py`](hires_pack_apply_duplicate_review.py) | `95433aa0` |
| [`hires_pack_apply_exclusion_review.py`](hires_pack_apply_exclusion_review.py) | `48369936` |
| [`hires_pack_common.py`](hires_pack_common.py) | `f955759f` |
| [`hires_pack_emit_binary_package.py`](hires_pack_emit_binary_package.py) | `a083afd9` |
| [`hires_pack_emit_loader_manifest.py`](hires_pack_emit_loader_manifest.py) | `a50cb5a4` |
| [`hires_pack_emit_proxy_bindings.py`](hires_pack_emit_proxy_bindings.py) | `fa932c0f` |
| [`hires_pack_inspect_binary_package.py`](hires_pack_inspect_binary_package.py) | `1a776054` |
| [`hires_pack_jabo.py`](hires_pack_jabo.py) | `14ec942e` |
| [`hires_pack_install.py`](hires_pack_install.py) | `2af91ccb` |
| [`hires_pack_loose_png.py`](hires_pack_loose_png.py) | `88bad189` |
| [`hires_pack_materialize_package.py`](hires_pack_materialize_package.py) | `ca99336b` |
| [`hires_pack_migrate.py`](hires_pack_migrate.py) | `22d05626` |
| [`hires_pack_proxy_review.py`](hires_pack_proxy_review.py) | `46321c5c` |

The converter's repository-local imports are confined to this list, including
imports inside functions. Its external imports are Python's standard library
and Pillow (`PIL`); it imports no GPL module from the repository. No image or
compression library is vendored in this Python scope. Python retains its
PSF terms, Pillow retains its MIT-CMU license, and bundled codecs retain their
own licenses if redistributed.

To reuse the converter separately, retain these Python files, its JSON policies
and `LICENSE-hires-converter` together under a `tools/` directory. Install
Python and Pillow separately; no core build or emulator is required. Optional
review/enrichment inputs are data files, not imported emulator modules.
Other tools in this directory are outside this license's scope.

# Texture Pack Sources And Conversion

Texture packs and generated packages are local, untracked inputs. This file
records portable source provenance and the conversion contract; it does not
record which private host currently stores an artifact.

## Format Contract

- Runtime input is `.phrb` only. Enable hi-res textures in the core options,
  then install packs under the frontend's system directory:
  - `parallel-n64/hires/<ROM name>.phrb` for a single package, and/or
  - `parallel-n64/hires/<ROM name>/` for multiple top-level `*.phrb` files,
    loaded in sorted order. If both exist, the single package loads first.
  `<ROM name>` is the ROM header's 20-byte internal Name field, with only
  trailing spaces and NULs removed; spelling and case must match. For example,
  use `parallel-n64/hires/SUPER MARIO 64.phrb` for Super Mario 64. Other games'
  packs and loose `.phrb` files in the system directory root are ignored.
  If neither game path loads entries, hi-res stays off and the log lists the
  two checked paths.
- `PARALLEL_RDP_HIRES_CACHE_PATH` overrides game scoping. It loads only the
  named `.phrb` file, or the sorted top-level `*.phrb` files in the named
  directory. Any other path, including an extension-less file, logs
  `Hi-res replacement cache load failed` and renders native textures.
- `tools/hts2phrb.py --cache <input>` converts these inputs offline:
  - GlideN64/GLideNHQ `.hts` or `.htc` caches, or a directory holding one.
  - Loose Rice-named PNG directories (Rice and GLideN64 PNG releases). Pass
    the texture folder, normally `hires_texture/<ROM name>`; it is used when
    the directory holds no cache file. See [Loose PNG packs](#loose-png-packs).
  - Project64/Jabo packs named by 32-digit MD5. Pass the texture folder; it
    is converted as Jabo when its MD5 names outnumber its Rice names. See
    [Jabo packs](#jabo-packs).
  - Glide64 `*_HIRESTEXTURES.dat` caches, only with
    `--accept-glide64-pow2-padding`. Glide64 stores every texture padded to
    power-of-two size by edge replication and does not record the original
    size, so non-power-of-two textures convert at the wrong scale. Prefer the
    same pack's Rice PNG or GLideN64 release.
- Rice `_ci.bmp` textures keep one byte per palette index (CI4 or CI8); the
  renderer colours them with the live TMEM TLUT, ignoring the BMP preview palette.
  In the existing v7 asset fields, `format=GL_COLOR_INDEX8_EXT (0x80e5)`
  (plus the existing zlib flag), `texture_format=GL_COLOR_INDEX (0x1900)`, and
  `pixel_type=GL_UNSIGNED_BYTE (0x1401)` identify this encoding.
  Older cores cannot decode indexed assets.
- Not convertible: port-specific folders such as Dolphin `tex1_*` or rt64
  names, which the loose loaders skip by name.
- Packages are PHRB version 7, or version 8 when they hold Jabo records.
  Version 8 appends a key kind (0 Rice, 1 Jabo MD5) and a 16-byte key to
  every record; Jabo records have zero format, size and view fields and one
  version-7 asset. Rice-only conversions stay byte-identical version 7.
  Cores without Jabo support reject version 8 packages.
- ROMs, pack archives, extracted caches, and generated packages are never
  committed.
- Record the source URL and archive SHA-256 before promoting a new source.

## Memory And Disk

The converter streams texture data, so peak memory does not grow with the
total size of the pack's textures:

- `.hts` blobs are read by offset from the cache file while the package is
  written. The cache is never loaded whole.
- Loose PNG, Jabo, Glide64 `.dat`, and `.htc` inputs are decoded or
  decompressed in one pass. Each blob goes to an unlinked scratch file in the
  output directory, which disappears when the process exits. These inputs need
  free disk space for about the package size twice: once for the scratch file
  and once for the package.
- At most two textures per decoder process are in flight. Each process holds
  about one decoded texture. `--loose-png-workers` lowers the process count.

Per-entry manifests and reports stay in memory, at about 20 KB per cache
entry. For example, a 43,000-entry pack peaks below 1 GB.

## Loose PNG Packs

```sh
python3 tools/hts2phrb.py --cache "hires_texture/SUPER MARIO 64" \
  --output-dir artifacts/hts2phrb/sm64-png
```

The loader reproduces the records GLideNHQ (`TxHiResCache`/`TxHiResLoader`)
would write to a cache from that folder:

- Names must be `<ROM>#<CRC>#<fmt>#<siz>[#<PALCRC>]_<suffix>` with suffix
  `all.png`, `allciByRGBA.png`, `ciByRGBA.png`, `rgb.png`, `rgb.bmp`,
  `a.png` or `a.bmp`, or a `$` wildcard form (`#<CRC>#<fmt>#<siz>#$_`,
  `#$#<fmt>#<siz>#<PALCRC>_`). The key is `PALCRC << 32 | CRC` (the
  palette-only form keys on `PALCRC`) and the formatsize is `fmt | siz << 8`.
- `<ROM>` defaults to the most common prefix in the folder. Rice-era packs
  often use Rice's game names (`Mario Kart 64`) instead of the ROM header
  name; the key does not depend on it. Override it with `--rom-name`.
- Subfolders are searched; names starting with `.` or `~!~` are skipped. The
  first loadable file for a key and formatsize wins.
- `_rgb` + `_a` pairs take alpha from the red channel of `_a`; `_rgb` alone is
  opaque. fmt 4 (I) textures without alpha get alpha `(R + G + B) / 3`.
- `--loose-png-alpha full` (default) matches GlideN64's
  `txHiresFullAlphaChannel=True`. `rice` matches `False`, the libretro
  default, which rounds alpha and fills transparent texels for fmt 0/siz 2
  and CI textures.
- `source-input-report.json` in the output directory lists every skipped file
  and its reason.

Deliberate differences from current GLideNHQ, which this converter treats as
GLideNHQ defects:

- Name matching is case-insensitive, as GLideNHQ behaves on Windows. On other
  systems it requires exact case.
- Traversal is depth-first in case-insensitive name order (the NTFS order), so
  duplicate resolution is deterministic. GLideNHQ uses directory order.
- `_rgb`/`_a` partners are resolved in the file's own folder. Current
  GLideNHQ checks for the partner relative to the process working directory
  and so drops these pairs; the older GlideHQ changed into the folder first.
- BMP colors are decoded as stored. GLideNHQ copies BMP bytes without swapping
  blue and red.
- Textures larger than 4096 pixels are kept at full size, where GLideNHQ would
  downscale them. The report lists them as `oversize`.
- A wildcard name whose key is zero is skipped. In GLideNHQ it aborts the whole
  load.

## Jabo Packs

```sh
python3 tools/hts2phrb.py --cache "hires_textures/MARIOKART64" \
  --output-dir artifacts/hts2phrb/mk64-jabo
```

Jabo names each texture `<MD5>.<ext>` after the MD5 of the draw view as it
uploaded the texture to Direct3D: the GLideN64-compat window (SetTileSize
clamped by the masks, with LoadTile source rows), each texel converted to
Jabo's surface format and written little-endian, rows packed without padding.

| N64 format | Jabo surface | Texel |
|------------|--------------|-------|
| RGBA16 | A1R5G5B5 | N64 value rotated right by one bit |
| RGBA32 | A8R8G8B8 | bytes B, G, R, A |
| IA16 | A8R8G8B8 | I, I, I, A |
| IA8 | A4R4G4B4 | A4, I4, I4, I4 |
| IA4 | A4R4G4B4 | I4 = I3 << 1 (no low-bit fill), A4 = 0xF or 0 |
| I8 | A8R8G8B8 | I, I, I, I |
| I4 | A4R4G4B4 | every nibble = I |
| CI4/CI8, RGBA16 TLUT | A1R5G5B5 | palette entry, rendering tile's CI4 bank |
| CI4/CI8, IA16 TLUT | A8R8G8B8 | I, I, I, A of the entry (unverified) |

The key holds no palette field. Mappings were established offline from ROM
data against real pack names; see the issue log for unverified load shapes.

The converter (`tools/hires_pack_jabo.py`):

- Accepts names of exactly 32 hex digits with `.png`, `.dds`, `.jpg` or
  `.jpeg`, in any case. Suffixed copies (`<MD5> - Copy.png`, `<MD5>xx.png`),
  other image formats (`.pspimage`, `.bmp`), `Thumbs.db` and other files are
  skipped. `pack.xml` and `roms.ini` are only listed in the report.
- Traverses depth-first in case-insensitive name order without following
  symlinks. The first loadable file for an MD5 wins; later ones are reported
  as `duplicate-key`.
- Decodes by content, not extension: PNG and JPEG through Pillow (RGB gets
  alpha 255), DDS DXT1/DXT3/DXT5 or uncompressed 32-bit top mip through its
  own decoder. Alpha is kept verbatim.
- Emits one record per MD5 with no Rice family migration, sorted by MD5.
  `--bundle`, `--low32` and the review inputs do not apply and are refused.
- Writes `source-input-report.json` with every skipped file and its reason.

At runtime the draw-time compat lane computes the Jabo key only after the
Rice key misses, and only when the loaded packages hold Jabo records. Keys
are memoized per view. The `Hi-res Jabo summary` log line reports hits,
misses, MD5 computations and memo hits; Jabo hits are not counted in
`compat_draw_hits`.

## Source Catalog

| Game | Source | Cache format | Source URL | Archive SHA-256 |
|------|--------|--------------|------------|----------------|
| Paper Mario | Paper Mario Redone HD 4.0.1 | old-version `.hts` | Unknown; acquisition predates this ledger | Unknown; recover before redistribution |
| Super Mario 64 | SM64 Reloaded 2.6.0 HD | GLideNHQ `.hts` | [download](https://evilgames.eu/files/texture-packs/sm64-reloaded-v2.6.0-gliden64-hts-hd.7z) | `c641cdd7ff590ac3e5fd5936ad7cbc8af139bbd392276e14871a81cf075873e6` |
| Ocarina of Time | OoT Reloaded 11.0.0 HD | GLideNHQ `.hts` | [release page](https://evilgames.eu/texture-packs/oot-reloaded.htm) | `7a68eaf94e62d2059cca011d7a03fefb98f1fbc923809ff8b96dbb45bfa267d2` |
| Mario Kart 64 | MK64 Reloaded 2026.04.03 HD | GLideNHQ `.hts` | [release page](https://evilgames.eu/texture-packs/mk64-reloaded.htm) | `de63c1d640aad8dbad7701b47b5ba38ba522262d92668d028766e5c388f10bd0` |
| Majora's Mask | MM Reloaded 11.0.2 HD | GLideNHQ `.hts` | [download](https://evilgames.eu/files/texture-packs/mm-reloaded-v11.0.2-gliden64-hts-hd.7z) | `23e96dce91a41c13861685b43b51f2a588458262c756be55eec78d43d0e5854d` |

Formats keyed for a PC port, Ship of Harkinian, Citra, or another console are
not interchangeable with GlideN64/Rice identities.

## Observed Package Selection

The checked-in behavior currently differs by consumer:

- `tools/scenarios/lib/common.sh` checks the path-stable
  `local-pm64-exact-variant-set` package first, then zero-config. The current
  staged package at that stable path is byte-identical to curated-r2
  (`sha256: 388bce41777a45eecd7581fc03bf67c7c36512cc7d1dd786ddfb38a90964d71f`),
  despite the path not carrying the curation suffix.
- The Paper Mario runtime-conformance wrapper defaults to that same stable
  exact-variant-set path.
- Fixture YAML files still declare the zero-config package.
- External callers may provide another package explicitly.

This describes the code and current staged identity as they exist; it is not
authorization to change a runtime default. Because curation is not encoded in
the stable path, every evidence bundle must record the package hash actually
used.

## Paper Mario Curation

Two tracked exclusion reviews describe known pack-content removals:

- `tools/hires_pack_curation_pm64_exclusions.json` excludes the
  `fbebebeb` transparency-grid placeholder.
- `tools/hires_pack_curation_pm64_jungle_exclusions.json` excludes
  the `816a81b8` and `23ff3d81` overlay pair.

Apply both reviews to a fresh exact-variant-set conversion:

```sh
python3 tools/hires_pack_apply_exclusion_review.py \
  --loader-manifest artifacts/hts2phrb-review/local-pm64-exact-variant-set/loader-manifest.json \
  --exclusion-review tools/hires_pack_curation_pm64_exclusions.json \
  --exclusion-review tools/hires_pack_curation_pm64_jungle_exclusions.json \
  --output-dir artifacts/hts2phrb-review/local-pm64-curated
```

Run conversion commands from the repository root so relative source paths
resolve consistently. Use `python3 tools/hts2phrb.py --help` for the
current converter interface.

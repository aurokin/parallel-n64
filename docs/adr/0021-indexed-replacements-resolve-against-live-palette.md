# ADR-0021: Indexed replacements resolve against the live palette

- Status: Accepted (October 27, 2026)

## Context

Rice-format packs may ship colour-indexed replacements: `<rom>#<crc>#2#<size>_ci.bmp`
(format 2 = CI, size 0 = 4-bit, 1 = 8-bit), stored as 4- or 8-bit paletted BMPs whose
pixels are palette *indices*. Rice and GLideNHQ coloured them with the game's TLUT at
draw time (GLideN64 `TxImage.cpp` reads them as COLOR_INDEX8; `TxFilter.cpp` expands
them with the palette supplied at runtime). The BMP's own palette is a preview, not
the output colours. Before this change the converter skipped them with reason
`rice-ci-bmp-unsupported`. The Linux sweep found 290 such filenames in 9 packs;
Monopoly 64 (A011) consists only of valid CI BMPs and converted to an empty package.

## Decision

1. The converter keeps CI replacements as indices: one byte per pixel (CI4 values
   0–15, CI8 0–255), width and height, plus the key fields from the name (texture
   CRC, format CI, size CI4/CI8, palette CRC when the name has one). The BMP palette
   is discarded. PHRB uses its existing v7 asset encoding fields:
   `format=GL_COLOR_INDEX8_EXT (0x80e5)` plus the existing zlib flag,
   `texture_format=GL_COLOR_INDEX (0x1900)`, `pixel_type=GL_UNSIGNED_BYTE (0x1401)`.
   There is no layout or version change; Rice packages remain v7 (v8 with Jabo).
2. The Rice key (texture CRC, and palette CRC only when the record carries one)
   selects the record. The existing exact-checksum probe is followed by a
   texture-only probe for indexed CI records without a palette CRC. Pre-coloured
   records retain their existing lookup policy.
3. On a hit for an indexed record, the runtime expands indices to RGBA8 with the
   palette that the draw uses — the TMEM TLUT shadow already available at identity
   time (`tlut_tmem_shadow`, not the separate GLideN64 keying shadow): CI8 uses
   all 256 entries; CI4 uses the tile's 16-entry bank. The mirror's byte pairs
   are normalized to the native shader's `vram16` halfword order without changing
   the identity mirror or its key hashes. Entries are
   decoded by the TLUT type in othermode (RGBA16 or IA16), with the same conversion
   the native texture path uses for that TLUT type.
4. Indexed decode is deferred from upload to the draw, where the render tile
   and othermode are final. Expanded images are cached by (record, palette bytes hash);
   a palette change yields a new expansion, never a stale one. The hash includes the
   selected 16 or 256 native TLUT words and the TLUT type;
   draw bindings are refreshed when it changes. Upload, filtering and sampling then
   follow the normal RGBA path.
5. ADR-0020 is unaffected: expansion happens only where replacement is sampled (the
   presented pass).

## Consequences

- A011 and valid CI content in A018, A019, A027, A029, A065, A068 and A083 become
  usable. A031's 39 `_ci.bmp` names instead carry format 0; these remain rejected
  as `invalid-ci-formatsize`, since the decision requires format CI (2).
  Palette-specific pre-coloured PNGs retain their lookup and decode behavior.
  Normal first-loadable-file selection still applies to duplicate Rice keys.
- One uploaded RGBA image per (record, palette) is held in the existing resident-image
  registry, bounded by its byte budget and descriptor limit; CPU expansion is temporary.
- Older cores accept the package layout but cannot decode the indexed assets.

## Verification

- Unit: expansion for CI8, CI4 with bank 0 and a non-zero bank, RGBA16 and IA16 TLUTs;
  the same record expanded with two palettes gives two results; cache keyed by palette.
- Converter: `_ci.bmp` 4- and 8-bit inputs produce indexed records; A011 converts to 27.
- Runtime: A011 serves hits in both Monopoly 64 power-on scenarios at 4x and 1x; the
  hi-res-on end state still equals feature-off; captures inspected for plausible
  colours (compare a few textures with the game's native rendering).
- No change to non-CI record counts. Of the sweep's 142 converted packages,
  nine contain `_ci.bmp` filenames (A011 and the eight partial packs above).

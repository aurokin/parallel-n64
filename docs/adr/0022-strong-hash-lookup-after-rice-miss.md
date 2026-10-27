# ADR-0022: Strong-hash lookup after a Rice miss

- Status: Accepted (October 27, 2026)

## Context

GLideN64 can dump textures with a "strong" hash (`textureFilter.txStrongCRC`, added in
upstream ded38f88): XXH3-64 over the texture's active source rows, folded to 32 bits
(low XOR high). Dump names keep Rice syntax (`ROM#HASH#FMT#SIZE_all.png`, CI adds
`#PALETTE_HASH`), so nothing in a pack marks which keys are strong. On load, GLideN64
looks up the Rice hash first and, when that misses, computes the strong hash and
tries again regardless of the dumping setting (`Textures.cpp` `_loadHiresTexture`).
Corpus packs that need this: A148 (F-Zero X portraits, all 37 assets), A128–A131
(Resident Evil 2 Reloaded, 28 CI4 ammo-number assets), A140–A141 (Turok 3 Reloaded,
4 I4 crosshair assets). The pinned GLideN64 source is
[`TxUtil.cpp`](https://github.com/gonetz/GLideN64/blob/a04081ad6a966a9413bea63a55f502e0dc98bb18/src/GLideNHQ/TxUtil.cpp)
and [`Textures.cpp`](https://github.com/gonetz/GLideN64/blob/a04081ad6a966a9413bea63a55f502e0dc98bb18/src/Textures.cpp).
Offline investigation reproduced all 37 A148 keys from runtime RDRAM.

## Decision

1. Keys are stored as converted today; the converter does not need to know which
   convention a key uses.
2. When the Rice-hash lookup for a texture misses, the runtime computes GLideN64's
   strong hash for the same source view and looks it up in the same key space, with
   the same formatsize. Non-indexed: `StrongCRC32` over the packed active rows
   (`bytesPerLine = (width << size) >> 1`, row stride excluded, bytes as stored in
   RDRAM, rows narrower than 4 bytes handled as upstream does, bounded), folded.
   CI4/CI8: texture strong hash in the low 32 bits and the strong hash of the palette
   prefix up to the highest referenced index in the high 32 bits, as
   `checksum64strong()` does.
3. The strong hash is computed only after a Rice miss and only while a replacement
   provider is attached, and its result is memoised with the existing per-source-view
   identity so a texture is hashed at most once per content change.
4. XXH3 comes from the upstream xxHash single header (BSD-2-Clause), vendored with its
   licence.
5. Match semantics otherwise follow the Rice path (selection, variants, CI palette
   handling, ADR-0020/0021).

## Consequences

- A148 and the strong-hash assets of RE2 and Turok 3 Reloaded serve.
- Each Rice miss costs one XXH3 pass over the texture's bytes until memoised; it is
  measured with the unthrottled benchmark (fresh-process paired 16/1216-frame runs, three repetitions, 16-frame warm-up).
- A Rice key and a strong key could collide in principle; Rice is tried first, as in
  GLideN64.

## Verification

- Unit: the strong hash of fixed byte vectors equals the pinned upstream function's
  output (vectors from the A148 evidence: "1ST" 192 bytes → 82637937; Captain Falcon
  → 735e555d), including a row stride larger than the active row and a row narrower
  than 4 bytes; CI composition.
- Runtime: A148 serves in F-Zero X race scenarios; RE2 Reloaded and Turok 3 Reloaded
  packages serve their strong assets where a scenario shows them (or report that no
  scenario reaches them); hi-res-on end states equal feature-off.
- Performance: unthrottled fps for the benchmark scenes with their packs does not drop
  by more than noise.

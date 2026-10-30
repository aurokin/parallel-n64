# ADR-0024: Pack selection identity

- Status: Accepted (offline implementation; runtime and corpus proof pending)
- Date: 2026-10-30

## Decision

Compute one ident from a canonical ROM header: Name bytes 0x20–0x33 through
the first NUL, trailing spaces trimmed, `:` and `/` replaced by `-`. This is
the documented GLideN64 convention, implemented independently in MIT code.
CPSJ and CP2J use their four-character code to avoid selecting Western Stadium
packs. Blank, non-ASCII, unprintable or filename-invalid Names use the code too;
an unprintable code leaves no identity. Other carts use no region, revision or
CRC restriction. Hacks retaining a base header intentionally share its packs.

List `system/parallel-n64/hires/` once, only with hi-res enabled. Pure selection
policy takes the listing and selects ASCII case-insensitive ident matches:
standalone `.phrb` files first, then matching directories and their top-level
`.phrb` files, all in byte order and skipping dotfiles. Later packages keep
winning. The explicit `PARALLEL_RDP_HIRES_CACHE_PATH` tooling override still
bypasses game selection, including when the header has no usable identity.
Shared GLideN64-name entries for CPSJ/CP2J are diagnosed as packs for a
different game, without rename advice. Punctuation folding detects near misses
only; it never selects packages.
In particular `FZEROX` must not alias to retail `F-ZERO X`.

After indexing, count each package's unique runtime keys and keys hidden by a
later package, without altering any lookup index. Rice keys are checksum64 +
formatsize + selector; Jabo keys are MD5. The namespaces stay separate. Warn
once per load when more than 50% of a package's keys are hidden, naming the
final hider or “later packs” when several contribute. Log every package's
`records` and `hidden_by_later`. Cross-lane Rice-first/Jabo-second competition
is an accepted diagnostic limit.

Reuse ADR-0023's MIT renderer status and GPL notification path, with extended
messages and legacy fallback, once per event per content load. Identity owns
the loaded line and missing-pack diagnostics; memory owns the budget suffix,
rejections, GPU-support, pressure and read-error events. Loaded text is now
`Hi-res textures: 2 packs for MARIOKART64 - 16311 textures, 512 MB memory budget`.
A mostly hidden package additionally warns. Without a loaded package, report
near misses, then unconverted sources, then unusable identity, then no pack.
Unconverted detection uses the GLideN64 ident in the current hires root and
exact legacy Mupen64plus cache/hires_texture paths (no legacy directory scan);
see the [install contract](../../assets/TEXTURE_PACKS.md#install-and-load-diagnostics).

## Consequences and verification

No new option, runtime alias matching, in-core conversion, PHRB metadata or
version change. Converter ROM checks and installation flags are Phase 2, and
remain unimplemented here. An optional PHRB identity block could be placed
before the record table (v2–v8 readers use record_table_offset and blob_offset),
but no such block is needed or added.

Display-free tests use a shared JSON header fixture for future converter parity,
synthetic directory listings for selection and every unconverted-hint location,
and tiny PHRBs for full overlap, 5% add-on ordering, the strict 50% boundary,
key uniqueness, selectors/formatsize and lane separation. Core build and the
required offline gate do not establish live notification visibility, real-pack
corpus selection, or feature-off capture parity. No emulator is launched here.

# N64 Exact Key Delta Sheet

> **Status (2026-06):** research record from 2026-03 (Attempt B). The action items
> below are superseded by the Identity Decision in
> [docs/REBOOT_PLAN.md](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md) and
> [ADR-0006](/home/auro/code/parallel-n64/docs/adr/0006-replacement-identity.md):
> the GlideN64-compat Rice-CRC lane is the primary replacement-identity path and
> the native-sampled-identity program is frozen (code dormant, debug-flag only,
> no gates). Kept as the derivation record of which N64 identity fields exist and
> why upload-blob keying missed.

## Purpose (as written, 2026-03)

- Make the then-current ParaLLEl exact hi-res key explicit
- Compare it against the latest N64 identity research
- Name the highest-probability gaps behind the then-current Paper Mario CI/menu misses

## ParaLLEl Exact Key (as of 2026-03, Attempt B)

Today's primary lane is instead the GlideN64-compat Rice-CRC computed at draw time
from the tile descriptor (`rdp_renderer.cpp`, `rice_crc32_wrapped`; bank-0 palette
candidate for CI4 per ADR-0013), with the sampled-object exact lookup surviving
only behind the `hires_debug_sampled_object_exact_lookup` debug flag.

The Attempt B exact lookup described below was effectively:

- `checksum64 = compose_hires_checksum64(texture_crc, palette_crc)`
- `formatsize = formatsize_key(meta.fmt, meta.size)`

Current source:

- [rdp_renderer.cpp](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_renderer.cpp)
- [rdp_hires_ci_palette_policy.hpp](/home/auro/code/parallel-n64/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_ci_palette_policy.hpp)

What that meant (2026-03):

- `texture_crc` is computed from the raw RDRAM upload bytes selected by:
  - source base address
  - `key_width_pixels`
  - `key_height_pixels`
  - source VRAM bpp
  - source row stride
- `palette_crc` is only populated for CI textures when TLUT shadow state is valid
- `formatsize` comes from the sampled tile format/size

## What The Attempt B Exact Key Included

Directly included:

- raw uploaded texel bytes for the selected source rectangle
- CI palette influence through the current shadow-based palette CRC
- sampled tile `fmt`
- sampled tile `siz`
- CI4 palette bank input through `meta.palette` inside the current palette-CRC path

Implicitly but not explicitly named:

- width/height only affect lookup through the raw texel CRC input
- source row stride only affects lookup through the raw texel CRC input

## What The Attempt B Exact Key Did Not Include Explicitly

- `LoadTile` vs `LoadBlock` provenance
- copy-cycle vs normal textured draw provenance
- framebuffer-derived vs authored-RDRAM source classification
- `tlut_type`
- TMEM base address as a first-class identity field
- TMEM line/stride as a first-class identity field
- tile window origin as a first-class identity field
- clamp / mirror / mask / shift as first-class identity fields
- `SetTileSize` sampled window semantics as first-class identity fields
- texrect / BG-copy style path classification

## What The Research Says Should Matter

The latest `n64_docs` pass points at the post-load sampled tile as the authoritative object, not the raw upload blob alone.

High-confidence identity fields:

- sampled `fmt` / `siz`
- CI vs direct
- TLUT enabled
- `tlut_type`
- CI4 palette bank semantics
- TMEM address
- TMEM line/stride
- tile window / origin
- clamp / mirror / mask / shift
- upload provenance: `LoadTile` vs `LoadBlock`

High-confidence provenance classes:

- authored RDRAM texture load
- copy / texrect / BG-copy style path
- framebuffer-derived or readback-derived source

## Gaps Identified (2026-03)

### 1. We May Still Be Keying The Wrong Object

The Attempt B exact lookup was dominated by raw upload bytes plus its palette CRC.

Research direction:

- the N64 texture object is closer to the sampled tile after load semantics
- `SetTile`, `SetTileSize`, TMEM address/line, and sampler state can change meaning without changing the raw upload blob

Why this matters for current misses:

- it explains why repeated CI/menu misses can survive many CRC experiments

### 2. `tlut_type` Is Missing From Exact Identity

The Attempt B palette CRC computation did not take `tlut_type`.
(2026-06 note: the logical TLUT diagnostic decode now takes `tlut_type`
(`rdp_hires_ci_palette_policy.hpp`), while the Rice-CRC palette CRC intentionally
hashes raw TLUT words for GlideN64 compatibility.)

Research direction:

- the same TLUT words can decode differently under RGBA16 vs IA16 TLUT interpretation

Why this matters:

- exact CI identity can be wrong even when the raw palette bytes are “correct”

### 3. TMEM / Sampler State Were Only Side Conditions

The Attempt B exact lookup did not carry TMEM address, TMEM line, or sampler-state fields explicitly.

Research direction:

- those fields affect the sampled result and should not be treated as mere logging/debug context

Why this matters:

- current Paper Mario menu misses may be “same upload, different sampled object”

### 4. Provenance Is Not Part Of The Acceptance Story Yet

Current lookup does not distinguish authored texture classes from copy-cycle or framebuffer-derived content.

Research direction:

- copy / texrect / BG-copy and framebuffer-derived content should be explicit provenance classes

Why this matters:

- some “missing textures” may not be authored replacement candidates at all

## Conclusion At The Time (superseded 2026-06-10)

The likely next breakthrough is not a broader compatibility rule.

It is:

1. make provenance visible in strict bundles
2. make CI/TLUT identity more logical
3. move exact lookup closer to the sampled N64 object
4. keep compatibility/import policy explicit for the remaining ambiguous families

(2026-06 annotation: the reboot outcome contradicted this conclusion directly —
the adopted path WAS the broader compatibility rule (the GlideN64-compat Rice-CRC
lane, see the Reboot Plan Identity Decision), and item 3 is the program that was
frozen. The field analysis above remains the durable part of this sheet.)

## Direct Sampled-Object Evidence

A strict file-select sampled-object probe (2026-03) turned the abstract delta into a concrete mismatch:

- dominant upload-side miss family:
  - upload family `ab53409b` / `pcrc=00000000`
  - sampled draw-side object `fmt=2 siz=0 off=0 stride=8 wh=16x16 fs=2`
  - sampled key `7064585c`
  - sampled palette CRCs `a1c4a352` / `7ff0e39c`
- representative visible upload-side CI family:
  - upload family `2a1be0a4` / `pcrc=5c7e801a`
  - sampled draw-side object `fmt=2 siz=0 off=0 stride=32 wh=64x16 fs=2`
  - sampled key `c139c1c0`
  - sampled palette CRCs `80038dc8` / `7ff2e39c`
- neither sampled key exists in the current legacy pack index even though the upload-side low-32 keys do

That means the missing field set is not just “one more palette tweak.” The canonical identity object has shifted.

(2026-06 note: the legacy `.hts` pack index described here is gone — runtime packs
are `.phrb`-only (ADR-0007), and the misses this probe chased were later resolved
through the Rice-CRC compat lane and pack curation, not sampled-object keying.)

## Immediate Follow-On Work (superseded 2026-06-10)

All items below belong to the frozen native-sampled program; in particular the
imported-records design was abandoned — imported records are `.phrb` compat
exact-variant sets via `hts2phrb` (ADR-0007).

- keep the new provenance logging on strict title/file fixtures
- keep the logical TLUT diagnostic view that includes `tlut_type`
- compare the current exact key against one sampler-aware candidate model before changing default runtime behavior
- design imported records around sampled-object canonical IDs with explicit legacy upload-family aliases

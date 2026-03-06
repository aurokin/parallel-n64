# Hi-Res Texture Task Tracker

## Scope
- Target: latest hardware only.
- GPU path: descriptor indexing only.
- Fallback: auto-disable when required Vulkan capabilities are unavailable, with a logged reason.
- CI policy: local-only for now.
- Local cache artifacts (`*.htc`, `*.hts`) stay ignored by git.

## Core Invariants
- When HIRES is off, native rendering behavior must remain unchanged.
- HIRES-off mode must bypass replacement-provider lookup, registry upload/binding, and shader replacement sampling.
- HIRES-off mode must not mutate TMEM behavior, combiner behavior, blender behavior, or VI behavior.
- When HIRES is on, replacement happens only at texel fetch output before combiner/blender.
- Replacement logic must stay runtime-bypassable behind a cheap predicate.

## Vulkan Capability Contract
HIRES replacement requires all of the following:

- `supports_descriptor_indexing`
- `runtimeDescriptorArray`
- `shaderSampledImageArrayNonUniformIndexing`
- `descriptorBindingVariableDescriptorCount`
- `descriptorBindingPartiallyBound`
- `descriptorBindingSampledImageUpdateAfterBind`
- `maxDescriptorSetUpdateAfterBindSampledImages >= 4096`

If any requirement is missing:
- HIRES disables at runtime.
- The disable reason is logged.
- Provider/registry work stays off.

## Milestones
- [x] M0: Repo hygiene for local packs.
- [x] M1: Core options and runtime plumbing.
- [x] M2: Replacement provider (`.htc` + `.hts` parse/decode).
- [x] M3: Keying replication + logging harness.
- [x] M4: GPU registry with bindless descriptor uploads.
- [x] M5: Texel-stage replacement swap before combiner.
- [x] M6: CI/TLUT keying correctness.
- [x] M7: Filtering, LOD, and budget controls.
- [ ] M8: Validation, performance characterization, and final rollout docs.

## Current Status
- `M0` through `M7` are implemented locally.
- Current local readiness gate: `./run-tests.sh --profile hires-readiness`.
- Current full local gate: `./run-tests.sh`.
- Current runtime smoke target: `./run-n64-smoke-state.sh -- --verbose`.
- Latest Paper Mario state smoke is clean:
  - `lookups=13031 hits=13031 misses=0`
  - `bound_hits=13031 unbound_hits=0`
  - `provider=on`
- HIRES-off behavior remains an explicit invariant, not an optimization target.

## Stable Implementation Notes

### M4 Registry
- Replacement uploads are lazy.
- Descriptor indexing is the only supported runtime path.
- Descriptor residency is tracked with local counters and optional budget enforcement.

### M5 Sampling Path
- Replacement sampling happens at the texel stage, before combiner/blender.
- Copy-mode replacement sampling repacks back to R5G5B5A1 for framebuffer-copy compatibility.
- Upscaling-aware replacement sampling is normalized by `SCALING_FACTOR` so 4x output does not distort replacement UVs.

### M6 Keying
- `(checksum64, formatsize)` lookup is active.
- CI palette CRC candidates are deduplicated and tried before miss classification.
- CI low32 fallback supports:
  - strict unique match
  - preferred palette-hint match
  - deterministic newest-entry fallback when ambiguity remains
- TLUT shadow updates are TMEM-relative and clipped, preserving unrelated palette regions.

### M6/M7 Coverage Hardening
- LOAD_BLOCK keying includes:
  - row-stride-aware width fallback
  - block-tile offset fallback
  - block-shape reinterpret fallback
- Replacement tile bindings are propagated across valid alias groups, but stale shared-offset source reuse is explicitly rejected.
- Budget enforcement supports eviction and oversized-upload rejection accounting.

## Recent Fixes That Must Stay
- Bindless upload view selection now falls back to `set_texture()` when explicit unorm/srgb views are unavailable. This fixed black replacement textures.
- Tile alias reuse no longer inherits stale replacement bindings across descriptor-only updates. This fixed the corrupted Paper Mario file-select scene.
- Block lookup helpers now cover legacy pack encodings that key against narrower effective row widths or rebased block offsets.

## Probes Removed
These were explored and then intentionally removed because they did not improve the validated Paper Mario scene:
- host-visible TMEM recovery request during RDP init
- TLUT shadow rebuild from mapped TMEM
- extra CI ambiguous-candidate and TMEM-shadow debug logging
- experimental wrap/clamp sampling probe that did not change output

## Local Validation
Run these locally before and after HIRES changes:

1. `./run-build.sh`
2. `./run-tests.sh`
3. `./run-tests.sh --profile hires-readiness`
4. `./run-n64-smoke-state.sh -- --verbose`

Expected smoke characteristics on the current Paper Mario state:
- cache loads from RetroArch system directory
- provider remains `on`
- `unbound_hits=0`
- current baseline is `lookups=13031 hits=13031 misses=0`

Disabled-path expectation:
- with HIRES off, replacement activity must be fully bypassed
- native rendering path must remain behaviorally unchanged
- any regression seen with HIRES off is a blocker for further HIRES work

## Local Tools
- Mini-pack generator: `tools/hires_minipack.py`
- Generate from keys:
  - `python3 tools/hires_minipack.py from-keys --keys keys.csv --out-dir ./cache_minipack --name MINIPACK --emit hts,htc --scale 4 --compress none`
- Validate generated pack:
  - `python3 tools/hires_minipack.py validate --path ./cache_minipack`

## Remaining M8 Work
- Extend conformance coverage beyond the current Paper Mario smoke scene.
- Characterize performance and residency behavior under realistic pack pressure.
- Document rollout expectations and known hardware requirements.
- Revisit minipack hash conformance once fixture details are finalized.

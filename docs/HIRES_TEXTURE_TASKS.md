# Hi-Res Texture Status

## Scope
- Target: latest hardware only.
- GPU path: descriptor indexing only.
- Fallback: auto-disable when required Vulkan capabilities are unavailable, with a logged reason.
- Local cache artifacts (`*.htc`, `*.hts`) stay ignored by git.

## Invariants
- When HIRES is off, native rendering behavior must remain unchanged.
- HIRES-off mode must bypass replacement-provider lookup, registry upload/binding, and shader replacement sampling.
- HIRES-off mode must not change TMEM behavior, combiner behavior, blender behavior, or VI behavior.
- When HIRES is on, replacement happens only at texel fetch output before combiner/blender.

## Required Vulkan Capabilities
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

## Current Verified State
- Replacement provider parses `.htc` and `.hts`.
- Bindless descriptor-indexed upload path is active.
- Texel-stage replacement swap is active.
- Copy-mode replacement sampling is active.
- CI/TLUT-aware keying is active.
- Budget, eviction, filtering, and sRGB policy wiring are active.
- Current Paper Mario state smoke is clean:
  - `lookups=13031 hits=13031 misses=0`
  - `bound_hits=13031 unbound_hits=0`
  - `provider=on`

## Current Local References
- Fork under active development: `/home/auro/code/parallel-n64`
- Upstream parallel-n64 reference: `/home/auro/code/mupen/parallel-n64-upstream`
- Upstream paraLLEl-RDP reference: `/home/auro/code/mupen/parallel-rdp-upstream`
- RetroArch reference: `/home/auro/code/mupen/RetroArch-upstream`
- GLideN64 reference: `/home/auro/code/mupen/GLideN64-upstream`
- ROMs: `/home/auro/code/n64_roms`

## Still Incomplete Or Not Fully Proven
- Validation is still concentrated on the current Paper Mario scenes; broader scene and ROM coverage is incomplete.
- Performance and residency behavior under larger packs still needs characterization.
- Minipack hash conformance is still deferred pending fixture details.
- Rollout documentation for hardware expectations and limits is still incomplete.

## Removed Probes
- host-visible TMEM recovery request during RDP init
- TLUT shadow rebuild from mapped TMEM
- extra CI ambiguous-candidate and TMEM-shadow debug logging
- experimental wrap/clamp sampling probe that did not change output

## Local Validation
1. `./run-build.sh`
2. `./run-tests.sh`
3. `./run-tests.sh --profile hires-readiness`
4. `./run-n64-smoke-state.sh -- --verbose`

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

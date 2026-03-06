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
- Paper Mario button-path scene loads HIRES replacements on `parallel`.
- GLideN64 oracle path is reproducible with `./run-paper-mario-gliden64-capture.sh`.
- Preserved GLide oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/gliden64/oracle-gliden64-5`

## Current Local References
- Fork under active development: `/home/auro/code/parallel-n64`
- Read-only upstream parallel-n64 reference: `/home/auro/code/mupen/parallel-n64-upstream`
- Read-only upstream paraLLEl-RDP reference: `/home/auro/code/mupen/parallel-rdp-upstream`
- Read-only RetroArch reference: `/home/auro/code/mupen/RetroArch-upstream`
- Read-only GLideN64 reference: `/home/auro/code/mupen/GLideN64-upstream`
- Read-only Paper Mario decomp/reference: `/home/auro/code/paper_mario/papermariodx`
- Read-only Paper Mario pack source: `/home/auro/code/paper_mario/PAPER MARIO_HIRESTEXTURES.hts`
- ROMs: `/home/auro/code/n64_roms`

## Still Incomplete Or Not Fully Proven
- Validation is still concentrated on Paper Mario; broader scene and ROM coverage is incomplete.
- Current `parallel` Paper Mario button-path scene still diverges from the GLide oracle.
- The main visible gap is copy-mode stage-border corruption in the file-select scene.
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
5. `./run-paper-mario-hires-capture.sh --tag <tag>`
6. `./run-paper-mario-gliden64-capture.sh --tag <tag>` when an oracle refresh is needed

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
- Paper Mario `parallel` capture:
  - `./run-paper-mario-hires-capture.sh --tag <tag>`
- Paper Mario GLide oracle capture:
  - `./run-paper-mario-gliden64-capture.sh --tag <tag>`

## Paper Mario Notes
- Comparable scene path:
  1. boot
  2. wait `20s`
  3. press `Start`
  4. wait `5s`
  5. press `Start`
  6. wait `2s`
- Do not use save states from `/home/auro/code/paper_mario`.
- Those save states were produced from a modified ROM and a different emulator.
- Use the preserved GLide oracle before re-running GLide unless the scene or workflow changes materially.

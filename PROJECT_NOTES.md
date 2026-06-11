# Parallel N64 Hi-Res / Scaling Project Notes

This document is the running record of current thinking for the hi-res texture and scaling work in this repo.

It is intentionally not a final implementation plan. The goal is to collect what we know, what we suspect, what we learned from past work, and what constraints should shape the multi-phase plans we create later.

## Why This Exists

We spent significant time trying to make stable hi-res texture support and proper scaling happen in this codebase and did not get the result we wanted.

This restart should not behave like a normal feature branch. It should behave like a renderer program:

- preserve baseline stability
- learn from the failed attempt
- improve confidence and observability before major implementation
- split hard problems into testable phases
- keep enough flexibility to adapt if new evidence changes the approach

## Core Goals

- Load hi-res textures that scale properly with resolution.
- Keep baseline core behavior intact when hi-res/scaling support is disabled.
- Stay as close as practical to the standard renderer path when features are enabled, while accepting that enabled hi-res support is no longer pure LLE.
- Make the feature stable enough for real gameplay, not just isolated demos.
- Build the tooling needed for autonomous and semi-autonomous debugging, including save-state-driven testing and repeatable scene capture.

## Non-Negotiable Constraints

- If the new support is disabled, behavior should remain as close as possible to the standard stable renderer path.
- Any divergence from standard behavior must be deliberate, observable, and attributable.
- We should not force large architectural changes outside the video core unless there is a clear blocker.
- We should favor deterministic reproduction over ad hoc manual testing.
- We should not confuse "detected replacement texture" with "correctly integrated replacement texture."

## Current Working View

The current hi-res implementation should be treated as a prototype / spike, not a stable base that only needs polishing.

Reasons:

- The hook for enabling/disabling hi-res replacement is fairly contained.
- The renderer does perform hi-res keying and cache lookup.
- However, the current code does not yet look like a complete end-to-end replacement path.
- Several signs point to "lookup and metadata plumbing started" rather than "replacement rendering is fully integrated."

This is important because it changes the mindset:

- We should reuse the useful separation and cleanup work already done.
- We should not assume the current branch is close to feature-complete.
- We should be ready to redesign the hi-res path while keeping the repo cleanup and testability improvements.

## Current Code Observations

### 1. Gating Is in Better Shape Than the Feature Itself

The hi-res enable path is gated from the frontend/config side in files such as:

- `mupen64plus-video-paraLLEl/rdp.cpp`
- `mupen64plus-video-paraLLEl/parallel.cpp`
- `libretro/libretro.c`
- `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_device.cpp`

That is good news. It supports the requirement that the disabled path should remain intact and that the feature can stay opt-in.

### 2. The Current Renderer Path Appears Partial

The current renderer performs hi-res lookup during texture upload/keying.

Observed behavior:

- A replacement provider is attached to the renderer.
- Texture CRC / key computation is performed.
- A lookup is attempted against the replacement cache.
- Hit/miss information is tracked per tile.

Concerns:

- `ReplacementMeta.orig_w` and `ReplacementMeta.orig_h` do not appear to be populated in the current lookup path.
- `decode_rgba8()` exists in the replacement provider, but at the time of review it does not appear to have any call sites.
- The lookup result metadata appears underused after the hit/miss decision.

Working interpretation:

- We likely have the beginning of a replacement identification system.
- We do not yet appear to have a fully wired replacement upload/bind/sample/render pipeline.
- This likely explains why the feature did not reach stable behavior even after serious effort.

### 3. Hi-Res Support and Resolution Scaling Are Related but Not the Same Problem

We should separate:

- hi-res replacement texture support
- texture scaling / texrect / LOD / coordinate behavior under increased internal resolution

These two features interact, but they should not be planned as one indivisible implementation blob.

If we combine them too early, we increase ambiguity:

- Was a bug caused by replacement texture identity?
- By replacement upload?
- By sampler state?
- By UV normalization?
- By texrect math?
- By internal upscaling?
- By VI presentation?

We need a shared foundation, but separate phases.

## What Looks Worth Keeping

- The repo cleanup and refactoring work that improved readability and isolated logic into more testable policy layers.
- The existing test runner and local tiering model.
- The dump capture and replay validation direction.
- The decision to work from a cleaner point in history instead of piling onto a chaotic branch.

The project should build on these, not discard them.

## Tooling Assessment

There is already more structure here than a typical emulator fork:

- `run-tests.sh` provides local CTest orchestration and profiles.
- `docs/EMU_TESTING.md` documents test tiers and local workflows.
- `tests/emulator_behavior/` contains a non-trivial unit and behavior test surface.
- `tools/capture-rdp-dump.sh` shows that capture/replay workflows are already part of the repo story.

However, there is still a major confidence gap for this project.

### Tooling Gaps That Matter

- No obvious root-level formatting/lint flow for the C/C++ renderer code.
- No obvious root-level `clang-tidy` / `cppcheck` / static analysis workflow.
- No visible coverage-oriented workflow for the policy/testable logic that has already been extracted.
- Existing tests are useful, but they are not yet a dedicated hi-res/scaling confidence harness.
- We need faster scene reproduction and comparison loops built around savestates and known game points.
- Agents need one-command workflows for:
  - launch
  - load state / reach scene
  - toggle feature
  - capture frame or dump
  - compare against baseline
  - emit logs with enough context to explain divergence

### Conclusion on Tooling

Tooling is not overhead for this effort. Tooling is part of the implementation.

If we want agents to make repeated renderer changes safely, we need deterministic feedback that is:

- local
- fast
- reproducible
- scoped to the feature area
- understandable when it fails

## Suggested Project Shape

This should be approached in layers.

### Layer 0: Guardrails and Reproducibility

Before serious feature implementation, improve:

- formatting/lint/static-analysis entry points
- sanitizer coverage where practical
- save-state-based scene reproduction
- frame capture and comparison utilities
- feature-flag-driven logging and metrics

This does not mean a giant cleanup detour. It means building only the guardrails that directly support hi-res/scaling work.

### Layer 1: Observability Before Behavior Change

Before trying to render replacement textures correctly, make it easy to answer:

- what texture was keyed?
- what CRC / palette CRC / formatsize was used?
- was there a cache hit?
- what tile / TMEM / texrect state was active?
- what original dimensions were inferred?
- what replacement dimensions were selected?
- what scaling factor and sampler decisions were in effect?

If we cannot observe those things cheaply, we will repeat the same debugging failures.

### Layer 2: Native-Resolution Replacement First

Do not start with "perfect hi-res under every upscale mode."

Instead:

- prove replacement identity
- prove replacement loading
- prove replacement upload/bind/use
- prove correct sampling at native/internal-1x behavior first

Only after this is stable should we widen the feature surface.

### Layer 3: Resolution Interaction

After native-resolution replacement is stable, then address:

- internal upscaling interaction
- texrect behavior
- texture coordinate mapping
- texture LOD behavior
- filtering and mip behavior
- framebuffer / VI presentation interactions

This is likely the real heart of the project.

### Layer 4: Runtime Stability and Game Matrix

Once the model is correct in focused scenes:

- expand game coverage
- collect save states for known difficult cases
- build a curated compatibility matrix
- separate "known acceptable divergence" from "regression"

## Architecture Direction

Current opinion:

- Keep the disabled path as close as possible to baseline behavior.
- Isolate enabled-path logic behind early, explicit gates.
- Prefer small, explicit interfaces between:
  - keying
  - replacement lookup
  - replacement upload/registry
  - mapping from original texture semantics to replacement texture semantics
  - scaling / texrect / LOD policies

The most important architectural rule is this:

We should make the enabled path explainable.

If a replacement texture is used, we should be able to explain:

- why it matched
- what original texture semantics it replaced
- how dimensions were mapped
- what coordinate/scaling rules were applied
- why the final output differs from baseline

If we cannot explain that chain, the implementation is not ready.

## On Using Other Emulators and Documentation

We should absolutely use other emulators and N64 documentation as references.

But we should use them carefully:

- use them to understand texture identity, TLUT behavior, texrect behavior, UV math, LOD choices, and pack conventions
- do not blindly transplant architecture that assumes a different renderer model
- document where we intentionally follow another implementation and where we intentionally diverge

External references should guide us, not replace thinking.

## On Forking Other Parts of the Stack

We may need forks outside the video core, including RetroArch or adjacent tooling, for autonomous testing and debugging.

Current opinion:

- do not fork higher in the stack until we can point to a concrete blocker
- prefer thin harnesses and wrappers first
- fork only where it materially improves deterministic reproduction or feature observability

Possible reasons to fork later:

- automated scene navigation
- scripted savestate load pipelines
- richer debug controls
- better frame capture hooks
- agent-friendly input / launch / capture orchestration

But that should be a justified expansion, not the first move.

## Risks

- Repeating a feature-first approach without stronger observability.
- Treating hi-res replacement and scaling as one monolithic implementation.
- Accidentally destabilizing baseline renderer behavior in the disabled path.
- Spending too long on generalized cleanup that does not directly support the renderer goals.
- Importing assumptions from other emulators that do not fit ParaLLEl-RDP's constraints.
- Underestimating texrect / LOD / coordinate edge cases.

## Open Questions

- What exact failure modes did the previous implementation hit in practice?
- Which games/scenes are the best canonical save-state fixtures for this project?
- What pack/cache format should be treated as the initial supported target?
- How much of the current replacement cache format is worth preserving?
- Where should replacement image residency and lifetime be managed?
- What is the cleanest contract between replacement texture semantics and the existing renderer/shader path?
- Which scaling bugs belong to the standard renderer versus the new feature path?

## Immediate Direction Before Multi-Phase Planning

The next step should not be "jump into implementation."

The next step should be:

1. Keep enriching this document with findings from local code review and past lessons.
2. Identify the exact status of the current hi-res prototype, including what is incomplete versus what is merely unstable.
3. Define the minimum tooling upgrades needed for safe planning.
4. Build the first stable problem statement for:
   - hi-res replacement correctness
   - scaling correctness
   - disabled-path invariants
5. Turn those into multiple multi-phase plans and choose the one with the best stability/confidence profile.

## Current Bottom Line

This project is viable.

But it should be treated as:

- a renderer redesign in a constrained area
- supported by tooling and deterministic testing
- staged across multiple phases
- aggressively protective of the disabled baseline path

The prior year of work was not wasted. It produced cleaner code, better separation, and better knowledge of what does not work. That is exactly the material we should use to make a stronger plan now.

## Research Sweep: Cross-Emulator Patterns and N64 Behavior

This section captures findings from the local reference trees:

- `/home/auro/code/emulator_references`
- `/home/auro/code/n64_docs`

The goal of this sweep was not to copy another emulator architecture blindly. The goal was to identify stable patterns that transfer well to Parallel N64, and to identify N64-specific behavior that makes some generic emulator patterns unsafe.

### Cross-Emulator Patterns That Transfer Well

Across DuckStation, PCSX2, PPSSPP, Flycast, Dolphin, and Azahar, the strongest shared pattern is this:

- keep enhancement systems narrowly scoped
- integrate them at one explicit renderer boundary
- preserve baseline/native behavior when the feature is disabled

That pattern shows up in different forms, but the architectural conclusion is consistent.

#### 1. Texture Replacement Should Be Its Own Subsystem

The strongest references here are:

- `duckstation-upstream/src/core/gpu_hw_texture_cache.cpp`
- `pcsx2-upstream/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp`
- `ppsspp-upstream/GPU/Common/TextureReplacer.cpp`
- `flycast-upstream/core/rend/CustomTexture.cpp`

Shared lessons:

- replacement discovery/loading should not be scattered across draw code
- replacement metadata, indexing, decode, reload, and dump policy should live together
- async disk/decode work should happen off the render thread
- GPU upload/final binding should still be injected through the renderer thread / renderer-owned boundary

This argues for a Parallel N64 design where hi-res replacement is a distinct subsystem with explicit responsibilities:

- key generation / identity
- replacement metadata/index lookup
- CPU-side decode/cache
- GPU residency/registry
- policy for reload / dumping / budget / eviction

#### 2. Texture Upscaling Must Be Separate From Internal Resolution Scaling

The cleanest references here are:

- `flycast-upstream/core/rend/TexCache.cpp`
- `duckstation-upstream/src/core/gpu_hw_shadergen.cpp`
- PCSX2 upscaling-fix model in the GS renderer stack

Shared lesson:

- texture upscaling is not the same problem as render/output/internal-resolution scaling

This is directly relevant to Parallel N64. If we merge these concepts too early, we lose the ability to answer whether a bug comes from:

- replacement texture identity
- replacement sampling
- texrect coordinate math
- internal upscaling math
- VI presentation behavior

This reinforces the project direction already suspected locally:

- hi-res replacement
- texture upscaling
- internal RDP upscaling
- VI presentation scaling

should be treated as distinct systems with explicit handoff points.

#### 3. Capture, Dump, and Screenshot Paths Should Be Renderer-Owned Services

The strongest references here are:

- `dolphin-upstream/Source/Core/VideoCommon/FrameDumper.cpp`
- `dolphin-upstream/Source/Core/VideoCommon/Present.cpp`
- `azahar-upstream/src/video_core/renderer_base.h`
- `azahar-upstream/src/video_core/renderer_opengl/frame_dumper_opengl.cpp`
- `ppsspp-upstream/Core/Screenshot.cpp`
- `pcsx2-upstream/pcsx2/GS/Renderers/Common/GSRenderer.cpp`

Shared lessons:

- UI code should request capture, not perform capture directly
- presentation policy should be separated from capture mechanics
- async readback/encode should be the default
- capture requests should be scheduled and deterministic

For Parallel N64, that suggests:

- frame capture should hang off a renderer-owned interface
- capture should understand whether it is grabbing:
  - native RDP output
  - upscaled RDP output
  - VI-processed output
  - dump/replay validation output
- capture should be scriptable and safe for automation

#### 4. Minimal Replayable Graphics Artifacts Are Critical

The strongest references here are:

- `pcsx2-upstream/pcsx2/GSDumpReplayer.cpp`
- `dolphin-upstream/Source/Core/Core/FifoPlayer/FifoRecorder.cpp`
- `duckstation-upstream/src/duckstation-regtest/regtest_host.cpp`
- `parallel-rdp_README.md` in local docs

Shared lesson:

- renderer work becomes dramatically more tractable when bugs can be replayed from a minimal graphics artifact instead of full gameplay repro

For Parallel N64, the direct analogue is not GS dump or GC/Wii FIFO capture. The analogue is:

- RSP/RDP command-stream-centered replay
- tightly scoped memory/TMEM/TLUT state capture
- embedded preview screenshots for triage

This is an argument for expanding the existing dump/replay story rather than treating it as optional tooling.

#### 5. Enhancement Features Should Stay Explicitly Opt-In and Isolated

The strongest references here are:

- `dolphin-upstream/Source/Core/VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.cpp`
- `duckstation-upstream/src/core/gpu_hw.cpp`
- `pcsx2-upstream/pcsx2/VMManager.cpp`

Shared lessons:

- enhanced paths should be activated deliberately
- replay/debug modes may need different guardrails than regular emulation
- native/baseline paths should not be contaminated by enhancement-specific assumptions

This matches one of the core non-negotiables for this project:

- when the feature is disabled, the core should behave like the stable baseline path

### Cross-Emulator Patterns That Do Not Transfer Cleanly

Some useful patterns should only transfer at the architectural level, not at the implementation level.

Examples:

- PS1/PS2-specific texture identifiers, VRAM hashes, CLUT naming, half-pixel offsets, and palette-draw hacks do not transfer to N64 directly.
- GameCube/Wii FIFO recording is not directly reusable. The transferable idea is minimal graphics repro capture, not the exact recorder format.
- GL-mailbox and PBO frame-dumper implementations in Azahar transfer as async readback ideas, not as concrete implementation templates.
- Hash-only replacement schemes are risky for N64 because raw texel bytes are not enough when TLUT/TMEM semantics matter.
- Generic "modern bilinear" assumptions do not transfer because N64 texture filtering behavior is special.

### N64 Behavior Findings That Matter Most

The doc sweep sharpened several areas that are likely to dominate correctness work.

#### 1. Texrects Are the Highest-Risk Primitive For Hi-Res Scaling

The most important references are:

- `n64brew_Reality_Display_Processor_Commands.html`
- `official_manual/.../gSPTextureRectangle.htm`

Important behavior:

- texrect coordinates and texture coordinates use fixed-point formats with non-trivial rounding behavior
- `COPY` / `FILL` and `1-cycle` / `2-cycle` do not agree on lower-right edge behavior
- `COPY` mode has step-size requirements tied to fetch width

Implication:

- keeping texrects at native resolution is not just a hack; it is likely a correctness-preserving strategy for a large class of HUD/sprite/menu behavior

This supports the current existing quirk in `rdp_tex_rect_policy.hpp`.

#### 2. TMEM / Tile / TLUT State Is Part Of Texture Meaning

The most important references are:

- `n64brew_Reality_Display_Processor_Commands.html`
- `official_manual/.../pro14/14-03.htm`
- `official_manual/.../qa/graphics/texture.htm`

Important behavior:

- TMEM is only 4 KB and layout-sensitive
- tile fields are not cosmetic metadata; they change how texture data is interpreted
- CI/TLUT behavior changes the semantic identity of what is sampled
- `LoadBlock`, `LoadTile`, and `LoadTLUT` all have fragile edge conditions

Implication:

- hi-res replacement identity for N64 cannot be a naive decoded-texture hash
- replacement keys must account for TLUT/palette state where relevant
- this validates the current direction of shadowing TLUT state and including palette CRC in lookup keys

#### 3. N64 Filtering Is Not Generic Bilinear

The most important references are:

- `official_manual/.../pro14/14-01.htm`
- `official_manual/.../gDPSetTextureFilter.htm`

Important behavior:

- `G_TF_BILERP` is the characteristic N64 3-point triangular filter
- some exact averaging behavior is a special case rather than the default

Implication:

- enhanced replacement/upscale paths cannot assume modern GPU bilinear is visually equivalent
- if we bind hi-res textures and sample them with ordinary bilinear while the original content expected N64 filtering behavior, edges, UI, and mip transitions will drift

#### 4. LOD And Mip Behavior Is Tile-Relative

The most important references are:

- `official_manual/.../tutorial/graphics/9/9_3.htm`
- `official_manual/.../tutorial/graphics/9/9_6.htm`
- `official_manual/.../tutorial/graphics/9/9_7.htm`
- `official_manual/.../gDPSetTextureLOD.htm`

Important behavior:

- LOD chooses relative tile pairs, not generic image mips in the modern sense
- detail/sharpen behavior depends on neighboring tile layout
- texrect behavior can differ from ordinary primitive assumptions

Implication:

- future hi-res replacement support should expect LOD-sensitive content to require more than a flat single-texture replacement model
- grouping or expressing replacement assets relative to primitive-tile/mip relationships may become necessary

#### 5. VI Is The Real Presentation Stage

The most important references are:

- `n64brew_Video_Interface.html`
- local `video_interface.*` and `vi_*policy*` files in Parallel N64

Important behavior:

- VI applies final scaling and presentation behavior
- VI is responsible for resample/AA/divot/dedither/gamma/interlace-domain effects
- `ORIGIN`, `WIDTH`, `X_SCALE`, `Y_SCALE`, and field handling all matter directly
- mid-frame VI changes matter

Implication:

- hi-res RDP work must not bypass VI semantics
- keeping RDP enhancement separate from VI scanout/presentation is the right direction
- any stable design must continue to treat VI as the final authority on presentation behavior

### Strongest Direct Implications For Parallel N64

At this point, the strongest actionable conclusions are:

- Treat hi-res replacement as a dedicated subsystem, not a scattered renderer feature.
- Treat texture upscaling, internal RDP upscaling, and VI presentation scaling as separate systems.
- Keep texrect/native-resolution behavior as an explicit correctness mode, likely on by default for risky paths.
- Continue including TLUT/palette state in replacement identity.
- Do not assume modern bilinear filtering is acceptable for enhanced paths.
- Treat renderer-owned async capture and minimal replay artifacts as first-class infrastructure, not optional tooling.
- Prefer per-game/per-scene fix metadata over global hacks when upscale correctness diverges.
- Build automation around deterministic capture, replay, savestates, and preview images.

### Most Useful Reference Files To Revisit Later

For texture replacement / upscaling architecture:

- `/home/auro/code/emulator_references/duckstation-upstream/src/core/gpu_hw_texture_cache.cpp`
- `/home/auro/code/emulator_references/pcsx2-upstream/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp`
- `/home/auro/code/emulator_references/ppsspp-upstream/GPU/Common/TextureReplacer.cpp`
- `/home/auro/code/emulator_references/flycast-upstream/core/rend/TexCache.cpp`
- `/home/auro/code/emulator_references/flycast-upstream/core/rend/CustomTexture.cpp`

For capture / dump / automation / replay:

- `/home/auro/code/emulator_references/dolphin-upstream/Source/Core/VideoCommon/FrameDumper.cpp`
- `/home/auro/code/emulator_references/dolphin-upstream/Source/Core/Core/FifoPlayer/FifoRecorder.cpp`
- `/home/auro/code/emulator_references/duckstation-upstream/src/duckstation-regtest/regtest_host.cpp`
- `/home/auro/code/emulator_references/pcsx2-upstream/pcsx2/GSDumpReplayer.cpp`
- `/home/auro/code/emulator_references/ppsspp-upstream/headless/Headless.cpp`
- `/home/auro/code/emulator_references/azahar-upstream/src/video_core/renderer_base.h`

For N64 behavior:

- `/home/auro/code/n64_docs/n64brew_Reality_Display_Processor_Commands.html`
- `/home/auro/code/n64_docs/n64brew_Reality_Display_Processor_Pipeline.html`
- `/home/auro/code/n64_docs/n64brew_Video_Interface.html`
- `/home/auro/code/n64_docs/official_manual/N64OnlineManuals51/n64man/gsp/gSPTextureRectangle.htm`
- `/home/auro/code/n64_docs/official_manual/N64OnlineManuals51/pro-man/pro14/14-01.htm`
- `/home/auro/code/n64_docs/official_manual/N64OnlineManuals51/pro-man/pro14/14-02.htm`
- `/home/auro/code/n64_docs/official_manual/N64OnlineManuals51/pro-man/pro14/14-03.htm`

### New Planning Bias From This Research

This research changes the planning bias in a few important ways.

Before this sweep, a reasonable fear was that the project might need one giant, tightly coupled rendering redesign.

After this sweep, the better bias is:

- design a narrow replacement subsystem
- preserve texrect/native correctness aggressively
- make scaling behavior explicit rather than implicit
- push capture/replay/testing infrastructure much earlier in the project
- treat VI as a hard boundary
- accept that some feature areas may need per-game/per-scene policy rather than one universal rule

That is a much more manageable shape than a single monolithic "hi-res plus scaling" implementation.

## Failed Attempt Analysis: `hires/current-stack-2026-03-18`

A historical worktree was created at:

- `/home/auro/code/parallel-n64-failed-attempt`

using branch:

- `hires/current-stack-2026-03-18`

Current checkout in that worktree:

- `b9386d4f` `Add stable HIRES occurrence matcher`

This section captures the current analysis of what appears to have worked in that branch, what appears to have failed, and what should influence the next design.

### High-Level Read

The failed branch does not look like wasted effort or a random pile of experiments.
It looks like a serious implementation attempt that partially converged twice:

- first on a real HIRES replacement feature stack
- later on a cleaner ownership/provenance redesign

It appears to have stalled in the middle because Paper Mario exposed that the original runtime lookup/binding model had become too permissive, too heuristic, and too coupled to renderer internals.

The branch history suggests this rough progression:

1. Build a real HIRES replacement path.
2. Add testing, automation, registry/binding/runtime logic, and Paper Mario capture flows.
3. Discover remaining correctness failures that are not simple missing-feature bugs.
4. Add probes, diagnostics, provenance reporting, and scene-specific experiments.
5. Begin a redesign around ownership/provenance and lookup-vs-binding separation.
6. Stop before that redesign becomes a smaller, proven, stable implementation.

### What Worked

#### 1. The Replacement Asset Boundary Was A Good Cut

The strongest durable boundary from the failed attempt is the replacement provider itself.
That boundary survived into the current repo.

Files:

- failed: `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.hpp`
- failed: `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.cpp`
- current: `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.hpp`
- current: `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.cpp`

Interpretation:

- cache parsing
- lookup
- decode
- replacement asset metadata

belong in a standalone subsystem.

This should be preserved.

#### 2. The Failed Attempt Did Build Real Renderer Plumbing

The failed branch was not just documentation and lookup experiments.
It did wire hi-res lookup into the renderer upload lifecycle.

Files:

- failed: `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_renderer.hpp`
- failed: `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_renderer.cpp`

Interpretation:

- provider injection existed
- per-tile replacement state existed
- lookup during texture upload existed
- registry/residency logic existed

So the effort did get beyond pure metadata plumbing.

#### 3. The Branch Added Valuable Tooling And Diagnostics

The failed branch had a lot of useful infrastructure, especially around:

- HIRES-specific unit tests
- dump/replay seams
- provenance/debug reporting
- automated state refresh and scripted repro flows

Files:

- `run-tests.sh`
- `run-dump-tests.sh`
- `tests/emulator_behavior/CMakeLists.txt`
- `tests/hires_textures/CMakeLists.txt`
- `docs/HIRES_BEHAVIOR.md`
- `tools/hires_draw_debug_report.py`
- `tools/hires_draw_provenance_report.py`
- `tools/virtual_gamepad.py`

Interpretation:

- the branch invested heavily in observability
- several of those ideas are worth preserving conceptually
- the branch understood that renderer work needed stronger debugging surfaces

#### 4. Some Smaller Helper Boundaries Were Durable

Several helper-policy boundaries existed in the failed attempt and still exist in reduced form now.
Those are likely the abstractions that actually paid for themselves.

Examples:

- lookup policy
- registry policy
- key state policy
- CI palette handling
- tile state / tile-oriented lookup state
- VI scale policy

This is a useful signal for what the next design should keep small and explicit.

### What Partially Worked

#### 1. GPU Residency And Registry Logic Was Useful, But Too Entangled

The failed attempt had real residency and registry management, but it appears to have become too tightly coupled to the renderer internals and binding model.

Files:

- failed: `rdp_hires_binding_policy.hpp`
- failed: `rdp_hires_bindless_view_policy.hpp`
- failed: `rdp_renderer.hpp`
- current: `rdp_hires_registry_policy.hpp`

Interpretation:

- the registry concept was sound
- the wider binding/bindless/consumer machinery was not durable in its failed-branch form

#### 2. The Scaling Path Was Real, But The Decomposition Was Mixed

The scaling path was not fake.
`VideoInterface` and upscaled-domain handling were doing real work in both trees.

Files:

- failed: `video_interface.cpp`
- failed: `vi_scale_sampling_policy.hpp`
- failed: `vi_scanout_flow_policy.hpp`
- current: `video_interface.cpp`
- current: `vi_scale_policy.hpp`

Interpretation:

- the broader scaling / VI area was correctly identified as a separate problem
- some of the micro-policy decomposition in the failed branch does not appear to have held up

### What Failed Or Appears Incomplete

#### 1. The Architecture Became A Policy Explosion

The failed branch introduced many small policy headers that appear to have existed mainly to support one complicated runtime path.
Most of those did not survive into the current repo.

Examples:

- `rdp_hires_consumer_policy.hpp`
- `rdp_hires_shader_policy.hpp`
- `rdp_hires_tile_alias_policy.hpp`
- `rdp_hires_binding_policy.hpp`
- `rdp_hires_ownership_policy.hpp`
- `vi_scale_sampling_policy.hpp`
- `vi_scanout_flow_policy.hpp`

Interpretation:

- the design was becoming over-factored in the wrong place
- the abstractions were not reducing complexity; they were distributing it
- this made the branch look more complete than it really was

#### 2. The Runtime Model Seems To Have Stopped At Lookup / Provenance / Binding Bookkeeping

One of the strongest themes from the failed-branch analysis is that it appears to have accumulated:

- lookup modes
- alias counters
- descriptor bookkeeping
- fallback matrices
- provenance/birth-family logic
- draw/debug statistics

faster than it established a small, stable rendering contract.

Interpretation:

- the feature was no longer “find replacement, bind replacement, render replacement correctly”
- it became “keep trying increasingly clever ways to justify a replacement match and explain it afterward”

This is a classic sign that runtime identity and rendering semantics were not strict enough.

#### 3. Replacement Identity Became Too Permissive

The strongest likely technical failure point is that the old stack did not keep replacement identity strict enough.

Reported risk areas:

- reinterpretation fallbacks by tile mask, stride, block shape, and CI fallback paths
- hand-picked birth-family / pattern heuristics
- alternate CRC candidates and palette-ambiguous fallback behavior

Files called out by analysis:

- failed: `rdp_renderer.cpp`
- failed: `rdp_hires_lookup_policy.hpp`
- failed: `rdp_hires_runtime_policy.hpp`
- failed: `rdp_hires_ci_palette_policy.hpp`
- failed: `rdp_hires_tlut_shadow_policy.hpp`

Interpretation:

- the branch appears to have shifted from strict identity toward “acceptable reinterpretation if it makes Paper Mario work”
- that direction is fundamentally risky for N64, where TMEM/TLUT/tile semantics are part of texture meaning

#### 4. TLUT / CI Handling Became Heuristic

The failed-attempt analysis strongly suggests that CI/TLUT handling moved toward heuristic recovery rather than exact semantic identity.

Interpretation:

- multiple CRC candidate paths
- alternate palette/layout interpretations
- palette-ambiguous fallbacks

may help some scenes match, but they also weaken confidence that the selected replacement is actually correct.

Given what the N64 docs say, this is a major red flag.

#### 5. Texrect Correctness Was Still At Risk When Replacement Was Active

The failed-branch analysis suggests texrect handling was reduced to a few generalized flags and could stop being strictly native-resolution when replacement textures were active.

Files called out by analysis:

- failed: `rdp_tex_rect_policy.hpp`
- failed: `rdp_renderer.cpp`
- failed: `shaders/shading.h`

Interpretation:

- this aligns with the doc-backed concern that texrects are the highest-risk primitive for hi-res scaling
- it reinforces the conclusion that texrect/native behavior needs to be preserved aggressively

#### 6. HIRES Sampling Drifted Toward Modern GPU Assumptions

The failed branch exposed sampling policy in terms like:

- nearest
- linear
- trilinear

and derived “original dimensions” in ways that appear more like modern texture abstraction than strict N64 semantics.

Interpretation:

- that is risky given N64 3-point filtering, tile-relative LOD behavior, and texrect-specific rules
- this likely contributed to visual drift even when replacement lookup itself succeeded

#### 7. VI And Upscale Logic Became Too Blurred

The failed-attempt analysis also suggests that VI presentation behavior became too entangled with upscale policy.

Files called out by analysis:

- failed: `vi_scale_policy.hpp`
- failed: `vi_scale_sampling_policy.hpp`
- failed: `video_interface.cpp`

Interpretation:

- this matches the concern already raised by the doc sweep: VI must remain the final presentation authority
- RDP enhancement and VI presentation should not be fused into one correctness model

### Tooling Lessons From The Failed Attempt

The failed attempt had strong tooling, but it was not sufficiently hermetic.

#### What Was Good

- real test taxonomy, including HIRES-specific lanes
- useful dump/replay seam
- good debug/provenance reporting
- scripted scene capture and state refresh flows

#### What Was Missing

- too much of the loop was Paper Mario specific
- too much of the automation depended on live frontend state, `/tmp`, `tmux`, `nc`, `/dev/uinput`, and local ROM/layout assumptions
- too many tests locked script contracts instead of renderer truth
- there was not an equivalent committed small HIRES replay corpus matching the strength of the generic RDP dump seam

Interpretation:

- the helpers were powerful
- but the helpers became the foundation instead of sitting on top of a hermetic fixture layer

That appears to have made stabilization harder.

### Branch History Read

The history suggests 198 commits over roughly 10 days after branching from `master` on March 5, 2026.

Most important phases inferred from the branch:

- March 5: core HIRES machinery and milestone-style implementation work
- March 6-7: Paper Mario correctness and scaling/VI experiments
- March 8-13: heavy diagnostics, compare tools, provenance tools, and scene-specific probes
- March 14 onward: a cleaner ownership/provenance redesign begins, but does not finish strongly enough to become the new stable core

This is consistent with a branch that did real work, but eventually got trapped between diagnosis and redesign.

### Most Valuable Conceptual Survivors From The Failed Attempt

The strongest concepts to preserve from that branch are:

- the replacement provider boundary
- strict small helper policies that survived into current repo form
- registry/residency as a concept, but not the old full binding/consumer layer
- debug/provenance tooling as offline analysis support
- capture/replay/testing as first-class project infrastructure

### Most Important Lessons For The Next Attempt

- Do not resurrect the failed branch’s full policy layer as-is.
- Keep `texture_replacement.*` as the asset/index/decode subsystem.
- Keep only the smaller helper boundaries that survived naturally.
- Make the renderer contract explicit and small: lookup resolves a concrete replacement state/handle; draw code consumes that resolved state.
- Keep scaling architecture separate from hi-res replacement.
- Treat provenance, aliasing, reinterpretation, and occurrence analysis as debug/offline tooling, not as first-class runtime architecture.
- Add proof tests for actual rendered output, not just lookup success or script success.
- Build a committed, minimal, hermetic HIRES replay corpus before relying on scene-specific orchestration again.

### Current Bottom Line On The Failed Attempt

The failed branch did not fail because nothing worked.
It failed because too many things worked partially at once.

The branch had:

- real feature progress
- real tooling progress
- real insight into Paper Mario and HIRES behavior

But the runtime architecture appears to have become too permissive and too coupled, while the testing/debugging stack became too scene-specific and too orchestration-heavy.

That is useful information.
It suggests the next attempt should be:

- narrower
- stricter about identity
- more explicit about boundaries
- more conservative about texrect and VI correctness
- built on hermetic fixtures before scene choreography

## RetroArch, TAS, and Paper Mario Research For Agent-First Tooling

### Current View

The best path is not to make agents better at driving the RetroArch UI.

The best path is:

- RetroArch provides a small, machine-readable, merge-friendly control and capture surface
- the core provides renderer- and emulation-specific debug exports
- the game provides semantic scene/state labels
- wrappers orchestrate workflows, but are not the correctness foundation

This is closer to how serious TAS work actually succeeds:

- deterministic replay
- frame-based control
- savestate branching
- memory/state visibility
- frame advance and checkpointing
- explicit controller emulation instead of GUI automation

### RetroArch Baseline Reality

RetroArch already has more useful seams than expected.

#### Strong Existing Seams

- command interface over stdin, UDP, and local socket paths
- replay / BSV recording and playback
- savestate integration with replay state
- frame advance and rewind support
- screenshot and save/load task layers
- existing core-memory read/write commands
- internal test joypad/input drivers that already model scripted input by frame

Interpretation:

- we do not need to invent a control system from nothing
- we need to make the existing seams machine-friendly, deterministic, and easier to consume

### What Should Change In RetroArch

The highest-value RetroArch changes are additive and upstream-friendly.

#### 1. Add Machine-Readable Command Replies

Priority commands:

- `GET_STATUS_JSON`
- `GET_SESSION`
- `GET_PATHS`
- `GET_CONTENT_INFO`

Why:

- agents should not parse human-oriented text output when structured data can be returned directly
- this is low-risk and merge-friendly

#### 2. Add Request IDs And Async Completion Replies

Priority operations:

- save state
- load state
- screenshot / frame capture

Why:

- current command semantics are immediate, while real completion occurs later in task layers
- agents need to know when the action has actually completed

#### 3. Add First-Class Core Option Commands

Priority commands:

- `GET_CORE_OPTIONS`
- `GET_CORE_OPTION`
- `SET_CORE_OPTION`
- `RESET_CORE_OPTIONS`
- `FLUSH_CORE_OPTIONS`

Why:

- core options are part of test determinism
- agents need to verify and mutate feature flags without using menus

#### 4. Add A Proper Local Agent Socket

Preferred transport:

- Unix domain socket or equivalent local machine transport

Why:

- this is a better fit than bolting more behavior onto UDP
- local agent orchestration should be reliable, ordered, and easy to secure

#### 5. Add Explicit Capture Commands

Priority commands:

- `CAPTURE_SCREENSHOT`
- `CAPTURE_FRAME`
- `GET_FRAME_INFO`

Desired metadata:

- source type
- viewport dimensions
- presentation dimensions
- frame number
- content/core identity

Why:

- agents struggle to “see”
- better capture APIs are a direct force multiplier for debugging and testing

#### 6. Expose Raw Savestate Import / Export

Why:

- save/load hotkey semantics are not the right abstraction for tooling
- deterministic fixture transport should be scriptable as data movement, not fake UI

#### 7. Add Memory Discovery, Not Just Memory Poking

Priority command:

- `GET_MEMORY_MAP`

Why:

- raw `READ_CORE_MEMORY` / `WRITE_CORE_MEMORY` is not enough if the agent cannot discover valid regions first
- the frontend should expose the generic memory map; semantic meaning stays in the core/game

#### 8. Add Log Streaming / Tail

Priority commands:

- `GET_LOG_TAIL`
- `SUBSCRIBE_LOGS`

Why:

- scraping stdout is brittle
- agents need a stable stream of runtime events

#### 9. Add Exportable Debug Bundles

Priority command:

- `EXPORT_DEBUG_BUNDLE`

Bundle contents should include:

- session state
- content/core identity
- active core options
- log tail
- save paths
- screenshot paths
- memory map summary

Why:

- this creates reproducible handoff artifacts for agents and humans

### What Should Not Go Into RetroArch

- emulator-specific renderer semantics
- game-specific knowledge
- menu-driven automation as the primary control plane
- changes to existing text command contracts when additive machine commands will do

Interpretation:

- RetroArch should transport and orchestrate
- the core and the game should provide semantic meaning

### Input Control: TAS Lessons Applied

The strongest control model is internal controller emulation, not OS-level gamepad fakery and not menu automation.

#### Best Model

- keep orchestration outside RetroArch via command/IPC
- inject authoritative controller state inside RetroArch at or near the input callback boundary
- preserve BSV replay as the canonical deterministic replay/branching format

Why:

- TAS workflows depend on frame-accurate input authority
- RetroArch already has BSV replay, checkpointing, frame advance, and test joypad scaffolding
- this means the likely right move is an “agent joypad” or comparable internal input source

#### Practical Split

Outside RetroArch:

- launch content
- configure options
- save/load state
- request captures
- query status
- fetch logs
- inspect memory

Inside RetroArch:

- apply frame-stamped controller input
- optionally mirror that input into replay machinery

Interpretation:

- this gives us deterministic control without forcing the agent to “play the menu”

### Merge-Friendly RetroArch Strategy

RetroArch is currently behind upstream.

Observed local state on March 21, 2026:

- `/home/auro/code/RetroArch` branch: `master`
- local `master` at `53c66ce970`
- upstream `master` at `b0624a720a`
- local branch behind upstream by 119 commits

Interpretation:

- we should avoid large invasive frontend rewrites
- the right strategy is an additive tooling layer, likely near command handling
- wrapper-side conveniences can ship faster while upstream-friendly tooling patches stay narrow

### Paper Mario Is A Strong Debug Target

Paper Mario is not just a convenient test game.
It is one of the best possible debug targets because the decomp exposes semantic state directly.

#### Why It Is Valuable

- title, intro, file select, pause, map transitions, and battle paths are named and isolated
- game state already exposes fields like `areaID`, `mapID`, `entryID`, `introPart`, `startupState`, player position, and other high-signal values
- the DX fork already includes substantial debug tooling

#### Existing Valuable Game-Side Tooling

- debug menu
- map select
- battle select
- quick save/load
- live map / position displays
- collision viewer
- EVT debugger with pause/step behavior
- profiling and crash/backtrace support

Interpretation:

- the game already gives us a semantic labeling layer that most emulator test targets do not
- we should leverage that instead of treating Paper Mario as only a visual fixture

### Game-Side Telemetry Worth Exposing

If we add a compact game-side telemetry channel, the most useful payloads are:

- `context`
- `areaID`
- `mapID`
- `entryID`
- resolved map name
- `mainScriptID`
- `loadType`
- `introPart`
- `startupState`
- player `x/y/z/yaw`
- save slot
- story progress
- partner
- current battle/stage identity

Important transition events:

- `GotoMap*` called
- map transition state begins
- `load_map_by_IDs` begins
- `load_map_by_IDs` completes
- main script starts
- fade-in completes

Interpretation:

- this would let frame captures, replay logs, and renderer traces be keyed to exact game meaning
- it would reduce the amount of guessing agents have to do from image output alone

### Best Early Paper Mario Fixtures

The current best first-pass fixture ladder is:

- title screen
- file select main menu
- `hos_05 ENTRY_3`
- `osr_00 ENTRY_3`
- pause stats/items

#### Why These Matter

Title screen:

- zero-input baseline
- useful for logo scaling and texrect/scissor behavior

File select:

- menu/HUD/window stress test
- low gameplay noise

`hos_05 ENTRY_3`:

- likely one of the best intro/scaling/hi-res targets
- deterministic storybook-like sequence with custom 2D behavior

`osr_00 ENTRY_3`:

- strong message/image system fixture

Pause stats/items:

- dense texrect/HUD/icon/number workload once stable saves are available

#### Additional Strong Fixtures

- `hos_04 ENTRY_4`
- `kkj_00 ENTRY_5`
- `kkj_13 ENTRY_2/3`
- `osr_03 ENTRY_2`
- `hos_10 ENTRY_5`

Interpretation:

- this is enough to build a real fixture matrix
- we do not need to depend on ad hoc free-play exploration first

### Renderer-Relevant Paper Mario Paths

Useful renderer-facing UI and 2D paths called out by research:

- message drawing
- draw box/window systems
- HUD element rendering
- inventory/status paths
- file menu rendering helpers
- pause rendering helpers
- screen render utility paths

Interpretation:

- these are promising fixture families for texrect-heavy correctness
- they give us a path to organize tests by feature type, not just by scene

### Proposed Boundary Model

The clean architecture for agent-first debugging appears to be:

- RetroArch:
  structured control, capture, log, memory-map, replay orchestration
- parallel video core:
  renderer-specific traces, replacement/scaling instrumentation, image/debug exports
- Paper Mario:
  semantic scene/state labeling and deterministic fixture entry
- wrappers:
  scenario runners, state preparation, report collation

This is important.
It avoids putting too much meaning into the frontend while also avoiding scene-specific shell orchestration as the foundation.

### Immediate Planning Implications

Before a full implementation plan, the strongest near-term planning items are:

- define the minimum RetroArch patch set needed for agent-first operation
- decide whether the first control prototype is command-only or command plus internal agent joypad
- define the first Paper Mario fixture matrix and its acceptance checks
- define the first game-side telemetry payload
- define what renderer/core-side capture and trace outputs should exist in phase 1

### Current Bottom Line

The project now has a clearer non-UI debugging direction:

- machine-readable frontend control
- deterministic internal input
- semantic game-state labels
- fixture-driven captures
- renderer-specific traces where they belong

That is a materially better foundation than relying on UI automation or scene choreography alone.

## 2026-06-10 Reboot

The project rebooted today on branch `parallelish-reboot`. Decisions, all approved:

- Two prior attempts are closed. Attempt A (`origin/hires/current-stack-2026-03-18`,
  abandoned 2026-03) died chasing GlideN64 pixel-diff scores. Attempt B (master,
  2026-03-21..05-01, frozen) built the PHRB runtime but drowned in a circular
  "native sampled identity" enrichment program and metadata-counter gates. Its plan
  stack is archived under `docs/history/`.
- Protected property: with hi-res + scaling OFF the core must stay upstream-grade
  stable; feature-off screenshot digests are a valid gate there and only there.
  Hi-res-ON validation never uses pixel digests or exact metadata counts —
  class-level semantics plus visual review only.
- Identity: the GlideN64-compat Rice-CRC lane is the primary path (proven on SM64,
  OoT, and Paper Mario). The native-sampled-identity/enrichment program is frozen:
  code dormant, no gates, no further converter-review work.
- GlideN64 guardrails: source-level semantic oracle, txDump hash-coverage oracle, and
  side-by-side content screenshots only. Hard bans: no numeric image-similarity
  metrics in pass/fail logic, no commit justified by "match GlideN64", no per-scene
  or per-descriptor renderer overrides.
- Test surface trimmed 125 -> 48 (commits b6f54147..b6e0b767): emu-required = 42,
  emu-runtime-conformance = lavapipe smoke + one class-level Paper Mario lane,
  SM64/OoT lanes on-demand. The dump gate is deleted; missing runtime prerequisites
  now fail loudly.
- Fresh-machine reality: ROMs at `/pluto/game/rom/n64`, only the Paper Mario pack on
  disk (`assets/TEXTURE_PACKS.md` tracks acquisition), savestate remint pending,
  RetroArch `agent-control` branch with patches backed up in `tools/retroarch-patches/`,
  GlideN64 reference vehicle is mupen64plus-next at `/home/auro/code/cores/`.
- New controlling plan and work order: `docs/REBOOT_PLAN.md`.

## 2026-06-10 Control stack live + sampler falsification result

- The savestate ladder is reminted on this machine against post-merge master:
  title screen -> file select -> kmr_03 ENTRY_5 (960-frame attract ladder), each
  visually verified and deterministic across independent sessions (title and
  kmr_03 proven byte-identical twice). New canonical feature-off capture digests:
  title `351cf979...`, file-select `4b517fba...`, kmr_03 `35213195...`. The
  patched RetroArch (`agent-control`) answered every command live. /pluto is
  mounted read-only; interim state backups live at `~/backups/parallel-n64/`.
- Sampler falsification experiment (artifacts/experiments/sampler-falsification-062900,
  6 conditions: hires on/off x 1x/4x x texrect-native on/off, title fixture,
  zero-config compat PHRB):
  - Replacement WORKS: correct MasterKillua art, correct placement at 1x and 4x;
    feature-off 4x control reproduced the canonical digest exactly.
  - FALSIFIED: the predicted /SCALING_FACTOR wrong-region bug. At 4x with texrect
    upscaling enabled the art is placed correctly - ST is scaled-space there and
    the division is correct. Static analysis from the research sweep was wrong on
    coordinate space; the historic "hi-res-ON byte-identical to OFF" captures were
    a property of the old enriched package, not the pipeline (zero-config ON
    differs from OFF as expected).
  - CONFIRMED: the sub-texel fraction discard. Replaced content renders at
    identical effective resolution at 1x and 4x, with texrect-native on or off -
    pack detail is decimated to the native texel grid everywhere. This is the
    single bug standing between "packs load" and "packs look hi-res".
  - Scene evidence: 11,463 compat entries, 178/196 upload hits, 12 compat draw
    hits, native_sampled=0 - the compat lane carries the whole scene zero-config.
- Scaling fix scope is therefore: preserve the fp5 fraction and filter in
  replacement-texel space (attempt A semantics), then mips + wire hirestex-filter.
  Do NOT remove the /SCALING_FACTOR normalization.
- Shader regen toolchain: modern slangmosh emits an incompatible interface
  (namespace via --namespace, 3-arg ctor + reflection bank needing a newer Vulkan
  backend than the fork's 2020 snapshot). A 2020-vintage build from the fork's
  pinned upstream commit (7a3e561e) is in progress; regen script now rewrites the
  Granite include path at run time.
- Fraction fix landed (cb46b7d2): hires sampling keeps the fp5 sub-texel
  fraction, so replacement detail finally resolves past the native texel grid.
  Verified crisp credit text at 4x with texrect upscaling forced on; feature-off
  title digest reproduced exactly.
- Texrect exemption landed (4f79390b): native_resolution_tex_rect's grid snap
  is now cleared per-draw when a replacement resolved for a non-copy, non-flip
  texrect (bit is GPU-only, patched on the queued setup after the draw-time CRC
  fallback). With DEFAULT options (4x, native-texrect enabled) the replaced
  title credit text now matches the quirk-disabled reference; copy-mode and
  flip texrects keep the snap, so the historic copy-strip breakage stays
  guarded. Feature-off stayed bit-identical (351cf979...); emu-required 42/42,
  emu-runtime-conformance 2/2. Evidence:
  artifacts/experiments/texrect-exempt-135246/ (compare-credit-zoom.png rows:
  before / after / quirk-off reference).
- Alpha-weighted bilinear landed (3da623f5): the pack stores black RGB under
  transparent texels (267/268 sampled cutout textures), so the plain RGBA lerp
  was bleeding dark halos into cutout edges. RGB taps are now weighted by
  alpha; curtain/bush halos clear up on kmr_03. The fence and stone-path
  fringes did NOT move (diff-map proof) - those need the GlideN64 oracle to
  classify as pack art vs renderer issue.
- GlideN64 reference rig landed (1eac0dbd): adapter gained generic
  --core-options-template/--extra-append-config, gliden64-reference-capture.sh
  boots the .hts pack on mupen64plus-next and screenshots the title scene,
  compose-reference-pair.py builds labeled review pairs, and
  emu.support.gliden64_reference_guardrails (canary-tested) bans image metrics
  and GlideN64 digest gating from the gate surface. First side-by-side:
  artifacts/experiments/texrect-exempt-135246/sidebyside-title.png - same pack
  content on both renderers.
- Open: kmr_03 GlideN64 reference needs gameplay reach (file select -> walk),
  which wants the interactive agent-play adapter mode. Mips/trilinear and
  hirestex-filter wiring stay queued behind that oracle.
- Interactive agent-play adapter landed (a7fbf4ad):
  retroarch_interactive_session.sh keeps one session alive across agent turns
  (setsid process group + shared runtime flock + TTL timeout watchdog, all
  proven live: clean stop, zero orphans, 20s-TTL session self-terminated).
  Agent-play loop proven on kmr_03: screenshot -> decide -> analog input
  walked Mario across a screen boundary into a Koopa battle; slot-1
  checkpoint reproduced it. KEY WORKFLOW LESSON: the game runs in real time
  between agent commands (the battle itself was an accident of thinking
  time) - deterministic play means staying paused and using
  `input --frames N`, which proved bit-identical on replay (9e1b7845... twice
  for load + 60 stepped frames + capture).
- This unblocks GlideN64 mid-game reference reach (boot -> play to scene on
  the mupen64plus-next vehicle) for the fence/stone-path halo classification,
  and unblocks new fixture minting anywhere an agent can walk.
- Breadth packs acquired and validated (be119ac7, 83d0dfad): user staged
  packs on koopa; OoT Reloaded v11.0.0 + MK64 Reloaded v2026.04.03 GlideN64
  .hts pulled from there, SM64 Reloaded v2.6.0 + MM Reloaded v11.0.2 fetched
  directly from evilgames.eu (MM Reloaded IS the missing "MM GlideN64
  edition"). Rejected with evidence: sm64redrawn-master.pak (sm64nx port
  pack), MM 3D 4K (Citra), OoT SoH .o2r. All GhostlyDark packs are
  new-version GLideNHQ caches (0x08000000 at offset 0) - hires_pack_common
  already parses both layouts.
- Zero-config conversions + boot validations all green on the same day:
  SM64 700MB/2,530 records, MK64 3.6GB/10,906, OoT 9.5GB/43,267 (1 deferred
  compat record). Cross-game boot lane passed for all three with clean
  captures. Class-level finding to remember: SM64 resolves ENTIRELY via the
  draw-time compat CRC lane (upload hits 0) - if draw-time CRC cost ever
  matters, SM64 is the stress title; it is also the natural first user of a
  txDump coverage diff.
- txDump oracle landed (5f7ea3cc): env-gated 1-line patch
  (GLN64_TXDUMP=1) in the mupen64plus-libretro-nx clone, verified dumping
  Rice-CRC-named PNGs on the Paper Mario title scene. Patch + rebuild recipe
  in tools/gliden64-txdump-patch/.
- MM Reloaded v11.0.2 landed and validated same-day: 690MB 7z -> 3.9GB .hts
  -> 3.9GB PHRB (13,689 records, 1 deferred compat). Boot lane passed with
  hi-res Clock Town intro capture. ALL FIVE titles (Paper Mario, SM64, OoT,
  MK64, MM) now have on-disk .hts sources, zero-config PHRB packages, and
  green boot validations. The "MM as very-large-pack stress test" slot is
  now real: 9.5GB OoT + 3.9GB MM exercise streaming load.
- Fence/stone-path fringe classification CLOSED via pack-art forensics (no
  GlideN64 playthrough needed): extracted all 118 asset variants for the 81
  kmr_03 draw keys from the Paper Mario PHRB (keys are
  (palette_crc<<32)|texture_crc; records group by low-32, asset
  legacy_checksum64 carries the pair - the earlier "wrong striped texture"
  was a first-variant-of-record mismatch). The fence's black outline is
  BAKED ART (pure-black antialiased stroke, 0.94% partial-alpha texels all
  RGB=0); the raggedness/speckle was renderer-side.
- Hi-res sampler quality unit landed (e647e357): (1) CPU-built full mip
  chains at replacement upload with alpha-weighted 2x2 reduce (cutout mips
  stay halo-free), VRAM accounting budgets the chain; (2) hirestex-filter
  wired end-to-end (was parsed-but-dead) - nearest/bilinear/trilinear via
  GlobalFBInfo.hires_filter; trilinear computes replacement LOD from
  per-pixel ST screen derivatives (reuses interpolate_stz's uses_lod path,
  paid only on replaced draws); (3) THE BIG ONE: replaced tiles are now
  truly direct-sampled - the N64 3-point combine had been re-blending
  already-filtered hi-res taps at native-texel spacing on EVERY quad-mode
  replaced draw (frac was computed before the old sample_quad=false), which
  was the real source of both the fence-outline stipple and stone-path
  speckle (NOT alpha-test dither, NOT missing mips alone). frac/sum_frac
  zeroed so the combine reduces to t_base; TLUT draws included.
  Falsification trail: naive TLUT bypass corrupted the scene twice
  (uninitialized t10/t01 x nonzero frac, then sum_frac>=32 full-weight
  blend) before the correct reduction.
- Verified: feature-off digests bit-exact through all of it (kmr_03
  35213195..., title 351cf979...); kmr_03 hi-res ON clean at linear AND
  trilinear (fence pack-faithful, stones smooth, full frame intact);
  emu-required 43/43; emu-runtime-conformance 2/2; SM64 + OoT hi-res boot
  lanes green on the new core. Dead hirestex-srgb option deleted
  (a020212b) - sRGB-authored art rendered as UNORM matches GlideN64.
- Open follow-ups: consider flipping hirestex-filter default to trilinear
  after breadth scenes beyond boot are reviewed; stdin adapter gained
  PARALLEL_RDP_HIRES_FILTER_OVERRIDE for sweeps.
- hirestex-filter default flipped to trilinear (9c0d6ae3), closing the
  follow-up from the sampler unit. Evidence: serial attract-mode capture
  pairs for all four breadth titles (4 timestamps each, ~25-115s, filter
  mode proven per bundle log) + the kmr_03 fixture pair, judged by a
  10-agent review panel (per-title reviewer + adversarial skeptic, arbiter
  only on disagreement - none triggered). 5/5 accept; skeptics with
  magnified/brightness-normalized crops found no over-blur, mip seams,
  wrong textures, or UI softening, and flagged bilinear-only speckle on
  minified content (SM64 moat bricks/water, OoT field grass, MK64 distant
  road) as evidence FOR the flip. Caveat recorded by the panel: MM pairs
  were weakened by attract-fade timing drift; judged per-panel, still
  clean. Explicit "linear"/"nearest" selections unchanged; the core's
  declared default empirically resolves to mode 2 when the option key is
  absent. Feature-off digests stayed bit-exact through the flip
  (hirestex-filter provably inert with hi-res off); emu-required 43/43,
  emu-runtime-conformance 2/2. Review bundles + pairs under
  artifacts/experiments/trilinear-default-204024/.
- Copy-pipe hi-res sampling fixed (SM64 UI investigation). User report:
  SM64 title/HUD elements "don't look hi-res". Instrumented run
  (PARALLEL_RDP_HIRES_DEBUG) showed 100% of textured draws binding
  replacements (943k triangle draws, 111 unique keys, zero misses) - but
  PRESS START letters and HUD glyphs are COPY-mode TEX_RECTs, and
  remap_hires_st_fp5_copy divided ST by SCALING_FACTOR even though
  interpolate_st_copy already collapses dx to the native grid
  (dx >>= SCALING_LOG2). The double division compressed every copy-pipe
  replacement lookup into the top-left corner of the HD image, so
  copy-mode UI never showed pack art. One-line shader fix (remove the
  second division) + slangmosh regen. Verification was probe-driven
  after visual comparison proved unreliable (the SM64 Reloaded letter
  art is a faithful redraw, and the native-res tex-rect snap decimates
  it back to ~16x16, so fixed vs broken looks similar at a glance):
  magenta probe proved the copy hires branch executes for the letter
  rects; a red-channel grayscale probe matched the pack letters'
  per-glyph red pattern exactly (both R's dark at 28, P/E/A/T bright
  230+, S mid 102) in both words; offline provider dump
  (tools/hires_dump_entry) confirmed runtime resolution returns the same
  art as the PHRB inventory. HUD digits/icons (coin, star, x, lives 4,
  camera) now render pack art visibly. Gates: emu-required 43/43,
  emu-runtime-conformance 2/2, kmr_03 feature-off digest bit-exact
  (35213195...), kmr_03 hi-res ON visually unchanged (triangle-dominated
  scene; expected). Evidence bundles under
  artifacts/experiments/sm64-ui-coverage-261610/.
- Open follow-up from the same investigation: replaced COPY-mode
  tex-rects still rasterize with the native-resolution snap (the
  draw-time exemption in rdp_renderer.cpp deliberately excludes copy
  rects), so copy-pipe UI replacements render as ~orig-res decimations
  of the HD art rather than full-res. Lifting the snap for replaced copy
  rects is the remaining headroom; it needs its own falsification pass
  (copy-mode strips at upscale were the reason native_resolution_tex_rect
  exists). Also note: SM64 pack letter glyph entries appear recolored
  relative to the original PRESS START palette (artist choice in the
  Reloaded pack), which is visible now that the letters actually replace.

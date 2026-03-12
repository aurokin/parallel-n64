# HIRES Behavior

## Scope
- Target renderer: `parallel`
- GPU requirement: Vulkan descriptor indexing only
- Runtime fallback: auto-disable when required Vulkan descriptor-indexing support is unavailable
- Git policy: replacement cache artifacts such as `*.htc` and `*.hts` stay untracked

## Current Implementation
- Libretro exposes these HIRES options:
  - `parallel-n64-parallel-rdp-hirestex`
  - `parallel-n64-parallel-rdp-hirestex-filter`
  - `parallel-n64-parallel-rdp-hirestex-srgb`
  - `parallel-n64-parallel-rdp-hirestex-budget-mb`
- The frontend passes the RetroArch system directory into the core.
- The core currently scans that directory for `.hts` and `.htc` packs instead of selecting a pack by ROM identity.
- Successful cache load attaches the replacement provider, enables replacement residency management, and enables the bindless HIRES shader path.
- Replacement happens at texel fetch output before combiner and blender evaluation.
- Current replacement flow includes:
  - tile and block lookup
  - deferred block retry after `set_tile_size()`
  - TMEM alias propagation
  - copy-mode replacement sampling
  - CI/TLUT-aware lookup with palette-hint fallback
  - native `I`-format semantic normalization in the shader
  - transparent replacement RGB sanitization before upload

## Supported Formats
- Pack containers:
  - `.hts`
  - `.htc`
- Pack payload decode formats:
  - `RGBA8`
  - `RGB8`
  - `RGB565`
  - `RGBA5551`
  - `RGBA4444`
  - `LUMINANCE8`
- Compressed payloads:
  - gzip-compressed payload blobs are decoded when the pack flags request it
- Native lookup coverage:
  - format-size-aware matching
  - CI lookup with low-32 checksum fallback and palette-aware disambiguation
  - Paper Mario validation currently exercises RGBA, CI/TLUT, copy-mode, and intensity-backed replacement paths

## Required Vulkan Features
- `descriptorIndexing`
- `runtimeDescriptorArray`
- `shaderSampledImageArrayNonUniformIndexing`
- `descriptorBindingVariableDescriptorCount`
- `descriptorBindingPartiallyBound`
- `descriptorBindingSampledImageUpdateAfterBind`
- `maxDescriptorSetUpdateAfterBindSampledImages >= 4096`

## How To Turn It On
- Use the `parallel` graphics path.
- Enable:
  - `parallel-n64-parallel-rdp-hirestex = enabled`
- Optional tuning:
  - `parallel-n64-parallel-rdp-hirestex-filter = linear|nearest|trilinear`
  - `parallel-n64-parallel-rdp-hirestex-srgb = auto|on|off`
  - `parallel-n64-parallel-rdp-hirestex-budget-mb = 0|128|256|512|1024|2048|4096`
- Place packs in the RetroArch system directory used by the core.
- Current limitation:
  - the core scans the configured system directory directly
  - it does not yet resolve packs by ROM identity

## How We Did It
- Provider:
  - parse `.hts` and `.htc`
  - decode supported payload formats to RGBA8
- Runtime gating:
  - require explicit enablement plus Vulkan capability support
  - detach the provider when HIRES is disabled or cache load fails
- Registry:
  - keep replacement residency and descriptor ownership in a bindless registry
  - enforce the configured memory budget through renderer-managed residency
- Keying:
  - hash native texture content
  - track TLUT state for CI lookup
  - retry some block loads once tile sizing becomes informative
- Shader path:
  - upload replacements as sampled Vulkan images
  - bind replacements through the existing shader bank
  - remap N64 coordinates into replacement texture space at fetch time
  - preserve native combiner and blender behavior after replacement texels are produced

## Core Behavior With HIRES On
- The core attempts to load replacement packs from the system directory during configure time.
- Successful load enables provider lookup, registry upload, residency tracking, and descriptor-backed replacement sampling.
- Shader-bank state may rebuild when the effective `HIRES_REPLACEMENT` define changes.
- Draws with a valid replacement binding sample replacement texels instead of native TMEM texels.
- Filtering and sRGB behavior follow the HIRES core options.
- `trilinear` uploads replacement textures with mipmaps.
- `linear` and `nearest` keep the non-mipped upload path.

## Core Behavior With HIRES Off
- Provider lookup is bypassed.
- Replacement registry upload and residency work is bypassed.
- HIRES descriptor binding is bypassed.
- HIRES shader sampling is bypassed.
- Native TMEM, combiner, blender, and VI behavior are expected to remain unchanged.
- Any HIRES-off regression is a blocker.

## Validation Status
- Safety gate:
  - `./run-tests.sh --profile hires-readiness`
- Runtime smoke:
  - `./run-n64-smoke-state.sh -- --verbose`
- Comparable Paper Mario capture on `parallel`:
  - `./run-paper-mario-hires-capture.sh --tag <tag>`
  - for intro/title captures without controller input:
    - `./run-paper-mario-hires-capture.sh --smoke-mode timed --screenshot-at <sec> --tag <tag> --require-hires`
  - current useful intro `Today...` scene:
    - `./run-paper-mario-hires-intro22-capture.sh --tag <tag>`
    - current aligned default is `parallel=22s`, `glide=19s`
  - to seed a deterministic intro/title save state for later replay:
    - `./run-paper-mario-hires-capture.sh --smoke-mode timed --screenshot-at <sec> --timed-save-state-at <sec> --savestate-dir <dir> --tag <tag> --require-hires`
- Comparable Paper Mario HIRES zoom compare on `parallel`:
  - `./run-paper-mario-hires-zoom-compare.sh`
  - `./run-paper-mario-hires-intro22-compare.sh`
  - `./run-paper-mario-open-compare.sh --profile intro22` rebuilds a canonical latest compare before opening
- Intro22 early-step HIRES debug knobs:
  - descriptor-targeted env lists are available for early draw-state sweeps without source edits:
    - `PARALLEL_HIRES_CLEAR_FORCE_BLEND_DESC`
    - `PARALLEL_HIRES_CLEAR_MULTI_CYCLE_DESC`
    - `PARALLEL_HIRES_CLEAR_IMAGE_READ_DESC`
    - `PARALLEL_HIRES_CLEAR_DITHER_DESC`
    - `PARALLEL_HIRES_CLEAR_DEPTH_TEST_DESC`
    - `PARALLEL_HIRES_CLEAR_DEPTH_UPDATE_DESC`
    - `PARALLEL_HIRES_CLEAR_COLOR_ON_CVG_DESC`
    - `PARALLEL_HIRES_CLEAR_AA_DESC`
    - `PARALLEL_HIRES_CLEAR_ALPHA_TEST_DESC`
    - `PARALLEL_HIRES_FORCE_NATIVE_TEXRECT_DESC`
    - `PARALLEL_HIRES_FORCE_UPSCALED_TEXRECT_DESC`
    - `PARALLEL_HIRES_BLEND_1A_MEMORY_DESC`
    - `PARALLEL_HIRES_BLEND_1A_PIXEL_DESC`
    - `PARALLEL_HIRES_BLEND_1B_SHADE_ALPHA_DESC`
    - `PARALLEL_HIRES_BLEND_1B_PIXEL_ALPHA_DESC`
    - `PARALLEL_HIRES_BLEND_2A_MEMORY_DESC`
    - `PARALLEL_HIRES_BLEND_2A_PIXEL_DESC`
    - `PARALLEL_HIRES_BLEND_2B_MEMORY_ALPHA_DESC`
    - `PARALLEL_HIRES_BLEND_2B_INV_PIXEL_ALPHA_DESC`
    - `PARALLEL_HIRES_BLEND_2B_ONE_DESC`
    - `PARALLEL_HIRES_BLEND_2B_ZERO_DESC`
    - `PARALLEL_HIRES_FORCE_BLEND_EN_ON_DESC`
    - `PARALLEL_HIRES_FORCE_BLEND_EN_OFF_DESC`
    - `PARALLEL_HIRES_FORCE_CVG_WRAP_ON_DESC`
    - `PARALLEL_HIRES_FORCE_CVG_WRAP_OFF_DESC`
  - `PARALLEL_HIRES_FORCE_BLEND_SHIFT_ZERO_DESC`
  - `PARALLEL_HIRES_FORCE_BLEND_SHIFT_MAX_DESC`
  - `PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_TEXEL0_DESC`
  - `PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_SHADE_DESC`
  - `PARALLEL_HIRES_FORCE_PIXEL_ALPHA_FULL_DESC`
  - `PARALLEL_HIRES_FORCE_PIXEL_ALPHA_ZERO_DESC`
  - `PARALLEL_HIRES_LOG_STATE_DESC`
  - optional subtype filters for those same probes:
    - `PARALLEL_HIRES_MATCH_RASTER_FLAGS`
    - `PARALLEL_HIRES_MATCH_C0_A`
  - each env accepts a comma-separated descriptor list such as `40,41,42`
- Paper Mario scene manifest:
  - `docs/PAPER_MARIO_SCENES.md`
- Preserved GLideN64 4x HIRES-on intro22 matched oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/hires/oracle-gliden64-4x-hires-on-intro22-matched-1/Paper Mario (USA)-260308-170727.png`
- Preserved GLideN64 4x HIRES-on no-input 16s oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/hires/oracle-gliden64-4x-hires-on-noinput-16s-1/Paper Mario (USA)-260308-011300.png`
- Preserved GLideN64 oracle:
  - `/home/auro/code/parallel-n64-paper-mario-backups/20260306-hires-audit/gliden64/oracle-gliden64-5`
- Refresh GLideN64 only when the oracle scene or workflow changes materially:
  - `./run-paper-mario-gliden64-capture.sh --tag <tag>`
- Current validation is strongest on Paper Mario.
- Current known gap:
  - Paper Mario still diverges from the GLide oracle on file-select stage/frame scaling and minification

## References
- Writable repo:
  - `/home/auro/code/parallel-n64`
- Read-only upstream references:
  - `/home/auro/code/mupen/parallel-n64-upstream`
  - `/home/auro/code/mupen/parallel-rdp-upstream`
  - `/home/auro/code/mupen/RetroArch-upstream`
  - `/home/auro/code/mupen/GLideN64-upstream`
- Read-only Paper Mario references:
  - `/home/auro/code/paper_mario/papermariodx`
  - `/home/auro/code/paper_mario/PAPER MARIO_HIRESTEXTURES.hts`

## Roadmap
- Load packs by ROM identity instead of scanning the system directory directly
- Expand texture-pack container and payload format support
- Revise external APIs and core-option wiring to simplify configuration
- Improve stability and validation across more scenes, packs, and ROMs

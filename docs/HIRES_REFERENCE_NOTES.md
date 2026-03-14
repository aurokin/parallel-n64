# HIRES Reference Notes

## Reference Roots
- Local reference bundle:
  - `/home/auro/code/emulator_references`
- Most relevant repos/files:
  - Dolphin:
    - `/home/auro/code/emulator_references/dolphin-upstream/Source/Core/VideoCommon/HiresTextures.cpp`
    - `/home/auro/code/emulator_references/dolphin-upstream/Source/Core/VideoCommon/TextureCacheBase.cpp`
    - `/home/auro/code/emulator_references/dolphin-upstream/Source/Core/VideoCommon/Assets/TextureAssetUtils.cpp`
  - PPSSPP:
    - `/home/auro/code/emulator_references/ppsspp-upstream/GPU/Common/TextureReplacer.cpp`
    - `/home/auro/code/emulator_references/ppsspp-upstream/GPU/Common/TextureReplacer.h`
  - Flycast:
    - `/home/auro/code/emulator_references/flycast-upstream/core/rend/CustomTexture.cpp`
    - `/home/auro/code/emulator_references/flycast-upstream/core/rend/TexCache.cpp`
  - PCSX2:
    - `/home/auro/code/emulator_references/pcsx2-upstream/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp`
    - `/home/auro/code/emulator_references/pcsx2-upstream/pcsx2/GS/Renderers/HW/GSTextureReplacementLoaders.cpp`
  - DuckStation:
    - `/home/auro/code/emulator_references/duckstation-upstream/src/core/gpu_hw_texture_cache.cpp`
    - `/home/auro/code/emulator_references/duckstation-upstream/src/core/settings.cpp`

## Shared Patterns In Other Emulators
- Replacement binding is cache-owned, not draw-owned.
  - Dolphin loads custom texture data in the texture cache and creates the cache entry from it before later draw composition.
  - Flycast checks custom textures in `TexCache` and loads them onto the cache object, not in a later combiner path.
  - PCSX2 keys replacements from exact texture/hash/region metadata and keeps the replacement map outside later draw composition.
- Replacement validity is checked early.
  - Dolphin validates aspect ratio and integer upscale compatibility in `TextureAssetUtils.cpp`.
  - PCSX2 parses exact filename metadata and rejects invalid replacement names up front.
- Alias and wildcard behavior is explicit and narrow.
  - Dolphin allows only a small set of filename wildcards.
  - PPSSPP supports aliases and wildcards, but they are declared in `textures.ini`, not inferred ad hoc from later draw state.
- Distinct replacement classes are modeled separately.
  - DuckStation separates:
    - texture-page replacements
    - VRAM-write replacements
    - replacement upload tracking
  - This is important for copy/write/compositor-heavy scenes.

## Processes We Appear To Be Missing
- Exact upload-owner binding.
  - Other emulators keep replacement ownership attached to the texture-cache/upload identity.
  - Our fork currently reinterprets replacements later in the RDP draw path through fallback matching and alias propagation.
- Early replacement validation.
  - We do not currently validate native-vs-replacement shape in the same strong way before binding.
- Explicit replacement classes.
  - We do not separate page-style, write-style, and normal texture replacements as cleanly as DuckStation.
- Metadata-driven aliasing.
  - PPSSPP-style aliases are pack-declared.
  - Our alias propagation is inferred from tile relationships at runtime, which is much riskier.
- Upload tracking as a diagnostic mode.
  - DuckStation's `AlwaysTrackUploads` is a useful model for a future debug mode that records upload ownership without immediately broadening replacement consumption.

## Current Architectural Read
- Our current HIRES path is most unlike Dolphin/Flycast/PCSX2 in one specific way:
  - replacements are accepted permissively and then consumed inside the RDP draw/composition path
  - this creates the exact bug class we keep seeing: washed-out stitching, fallback-backed misbinds, and lane-specific composition corruption
- The `strict` lookup probe confirms this:
  - on `intro22-state + 1f`, `strict` lookup produced `lookups=4738 hits=0 draw_with_replacement=0`
  - the current Paper Mario path is therefore heavily dependent on permissive fallback matching

## Best Candidate Re-Engineering Directions
- Stage 1:
  - instrument and classify actual hit sources by fallback family
  - exact match
  - CI low32 fallback
  - tile-mask fallback
  - tile-stride fallback
  - block-tile fallback
  - block-shape fallback
  - alias propagation
- Stage 2:
  - move toward upload-owner replacement binding instead of draw-time reinterpretation
- Stage 3:
  - split fallback-backed copy/write consumers from normal texture consumers
  - DuckStation's page/write split is the closest model in the current reference set
- Stage 4:
  - only keep wildcard/alias behavior that can be justified by explicit pack metadata or a narrow compatibility rule

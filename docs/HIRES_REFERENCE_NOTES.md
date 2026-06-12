# HIRES Reference Notes

Cross-emulator survey of how hi-res texture replacement is keyed and bound
elsewhere. The survey sections are the durable content; the attempt-era analysis
that used to close this doc was superseded by the 2026-06-10 reboot (note at the
end).

## Reference Roots

The `emulator_references` bundle is not currently on disk; re-clone the upstreams
if needed (see [WORKSPACE_PATHS.md](/home/auro/code/parallel-n64/docs/WORKSPACE_PATHS.md)).
Most relevant files, relative to a fresh checkout of each upstream:

- Dolphin:
  - `Source/Core/VideoCommon/HiresTextures.cpp`
  - `Source/Core/VideoCommon/TextureCacheBase.cpp`
  - `Source/Core/VideoCommon/Assets/TextureAssetUtils.cpp`
- PPSSPP:
  - `GPU/Common/TextureReplacer.cpp`
  - `GPU/Common/TextureReplacer.h`
- Flycast:
  - `core/rend/CustomTexture.cpp`
  - `core/rend/TexCache.cpp`
- PCSX2:
  - `pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp`
  - `pcsx2/GS/Renderers/HW/GSTextureReplacementLoaders.cpp`
- DuckStation:
  - `src/core/gpu_hw_texture_cache.cpp`
  - `src/core/settings.cpp`

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
- Diagnostic upload tracking exists as a dedicated mode.
  - DuckStation's `AlwaysTrackUploads` records upload ownership without broadening
    replacement consumption — a useful model for future debug modes here.

## Historical note (attempt-era analysis, superseded 2026-06-10)

This doc previously closed with an architectural read of the pre-reboot fork
(permissive draw-path replacement consumption, a `strict`-lookup probe, and a
staged re-engineering plan toward upload-owner binding and fallback-family
classification). That analysis was recovered from the Attempt A stack
(`origin/hires/current-stack-2026-03-18`, salvage commit c501cdef) and described
the pre-reboot replacement path; the lookup mode and fixture it cited no longer
exist, and the fallback-family program it targeted is the one frozen at the
reboot. The adopted direction is the
GlideN64-compat Rice-CRC draw-time lane with strict identity — see
[ADR-0006](/home/auro/code/parallel-n64/docs/adr/0006-replacement-identity.md)
and the Identity Decision in
[docs/REBOOT_PLAN.md](/home/auro/code/parallel-n64/docs/REBOOT_PLAN.md). Note the
survey's "cache-owned, not draw-owned" pattern did not transfer to N64: pack
identities are authored against GlideN64's render-tile view, which is only fully
known at draw time (ADR-0014), so the strictness lives in the identity convention
rather than in upload-owner binding.

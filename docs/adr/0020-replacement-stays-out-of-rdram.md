# ADR-0020: Replacement stays out of the RDRAM render targets

- Status: Accepted
- Date: 2026-10-18; amended 2026-10-25 (1x)

## Context

An upscaled frame is rasterized twice: a native-resolution pass whose color and
depth are written to RDRAM, where the CPU can read them, and an upscaled pass
that is presented. Hi-res replacement ran in both. Replacement alpha (cutouts,
the HLE alpha-kill rule, alpha test) therefore changed which pixels the native
pass wrote, and a game that reads its render targets back could run differently
with a pack loaded. Ocarina of Time does: in a Kokiri Forest segment a few
depth-buffer pixels differed first and game memory, RSP memory and CPU
registers followed (see the
[issue log](../ISSUE_LOG.md#replacement-changes-the-native-rdram-render-targets)).

## Decision

1. When an upscaled pass exists, the native pass renders with the game's own
   textures; only the upscaled, presented pass samples replacements. A per-pass
   `hires_replacement` flag in `GlobalFBInfo` gates replacement sampling and the
   HLE alpha-kill rule.
2. Without upscaling (1x, the core option's default) the native pass would be
   the presented image. While a replacement provider is attached, the renderer
   therefore adds a presented pass at native resolution through the
   upscaled-domain path with factor 1; the native pass renders without
   replacements and VI scans out the presented copy. Without a pack, 1x keeps
   the single upstream pass. (Amended October 25, 2026; the original decision
   let the 1x native pass replace, and a pack could change game state.)
3. `PARALLEL_RDP_HIRES_NATIVE_PASS=replace` restores the earlier behavior for
   comparison; `native` is the default.

## Consequences

- At every upscale factor, a feature-on run's emulation state, RDRAM included,
  equals the feature-off run of the same scenario, so a pack cannot change game
  behavior. This strengthens ADR-0019 decision 4 for all runtime classes.
- At 1x with a pack loaded the RDP rasterizes each frame twice, as it does when
  upscaling; the measured cost is small (below).
- Effects that the game builds from a copy of its own framebuffer use the native
  pass's content. Over the lab corpus this made an OoT pause background sharper,
  left four effects visually unchanged, and gave the Paper Mario battle-entry
  transition its feature-off fade brightness with faint native ground art in the
  copied screen. The operator accepted that trade-off.
- On-mode goldens recorded before this change are replaced with a justification
  naming it.

## Evidence

October 18, 2026 lab corpus at 4x (Paper Mario, Ocarina of Time, Majora's Mask,
GoldenEye 007, Super Mario 64, Mario Kart 64): 261 feature-on scenarios with
`native` all ended in exactly the feature-off end state; presented captures
matched the earlier feature-on goldens in 255; feature-off on the same build
passed all 262 scenarios against its goldens.

October 24–25, 2026 at 1x (Mander RADV, 44 lab scenarios: Ocarina of Time,
Majora's Mask, Super Mario 64, Paper Mario, Mario Kart 64, GoldenEye 007): before
the amendment, four feature-on end states differed from feature-off outside RDRAM
and 36 more differed in framebuffer and depth ranges; after it, all 44 feature-on
end states equal feature-off byte for byte, and presented captures still differ
from feature-off in 43 of 44 scenarios. Summed scenario time changed by +0.2%
(median paired fps −1.6%). Feature-off at 1x and 4x, and feature-on at 4x, pass
their existing goldens on the amended core.

# ADR-0020: Replacement stays out of the RDRAM render targets when upscaling

- Status: Accepted
- Date: 2026-10-18

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
2. Without upscaling the native pass is the presented image and keeps replacing.
3. `PARALLEL_RDP_HIRES_NATIVE_PASS=replace` restores the earlier behavior for
   comparison; `native` is the default.

## Consequences

- With upscaling, a feature-on run's emulation state, RDRAM included, equals the
  feature-off run of the same scenario, so a pack cannot change game behavior.
  This strengthens ADR-0019 decision 4 for upscaled runtime classes.
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

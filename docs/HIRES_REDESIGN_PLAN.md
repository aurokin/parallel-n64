# HIRES Redesign Plan

## Why
- The remaining Paper Mario artifacts are no longer explained by one-off descriptor bugs.
- The dominant failures are architectural:
  - draw-time fallback reinterpretation
  - alias-propagated replacement binding
  - repeated descriptorless consumer families that accumulate over framebuffer state
- The current renderer mixes two different concerns in one place:
  - how a replacement was found
  - how that replacement gets bound and propagated to live tiles

That coupling made the earlier debugging productive, but it also made the system hard to reason about and easy to overfit.

## Goal
Move toward a cache-owner style HIRES system where:
- lookup provenance is explicit
- binding/application policy is explicit
- reinterpretation is narrow and justified
- HIRES-off behavior remains unchanged

## Current Direction
The current preferred architecture probe is:
- `parallel-n64-parallel-rdp-hirestex-lookup = no-reinterp`

Why:
- it keeps primary/provider hits
- it keeps alias propagation
- it removes block reinterpretation classes
- it has been the strongest shared signal that block reinterpretation is the broader corruption source

## First Refactor Cut
Separate post-lookup binding from lookup itself.

The new policy layer should own:
- lookup provenance
- lookup owner tile
- lookup key dimensions
- sampling/orig dimensions
- descriptor binding metadata
- whether the result is applied only to the owner tile or propagated across the alias group

This is intentionally a refactor first, not a behavior change.

## Binding Policy Breakpoint
The next redesign cut must keep two invariants true:
- permissive/default HIRES binding stays pixel-identical
- owner-style binding becomes meaningfully narrower

The binding-policy layer now owns both:
- post-lookup owner-tile binding
- alias-source rebinding after live tile metadata changes

The verified result on the canonical `intro22-state + 1f` path is:
- permissive/default remains exact-baseline identical
- `owner` remains exact-image identical to the prior owner probe
- `owner` now reports `alias_bindings=0`

That matters because the earlier `owner` probe could still be contaminated by open-coded alias-source rebinding in `rdp_renderer.cpp`. Future redesign work should treat `rdp_hires_binding_policy.hpp` as the single place where lookup provenance becomes live tile binding behavior.

The next supporting step is also in place:
- lookup mode semantics are now expressed as `HiresLookupModePolicy`
- the renderer no longer relies on one broad `hires_lookup_fallbacks` boolean to decide which provenance families are legal
- future reinterpretation redesigns should change that policy object, then let lookup/binding code consume it

The next provenance step is now in place too:
- birth metadata is represented as `HiresLookupBirthSignature`
- binding decisions carry that typed signature instead of loose `load_fs/lookup_fs/lookup_tile/key_wh` scalars
- reinterpretation probes now match against that signature object

That is the bridge to provenance-family rules: the next narrowing cut can classify or filter whole birth-signature families without rebuilding that data shape again.

## Redesign Stages
### Stage 1: Make ownership explicit
- Keep current behavior.
- Centralize the post-lookup binding decision.
- Stop open-coding state writes and alias propagation in `rdp_renderer.cpp`.

### Stage 2: Split producer and consumer classes
- Treat normal sampled tiles separately from:
  - copy/write style producers
  - descriptorless consumers
  - reinterpretation-backed consumers
- The current scene evidence says these classes need different rules.

### Stage 3: Narrow reinterpretation
- Preserve primary/provider hits.
- Preserve only reinterpretation classes that can be justified by:
  - upload-owner semantics
  - explicit pack metadata
  - documented compatibility rules
- Stop relying on broad runtime reshaping of TMEM uploads.

### Stage 4: Move toward upload-owner binding
- Bind the replacement to the lookup/upload owner first.
- Make later consumer use explicit instead of inferred from broad alias rules.
- This is the model used by other emulator texture-cache systems.

## Practical Validation Rules
- Use only the canonical `intro22-state + 1f` path for renderer truth.
- Compare probe vs baseline first.
- Compare against GLide after the probe shows a real baseline diff.
- Keep `noinput16` as a secondary sanity scene, not the primary truth scene.
- HIRES-off behavior is invariant.

## Immediate Next Work
1. Keep using the new binding-policy layer as the only place where lookup results become live tile bindings.
2. Add explicit binding-policy modes behind that layer.
3. Rework reinterpretation acceptance at the lookup-provenance level, not in scattered draw-time branches.
4. Revisit descriptorless consumer families once the ownership path is cleaner.

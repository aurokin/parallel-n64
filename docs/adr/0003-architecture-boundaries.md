# ADR-0003: Renderer And Repository Boundaries

- Status: Accepted, amended
- Date: reaffirmed 2026-07-10

## Context

Cross-emulator study and failed local attempts both showed that replacement,
upscaling, presentation, and orchestration become unmaintainable when their
policy is mixed.

## Decision

Keep four renderer systems distinct:

1. hi-res texture replacement;
2. texture upscaling;
3. internal RDP scaling;
4. VI presentation.

Replacement keying, decode/cache, residency, and budget policy stay behind one
provider boundary. Draw code consumes resolved replacement state. VI remains
the final presentation authority.

Repository ownership is equally explicit:

- `parallel-n64` owns renderer/core behavior, fixtures, portable
  scenarios/adapters, and renderer evidence;
- RetroArch owns generic control, capture, state, replay, and headless
  transport;
- game source labels semantic state but is not runtime authority;
- eval scoring, gameplay research, private topology, and fleet scheduling stay
  in their owning external systems.

No public product path may require a private orchestration repository.

## VI Reference Constraints

The removed VI research notebook preserved four hardware facts that remain
relevant if VI work resumes: accumulated `Y_SCALE`, field-relative
half-line offsets, the `X_SCALE=0x200` quirk, and interlaced line
weighting. New VI behavior must derive from documented semantics and focused
tests, not scene-tuned constants.

## Consequences

Classify defects as baseline, replacement, scaling, or tooling/fixture before
editing. Move private values outward through explicit arguments or environment
variables; never teach product code to discover a fleet controller.

## Evidence

- `texture_replacement.{hpp,cpp}`;
- vendored `parallel-rdp/video_interface.cpp` and `shaders/vi_scale.frag`;
- `vi_scanout_policy.hpp` and `vi_scale_policy.hpp`;
- repository-local fixture and adapter test contracts.

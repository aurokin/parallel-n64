# ADR-0003: Architecture boundaries: four systems, one provider seam, VI authority

- Status: Accepted
- Date: pre-reboot (research sweep + failed-attempt analysis); in force unchanged

## Context

A cross-emulator survey (DuckStation, PCSX2, PPSSPP, Flycast, Dolphin, Azahar —
five of these surveyed in
[docs/HIRES_REFERENCE_NOTES.md](/home/auro/code/parallel-n64/docs/HIRES_REFERENCE_NOTES.md);
Azahar covered in the PROJECT_NOTES.md research sweep)
and the post-mortem of the failed attempts converged on the same structural rules.
The failed branch's policy-header explosion (consumer/alias/binding/ownership
policies) distributed complexity instead of reducing it.

## Decision

1. **Four separate systems.** Hi-res replacement, texture upscaling, internal RDP
   upscaling, and VI presentation scaling are designed and debugged as distinct
   systems with explicit handoffs — never one monolithic "hi-res plus scaling"
   feature. This underpins the standing bug-classification rule: every defect is
   classified baseline / hi-res / scaling / tooling-or-fixture.
2. **One replacement subsystem behind a provider boundary.** Key generation,
   lookup, CPU decode/cache, GPU residency, and budget policy live together in
   `texture_replacement.*`, injected as a provider; draw code only consumes
   resolved replacement state.
3. **The enabled path must be explainable end-to-end.** If a replacement is used,
   the system must be able to say why it matched, what it replaced, how dimensions
   were mapped, and why output differs from baseline — otherwise the implementation
   is not ready. This drives the observability conventions (keying summaries,
   class-level counters, `hires-evidence.json`, attempted-key miss logging).
4. **VI is the final presentation authority.** RDP enhancement never bypasses VI
   semantics; the failed attempt's VI micro-policies were dropped.
5. **Provenance/alias/occurrence analysis is offline tooling**, not runtime
   architecture. The runtime contract stays small.
6. **Four-layer repo boundary model:** RetroArch (agent-control branch) transports
   structured control/capture; the video core explains renderer behavior; the
   Paper Mario decomp labels game state; wrappers orchestrate. No emulator-specific
   renderer meaning in the frontend; cross-project orchestration stays in
   parallel-n64.

## Consequences

- Bugs stay attributable: the 2026-06 forensics arcs (sampler, texrect, keying,
  orig-dims) each landed in exactly one system.
- The provider boundary survived two failed attempts and the reboot — it is the
  one piece of prior architecture treated as proven.

## Evidence

- PROJECT_NOTES.md "Research Sweep", "Architecture Direction", "Proposed Boundary
  Model", failed-attempt analysis; `mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.{hpp,cpp}`.

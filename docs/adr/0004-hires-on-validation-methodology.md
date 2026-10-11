# ADR-0004: Validate Hi-Res-On Semantically And Visually

- Status: Accepted
- Date: 2026-06-10; reviewer-hint amendment 2026-06-12; temporal review amendment 2026-10-11

## Context

Hi-res-on output intentionally differs from baseline. Pixel digests and exact
metadata counts therefore create false confidence.

## Decision

Use:

1. class-level semantics such as entry presence, draw-hit presence, expected
   source mode, and explicit fallback reasons;
2. paired captures with an explicit verdict: correct, missing, wrong texture,
   wrong region, or corruption;
3. falsification experiments and targeted probes before changing renderer or
   shader behavior.

Reviewer context may identify the scene but must not describe the suspected
artifact as expected content. Independent review is useful for consequential
visual claims, but no fixed agent count is part of the contract.

Corruption always fails. An explicit fallback with a valid reason is acceptable
when the scenario permits it.

### Reference roles and animated effects

Choose the expected property before choosing the reference:

| Question | Evidence that can answer it |
| --- | --- |
| Geometry, perspective, depth, coverage or native combiner behavior | Native-texture control, upstream behavior and available hardware/command evidence. The maintained renderer is not automatically correct merely because it is the control. |
| Which repaint/key/alpha content belongs to a draw | Pack bytes, native texture views, draw-time identity and GlideN64's sanctioned content/source/dump evidence under ADR-0005. |
| Replacement filtering, cutout, blending or fallback | The class contract, an isolated counterexample, same-runtime controls and frame-aligned semantic/visual evidence. |
| Animation phase, camera or dialogue state | Named start, completed load, exact input/frame sequence and an explicit game-state cue. Wall-clock proximity and filenames are insufficient. |

GlideN64 content agreement cannot establish geometry or coverage correctness.
Native output alone cannot establish the desired appearance of authored
replacement alpha. Record uncertainty when the references disagree; investigate
the relevant mechanism rather than choosing the prettier image.

For a transient effect, retain a sequence covering before onset, onset, peak,
decay and recovery where those phases exist. For persistent changes, record
onset and stable state with a bounded stop. A single selected frame or final eval capture may miss the
event. Match effect/game phase across controls; different cores can require
independent scene routes and incompatible savestates must not be interchanged.
Use matched settings and vary one cause at a time within the maintained-core
comparison. Preserve old and new source/runtime identities as separate conditions.

Distinguish the observed frontend frame, game-state counter and captured image
phase. An acknowledgement or a status sample after a screenshot does not prove
which GPU frame was copied. At a transition, RAM can report the next mode while
the image still depicts the preceding scene. Require capture-completion evidence
and an appropriate phase/freshness check before attributing that difference to
the renderer; record the limitation if no intrinsic captured-frame identity exists.

Separate an observed anomaly from a confirmed renderer defect. Confirmation
requires a reproducible input/phase, an explicit violated property, and evidence
distinguishing renderer behavior from pack content, intentional animation,
state/capture faults and backend-specific behavior. Automated or model review
may propose candidates; it does not turn visual differences into correctness
gates. Keep rejected hypotheses and negative controls with the case.

Closing a confirmed renderer-defect fix requires a class-general regression or
counterexample that fails before
the change, passes after it, and leaves the protected path intact, plus review
of the affected runtime sequence. A unit test of arithmetic alone does not close
a reported scene artifact. Historical footage is evidence for its recorded
condition, not proof that the defect persists or is fixed on the current core.

## Consequences

- No hi-res-on digest or exact-count gate.
- Every runtime claim leaves a bundle with identities, logs, semantic evidence,
  and the capture used for judgment.
- Shader probes are diagnostic evidence, not production policy.

## Evidence

The required gate enforces the no-metric policy; runtime authority runners emit
the class-level evidence consumed by reviewers.

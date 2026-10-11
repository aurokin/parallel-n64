# Reproducible renderer evidence

Keep enough to reproduce and judge a case. A complete recording is temporary
inspection material by default, rather than the permanent result of a run.

## Durable case record

For each retained defect or useful coverage checkpoint, record:

- Case ID, visible symptom, renderer class, expected property and the source or
  control supporting that expectation. Distinguish an observation from a
  reproduced defect and a qualified correction.
- Source commit and actual core binary hash, frontend and adapter identities,
  ROM revision/hash, source and converted pack identities, relevant options,
  effective configuration, graphics backend, driver and output size. Reference
  shared inputs by identity and locator instead of copying them into every case.
- Reproduction entry: fresh boot with empty saves, an in-game save, or a protected
  pre-event state. Include the state's hash, origin and producing runtime. Never
  assume a state works across cores, builds or backends.
- Exact input and neutral holds, frame steps, load/settle sequence, observable
  start/stop cues, and a runnable script with its dependencies. Say whether fresh
  boot is repeatable, random, or only a way to discover the event.
- A few original full-resolution PNGs showing the relevant before/active/after
  phases and controls; retain event-local diagnostics that support the cause.
- Tested correction commit/build, replay results, visual reviewer, regression
  controls, uncertainties and remaining coverage gaps.

Use existing scenario, adapter and lab output; a new case catalog or gameplay
system is unnecessary. A state plus a vague scene name is insufficient. Cold
loads can require different settling from a warm scene: retain the actual
qualified sequence without silently changing a fixture's authority contract.
Fixture promotion still follows [ADR-0008](adr/0008-fixture-and-evidence-authority.md).

For feature-off pixel parity, match the process start and ordered load/step
history as well as the selected state's bytes. The VI's gamma-dither seed uses
its runtime frame count, which a CPU-state reload does not reset. An extra title
reload before the same race state can therefore change pixels without changing
the native scene. Record that history instead of weakening the parity check.

## Capture and closure

Default to screenshots and event states during play or automatic demos. Capture
short video when motion, ordering or a transient fault cannot be judged from
those assets. A full loop can be recorded for first-pass alignment, with an
explicit temporary byte budget and disk headroom for its pending flush.

Before retiring that recording:

1. Inspect it and extract the meaningful original frames and event states, or
   record why a fresh-boot recipe is sufficient. A randomly selected route does
   not become an exact replay merely because its inputs were neutral.
2. Verify retained bytes and references, and cold-replay actionable cases on the
   named candidate. Preserve failed or untested availability labels.
3. Record the retired file's path, byte count, hash, reason and date in a separate
   retirement receipt. Keep the original capture/validation record intact.
4. Remove full masters, trimmed duplicates and display proxies together unless
   a named unresolved motion case requires a bounded event clip. Such a clip
   needs a reason and a review/removal condition; avoid indefinite full videos.
5. Update current links and report measured remaining storage. Shared packs,
   ROMs, protected states and unrelated historical archives are separate inputs.

Aim for at most 100 MiB per durable defect/checkpoint, excluding shared ROMs,
packs and reusable runtimes. Review a case above 250 MiB and record a concrete
reason or reduce it. These are retention budgets, not renderer correctness
gates. Size the temporary run separately, check free space between phases and
before large writes, and stop before the operator's free-space floor is crossed.

## Interpreting comparisons

GLideN64 supplies replacement-content and key reference evidence. Native
ParaLLEl controls establish RDP composition and sampling expectations. Neither
whole-frame similarity nor replacement-hit counts qualify hires output.
Compare aligned surfaces and semantic classes; record intentional pack curation,
scripted lighting, camera differences and random demo variants explicitly.

A clean automatic demo is bounded coverage. Choose the next run for a missing
renderer class or a reproducible lead, rather than accumulating playtime,
recordings or additional packs without an inspection and reuse purpose.

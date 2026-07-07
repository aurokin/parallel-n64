# Issue Log

Working register of known/suspected issues. Opened 2026-07-05.

**Policy in effect (2026-07-05):** emulator/core and renderer *dev and analysis* are
ON HOLD — pre-Fable emulator fixes have been hit-or-miss, so new core work waits for a
careful Fable-era pass. Log issues here instead of fixing them. **Eval-harness and lab
fixes remain actionable.** Each entry is tagged `HELD-CORE`, `HELD-RENDERER`,
`ACTIONABLE-EVAL`, or `ACTIONABLE-LAB`.

## Owner rulings — 2026-07-05
- **R1 · baseline** — RATIFY P1F-002 as the v3 gpt-5.5 baseline; record G4 as
  ">5402.3 (cap-censored; S4 5283.3)"; no rerun. **Effective when its replay verification
  returns clean** (it is last in the replay-fleet queue). Baseline is playbook-only;
  objectives cells carry the guide-variant delta.
- **R2 · guardrails** — ADD BOTH: the pre-tar manifest check (IL-8) and the
  no-rsync-into-eval-runs rule (IL-9). In progress (eval-harness work).
- **R3 · guide freeze** — APPROVED, but the guide was intentionally re-opened for the v3
  route-guidance redesign (objectives out, walkthrough in). New sequence: freeze happens
  **after the walkthrough branch lands + owner reviews the assembled guide** (R5), not at
  fleet-green. **DONE 2026-07-05** — R5 approved; frozen + merged to master (`0f4a48b`,
  pushed to origin).
- **R4 · P3 G4 divergence (decided 2026-07-05): ACCEPT, log for later.** Fleet-green
  **soundness gate = MET.** The 90-min scored surface (G1–G3 + S1–S4) replays
  deterministically across all 7 archives; the marathon's G4 battle is the only divergence
  and no 90-min cell reaches it. G4-determinism-across-a-marathon logged as a deferred
  investigation (IL-14), to revisit when marathons resume.
- **R5 · guide review — COMPLETED 2026-07-05.** Owner approved the assembled v3 guide.
  Both branches merged to master and pushed: `f25c2aa` (guardrails: IL-8 manifest check +
  IL-9 no-rsync) then `0f4a48b` (v3 finalization freeze). Merged master renders
  byte-identical to the reviewed R5 artifact. Verification doubled: the finalization
  workflow's own 4-agent self-verify (verbatim / refs / exports) all PASS, and an
  independent re-check agreed — walkthrough split byte-identical 3 ways (sha256
  321bc3f2…), retirement/freeze grep-clean, 25/25 export-matrix assertions. Two
  non-defect notes: the source guide's own Appendix E/F numbering quirk (preserved
  verbatim, mapped correctly in index.md) and a harmless pre-existing dead
  `{{VISION_TOOLS_RULE}}` no-op replace in the render.
- **R6 · lock loop; soundness resets on any bench change (decided 2026-07-05):** v3 is NOT
  "locked" by a single baseline re-cut. Step 5 is a **loop**: freeze the bench → cut one
  baseline (gpt-5.5, current default guide, mander) → **deep-analyze that run's log** →
  triage findings to {bugfix, doc update, none}. If any change lands, the bench changed and
  the **soundness gate RESETS** — re-freeze and re-cut. Iterate until a baseline run needs no
  changes; only then is v3 **LOCKED** and scored waves open. The baseline re-cut's own archive
  doubles as the fresh soundness probe (replay-verify it on the frozen driver each iteration).
  Corollary: the R4 fleet-green declaration verified the **pre-change** archive set (old
  playbook/objectives guide, pre-fix driver) — it does **not** carry to the changed stack.
  Standing principle beyond this loop: any later bench change (bugfix OR doc update) re-opens
  the soundness gate.

## v3 finalization pass — checklist ✅ COMPLETE (merged to master `0f4a48b`, pushed 2026-07-05)
All items landed, R5-approved, merged. Next: the lock loop (R6) — baseline re-cut on mander.
1. Retire `--scripts-provided` (flag, `field_probe.py` copy, usage/comment; run_eval passthrough).
2. Freeze `--with-macros` — KEEP machinery; add a FROZEN usage comment + a runtime WARNING when
   invoked (immature macro set, not for scored evals, revisit later). Do NOT remove.
3. `--bare-adapter` — keep as-is; **VERIFY it still renders the base screenshot guidance**
   (adapter `screenshot` command + "screenshots are your eyes" — both are base/unconditional,
   confirmed 2026-07-05; ensure the restructure doesn't break it).
3b. TAS-block CORE IDEA — observability + state snapshotting (correction + expansion 2026-07-05):
   the TAS block already surfaces `tas.shot` and "savestate branching" (an earlier "zero
   screenshot content" note was a bad-grep false negative), but the *core philosophy* is diffuse,
   never stated as the point. FOLLOW-UP EDIT (post-workflow, same branch): make the pairing
   explicit as the central pattern — at each segment, pair a `tas.shot(label)` capture with a
   `tas.save`/`tas.saveSlot` snapshot, so the script is both **observable** (see every step, not
   running blind) and **recoverable** (`tas.load` back to any captured point to retry a stretch
   without replaying from the top). Common/recommended, not mandatory. Depth already lives in the
   docs (AGENT_GAMEPLAY.md crawl/capture/save-rings; TAS_SCRIPT_MODEL.md sweep/branch/stateRing);
   the BLOCK is what must state the idea prominently. Bring the revised block in the R5 review.
4. Marathon cap: `--marathon` defaults `--cap-minutes` to **360** unless explicitly set.
5. Walkthrough → progressive disclosure: split into `walkthrough/` per-chapter files (author's
   own chapter boundaries, verbatim) + index carrying the provenance header; guide points at the
   index. (PENDING owner thumbs-up on structure.)
6. Housekeeping: return the main checkout to `master` after merges (currently parked on the branch).

## v3 SOUNDNESS: gate RESET on bench change (updated 2026-07-05, per R6)
The 2026-07-05 fleet-green declaration (defects fixed+validated ✓ · replay green 6/7 + P3-G4
accepted per R4 ✓ · label/instr conformance ✓ · pre-round gates ✓) verified the **pre-change**
stack — old playbook/objectives guide, pre-fix driver. The v3 finalization batch (walkthrough
default-on, objectives retired, scripts-provided retired, macros frozen, marathon cap wiring,
TAS-block edit) changes the bench, so the gate is now **PROVISIONAL** and is re-earned inside
the lock loop (R6), not carried over.
Sequence to **scoring opens**: finalization workflow + TAS-block edit land ✅ → owner guide
review (R5) ✅ → freeze + merge (master `0f4a48b`, pushed) ✅ → lock loop ✅ (iter-1 recut,
iter-2 clean + replay-verified, below) → v3 LOCKED 2026-07-06 (iter-2 needed no bench change
and its archive replay-verified deterministically on the frozen driver — both halves of the
soundness condition met on the same recut) → **bench window re-opened 2026-07-06 (owner):**
"get all pending changes landed (or declined) before v3 starts" — per the R6 standing
principle the gate reset; the window's changes were batched (window section below) and ONE
recut re-locked → **v3 RE-LOCKED 2026-07-06 (iter-3)**: baseline-gpt55-v3-003 came back
clean (deep triage: lock-eligible, zero required changes) and its archive replay-verified
deterministically on the frozen driver (all 9 gates, max |Δ| 150 fc, G3/S1 exact) — both
halves of the soundness condition met on the same recut, again → wave 1 ran PROVISIONAL
(pre-lock) per the ratified SCORING_PROTOCOL: pair 1 (opus terminal G4 79.4m + gpt cap;
gate CLEAN, analyzer-layer fixes only) and pair 2 (DOUBLE TERMINAL on the headless class:
gpt@saur 70.2m, opus@tortle 81.3m) → **bench window re-opened 2026-07-06 at the pair-2
boundary (owner): playbook2.** Owner-directed batch: the verified gaming-tips landing
(evals `23aac0d`, T1–T9 + corrected T10, `guide=playbook2+walkthrough`), toolkit I1
shot-telemetry stamping + shot index (lab `95209e5`), AGENT_GUIDE analog `--mask` line,
adapter `cmd_input` honest errors (`6bb1ea91`). NOT landed (deliberate): LOAD_STATE reply
newline/success token — the success line is OSD-only in RetroArch, the glue sits in
agent-control, and a five-host rebuild mid-wave is not worth an already-compensated
cosmetic; stays queued. Pair 3 (gpt@luma + opus@metapod) cuts on the playbook2 stack;
pair-1/2 runs stand as pre-lock probes and their ledgering falls to the post-wave lock
ruling (guide-confound documented in LABEL_CONTRACT) → **pair-2 gate CLEAN (2026-07-06,
evals `docs/forensics/w1p2-pair-triage.md`):** 5-lane triage + adversarial verification (53
findings, 0 refuted) + decider. Probes: saur VERIFIED 4/4; tortle dispositioned
PREFIX-DETERMINISTIC — the replay forked at the run's FIRST savestate load (44-load run;
replayed saves mint at drifted fc, a load straddling the kkj_00/kkj_01 door re-anchored the
replay behind it), G4 internally attested (battleID 0x2301 raw reads, 2-poll stable); a
replay-driver re-anchoring enhancement is queued to the lock window. Two evidence riders
landed inside the open window (execution-neutral): run_summary.py read-tier classifier fix
(exact poll-battery wire-string match + honest labels; re-stamp sweep) and cut_run.sh CLI
identity echo (codex JSONL carries no model string — labels were launcher-only claims).
Substantive corrections: the pair-1 "0.96x stepping band" RETRACTED (mander primary peaks
16.7 fc/s, 25.3 not derivable); opus S1u stamp quarantined (scorer fired on a map-transition
pos transient under inputDis=2 — v2.1 semantics + wave-1 S1u re-derivation queued to the
lock). Headless host axis filed: metapod-grade latency, no directional fc-at-gate bias,
pair-1 cap/fc recommendation survives. → **pair-3 gate CLEAN (2026-07-07, evals
`docs/forensics/w1p3-pair-triage.md`): double time-cap, WAVE 1 COMPLETE.** First playbook2
pair (gpt@luma + opus@metapod, both macOS); both probes VERIFIED (luma exact-zero fc deltas —
strongest determinism evidence of the wave; the "missing slot1" manifest wart dispositioned
environmental after the replay reproduced the stream with the file intact). Losses
within-variance: both died in the kkj castle-room hazard family (opus 49 min in kkj_01, ~8–11
min short of terminal at the cap; gpt declared the correct kkj_00 upper door "non-route" after
two presses 90–160 units off-center — zero memory reads all run). Playbook2 attribution:
content tips helped where internalized (opus §9 y-telemetry), procedural clauses did not bind;
the §4 door-clause adjudication (owner-requested workflow) + the sweep-macro tip feed the lock
window. No bench change. Wave probe record 5 VERIFIED + 1 PREFIX-DETERMINISTIC. Next: the
post-wave lock ruling. → **lock window OPENED 2026-07-07 with the owner-ratified door-clause
reword (playbook3):** adjudication verdict REWORD (14 agents) + a two-judge hardening pass that
caught a diagonal-slide defect in the proposed hardening (a compliant sweep would have been
all-null presses per §4's own physics); landed text uses the sequential stop→hold→press idiom,
per-press outcome checks with mid-sweep abort, a door-width granularity floor, and the
dead-end-room escape. Guide label → `playbook3+walkthrough` (evals `LABEL_CONTRACT` updated);
playbook3 smoke owed before the next scored cut; remaining lock agenda unchanged.

## Lock-loop iterations (R6)
- **Iter 1 — baseline-gpt55-v3-001 (2026-07-06): recut-required.** Clean full 90-min time-cap on
  the frozen stack; integrity clean and archive-CLEANER than P1F-002 (IL-8 verdict `ok`, all 8
  slots present, identities byte-identical, audit empty, 0 WRITE_CORE_MEMORY). Gate vector
  G1–G3+S1 ~40–50% faster than P1F-002 (walkthrough helped early nav), then a ~54-min S1→S2
  stall (kkj_01 staircase confusion + NPC-collider door trap) cost S3/S4 — classed
  **agent-behavior/execution-variance** (walkthrough neither cause nor cure; P1F-002 crossed the
  same room 5.2× faster). Deep-analysis workflow (4 analyzers + triage) → recut-required NOT for
  bad evidence but for bench-surface fixes. Committed **c238a26** (n64-agent-evals master, pushed):
  README+guard `-s workspace-write`→`danger-full-access` pair, de-false the analog comma warning,
  generic PLAYBOOK door-failure rule (trigger contact / NPC-steal / non-exit doors → timebox +
  move-on), walkthrough index orientation note, WALKTHROUGH_BLOCK human-guide framing, diagnostic
  S1u-kkj01-upper subgate (verified fires t=2077.7s), forensics note. Full forensics:
  n64-agent-evals `docs/forensics/baseline-gpt55-v3-001-triage.md`.
- **Iter 2 — baseline-gpt55-v3-002 (2026-07-06): CLEAN → v3 LOCKED.** Recut on
  c238a26. **First baseline to complete the full ladder** — reached terminal G4 (Bowser intro battle
  `0x2301`, decomp+final-frame-confirmed true positive) at 3363.4s (56 min), `agent_rc=0`, clean
  integrity (archive `ok`, identity byte-identical, audit empty, 1 session, 0 comma-analog, monotonic
  frame_clock). The kkj_01 S1→S2 crossing that stalled v3-001 (3266s) collapsed to 729s; the PLAYBOOK
  door-rule resolved the exact iter-1 NPC-on-door trap **in one press** (rollout msg 102→103), and the
  new `S1u` subgate fired correctly (1915.6s). Iteration-2 analysis (2 verifiers + decider): both
  clean, **zero new bench defects → lock-eligible**. Honest caveat: at n=1 the full speedup is not
  attributable to the fixes (P1F-002 crossed the same room fast without them); best read is the fixes
  handle the NPC-door failure mode deterministically and rollout variance supplied the rest — does not
  affect lock-eligibility (which turns on absence of new defects). **R6 replay probe (2026-07-06,
  mander): `verified`.** `backfill_replay.py` fed the archived 2075-command trace to a fresh session
  (dry-run est. 32.7 min); `backfill_status=verified`, `determinism.verdict=verified`,
  `failed_sends=[]`. All 9 gates re-fired within fc-tolerance 900 — max drift 220 fc (S4), and
  **terminal G4 re-fired at Δ−28 fc**: the pre-negotiated R4/IL-14 G4-divergence exemption was not
  needed (P3's battle-region nondeterminism does not manifest at this run's 21.8k stepped frames;
  IL-14 stays scoped to marathons). Evidence: `~/eval-runs/baseline-gpt55-v3-002/backfill/`
  (backfill-record.json, polls.jsonl, ledger-annotation.proposed.json — ledger untouched).
  **Clean iteration + deterministic replay = LOCK. Scoring opens.** (Superseded same day by
  the owner-declared bench window; see iter-3.)
- **Iter 3 — baseline-gpt55-v3-003 (2026-07-06): CLEAN → v3 RE-LOCKED.** Recut on the
  post-window stack (harness `18abe48`, core `a35c1251…` @ parallel-n64 `f7d79af6`,
  RetroArch `50090d52ee`), launched via cut_run.sh (defaults auto-applied, label
  contract-exact). Second consecutive full-ladder run: terminal G4 at 3847.7s (64.1 min),
  `agent_rc=0`, 1 session, 0 reattaches, archive `ok` (missing/extra empty), audit empty,
  identity drift no, lab_snoop clean, 358/358 stream lines parse, zero IL-10 regression
  greps. The run live-exercised the window's core change (one slot-7 load through the IL-15
  gate, succeeded, designed frame re-sync observed). Deep-triage workflow (4 lanes +
  adversarial verify + decider, 7 agents): **lock-eligible, zero required changes** — one
  confirmed non-blocking harness defect (IL-16 below: run_summary.py SCORER_READS telemetry
  misclassification, pre-existing byte-for-byte in locked v3-002 → queued, not a reset).
  **R6 replay probe: `verified`** — `backfill_status=verified`, `failed_sends=[]`,
  `prefix_deterministic=true`, all 9 gates re-fired within tolerance: G1 −150, G2 −12,
  **G3 0**, **S1 0**, S1u +1, S2 +1, S3 +1, S4 −37, G4 −60 fc. Both soundness halves met on
  the same recut. Evidence: `~/eval-runs/baseline-gpt55-v3-003/` (+ `backfill/`);
  forensics: n64-agent-evals `docs/forensics/baseline-gpt55-v3-003-triage.md`.

## Pre-scoring bench window — opened 2026-07-06 (owner), closes via recut v3-003
Owner directive: land or decline every pending change before scoring starts, and bring the
new headless hosts (saur/tortle) into the fleet properly. Everything below landed in this
window; per the R6 standing principle the soundness gate re-arms with ONE recut + replay probe.
- **Harness (n64-agent-evals `18abe48`, `4b7b239`):** `--start-paused` is now default-on in
  run_eval.sh (v3 pause regime by default; `--no-start-paused` warns loudly, control arms
  only); backfill dry-run gained the pre-honest-pause adapter scan (IL-12 guard — flags
  pre-4339d77f workspaces via the "STEP_FRAME acks on ACCEPTANCE" marker; controls: grok-003
  flags, v3-002 clean); `cut_run.sh` canonical launcher auto-applies the v3 defaults
  (eval file, 90-min cap, pause regime, label contract, codex `danger-full-access`; presets
  codex-gpt55 / claude-opus48).
- **Adapter (parallel-n64 `6a771a34`):** IL-10 slot-cursor fix — entry below now FIXED,
  live-proved on mander and headless on saur.
- **Core (parallel-n64 `e64ae95a`):** IL-15 load-path entry gate — entry below now CLOSED.
- **Headless fleet expansion (parallel-n64 `144580fb`+`fb441c64`+`da612380`; RetroArch
  agent-control `50090d52ee`):** headless-vulkan spike merged after adversarial review
  (verdict merge-with-fixes). Fixes applied at merge: null input drivers made conditional on
  `RETROARCH_VIDEO_CONTEXT_DRIVER` (display hosts' append config stays byte-identical);
  `headless_vk` ordered after `gfx_ctx_null` so auto-fallback can never silently select it
  (explicit ident match only); env value validated; and the interactive-session adapter
  wired too — the spike had wired only the stdin/scenario adapter, which would have left
  the eval harness unable to run headless. saur+tortle brought up for real: missing dev
  deps installed (freetype, x11-xcb, lzma — the koopa log's `_spikes` checkouts no longer
  existed; rebuilt from the proper fleet checkouts), RetroArch agent-control + core rebuilt
  on both, end-to-end verification on BOTH hosts: B50 VF selected (not llvmpipe), headless
  surface created, full command proofs, semantic verification passed, captures
  byte-identical across the two hosts AND matching the spike's validated hash (third
  independent reproduction) → headless feature-off baseline minted
  (`EXPECTED_SCREENSHOT_SHA256_OFF_HEADLESS`, `da612380`) and the strict digest gate
  re-verified green on saur. Eval-harness command surface (paused start / step / save-load
  / screenshot) proven headless via the interactive adapter on saur.
- **Player-path validation (pre-window stack):** smoke-opus48-v3-001 (opus-4.8, --smoke):
  G1 at 763s, cap-kill rc-124 with stream-json intact (654 lines, 77 tool_use), archive ok.
- **Window CLOSED 2026-07-06** via the iter-3 recut (below). Frozen identities: harness
  `18abe48`+cut_run `4b7b239`, parallel-n64 `f7d79af6`, core sha256 `a35c1251…`, RetroArch
  agent-control `50090d52ee`. SCORING_PROTOCOL owner-decision cells remain the only
  pre-scoring open item (recommendations committed at evals `75f8a6e`).

## v3 scope decisions — 2026-07-05
- **Marathons deferred** until v3 is tuned and all v3 scored evals have run; revisit after.
- **Marathon cap = 6h (decided 2026-07-05).** No baked default exists today (base eval cap
  90 min; marathons override via `--cap-minutes` at launch — P3 used 360, marathon-002 180).
  TO IMPLEMENT in the finalization pass: wire `--marathon` to default `--cap-minutes` to
  **360** (overridable); base eval cap stays 90 for standard runs.
- **Marathon roster** likely **gpt-5.5 + opus-4.8 only** (they are far ahead; other models
  are a token-waste beyond the standard eval).
- **Non-frontier models** (glm, kimi, minimax, grok, cursor, gemini, sonnet) get the
  **typical eval only** — no marathons for now.
- **Fable runs** remain scheduled for the **very end of v3**.
- **Objectives — RETIRED for v3 (decided 2026-07-05).** The base AGENT_GUIDE already states
  the four top-level goals in order, so OBJECTIVES.md only added the castle subgate names +
  formal gate labels — redundant, and doubly so once the walkthrough lands. Remove the
  `--objectives` flag, the `OBJECTIVES_BLOCK`, and `templates/OBJECTIVES.md`; fold into the
  walkthrough-provision change. Preserve P2/D2 history (real past sounding run) — retire the
  provision, don't rewrite the record.
- **WALKTHROUGH provision (NEW, decided 2026-07-05)** — add the *most popular* Paper Mario
  (N64) walkthrough to the repo as clean markdown, kept **as close as possible to the
  original source** (realistic to what an agent would grab online — agents have succeeded by
  searching for + reading it). **Default-on = core part of v3** (confirmed): baseline must be
  re-cut as playbook+walkthrough (P1F-002 is playbook-only), and v3 shifts toward measuring
  *execution of a known route* vs *discovery*. Source = **GameFAQs** (top FAQ; plain text →
  minimal markdown drift; provenance header w/ author + URL). OPEN: (a) scope — full-game
  document vs verbatim opening section; (b) objectives-redundancy now RESOLVED (retired).
- **CU — post-base-eval investigation (decided)**: keep tuning CU; run CU cells *after* the v3
  base evals as a standalone help-vs-hurt study. Observation to preserve: CU adds **no
  capability** the toolkit lacks (agents already capture+read screenshots deliberately) — it is
  only an *ambient perception loop*; v2 evidence is that ambient < deliberate, so CU may be a
  distraction rather than an aid. Interesting either way; prove it post-base.
- **Toolkit-composition flags (axis A) — decided 2026-07-05:**
  - `--bare-adapter` → **KEEP** as an optional variant (not default, not first wave). Stretch
    goal: run bare-adapter × CU as a 2×2 (with and without computer use) — isolates
    tool-sophistication from perception.
  - `--scripts-provided` → **RETIRE** (legacy, vestigial, lab-coupled, no demonstrated value).
  - `--with-macros` → **FROZEN** (not retired): keep the machinery, but mark it clearly
    not-for-eval-use (runtime WARNING + comment) and run no scored cells with it. Rationale:
    the macro run is superhuman (~5 min, well past any human) and genuinely interesting, but
    the macro set is immature and it introduces a variable; revisit down the road.

---

## CLEARED-CORE — Fable-era re-review COMPLETE (2026-07-06): IL-1 + IL-2 both cleared
Method: two-round adversarial workflow — 6 lens-reviewers (3 per commit) → dedup → 3-vote
adversarial panel per finding (code-trace / reachability / regression-vs-baseline refuters) →
synthesis → completeness critic → 5-gap closure round. ~40 agents; **zero `defect_in_fix`
votes on any finding at any confidence**. Both verdicts APPROVE_WITH_NOTES; no core change;
v3 bench freeze at `f07f8493` undisturbed.

- **IL-1 · savestate frame-0 NULL deref guard — CLEARED.** `25f39b5b` re-derived from scratch.
  Root cause confirmed: `savestates_get_m64p_size` dry-runs the writer and
  `WRITEDATA(writer, uint32_t, *r4300_pc())` (savestates.c:563) evaluates the deref before the
  writer's NULL-buffer check, so the size path derefs unconditionally; `PC` is BSS-NULL until
  `r4300_init`. Re-review question ("can the guard mask a legitimately-sized frame-0 serialize
  on any core path?") answered **NO**: at frame 0 `initializing==true` on every path, no valid
  size exists to produce (interpreter/cached would crash; NEW_DYNAREC dynarec would emit a
  bogus non-savepoint size), and the core advertises `RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE`
  (libretro.c:1083) — the contract that licenses 0-until-initialized. Safety invariant proven,
  not assumed: `initializing=false` (libretro.c:865) → `main_pre_run()` → `r4300_init` sets PC
  in one uninterrupted game-thread slice with zero `co_switch` (the core contains none), while
  the main thread is parked at libretro.c:2075; mid-session gfx context resets cannot regress
  it (flag set true only in retro_init; `reinit_gfx_plugin` skips its co_switch after the first
  reset). Deployment check strengthens the fix: on x86_64 NEW_DYNAREC is uncompiled
  (Makefile.common:151-158), so `r4300_pc()` returns `&PC->addr` for EVERY cpu_core — default
  `dynamic_recompiler` included — and the frame-0 crash was architecture-universal on the Linux
  eval build, not an interpreter niche. Residual notes (non-blocking): `initializing` re-armed
  only by retro_init, never retro_unload_game (latent unload→reload gap, unreachable via stock
  RetroArch); Run-Ahead could latch the early 0 for a session (config-gated, disabled in this
  deployment; pre-fix that path crashed, so no working feature regressed); fixed-0 is the
  correct sentinel since the real size is runtime-variable (event-queue length).
- **IL-2 · savestate OOB event-queue read clamp — CLEARED.** `f07f8493` re-derived. Strictly
  non-regressive: copies `min(remaining,1024)` ≤ the removed unconditional 1024 on every input.
  Re-review question ("can the clamp/terminator truncate a queue that legitimately fills the
  buffer?") answered **NO — a legitimate queue cannot fill the buffer**: the queue is the final
  serialized section (WRITEARRAY last, savestates.c:570), length `8N+4` with unconditional
  0xFFFFFFFF terminator (interrupt.c:317-327), hard-capped by `POOL_CAPACITY=16`
  (interrupt.c:69) at **132 bytes << 1024**; size/save symmetry (both paths run the same
  writer) plus the sole caller forwarding RetroArch's true length (libretro.c:2160) make
  `remaining == queuelength` exactly for every well-formed state, so the clamp bites only on
  corrupt/foreign buffers — its exact intent. Underflow checked: on a header-valid-but-truncated
  blob `remaining` wraps and the ternary saturates to 1024 — byte-identical to pre-fix on that
  input, never worse. Cross-fix composition checked: IL-1's size-0 cannot reach the load path
  (retro_unserialize's own init guard libretro.c:2157; magic check savestates.c:127 rejects
  empty buffers first). Endianness safe (`to_little_endian_buffer` swaps exactly 1024 bytes;
  0xFFFFFFFF is swap-palindromic). No pj64 loader in this tree. Residual gap promoted to
  **IL-15** below.

## HELD-CORE — open, not yet root-caused
- **IL-3 · load-settle input pathology** (v4 seed) — after `LOAD_STATE` + the 3-frame
  settle, input injected during the settle window can misbehave. Boundary between core
  and adapter; needs a minimal repro before any fix.
- **IL-14 · marathon battle-region replay determinism** (deferred per R4) — the P3 marathon
  replays G1–G3 deterministically but its `G4-bowser-battle` never re-fires despite the
  replay stepping past the original's G4 frame_clock; matches a precedent divergence class
  in the tool docstring. Scope update 2026-07-06: the v3-002 R6 probe replayed **G4
  deterministically** (Δ−28 fc at 21.8k stepped frames), so the divergence is specific to
  marathon scale (P3: 40.7k frames), not to the battle region per se — the 90-min scored
  surface is unaffected even when G4 fires. Investigate WHEN marathons resume
  (post-v3-base); likely RNG/timing in the forced-loss battle. Replay/emulator analysis —
  on hold now.
- **IL-15 · savestate load-path bounds hardening — CLOSED 2026-07-06 (bench window,
  `e64ae95a`).** Original finding (from the IL-1/IL-2 re-review): `savestates_load_m64p`
  had zero bounds checking on all reads preceding the event queue — GETDATA/COPYARRAY
  ignored `size` (incl. the 8 MB RDRAM COPYARRAY, savestates.c:263) and `retro_unserialize`
  forwards `size` unchecked (libretro.c:2160), unlike the save path's `size < required_size`
  guard (libretro.c:2146). Malformed/truncated input only; pre-existing. Fix: minimum-size
  entry gate — refuse any buffer smaller than the fixed prefix plus the 4-byte queue
  terminator, derived from the live writer (`savestates_get_m64p_size()` minus
  `save_eventqueue_infos()`) so it tracks the format; composes with IL-2's queue-tail clamp
  to bound every read. Provenance note: the implementation was found uncommitted in the
  working tree, author unknown; adopted only after a 3-refuter adversarial panel returned
  unanimous "sound" (threshold ≡ fixed_prefix+4 and machine-state-invariant; loader accepts
  only version 0x00010000; RDRAM span constant both sides; no init-order hazard).
  Validation caveat discovered en route: the emu-required gate does NOT compile
  savestates.c (renderer/policy tests only — necessary but not sufficient for core
  changes), so verification was a live core rebuild + on-display smoke on mander
  (frame-0 refusal, save, load through the gate, step) plus the same sequence headless
  on saur.

## HELD-RENDERER — standing lanes (already tracked in REBOOT_PLAN; listed for completeness)
- **IL-4** GlideN64-compat keying conformance, draw-time lane (ADR-0013/0014).
- **IL-5** glide-vs-parallel beat-comparison methodology (standing validation).
- **IL-6** pack-curation triage for content reclassified out of renderer scope (ADR-0015).
- **IL-7** breadth re-checks (SM64/OoT/MK64/MM) after any renderer change.

## ACTIONABLE-EVAL — eval/harness (fixes on the table)
- **IL-8 · pre-tar manifest check** in `run_eval.sh` finalize — enumerate expected
  artifacts (acked savestates, captures, logs) and warn/fail if `workspace.tar.gz` is
  missing any. Would have caught both Linux archive warts while evidence still existed.
  **DONE on branch `eval/archive-integrity-guardrails` (pending merge after fleet).**
  Derives ACKed slots from the tamper-evident `retroarch.log.copy`; WARN-non-fatal +
  `record.json.archive_manifest {expected,missing,extra,verdict}`; also flags
  opposite-polarity foreign writes (unacked slots / pre-run-mtime files) as `extra`.
  Demonstrated: flags P1F-002's absent slot-3 on the real archive; 3 controls pass.
- **IL-9 · no-rsync-into-eval-runs guardrail** — operational rule + optional check to
  prevent foreign writes into a live run dir (root cause of the P1F-001 contamination).
  **DONE on the same branch:** README "Archive integrity" no-rsync rule + the IL-8
  `extra` detection as the cheap enforceable guard.
- **IL-10 · slot-tracker desync on refused frame-0 save — FIXED 2026-07-06 (bench window,
  `6a771a34`).** The tracker now records the slot cursor right after the
  STATE_SLOT_PLUS/MINUS navigation loop — the cursor has moved regardless of whether the
  save succeeds — and the filename check stays as the desync detector. Live-proved on
  mander (frame-0 save refused → tracker=1 → save at frame 1 lands `.state1`, not the
  old-bug `.state2`; load+step clean) and the same save/load sequence proven headless on
  saur. Mechanism analysis from the investigation, kept for the record: the refusal does NOT
  advance the adapter's tracker — it advances RETROARCH's internal counter (the
  STATE_SLOT_PLUS/MINUS keybinds fire before the save ack,
  retroarch_interactive_session.sh:1035-1043) while `session.state-slot` stays stale (:1061 is
  success-only). Compounding: after one refused frame-0 save, EVERY later `save-slot` lands one
  slot high (the save succeeds, the filename check :1057-1059 fails loudly, the tracker never
  heals); recovery = manual SLOT_FILE write (undocumented) or emulator restart — the latter
  intersects the `session_starts>1` invalidation rule, so the worst case is one invalidated
  (re-runnable) run, never silent bad data. NOT integrity-flagging: IL-8 derives acked slots
  exclusively from retroarch.log.copy `Saving state .stateN` lines + on-disk state files
  (run_eval.sh:469-508); the tracker is not an input — no false missing/extra path exists. Loud
  at every failure point (refusal → `SAVE_STATE not acknowledged.`; desync → explicit "slot
  tracking desynced?"; phantom-slot load → error, not wrong state). Reachability: requires a
  save strictly before the first stepped frame; the guide steers away; ZERO post-fix
  occurrences across all archives (one pre-fix occurrence, P1F-001, via TAS-replay-after-restart
  — a still-live pattern with TAS default-on). Disposition: fixed in the 2026-07-06 bench
  window (see header); the scored-run triage greps for `Core does not support save states` /
  `slot tracking desynced` stay in the scoring-protocol checklist as regression tripwires.
  `retroarch_stdin_session.sh` unaffected (no slot tracking).
- **IL-11 · record.json external/CU subgate embed gap — CLOSED 2026-07-06 (pin artifact + one
  misfiled exhibit; no code gap).** `1517afa` covers BOTH modes by construction — spawned and
  external share the single finalize path (run_eval.sh:552-623; the commit message's "no second
  writer"). The three exhibits: cu-001/cu-002/cu-manual-001 ran on a harness DELIBERATELY
  pinned pre-fix (8fb6d58, for comparability with their toolkit comparator — their own
  scoring_caveats say so); cu-manual-002 was misfiled — it actually ran post-fix (94bb796) and
  is the external-mode POSITIVE CONTROL: its `subgates: {}` is genuinely empty (run died in
  kkj_00, no subgate reached), verified byte-equal against score.jsonl. Spawned control:
  baseline-gpt55-v3-002 embeds all 4 gates + 5 subgates. Residual rule for the scoring
  protocol: never pin a scored round's harness below `1517afa` (moot on the frozen bench
  `c238a26`, which is well past it).
- **IL-12 · grokcomposer-003 prefix-0 replay divergence — ROOT-CAUSED + CLOSED 2026-07-06 (era
  artifact; v3 immune; not grok-class).** The IL wording was a mischaracterization: all 1,068
  commands replayed cleanly (`failed_sends=[]`); `matched_prefix=0` is the GATE-vector prefix
  (backfill_replay.py:494) — the replay simply fired no gates. Root cause: the ORIGINAL run
  rode the pre-`4339d77f` dishonest-pause adapter — its retroarch.log.copy shows ~1,400 frames
  of adapter-masked unpaused free-run (frame 138→1596 with no pause-off in the trace; 1,105
  PLAYING acks overall) that carried boot→title-screen on wall-clock; the honest-adapter replay
  stayed paused, so the trace's tightly frame-counted file-select TAS burst fired into the logo
  screen instead — no save file, no G1, nothing downstream. Not archive corruption (intact,
  fully replayable); not a grok/opencode trace bug (siblings -001/-002 verified with
  matched_prefix 3; commands.log is CLI-agnostic). v3 scored runs are immune at the source
  (honest blocking STEP_FRAME + `--start-paused` arm, `4339d77f`/`2f28e96`). Optional
  class-level guard queued for the next driver window: a dry-run scan of retroarch.log.copy for
  PLAYING acks (or frame deltas exceeding cumulative STEP_FRAME between PAUSED acks) — flags
  every pre-fix archive with hidden free-run exposure, statically.
- **IL-16 · run_summary.py SCORER_READS stale — QUEUED bugfix, next patch window
  (non-blocking; found by iter-3 deep triage, verified pre-existing byte-for-byte in locked
  v3-002).** `run_summary.py:23-24` predates the scorer's pos/inputDis polls (`7a32d6c`), so
  the scorer's `0x8010EFF0` (A_PLAYER_POS) and `0x8010EFDC` (A_INPUT_DISABLED after
  MemoryReader `addr&~3` alignment) reads land in the `command_mix` "other addrs" bucket
  (~1054–1091 of v3-003's 1117; identical skew in v3-002), and `0x800740AC` is a dead entry
  (0 hits). Telemetry-only — verified zero consumers in gate scoring, lab_snoop_audit,
  results_table, replay_verify, or launchers; audit verdicts unaffected. Fix in the next
  window (never hot-patch the locked pin): update the address set, regenerate `command_mix`
  for v3-002/003 post-hoc from the preserved commands.log copies (no rerun), and annotate
  `gpt55-marathon-001-triage.md:46` which cited the skewed split. Riders queued with it:
  `LOAD_STATE_SLOT_PAUSED` reply lacks trailing newline + success token (glued log line;
  currently fully compensated by the adapter's log-watch — IL-15 refusals still fail loudly)
  and the shipped `tas/docs/AGENT_GAMEPLAY.md` carries stale metapod/macOS paths (agent not
  misled; uses AGENT_GUIDE's host-correct path).
- **IL-17 · RetroArch load-completion ack (`WAIT_LOAD_STATE`) — LANDED 2026-07-07 (five-host
  rebuild).** `LOAD_STATE_SLOT[_PAUSED]` only queues an async blocking load and replies before
  it is pumped, so its reply was never a completion ack (the load-path gap behind the IL-16
  rider). Added `WAIT_LOAD_STATE`, the load-path mirror of `WAIT_SAVE_STATE`: it drains the load
  task queue via the pre-existing, previously callerless `content_wait_for_load_state_task()`
  then replies `WAIT_LOAD_STATE DONE` (agent-control `9d00508114`; mirrored as
  `tools/retroarch-patches/0010-*`). Both adapters wired (`retroarch_stdin_session.sh`
  `6a02afc8`, `retroarch_interactive_session.sh` ack-map `f456a767`) — **availability only**:
  `cmd_load_slot`'s default flow is unchanged, so there is no old-binary/new-adapter coupling
  break, and the load-slot completion-barrier upgrade is a deliberate follow-up now that all
  five hosts run the new binary. Rebuilt on all five hosts (mander/saur/tortle/metapod/luma →
  `9d00508114`, `WAIT_LOAD_STATE` present in every binary, doctor clean); a live
  save→async-load→`WAIT_LOAD_STATE DONE` round-trip verified on mander.
- **IL-18 · `cut_run.sh` launch-shell PATH gap (luma) — FIXED 2026-07-07.** Post-rebuild
  readiness smokes caught luma's non-interactive ssh launch shell missing `~/.local/bin` (the
  fleet-convention CLI install dir), so both players (`codex`, `claude`) hit `command not found`
  (rc=127) — but only after a session dir was staged, because the cli-identity capture tolerates
  a `--version` miss and proceeds. Luma-only host-config drift (the other four hosts carry
  `~/.local/bin` on their launch PATH; RetroArch/MoltenVK self-test passed on luma, so the
  emulator side was healthy). Fixed in `harness/cut_run.sh` (`a8d8d10`, n64-agent-evals):
  prepend `~/.local/bin` for launch-shell PATH-robustness on any host + a loud hard-fail
  (`exit 3`) when the player CLI is unresolvable, before staging a run dir (no more silent
  rc=127 archives with `session_starts=0`). Luma's re-smoke then went green on both presets.
  Readiness smokes overall: **10/10** (5 hosts × 2 presets `codex-gpt55`/`claude-opus48`), each
  `session_starts=1`, `agent_rc=124` (15-min cap), archive `ok`/0-miss/0-extra, `identity_drift=no`,
  `audit_flags=[]`, scorer ran; headless hosts on `headless_vk`. Readiness only — no lock, next
  wave not launched (full record: n64-agent-evals `docs/SCORING_PROTOCOL.md`, 2026-07-07 entry).

## ACTIONABLE-LAB — lab (fixes on the table)
- **IL-13 · lab durable-state / macro follow-through — RECONCILED 2026-07-06 (paper side
  closed; live steps owner-gated).** The round-4 mint DOES clear the start-state blocker on
  paper: `paper-mario-jr-troopa2-battle-first-command-menu-20260704` is the exact
  battle-context command-ring state the packfix census lacked (battleID 515, Jump-selected
  ring, Mario HP 5/15 FP 1/10 + Bombette; state/capture sha256 re-verified on disk against
  the index). Caveats carried: savefile sha unknown; mint identity is feature-off Linux
  (core `2ee52f3e…`) vs the metapod hi-res identity the trust model requires for replay
  proof; the ring opens on Jump while the route leads with Refresh (ring-nav required). Lab
  MACROS.md updated (lab `5f9daf5`): stale "blocked" note replaced, candidate repointed at
  the mint. Reconciliation corrections: the register's "five candidates" was stale —
  MACROS.md HEAD lists NINE non-battle route-notes candidates, all genuinely unimplemented
  (zero `.ts`); and "what `48` marked complete" was a mischaracterization — eval work-item
  #48 (macro cells) was DROPPED from all rounds by owner ruling 2026-07-04, so the nine are
  parked, not on the eval critical path. Remaining to promote (owner-gated, metapod
  display): author the battle `.ts` + the missing `bomb()`/`refresh()`/ring-nav helpers in
  `src/games/paper-mario/battle.ts`; cross-OS load-validate the state under metapod MVK141
  hi-res; then ≥2 deterministic replays (battle action commands are timing-sensitive).
  New lab runtime bug logged from the sweep tooling: `pm64-hammer-sweep.ts` errors even in
  dry-run (`TAS_ACTION_COMMAND_FIELDS[0].type` unsupported) — fix before sweeps are usable.

---

### Archived resolved (for provenance)
- Analog comma bug — FIXED (guide `79d6acf`, adapter `690f69b1`). Adapter, not core.
- Toolkit double-wait — FIXED (lab `56e55f6`).
- Marathon quit-after-goal — FIXED (`d7e983f`), validated by P3.

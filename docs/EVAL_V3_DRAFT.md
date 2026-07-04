# Eval Round v3 — Draft Definition (task #74)

Date: 2026-07-04. Status: **DRAFT for owner review + gpt-5.5 sounding.**
Standalone document — does not modify [EVAL_PROGRAM.md](EVAL_PROGRAM.md)
(orchestrator-owned tonight); section references (§N) point there. Owner
rulings of 2026-07-04 are treated as fixed constraints throughout: no macro
cells anywhere; codex CU = manual kickoff via Codex.app only; claude CU =
tmux pilot, opus-4.8 only; agy returns conditionally with CU; one host always
reserved for dev; haste becomes a gameplay host after tonight; marathons
(`--marathon`, landed) run anytime without CU; fable-5 absolutely last;
gpt-5.5-xhigh utility play is workflow, not a scored cell.

## 1. Purpose

**What v2 answered** (§14/§15, 36-run ledger):
- **Scaffold axis (A) closed for gpt-5.5**: toolkit / scripts / bare all
  cleared G4 within ~9 min of each other; scaffold doesn't move the needle
  for the top model. Axis A is retired as a deliberate variable.
- **Castle failure taxonomy**: G3 is cheap (22 runs reached it), the castle
  is the discriminator. Named modes: control-layer loss (sonnet-5),
  navigation wall (cursor in kkj_00, 62 min), cleared (opus-4.8, first
  complete S1–S4 sub-gate vector). Scorer v2 sub-gates now give signal where
  wave-1 runs died silently.
- **CU verification matrix closed** (§15): claude CU works on both Macs via
  console-born tmux; codex CLI CU is broken by packaging (CUAService lacks
  the apple-events entitlement — do not chase) and runs via Codex.app manual
  kickoff; agy CU is researched but empirically unproven (AGY notes).

**What v3 must answer:**
1. **Does computer use actually help?** Axis B is the last open deliberate
   axis: gpt-5.5 (manual kickoff), opus-4.8 (tmux), gemini-3.5-flash
   (conditional on the luma smoke). Baselines exist for all three.
2. **How far do the top contenders get without the 90-minute censor?** Every
   clear landed at 70–85 min; the cap censored everything past G4. Marathon
   cells extend the top ~3.
3. **A clean minimax-m3 data point.** The -001 run is booked
   tooling/fixture (forensics #73) until the button-table ruling says
   otherwise; the rerun is blocked on that ruling.
4. **gemini-3.5-flash's real result.** -001 was a 17-minute self-stop,
   -002 is quota-voided; v3 spends its single quota shot on the best
   available surface.

v3 also **freezes conditions before scoring**: the §14 fix list lands (or is
explicitly ruled) before the first scored run, never mid-round.

## 2. Cell grid

Scored cells only. All cells: `axisA=toolkit`, `rules=v2` scorer (7a32d6c),
`guide=` per pre-round gate 4, hi-res ON at 4x, paused-stepped. Labels per
LABEL_CONTRACT.md; instrumentation tier declared per INSTRUMENTATION_TIERS.md.
Host assignments assume the default reservation (metapod stays reserved; see
D4) — the grid shifts one column if the owner moves the reservation to mander.

| Cell | CLI / model / effort | axisB | Mode | Host | Duration | Quota source | Instr |
|---|---|---|---|---|---|---|---|
| V3-CU-1 | codex / gpt-5.5 / xhigh | enabled+recommended | **manual kickoff** via Codex.app (MANUAL_KICKOFF.md), `--agent-external --computer-use` | luma (Mac console; owner present ~15 min) | 90 min cap, ~2.5 h wall | codex sub (biggest window) | none → reconstructed (app export) |
| V3-CU-2 | claude / opus-4.8 | enabled+recommended | **tmux pilot** (`harness/claude_cu_tmux_pilot.sh`; console-born tmux, TCC granted per §15) | luma (or metapod if freed) | 90 min cap, ~2 h wall | claude sub — **shared with orchestration; schedule off-peak** | reconstructed (interactive TUI; no `-p` stream) |
| V3-CU-3 | agy / gemini-3.5-flash / high | **conditional** — enabled+recommended if the luma smoke passes, else unmentioned (fallback, D8) | agy in console tmux (keyring), continuation=mechanical wrapper (validated) | luma | 90 min cap, ~2 h wall | agy sub — **exactly one run this round**; quota resets 2026-07-06 14:23Z | reconstructed (SQLite store) |
| V3-M-1 | codex / gpt-5.5 / xhigh | unmentioned | `--marathon --cap-minutes 360` | haste (post-tonight) or mander | ~6.5 h (overnight) | codex sub | stream |
| V3-M-2 | claude / opus-4.8 | unmentioned | `--marathon --cap-minutes 360` | mander or luma | ~6.5 h (overnight) | claude sub — off-peak, watch limit-pause | stream (`-p stream-json`) |
| V3-M-3 | glm-5.2 (CLI = D5) | unmentioned | `--marathon --cap-minutes 360`, vision=equalizer | mander | ~6.5 h | droid monthly credits **or** opencode weekly post-reset — owner decision D5 | envelope/stream per CLI |
| V3-R-1 | opencode / minimax-m3 | unmentioned | standard 90 min, vision=equalizer | mander (forensics-validated stack) | ~2 h wall | opencode weekly, post-reset (paused 07-02 at 84%/74%) | stream (opencode json) |
| V3-B-1 (optional, D7) | claude / opus-4.8 | unmentioned | standard 90 min under v3 conditions (fresh CU baseline) | luma | ~2 h wall | claude sub | stream |

Notes:
- **Baselines for axis B**: gpt-5.5's v3 baseline is the promoted sounding
  run P1 (free — see §5). opus-4.8's baseline is either wave-2
  opus48-toolkit-002 (with a cross-round condition caveat plus its
  `web-search-game-content` flag) or the fresh V3-B-1 (D7).
  gemini-3.5-flash's baseline is -001 (valid model-behavior point).
- **CU cells drop instrumentation tier** — both codex-manual and claude-tmux
  lose the event stream. Declare `instr=` honestly; web/vision/tool-mix
  fields ledger as unassessed, per the tiers doc.
- V3-CU-1 label carries `caveat=manual-kickoff-desktop-app-context`; the
  harness appends `mode=agent-external`; clock starts at first adapter
  command (owner reading/setup time is free).
- **Not in v3**: macro cells (ruling, dropped everywhere), fable-5 (absolutely
  last), grok/cursor/kimi repeats (no v3 question they answer; grok quota is
  plentiful if an idle-host filler is ever wanted), gpt-5.5 utility play
  (workflow, unscored).
- Serialization: one gameplay session per host (flock). With metapod
  reserved, luma carries the three CU-family cells serially; marathons run
  overnight on haste/mander. Whole round ≈ 4 standard cells (~8 h) + 2–3
  marathons (overnights) + sounding, spread over roughly a week.

### V3-R-1: minimax-m3 rerun preconditions (forensics #73 §Q5, verbatim)

1. **Owner ruling on §14 fix-list item 1 executed first.** Either (a) fix the
   guide table/example (diff below), or (b) rule "trap stays" — in which case
   this run already *is* the minimax-m3 answer for this trap and a rerun of
   the same cell will most likely reproduce it; rerunning as-is only makes
   sense under ruling (b) with that expectation on record. Note (b) is hard to
   defend for the example line: a factually false example command is
   misdocumentation, not a puzzle.
2. **Guide fixes ride along (if ruling (a)):** correct the button table,
   correct or qualify the "game stays PAUSED between your commands" claim
   (§14 item 2), and add the unaligned-byte-read warning to the
   READ_CORE_MEMORY bullet.
3. **Run on the current mander stack** (RetroArch `6ed66434e2` + adapter
   patches 0008/0009, core `2ee52f3e…`) — already validated end-to-end by
   droid-001's G1–G3.
4. **Ledger hygiene before rerun:** annotate the -001 ledger that
   "input-never-registered" / "contBitPattern stayed 0" describe the agent's
   *belief*, not the machine state (suggested annotation below), so the v3
   comparison doesn't inherit a false tooling-failure narrative.
5. Standard current-wave preconditions: scorer v2 (7a32d6c) with polls.jsonl,
   identity set recorded at start, §15 host-reservation policy respected.

(The "diff below" / "annotation below" references live in
`n64-agent-evals/docs/forensics/minimax3-001-dead-input.md`.) Same CLI as
-001 (opencode) for comparability; droid is the fallback if the opencode
weekly budget hasn't recovered, with the cross-CLI caveat the glm-5.2 pair
already established.

### V3-CU-3: agy conditional cell mechanics

Run the AGY_COMPUTER_USE_NOTES smoke checklist (items 1–5) on luma **before**
committing the quota shot: standalone-CLI CU capability, tmux compatibility,
TCC prompts, quota burn of a 2-minute CU task, current model-picker wording.
Prerequisites: quota meters green (reset 2026-07-06 14:23Z — the cell cannot
run before then), workspace kept lean (background indexing draws quota;
consider trimming the TAS engine's node_modules from the agy export or
disabling indexing), owner present for any TCC prompts. If the smoke shows CU
is browser-only or IDE-app-only: fallback per D8 (recommended: reclassify to
`axisB=unmentioned` and keep the clean toolkit run — don't burn the round's
single run debugging an unproven surface).

## 3. Gate / checkpoint design

Ledger evidence (23 gate-scored runs, 5 clears):
- **G1 is a sanity gate** (redteam-random reached it in ~2 min); its
  dispersion (257–2848 s) is contaminated by the button-table trap (sonnet
  ~47 min, opus ~11 min, minimax the full run). After the ruling, v3 pre-G1
  legs are **not comparable** with wave-2 pre-G1 legs — ledger note required.
- **G2 and G3 fire nearly together** (median gap well under 5 min — the
  house→Toad-Town→castle warp chain is fast once the save exists). Keep both:
  semantically distinct, cheap, and they anchor the historical comparisons.
- **The castle leg (G3→G4) is the discriminator**: ~2600–3750 s for the five
  clearers; before sub-gates, 12 of 14 G3-reachers died there with zero
  signal. Sub-gate hits so far: opus48-002 S1–S4, glm52-droid S1–S3,
  grokbuild-002 S1 — everyone else zero, and the cursor run shows why: a
  62-minute wall in kkj_00 *before* S1. The grounds→door leg is the first
  killer; the polls.jsonl position traces (already per-poll) plus the path
  visualizer cover it at class level without a new gate.

**v3 gate vector: unchanged G1–G4 + S1–S4** (scorer v2, 7a32d6c). No new
scored gates until the replay backfill + path visualizer show where v3 runs
actually stall (standing §14 direction: checkpoints from observed stalls, not
story structure). Guards stay: `demoState==0`, `storyByte==-128` for G2/G3/S*,
two-poll stability, arm-after-frame-clock-advance.

**Marathon scoring (post-G4)**: `--marathon` is landed — G4 still ledgers but
does not end the run; time cap only. No run has ever played past G4, so there
is no stall evidence to mint post-G4 gates from. Therefore:
- The **scored artifact is the story_curve** (storyByte is monotonic after
  G1 — §2 tiebreak) plus area/map path traces from polls.jsonl.
- **Reporting milestones, not gates**: derive class boundaries post-hoc from
  the decomp's GB_StoryProgress enum (prologue complete, Chapter 1 begun,
  Chapter 1 complete, …) as views over the curve. They appear in the results
  table as reached/not-reached but carry no in-run semantics and are not
  gates.
- Post-G4 scored gates get minted in v4 from where the v3 marathons actually
  stall — same evidence-first rule as the castle sub-gates.

Standing rules apply everywhere: **no pixel digests, no exact metadata
counts** — class-level semantics only (RAM-decoded state classes, explicit
end reasons, position classes). Battle context reads the (0,-1000,0) void
sentinel; pre-spawn reads are (0,0,0) — the marathon curve consumer must keep
ignoring those, as the scorer already does.

## 4. Pre-round gates (must be resolved before the first scored run)

| # | Item | Type | Status |
|---|---|---|---|
| 1 | **Button-table ruling** (§14 item 1) | **owner decision** | Ruling packet ready (`n64-agent-evals/docs/proposals/button-table-ruling.md`); Option A (fix) and Option B (honest trap) diffs ready to apply verbatim. Blocks V3-R-1 (its precondition 1). Recommendation: **Option A** — three victims, and a false example command is misdocumentation, not a puzzle. |
| 2 | **Pause-semantics adapter fix** (§14 item 2) | owner sign-off; **ready to apply** | Three hunks drafted (`docs/proposals/pause-semantics-fix.md`) against `tools/adapters/retroarch_interactive_session.sh` (this repo): blocking STEP_FRAME, frames-aware timeout, SET_PAUSE lie detector. Guide-side wording already applied. Rollout note: survey lab tooling for deliberate fire-and-forget STEP_FRAME first; between rounds = now. |
| 3 | **`--start-paused` export change** (pause proposal companion) | ready to apply, **gated on #5** | One-line change in `n64-agent-evals/harness/export_workspace.sh`: `start-game.sh` gains `--start-paused` (patch 0008) so `frame=` counts core frames from power-on and PAUSED becomes trustworthy. Held back only because it changes run conditions and requires every eval host on a patch-0008 build. |
| 4 | **Walkthrough provision decision** (§14 web ruling) | **owner decision**, sounding-informed | v3 provides a walkthrough as eval material so guidance is equalized by provision, not blocked. Open: form (full route vs objectives-only) and its effect on the discovery-vs-execution signal — explicitly deferred to gpt-5.5 sounding (P2 below). New `guide=` label value when it lands (e.g. `playbook+walkthrough`). |
| 5 | **Per-host stack promotion to 6ed66434 staging bundles** | **ready to apply** | mander is already on `6ed66434e2` (+patches 0008/0009, validated by glm52-droid-001). Promote metapod/luma/haste via `tools/adapters/build_retroarch_agent_control_macos.sh`; reverify each host with the null + codex-smoke pair before scoring. |
| 6 | Packaging-order fix (§14 item 3) | ready to apply | Write record.json last (or recompute post-tar) so `export_sha256` stops going stale (`export-hash-stale` flag on the cursor ledger). Mechanical. |
| 7 | Label + instrumentation normalization (§14 items 4–5) | landed as contracts | LABEL_CONTRACT.md + INSTRUMENTATION_TIERS.md govern from v3; drivers must emit conformant labels (`model=opus-4.8` not `claude-opus-4-8`, no `permission-mode=` leakage, `instr=` declared). Mechanical driver pass. |

Order of operations: rule #1 → apply #1/#2/#6/#7 → promote #5 → apply #3 →
sound (P1) → decide #4 during P2 → freeze the guide → score.

## 5. Sounding plan (gpt-5.5 until sound — standing rule)

All soundings: codex / gpt-5.5 / xhigh, toolkit, on the fully-gated v3 stack.

| Pilot | What it validates | Counts as |
|---|---|---|
| **P1** — standard 90-min run under v3 conditions (gates 1/2/3/5/6/7 applied) | The condition changes end-to-end: fixed button table (expect a fast, uncontaminated pre-G1), `--start-paused` frame regime, pause hunks, packaging fix. Gate vector should sit in family with the three prior gpt-5.5 clears. | **The v3 gpt-5.5 baseline** if zero harness defects (promotion precedent: pilot-gpt55-toolkit-002). |
| **P2** — P1 + provided walkthrough (objectives-only form first) | What provision does to the discovery-vs-execution signal; feeds the D2/gate-4 form decision. Compare gate vector + web-tag behavior vs P1. | Decision input; scored only if the owner adopts its guide variant for the round. |
| **P3** — `--marathon --cap-minutes 360` | TTL scaling at 6 h, usage-limit pause handling on a long run (the standing manual caveat), post-G4 story-curve capture, overnight unattended operation. | **V3-M-1** if clean. |
| **P4** — first manual-kickoff run (`pilot-gpt55-cu-manual-001`, per MANUAL_KICKOFF.md) | External-agent mode live: clock-start on first adapter command, wind-down, Codex.app conversation export, owner procedure. | **V3-CU-1** if clean. |

**"Sound" means, operationally:** (a) P1 plus at least one other pilot
complete with zero §8-validity invalidations and zero new harness defects;
(b) input-trace replay verification green on every pilot; (c) records
conform to the label contract and their declared `instr=` tier with no
reconstruction surprises; (d) all seven pre-round gates closed (applied or
explicitly ruled). Only then do non-gpt-5.5 scored cells start. The standing
revert trigger stays armed: any scored run that surfaces a harness defect
pauses the round and falls back to gpt-5.5-only until fixed.

## 6. Open owner decisions

| # | Decision | Recommendation | Default if no answer |
|---|---|---|---|
| D1 | **Button table: fix (A) or honest trap (B)?** | **Option A.** Three victims, one total loss; the false example line is indefensible; V3-R-1 is blocked on this. | Apply Option A. |
| D2 | **Walkthrough form: full route vs objectives-only?** | **Objectives-only** — equalizes goal knowledge while preserving the discovery signal (route-finding is what the castle discriminates). | Run P2 objectives-only; if still unruled after P2, ship objectives-only for v3. |
| D3 | **Vision equalizer hosting** (haste becomes a gameplay host): (a) time-multiplex haste — equalizer-dependent cells never overlap haste gameplay; (b) re-host the describer elsewhere — no other GPU fits the 35B (mander's GPU carries the gameplay Vulkan stack); (c) drop the equalizer for v3 text-only cells with a `vision=none` caveat — confounds V3-R-1, whose -001 comparator used the equalizer. | **(a) time-multiplex.** Zero new deployment; the schedule already serializes per host, this just adds one cross-host constraint. | (a). |
| D4 | **Which host stays reserved after tonight?** Keep metapod reserved (CU cells serialize on luma, which is intermittently online — keep docked) vs move the reservation to mander (the dev primary anyway), freeing metapod as the second CU-capable Mac. | **Move to mander.** Dev work lives there by ruling; gameplay on mander was only ever opportunistic; two CU-capable Macs de-risks luma's availability. | Keep metapod reserved; all CU cells on luma. |
| D5 | **V3-M-3 (glm-5.2 marathon) quota source**: droid (fastest known glm harness, but ~6 h of scarce monthly credits) vs opencode post-reset (preserves credits; slower gate times; weekly budget) vs defer to v4. | **opencode post-reset**, with the established droid-vs-opencode comparability caveat. Marathon answers "how far", not "which harness". | Defer V3-M-3 to v4 if the opencode weekly hasn't comfortably recovered. |
| D6 | **Marathon cap value.** | **360 min** (4x standard; fits an overnight; TTL already scales with `--cap-minutes`). | 360 min. |
| D7 | **Fresh opus-4.8 v3 baseline (V3-B-1)?** The wave-2 clear carries the web-guidance flag and pre-dates the condition changes; a clean baseline sharpens the CU comparison but costs a shared-quota run. | **Yes, if claude quota allows** after V3-CU-2 and V3-M-2. | Skip; compare CU against wave-2 with an explicit cross-round caveat. |
| D8 | **agy fallback if the luma smoke shows no usable CU**: (a) IDE-desktop-app manual kickoff (codex-analog), or (b) reclassify to `axisB=unmentioned` and run the clean toolkit cell. | **(b).** The IDE surface is unproven and the round has exactly one quota shot; (a) risks spending it on surface debugging. | (b). |

---
*Cross-references: EVAL_PROGRAM.md §5 (axes), §6 (roster/quotas), §8
(anti-cheat/web), §9 (hosts), §14 (fix list, v3 foundation), §15 (CU matrix,
2026-07-04 rulings); AGY_COMPUTER_USE_NOTES.md; n64-agent-evals:
docs/forensics/minimax3-001-dead-input.md, docs/proposals/*,
docs/LABEL_CONTRACT.md, docs/INSTRUMENTATION_TIERS.md, docs/MANUAL_KICKOFF.md,
ledger/ (36 runs).*

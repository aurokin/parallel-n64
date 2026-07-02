# N64 Agent-Play Eval Program (v1, 2026-07-02)

Design for reusable, rerunnable evaluations of coding-agent models playing N64 games
through our RetroArch agent-control stack, with ParaLLEl hi-res texture packs ON.
Purpose: (1) choose which models we let drive and playtest, per gameplay-mechanics
class; (2) produce publishable results; (3) exercise the ParaLLEl hi-res path on real
gameplay across more titles. Owner rulings baked in: **no dollar cap — time is the
cap (90 min/run)**; few deliberate variation axes; dead-simple unambiguous agent
startup; Fable 5 tested absolutely last.

Everything here that touches gameplay tooling builds on the proven adapter stack
(ADR-0009; `tools/adapters/retroarch_interactive_session.sh`). Nothing in this
program may become a correctness authority for the renderer (ADR-0017 discipline:
the lab and eval repos stay out of parallel-n64's gates).

## 1. Where evals live

**Recommendation: a new repo (working name `n64-agent-evals`)**, not the lab.
Rationale (this was the open "own repo?" question):
- The agent workspace must be a **generated export, not a checkout**: no git
  history, no macro/TAS scripts, no route notes, no durable-state indexes, no prior
  evidence bundles. Cursor's June 2026 audit found 63% of one model's SWE-bench Pro
  "wins" were retrieval (upstream lookups + git-history mining); our lab repo is
  exactly the "gold answers in the workspace" shape. A separate repo makes "ship an
  export" the default instead of a discipline.
- The lab keeps owning durable states, macros, route notes (its job). The evals repo
  owns: eval definitions, workspace-export generator, runner, scorer, results ledger.
- parallel-n64 stays the planning source of truth; this doc is the plan of record
  until the evals repo exists, then moves there.

## 2. Eval definition (the reusable unit)

One eval = one YAML file, e.g. `evals/pm64-intro-bowser.yaml`:

```yaml
id: pm64-intro-bowser
game: paper-mario-usa
rom_sha256: <pinned>
start: clean-boot            # no savestates, no save files, no SRM — verified at start
time_cap_minutes: 90
mode: paused-stepped         # model thinking time does not leak into game state
hires: on                    # pack sha pinned below
pack_sha256: 388bce41…
gates:                       # ordered; scored as (gate, elapsed_game_time, elapsed_wall)
  - id: G1-save-created      # created a save file and entered gameplay
    within_minutes: 20       # "sensible gate": if not by 20min, run continues but gate is missed
  - id: G2-toad-town         # mac_00 reached
  - id: G3-castle            # kkj area reached
  - id: G4-bowser-battle     # terminal: prologue Bowser encounter entered
score: gate-times            # vector of reached gates + times; no pass/fail scalar
```

Runs are scored by **which gates were reached and when** — never by agent
self-report, never by pixels. Re-running the same YAML on the same pinned
identities is the reproducibility contract.

### Eval #1 concrete: Paper Mario clean-boot → prologue Bowser

Telemetry (RAM polls by the HARNESS through the adapter's `READ_CORE_MEMORY`; the
existing probe stack already handles the N64 word-swap — reuse
`field_probe.py` / `pm64-fields.ts` decode, NOT the raw `telemetry.ts` MemoryField
reader, which does not word-swap):

| Gate | Cue (all guarded by `demoState==0` and stable across 2 polls) |
|---|---|
| G0 liveness | `READ_CORE_MEMORY` answers; frameCounter (gGameStatus+0x134) advances |
| G1 save created + in gameplay | storyByte (s8 @ 0x800DBD70, GB_StoryProgress) == −128 AND any gSaveSlotHasData (0x80077A24) flag set AND area/map == 0/11 (kmr_20, Mario's house) AND context==0 |
| G2 Toad Town | area/map == 1/1 (mac_00 entry 5 on the intro route) |
| G3 castle | areaID == 4 (kkj) |
| G4 Bowser battle | context==1 (battle) while area/map == 4/7 (kkj_13), confirm gCurrentBattleID (0x800DC4E8) == 0x2301 |
| tiebreak/progress | storyByte is monotonic — use as continuous progress score after G1 |

Guard rationale: the attract demo loads real maps and writes area/map — every gate
must require `demoState==0`. "No saves at start": Paper Mario saves to FlashRAM
(blank = 0xFF-filled, offset 0x28800 in the .srm layout); each adapter bundle gets a
private savefile dir, so clean-boot is the default — the harness additionally
verifies the flash region is blank at start.

**Attested 2026-07-02** (mander, HEAD core, mode off, word-swap-corrected reads
over `READ_CORE_MEMORY` against four lab durable states — predictions written
down before reading):

| Durable state | storyByte | battleID | demoState | area/map |
|---|---|---|---|---|
| bowser-prologue-battle-start | **−128** (=STORY_INTRO, predicted) | **0x2301** (predicted) | 0 | 4/7 = kkj_13 (predicted) |
| kmr03-post-fall-first-control | **−122** (=STORY_CH0_FELL_OFF_CLIFF, predicted) | 0x0 | 0 | 0/2 |
| kmr02-goombario-joined | **−115** (=STORY_CH0_GOOMBARIO_JOINED_PARTY, predicted) | 0x0 | 0 | 0/1 |
| toad-town-plaza-saveblock-saved | **−107** (=STORY_CH0_ARRIVED_AT_TOAD_TOWN, predicted) | 0x0 | 0 | 1/2 |

4/4 exact. New facts for the scorer: gCurrentBattleID's no-battle sentinel is
**0x0** (not −1); areaID/mapID live at gGameStatus+0x86/+0x8C (s16). One caveat
found in the decomp: `gSaveSlotHasData` initializes to all-true at title-screen
data init and only becomes meaningful after the fio flash scan validates
checksums — so G1's primary "save created" signal should be the harness's
offline check of the bundle .srm flash region (blank = 0xFF-filled), with
gSaveSlotHasData as the in-RAM cross-check. Still to observe live (during the
pilot, not blocking): gSaveSlotHasData 0→1 across an in-eval file creation.
Attestation driver: scratchpad attest_gates.py/attest-run.sh pattern — bundle
per state, `--state-source <lab>/durable-states/<id>/state`, load slot 0
paused, STEP_FRAME 3, read, stop (drop `--savefile-source` for pre-0617 mints
that lack `savefiles/`).

## 3. Harness architecture

- **Runner**: one per host, one gameplay session at a time (the adapter already
  enforces this with a runtime flock and refuses to start alongside another
  RetroArch). `--ttl-seconds 6300+` so the TTL grace watchdog (QUIT at ttl−30s)
  never fires inside the 90-minute cap.
- **Paused-stepped play is the default mode**: the agent acts via the adapter's
  `input --mask HEX --frames N` (set input → STEP_FRAME N → poll PAUSED → clear).
  Model latency is thus fairness-neutral; "elapsed game time" = stepped frames.
  Real-time play is a possible future variant, not v1.
- **Scorer is harness-owned and outside the agent boundary** (anti-cheat P0): a
  separate process (different user or at minimum a workspace the agent can't write)
  polls the gates over the same adapter channel and writes the score ledger.
  The agent may read its own telemetry; it cannot write the scorer's.
- **Input-trace replay as the integrity anchor**: record every input command + the
  start identity (ROM/core/config/pack hashes); a scored run must replay
  deterministically on a clean harness to the same gate vector. Divergent or
  unreplayable ⇒ fail-with-audit. This single mechanism defeats RAM pokes,
  savestate skips, cheat files, and telemetry tampering — and our TAS macro work
  already proved the replay determinism this relies on.
- **Fresh workspace per attempt** (not per suite): prevents persistent-implant and
  cross-run leakage patterns.
- **Session-limit handling**: each CLI has a detectable usage-limit message and a
  non-interactive resume (`codex exec resume <id>`, `cursor-agent --resume`,
  `grok -r <sessionId>`, `droid exec` (monthly credits — pausing 5h does NOT help),
  `opencode run -s <id>`, `claude -p --resume <id>`). The runner pattern-matches
  the limit strings, pauses the run clock, and resumes in the next window.
  Time-cap accounting counts only active-agent wall time.

## 4. The agent workspace (zero ambiguity; REVISED 2026-07-02 per owner rulings)

The eval measures **tool-assisted play — how far agents get WITH the toolset —
not blind play**. Generated export containing:
- `AGENT_GUIDE.md` — the one entry point: the game, the goals, **one command**
  that starts the emulator correctly (`./start-game.sh`, host-correct absolute
  paths written by the generator), the adapter command reference (input,
  frame-stepping, screenshots, memory reads incl. the word-swap note,
  save-slot/load-slot), and an allowed/not-allowed block (below).
- The adapter scripts.
- **The lab's Dynamic TAS workflow engine** (`tas/`: the TypeScript runtime,
  semantic PM64 inputs, evidence traces, compiled dist + node_modules,
  examples, and the TAS_SCRIPT_MODEL/AGENT_GAMEPLAY method docs) — the guide
  frames TAS scripting as the intended way to play. The lab's route/battle
  macro LIBRARY is deliberately not shipped: the engine is the tool, the
  macros are our solutions (owner can override).
- Fresh single-commit git history (codex trust check; agent checkpointing);
  no lab/project history, no prior captures.

**Allowed** (and advertised in the guide): emulator savestates (checkpoint/
retry/branch), walkthroughs and any external reference incl. web, reading RAM
and inspecting the ROM/binary, building own scripts and macros in-workspace.
**Not allowed**: writing game memory (WRITE_CORE_MEMORY), cheat files,
tampering with emulator/config/logs/scoring, and **using other agents' runs or
artifacts** (the one hard anti-cheat rule). The contamination scan now targets
concrete solution artifacts (scored gate internals, the lab macro library),
not vocabulary.

## 5. Variation axes (deliberately few — run only to prove a point)

| Axis | Values | When to run |
|---|---|---|
| A. Tooling provided | `toolkit` (default: TAS engine + adapter, NO macro library — agents designing their own macro libraries is a large part of what the eval measures, owner ruling) vs `toolkit+macros` (`--with-macros`: adds the lab route/battle macro library, for the with-vs-without comparison) vs `bare-adapter` (adapter only; quantifies the toolkit's value) | Pilot A/B on 1–2 models; then default toolkit |
| B. Computer use | `unmentioned` (default) vs `enabled+recommended` | codex + claude only (only CLIs with computer use today), macOS host only. CUA drivers for other models: out of scope v1 |

Fixed (not axes): hi-res ON at 4x (feature-off runs only as explicit baseline
comparisons); paused-stepped mode; image tools for text-only models (an equalizer,
not a variable — see §7).
Every run record states its axis values; results never mix axes silently.

## 6. Roster and run order

| Order | CLI | Models | Effort | Notes |
|---|---|---|---|---|
| 1 | codex | gpt-5.4, gpt-5.5 | `model_reasoning_effort=xhigh` | priority; biggest window |
| 2 | cursor-agent | composer-2.5 | — | cursor-vs-grok comparison |
| 3 | grok | grok-build | — | |
| 4 | droid | glm-5.2 → kimi-k2.7-code → minimax-m3 | `-r high`+ | **monthly org credits** — schedule sparingly |
| 5 | opencode | opencode-go/{glm-5.2, kimi-k2.7-code, minimax-m3} | `--variant` | Go providers ONLY (never the ambient Vercel/Fireworks/Cloudflare env keys) |
| 6 | claude | sonnet-5 → opus-4.8 → **fable-5 last** | ultracode keyword / `--effort` | usage-sensitive; test at the end |

Canonical non-interactive recipes (verified against installed CLIs; always pass
flags explicitly — several hosts have aggressive defaults in user config, so use
`codex --ignore-user-config` / `claude --bare`-style isolation where supported):

```
codex  exec -C <ws> -m gpt-5.5 -c model_reasoning_effort=xhigh -s workspace-write \
       -c approval_policy=never --json -o last.txt "<prompt>"        # wrap in GNU timeout
cursor-agent -p --output-format json --force --workspace <ws> --model composer-2.5 "<prompt>"
grok   -p "<prompt>" --cwd <ws> -m grok-build --always-approve --output-format json
droid  exec --cwd <ws> -m glm-5.2 -r high --auto medium -o json "<prompt>"
opencode run --dir <ws> -m opencode-go/glm-5.2 --format json "<prompt>"
claude -p --output-format json --model claude-sonnet-5 --permission-mode bypassPermissions "<prompt>"
```

droid note: `--auto medium` refuses MCP tool calls ("insufficient permission to
proceed... Re-run with --auto high"), so droid runs that use the image tools (§7)
must pass `--auto high`.

Usage-limit patterns (runner greps): codex `/hit your usage limit/i` (+`resets_at`
in the turn.failed event); cursor server-provided messages; grok
`usage limit reached|out of credits`; droid `monthly compute usage limit` (do not
retry in 5h); opencode `usage limit reached. It will reset in`; claude
`five_hour`/`seven_day` rateLimitType + `usage limit reached`.

## 7. Image tools for text-only models (droid + opencode only)

Some Go-tier models lack image input. Equalizer service on **haste** (RTX 5090)
— DEPLOYED and validated end-to-end 2026-07-02 (mander → haste on a live f10440
capture):
- **Locator**: NVIDIA LocateAnything-3B — open-vocab boxes from text ("the START
  button", "the green pipe"). Running via LocalAI's MIT ggml port at
  `haste:~/code/locate-anything.cpp` (q8_0 GGUF, CPU, `--mode hybrid`). License
  note: official weights are NVIDIA non-commercial (research use) — fine for
  internal tooling; Qwen3-VL is the Apache-2.0 fallback if that ever matters.
- **Describer**: the already-serving qwen3.6-35b-a3b-multi vLLM profile (:8021),
  called with a grounded system prompt (describe pixels only, never guess the
  title) and `enable_thinking: false`.
- **Service**: `self-host-llm` repo on haste, `scripts/vision-tools start` →
  FastAPI on :8022 (`/describe`, `/locate`, `/health`). It resizes inputs
  (locator ≤1440px longest side — full-res 2880×2160 OOMs next to the resident
  vLLM), serializes locates (RAM guard), and rescales boxes back to
  original-image pixel coords. Co-resident profile: qwen on GPU + locator on CPU
  simultaneously, so haste serves both tools without touching the gameplay hosts.
- **Wiring**: a local **stdio MCP shim** on each gameplay host —
  `tools/adapters/vision_tools_mcp.py` (`uv run`, PEP 723) exposing
  `describe_image(path, question?)` + `locate_on_image(path, query)`. The shim
  reads the LOCAL screenshot and uploads it to :8022, so agents pass small local
  paths and never handle image payloads (this beat the researched
  remote-streamable-HTTP option, where agents would have to move image bytes
  themselves). Project-level config only, scoped to the eval workspace: droid
  `.factory/mcp.json` (stdio), opencode `opencode.json` `mcp` (local) — snippets
  in the shim docstring. Both snippets live-verified 2026-07-02 with one-tool-call
  glm-5.2 tests from mander; droid additionally requires `--auto high` (see §6).
  VLM-native models do NOT get these tools (they have eyes already); this is an
  equalizer, and run records note which vision path a model used.
- **Measured latency** (co-resident, cross-host): describe ~0.5–2 s;
  locate ~75–85 s per query in hybrid mode (target count barely matters; fast
  mode is quicker but over-matches). Budget locates accordingly in eval time
  limits; a GPU locator build is the later optimization.

## 8. Anti-cheat (REVISED 2026-07-02 per owner rulings)

Owner rulings reframed the threat model. **Allowed and in-bounds**: savestate
save/load, walkthroughs and web reference, our tools, inspecting the binary/RAM
however they like — the eval tests tool-assisted play, and allowed affordances
must be readily available in the environment (§4). **The violations**:
- **Copying other models' work**: agents must not read other runs' workspaces
  or artifacts. Enforced structurally — fresh workspace per attempt, and each
  run's workspace is archived (`workspace.tar.gz`) and DELETED at run end, so
  later runs find nothing to mine; transcript audit catches snooping attempts.
- **Faking the scored state**: WRITE_CORE_MEMORY (exists in our patched
  frontend!) is flagged by the post-run audit from tamper-evident log copies
  (streamed to a randomized 0700 dir the agent can't find — the tamper
  red-team's ledger-forgery hunt came up empty). Log scrubbing is detected by
  size comparison against the harness copies.
- **Import of foreign savestates**: not directly observable, but the replay
  anchor catches it — replayed traces re-create every self-minted state, so a
  load of a state the replay never created diverges. Savestate ops replay as
  part of the trace (replay_verify.py); only WRITE_CORE_MEMORY refuses replay.
Score integrity mechanics unchanged: harness-owned scoring channel, identity
hashes at start/end, input-trace replay verification (§3). No egress allowlist
(walkthroughs are allowed); the transcript audit still classifies what was
retrieved vs derived, for analysis rather than enforcement. Red-team criteria:
null scores 0; random scores nothing beyond G1; tamper's RAM poke must flag
and its ledger forgery must fail (all verified 2026-07-02).

**WATCH ITEM (owner, 2026-07-02) — host exploration reaching the lab repo.**
The contamination that actually matters is an agent wandering the host into
`parallel-n64-lab` (macro library, route notes, durable-state indexes — the
withheld solutions). Full per-run sandbox hosts with only curated sources
mounted are not feasible with the current fleet, and agents legitimately may
analyze their own workspace tooling source and, if they choose, reference
sources (papermario decomp, parallel-n64, RetroArch). Not enforced today;
mitigation ladder when we get to it:
1. Post-run transcript grep (already possible — the CLIs log their own
   commands): flag any touch of `parallel-n64-lab`, `macros/`,
   `durable-states`, route notes → audit verdict on the run.
2. Cheap prevention: chmod 700 / rename the lab dir on gameplay hosts for the
   run's duration (same-user, so not airtight — but casual exploration stops,
   and un-hiding it would itself show in the transcript).
3. Long-term: dedicated runner accounts or VMs with read-only curated mounts.

## 9. Hosts

| Host | OS | Role |
|---|---|---|
| haste | CachyOS, RTX 5090 | **image-tool service host** (qwen3.6-35B + LocateAnything sidecar via self-host-llm, co-resident profile); gameplay-capable (validated 2026-07-02) but NOT used as a gameplay runner while the vision models are serving |
| mander | Linux | gameplay runner / orchestration (this host) |
| metapod | macOS | gameplay runner; the computer-use axis (codex/claude); MVK141 app path |
| koopa | macOS | **OFF-LIMITS** (primary work machine) |

(A separate "linux" host was listed in earlier drafts — a network sweep
2026-07-02 found no such host (DNS, tailnet, Proxmox); the phrase "haste
linux" in the fleet notes meant haste itself. Nearest candidate is the
dual-boot desktop chassis, offline in Windows. If it comes online as a Linux
runner, re-add it here.)

Vision serving builds on haste's existing `self-host-llm` stack (docker-compose
vLLM profiles; validated OpenAI-compatible endpoints :8020/:8021, vision enabled):
the describer is the already-deployed qwen-multi profile — no new deployment — and
LocateAnything runs as the CPU-ggml sidecar, co-resident with the GPU-resident
Qwen (a GPU locator build is a later optimization). The :8022 vision-tools
service fronts both; gameplay hosts reach it through the local stdio shim (§7).

One gameplay session per host, ever (flock-enforced). A full 11-model sweep of one
eval ≈ 11 × ≤90 min across 3–4 hosts ≈ one long afternoon, before retries.

## 10. Targets roadmap (mechanics × packs)

Ground truth from the pack-catalog research (two deep-research passes + hub
verification): outside Paper Mario, the "turn-based with interactive components"
niche has essentially **zero** community hi-res pack coverage — mechanics-first and
pack-first pull apart. So two tiers:

**Tier A — mechanics-first** (the model-capability question):
1. Paper Mario (anchor; pack on disk, curated r2). Eval #1 above; later evals can
   reuse lab durable states as starts (chapter starts, battle starts) once minted.
2. Pokémon Stadium 2 — the only substantive pack on a turn-based title
   (HiperCambio Rice-format, 2300+ textures — also a legacy-format ingestion test).
   Caveat honestly: it IS Pokémon battles, but the rental/cup wrapper adds the
   interactive structure. First goal: win the first Poké Cup rental battle set.
3. Mario Party 2 — best mechanical fit, no established pack (run feature-off or
   verify Gaming Revived coverage). First goal: complete a 20-turn board vs CPUs.

**Tier B — pack-first breadth** (the renderer-validation question; goals are the
"sensible first goals"): MK64 (pack on disk): **3rd or better, Luigi Raceway 50cc
Mushroom Cup**. SM64 (on disk): first Star. OoT (on disk): Kokiri Sword + Deku
Shield + enter Deku Tree. SSB (Reloaded .hts, best new acquisition): clear 3 stages
of 1P on Very Easy. Pokémon Snap (ReVived .htc): unlock the Tunnel course. MM (on
disk): Moon's Tear + Clock Tower rooftop by night 3.

**Pack-format flexibility corpus** (separate lane, feeds hts2phrb breadth — the
"old formats" ask): legacy Rice PNG folder packs (Djipi OoT 2011, Mollymutt SM64/PM,
Kerber2k MK64, HiperCambio Stadium 2), Gaming Revived bulk .htc (~100–150 titles,
AI-upscale tier — ingestion smoke corpus, not art-quality reference), Glide64 .DAT
and Jabo 1.7 packs as conversion-only tests. All mutually incompatible formats —
preserve the distinction in TEXTURE_PACKS.md rows as they're acquired.

## 11. Metrics and the run ledger

One JSON record per run: eval id + axis values; identities (ROM/core/pack/RetroArch/
workspace-export shas, CLI + model + effort + CLI version); gate vector
(gate → stepped-frame count + wall time); story-byte curve; token/turn counts as
reported by the CLI's JSON output; end reason (cap / terminal gate / limit-pause
count); replay-verification status; audit verdict. The ledger is append-only in the
evals repo; a results table is generated, never hand-edited.

## 12. Rollout

1. ~~Attest the three new telemetry fields against lab durable states (§2).~~
   DONE 2026-07-02 — 4/4 exact (§2 table).
2. ~~Build the workspace-export generator + AGENT_GUIDE.md + start wrapper;
   red-team with null/random agents.~~ DONE 2026-07-02 — the repo exists at
   `~/code/n64-agent-evals` (local only, no remote yet). Null agent scored 0
   with a clean end record. The random agent scored 0 on G2+ but **genuinely
   reached G1 in ~2 min** (PM64 file-select survives button-mashing; verified
   via the .srm flash magic, and it exercised the gSaveSlotHasData 0→1 live
   transition §2 wanted observed). Criterion refined: null scores 0, random
   scores nothing beyond G1 — G1 is a sanity gate, discrimination starts at
   G2. Scorer hardening from the runs: gates arm only after the frame clock
   advances (pre-init RAM pattern-matches gates at frame 0), G1 dropped the
   missable kmr_20 dwell, frame clock is the u16 at gGameStatus+0x134
   (1 tick per 2 stepped VI frames, measured), SIGTERM flushes the vector.
   A deliberate-tamper red-team agent remains todo alongside the pilot.
3. Pilot: codex gpt-5.4 + gpt-5.5 on eval #1, axis A both values (self-built +
   scripts-provided), linux/haste. This calibrates the gates and the audit.
4. Sweep order per §6; metapod runs the computer-use axis with codex/claude.
5. Fable-5 last, after every other model's results are in.

# Open Technical Issues

This register holds technical details owned by `parallel-n64`.
Scheduling, priority, fleet rollout, eval operation, and gameplay research are
tracked by their owning systems.

## Savestate load does not reproduce the saving run

Status: resolved, October 19, 2026, for states saved by cores with both fixes
below; states saved earlier keep the gap.

Two causes, found with a probe that saves mid-run, continues, and compares the
continuation with a fresh-process load of that save (neutral input, hi-res off):

- **RDP noise seed.** ParaLLEl-RDP seeds each primitive's noise from a running
  primitive count that starts at zero in every process and was not saved. A
  loaded state drew different dither and combiner noise, so only framebuffer
  pixels diverged (Ocarina of Time, Majora's Mask). States now end with a
  12-byte `PRDP` trailer after the event queue carrying the count; states without
  it load as before.
- **Dynamic recompiler entry points.** A load jumps to the saved PC and drops
  recompiled code, but the saving run kept executing blocks compiled from
  earlier entry points, so cycle-count updates, and with them interrupt timing,
  fell elsewhere (CP0 Count, `next_interrupt`, the event queue, then game
  memory; GoldenEye, Paper Mario). The cached and pure interpreters reproduced.
  Saving now applies the same jump and invalidation, so a run that saves
  differs in timing from the same run without the save.

With both, nine probe states across Ocarina of Time, Majora's Mask, GoldenEye and
Paper Mario reproduce, and a seven-chunk Ocarina of Time recording with inputs
replays chunk for chunk from fresh processes (six chunks start from saves).

Original report (October 16):

Replaying 90 recorded Paper Mario segments with the recording core and
hi-res disabled reproduced all 9 power-on segments and 69 of 81 segments that
start from a mid-run savestate byte for byte. The other 12 end in a different
state. A second replay of those 12 matches the first exactly, so each fresh
load is deterministic but differs from the run that wrote the state. The
differences are the event queue, `next_interrupt` and CP0 Count, plus a few
RDRAM pages; one 808-frame segment diverges widely.

Three of the 12 were replayed again as a single continuous program from power-on
through the end of the failing segment, with no load. All three reproduce the
recorded end state. The savestate bytes are therefore correct, and an immediate
re-save after a load reproduces the file. Emulator state that is not serialized
still changes what happens after the load.

The start states of failing and passing segments do not differ in SP_STATUS
(the RSP is halted in every start state), in any device register word before
RDRAM, or in their pending event types. Code-reading candidates for the
unsaved state include:

- cxd4 RSP yield and timeout state: `rsp_task_locked`,
  `MF_SP_STATUS_TIMEOUT`, and the RSP scalar and vector registers;
- the interrupted call stack that resumes after `retro_unserialize`, because
  the load lands while the emulation coroutine is suspended inside an event
  handler;
- dynarec globals such as `dyna_interp`, `delay_slot` and `skip_jump`;
- the ParaLLEl-RDP partial command buffer.

None is confirmed. Instrumenting save and load points, and an A/B run with the
cached and pure interpreters, are the next steps.

Until this is fixed, regression scenarios that start from a savestate are
qualified individually: a scenario that does not reproduce its recording on the
recording core is excluded from gating
([ADR-0019](adr/0019-deterministic-emulation-state-gates.md)).

## Replacement changes the native RDRAM render targets

Status: resolved, October 18, 2026, by
[ADR-0020](adr/0020-replacement-stays-out-of-rdram.md): `native` is the default;
`PARALLEL_RDP_HIRES_NATIVE_PASS=replace` restores the earlier behavior.

Hi-res replacement is applied in the native-resolution rasterization pass as
well as the upscaled one: `tile_uses_hires_replacement()` does not depend on the
scale, and both `submit_render_pass()` and `submit_render_pass_upscaled()` bind
the same `HIRES_REPLACEMENT` rasterizer. Replacement alpha therefore decides,
through cutouts, the HLE alpha-kill rule and alpha test, which pixels write
color and depth to RDRAM. A game that reads its render targets back on the CPU
can then run differently with a pack loaded.

Ocarina of Time (USA 1.0, `local-oot-zero-config` pack, core `d574adc6`) shows
it. In a 2,970-frame segment of the opening Kokiri Forest flight, feature-off
and feature-on runs are each deterministic. State saved every frame from the
same start agrees outside the color buffers until frame 2,049. From frame 2,049
to 2,400 the only other differences are words of one 320-pixel-wide 16-bit
buffer (rows 0x280 bytes apart, depth-like values) at 0x80130000-0x80140000,
probably the depth buffer. From frame 2,500 game memory diverges as well, and
by frame 2,700 RSP memory and CPU registers do too. OoT reads its depth buffer
on the CPU for sun lens-flare visibility, which fits this chain, but the reading
code was not traced. Two other OoT segments of the same play diverge the same
way; Paper Mario, Super Mario 64, Mario Kart 64, Majora's Mask and GoldenEye
segments recorded so far do not.

`PARALLEL_RDP_HIRES_NATIVE_PASS=native` skips replacement in that pass when an
upscaled pass exists. Over the 262-scenario nightly lab corpus at 4x (PM, OoT, MM,
GoldenEye, SM64, MK64; the GoldenEye menu segment is feature-off only), every
feature-on run with `native` ended in exactly the feature-off end state, the OoT
segment included. Presented captures matched the earlier feature-on goldens in
255 of 261 scenarios. The six changes are framebuffer-copy effects: an OoT pause
background now sharp where it was blocky; MM pause and boot, the PM storybook and
game over below visible difference; and the PM battle-entry transition, whose
fade now matches feature-off brightness but whose screen copy shows faint native
ground art. Feature-off runs on the same build passed all 262 scenarios against
the feature-off goldens.

Options considered: skipping replacement in the native pass whenever an upscaled
pass exists (RDRAM then matches feature-off byte for byte, but effects built
from CPU-read framebuffers would show native art), or keeping current behavior
(likely closer to HLE renderers that copy their own depth back; not checked)
and gating CPU invariance per game. Evidence: lab regression `lab-play-oot` and the October 18
multi-game regression receipt in the private workspace.

## Replacement decal alpha removes shaded geometry

Status: corrected in source October 11, 2026; isolated live candidate qualified.
Installed-runtime rollout remains separate.

SM64's automatic Big Boo's Haunt demo exposed a hollow front on Boo with the
Reloaded HD pack. Eyes `b11fdc36` and mouth `070e836f` are RGBA16 identities whose
replacement alpha masks color over a shaded body. Their converted payloads equal
the source cache. Actual one-cycle triangle draws use normalized RGB mux
`01040804` and alpha mux `07070705`: `(TEXEL0-SHADE)*TEXEL0_ALPHA+SHADE`, with
environment-controlled output alpha. Rejecting every replacement alpha-zero
sample before the combiner incorrectly removes the shaded face geometry.

The shared shader policy now preserves empty one-cycle TEXEL0-over-SHADE decals
when output alpha is independent of textures. Secondary/pipelined samples,
conversion and two-cycle draws retain the existing cutout policy. Ordinary
sprite/effect cutouts and the later combined-alpha kill remain intact. This is a
combiner class rule, without a game, texture-key or descriptor override.

Two fresh-demo checkpoints reproduce the face hole with replacements enabled
and retain the face in matched native controls. Cold loads briefly show native
fallback output, so the accepted before/candidate comparisons step fifteen
neutral frames after the same state load. The candidate restores shaded Boo
fronts and authored eyes/mouth in both inspected phases; one phase also repairs
small holes in Mario's face. Surrounding replacement art remains coherent.
Both native-off captures are pixel-identical before and after the correction.

`emu.unit.hires_cutout_policy` exercises the production decision helper, the
captured mux and conservative coverage/dependency exclusions. GPU evidence is
separate from that selector test. The tracked shader header is regenerated using
the pinned 2020 packer and the candidate core is built in an isolated source
copy. Other combiner classes, all opacity phases and other backends remain
unqualified; broader multi-cycle alpha dependency propagation is not implemented.

## Full negative analog diagonal loses input

Status: corrected in source October 11, 2026; bounded live candidate qualification passed. Installed-runtime rollout remains separate.

The libretro controller bridge squared two signed 16-bit stick axes as `int`
before passing their sum to `sqrt`. For `(-32768, -32768)`, the sum is
2147483648, overflowing a signed 32-bit integer. The resulting invalid radius
can take the deadzone branch and emit zero N64 axes. Fresh gameplay reproduced
no movement for that full diagonal while cardinal inputs and a smaller negative
diagonal moved the player. This is a controller conversion defect; it does not
classify IL-3's historical load-settle report or establish a renderer fault.

The conversion now widens each product to `double` before adding. Deadzone,
sensitivity, angle snapping, button mapping and valid-axis rounding are unchanged.
`emu.unit.input_analog` links the production controller module, supplies frontend
callbacks, and checks emitted N64 axes for all full-range corners, cardinals,
neutral input, a smaller diagonal and configured deadzone behavior. A standalone
UndefinedBehaviorSanitizer run reproduces the overflow before the change and
passes after it. A matched fresh-process 4x Vulkan replay from the same protected
hammer checkpoint reproduced zero displacement on the original candidate and
movement on the corrected candidate for the same 12-frame full negative
diagonal, followed by normal idle control after release. Actual retained PNGs
and position probes were independently reviewed. This qualifies that bounded
controller behavior, not whole-game or other-backend correctness; the installed
runtime was unchanged.

## IL-3: Load-settle input pathology

Status: unconfirmed historical report; needs a minimal reproduction before fix work.

Input injected during the three-frame settle after a state load has shown
suspect behavior. The fault boundary may be core state restoration, frontend
task completion, or adapter sequencing. Do not change renderer/core behavior
until a minimal fixture identifies the layer. The completed load barrier and
bounded control checks do not reproduce this report or establish that it is
fixed. Do not treat it as a prerequisite for ordinary emulator development;
retain it as a reproduction lead if post-load input fails again.

Acceptance:

- a deterministic minimal reproduction;
- classification to core, RetroArch, or adapter;
- a focused regression test in the owning layer.

## IL-19: Complete `load-slot` with `WAIT_LOAD_STATE`

Status: implemented; frontend rollout must pass capability preflight.

`load-slot` now waits for the load command acknowledgement, then sends
`WAIT_LOAD_STATE` and requires a fresh `WAIT_LOAD_STATE DONE` before returning.
It checks this attempt's load record and failure log after the barrier because
DONE only reports task completion, not successful deserialization. The fixed
completion delay is removed. An explicit failed load may retry once after the
barrier and a one-second contention backoff. Acknowledgement or barrier timeout
leaves the outcome unknown and fails without another load.

`check-frontend --retroarch-bin PATH` probes the compiled command table with
`--verbose` and an invalid `--command` operand (no content launch or command
transmission), with a five-second timeout. It prints `FRONTEND_LOAD_BARRIER=ready` and exits zero only when
both load verbs and `WAIT_LOAD_STATE` are advertised. Failures report `missing`,
`unsupported`, `timeout`, or `probe-error` and exit nonzero, without exposing
binary diagnostics or private paths. `start` runs this check before creating a
session; `load-slot` repeats it against the recorded session binary before
sending a load or barrier. An absent recorded binary fails closed.

The display-free `emu.support.interactive_load_contract` test exercises the
actual adapter FIFO/log path with a fake frontend: delayed completion and
failure, explicit retry, missing load record, stale logs, acknowledgement and
barrier timeout, unsupported frontend, and start preflight. It does not qualify
a deployed frontend or resolve IL-3's separate post-load input pathology.

A bounded Linux headless Vulkan check on September 29, 2026 passed real slot-4
restoration, input/frame stepping and capture, explicit missing-slot failure,
successful recovery, and clean shutdown. The separately recorded runtime receipt
pins the frontend and adapter bytes. The capability probe also exposed a
command-only teardown crash fixed in the frontend owner before qualification.
Rollback keeps the preceding adapter and its matching frontend together; do not
substitute this adapter into an unqualified frozen runtime. No renderer/core
behavior or benchmark scoring changed.

## Batch adapter macOS metadata and early-exit cleanup

Status: fixed October 3, 2026.

The batch adapter used GNU-only `sed -i` to update bundle metadata after
launching RetroArch. BSD sed rejected it on macOS, and the adapter's exit trap
removed the FIFO without stopping its child. Metadata now updates the nested
JSON status through Python before launch and on completion. Early exits reap
only the adapter's owned child, with bounded TERM/KILL cleanup, before releasing
the runtime lock. Signal handlers use the same exit path.

The display-free batch lifecycle contract checks portable status updates,
preservation of unrelated metadata, rejection of invalid metadata before
launch, and command-failure child cleanup through the real FIFO. It passed on
Linux and macOS. The macOS title scenario subsequently completed command
execution and clean teardown. Its native sampled-entry failure exposed a stale
fixture requirement, corrected under ADR-0006 as described below.

## Paper Mario fixture identity gates

Status: aligned with ADR-0006 October 3, 2026.

The title, file-select and `kmr_03 ENTRY_5` runtime profiles still required native
sampled entries after that enrichment program was frozen. The active path uses
Rice-compatible draw-time replacement, so a compatibility-only package could
render correctly and still fail the obsolete gate.

These profiles now require compatible draw-hit presence. Loaded entries and
upload hits alone do not satisfy it. The authority wrapper accepts compatible
entries by default, requires actual draw-time activity, and removes its obsolete
native minimum/compat opt-in options. Native counts remain diagnostic evidence.
Provider/source, scene semantics, provenance, feature-off parity and explicit
visual review remain required; no renderer logic or pack identity changed.

Display-free contracts accept compatibility-only evidence and reject missing,
zero or malformed draw activity, including independent wrapper reuse checks.
Dormant converter enrichment input validation remains separate and unchanged.

## Hi-res replacement admission budget

Status: stable-descriptor admission accounting fixed; runtime pressure unqualified.

The opt-in `PARALLEL_RDP_HIRES_GPU_BUDGET_MB` configuration previously disappeared
when replacement resources reset during pack initialization. The setting now
survives provider/cache resets. Images that do not fit reject admission and use
the existing native-texture fallback. Already admitted replacement bindings stay
stable until the cache resets. Zero retains the default unlimited behavior.

The old single-image LRU eviction could still exceed the byte budget and reused
indices that buffered draws or tile bindings could still reference. It is removed;
introducing eviction requires an explicit descriptor-retirement design.

The display-free `emu.unit.hires_registry_budget` regression exercises distinct
slots, exact fits, full/oversized rejection, unlimited mode, descriptor exhaustion,
overflow-safe comparisons, and unchanged accounting on rejection. Renderer
compilation checks the integration. This accounts reserved replacement mip-chain
bytes; failed image creation can conservatively retain a reservation until cache
reset. It is not a physical VRAM ceiling. GPU-pressure rendering and nonzero-budget
runtime behavior still need bounded qualification before deployment.

An October 11 isolated 4x title probe with a 4 MiB admission limit completed
load, step, capture and shutdown with coherent mixed native/replacement output.
Its strict hi-res fixture rejected the missing compatible draw hits; it is a
fallback observation, not a passing replacement qualification or a measured
physical-memory bound. Broader pressure behavior remains unverified.

## Trilinear replacement cutout fringes

Status: alpha-weighted interpolation across mip levels fixed October 11, 2026.

Replacement mip samples contain straight RGB after opacity-weighted bilinear
filtering. Ordinary RGBA interpolation between them darkened cutout edges when
one mip contributed transparent black, or when the two opacities differed.
Trilinear filtering now weights the two RGB samples by their alpha contribution
and keeps interpolated alpha. Fully transparent results return zero RGBA.
Nearest, bilinear and feature-off sampling are unchanged.

`emu.unit.hires_mip_filter` executes the shared production shader arithmetic:
transparent boundaries, unequal opacity, ordinary opaque/equal-alpha filtering,
endpoints and a fractional-LOD cutout. The pinned shader tool regenerates the
same header repeatedly; all four feature-off rasterizer SPIR-V variants remain
byte-identical. These checks establish the sampling correction, not a verdict
on every historical scene or pack.

The earlier isolated rebuilt-core check reported hi-res title and named kmr03
success at 4x. The native RDRAM correction below invalidates its kmr03 scene
claim; the title evidence remains separate. Feature-off before/after title captures are
byte-identical on the same current frontend/configuration; both differ from the older recorded
headless baseline. That baseline mismatch is retained, not reminted by this
correction. Full GPU fractional-LOD cutout and macOS checks remain outstanding.

## CI32 diagnostic palette bounds

Status: out-of-range diagnostic groups rejected October 11, 2026.

The optional CI palette probe can derive groups above entry 255 from 16-bit
texels, while the renderer's TLUT shadow holds only 256 entries. Computing its
palette CRC read beyond that shadow. The probe now returns its existing
unavailable sentinel for an incomplete group rather than reading, wrapping or
hashing only a prefix. Normal replacement identity selection is unchanged.

The CI palette unit test covers the last valid group, first and highest
unavailable groups, and mixed valid/unavailable usage. AddressSanitizer exposed
the prior read and passes the corrected cases.

## Input override cleanup after a failed hold

Status: bounded release on failure/interruption added October 11, 2026.

The interactive adapter previously exited after failed input acknowledgement,
frame-status read or stepping without releasing its persistent input override.
Later stepping or state loads could therefore inherit held buttons or analog
input. An exit handler now attempts a bounded clear of the selected port and
checks its acknowledgement; an unreachable frontend still leaves an explicitly
uncertain outcome. Successful holds retain their existing clear sequence.

The display-free interactive contract exercises successful frame/analog holds,
missing set acknowledgement, failed status reads and interruption during a
stalled frame wait or timed hold through the real FIFO/log path. Timed holds
release promptly on interruption and reap their sleeping child. This does not
reproduce or close IL-3, whose historical fault boundary remains unconfirmed.

## Paper Mario fixture capture and scene admission

Status: invalid captures and wrong named world scenes rejected October 11, 2026;
kmr03 authority retargeted to `kmr_03 ENTRY_2` October 15, 2026, pending its
first runtime remint.

The shared verifier previously accepted a non-image capture and accepted any
world scene for the `kmr_03 ENTRY_5` fixture when world callbacks and replacement
traffic matched. It now requires a decodable capture with positive dimensions.
Fixture profiles may declare typed area/map/entry expectations; the kmr03
profile requires area 0, map `kmr_03`, entry 5. Generic world fixtures remain
unconstrained unless their profile declares a scene.

Focused contracts reject malformed/truncated captures and wrong or missing
scene identities. The later native RDRAM correction below additionally rejects
legacy named-scene metadata and invalidates the retained kmr03 scene claim.
These are evidence admission checks. Correct textures, regions, filtering and
composition still require semantic evidence and explicit visual judgment.

The `paper-mario-kmr-03-entry-5` fixture is retired. Its retained state decoded
as area 6/map 5/entry 2, the nok_12 attract demo, and no kmr_03 ENTRY_5 state
exists in the available corpus, so its runtime lane could only fail. Relabeling
those bytes would have recorded a false scene, so the authority was replaced.
Its manifest, scenario, graph node and title-plus-960-frame bootstrap remain in
Git history with their historical digests (capture `35213195…`, live probe
`4bd3929d…`); the local state was never tracked.

The replacement fixture is `paper-mario-kmr-03-entry-2` (graph node
`kmr_03_entry_2_idle`, capture `kmr_03_entry_2`). Its profile requires
qualified decoding of area 0, map `kmr_03` (area-local map index 2), entry 2.
Lineage is an import of the `landing-control` checkpoint from a qualified
frame-stepped Linux progression recording: first player control in kmr_03 after
the veranda fall. The state was saved at session frame 10230 and decoded there as
kmr_03 entry 2, story progress -122, HP 10, world mode. Its M64+SAVE body sha256
is `e98271b6…757d2d`, and the RetroArch file sha256 is `bff85c1b…478783`. The
savestate carries progression, so no savefile is staged. Remint loads paused,
settles three frames and captures feature-off.

The fixture and graph node stay `pending-remint` with no pinned feature-off
digest until the first runtime remint reports one. The display-free contracts
check the new scene, including rejection of the retired decode. They do not show
that the imported bytes load or render correctly in this core.

## Prologue shadow and Bowser sprite observations

Status: bounded current replay reviewed October 11, 2026; historical cause unresolved.

Existing Paper Mario evidence contains a rectangular dark floor region during
Peach/Bowser's castle confrontation, coarse native-looking jaw/chest regions in
an otherwise replaced Bowser closeup, and a later Star Rod frame with separated
torso/leg regions. These are distinct phase-specific observations, not one
established renderer mechanism. The ordinary eval ends at intro-battle start;
third-turn Star Rod use is later. The intro-attract closeup is another scene.

The published confrontation has a strong approximate match to historical
human-controlled emulator footage, whose effective configuration is incomplete.
It is not hardware ground truth or a controlled current-core comparison.
Retained October Linux pre-Peach and confrontation states, original actions,
configs and captures supply nearer replay candidates; current compatibility
has not been tested. Older first-menu states need separate loading/actionability
qualification before a transformation continuation.

A terminal capture's frontend-frame tag agrees with G4 RAM while the image
still depicts the preceding dialogue. Capture freshness and transition
presentation must be established separately before assigning a renderer fault.
A stale reused-slot index entry was also found; immutable copied state bytes
and their corroborated phase evidence take precedence over that slot receipt.

The older balloon-star stale TEXEL1 binding fix (`b3d9be57`) is in current source.
Prior black-hole and tint-loss accusations were overturned by authored content
and unmatched effect phases; they do not close these new observations. Several
historical Bowser filenames identify other scenes, so labels alone are unsafe.

The subsequent serial 4x Linux canary loads the castle dialogue, intro-attract
closeup and old first battle command menu. A literal archived continuation
reaches third-turn Star Rod use and the historical laugh caption with connected
Bowser parts. Matched dense native/replacement phases show cast rays, flare
rings, cyclic colors, expanding glow and dialogue fades in both conditions;
no replacement-only segmentation or opaque chest patch is reproduced there.
A new cold process reloads those starts and the protected pre-cast checkpoint,
then reaches the effect and recovery without a new playthrough.

The earlier arrival route also completes with matching native/replacement input
steps and final GameStatus bytes. Reviewed smash, arrival, dialogue and walking
phases show connected Bowser parts and actor-associated shadows in both; no
isolated floor slab is reproduced in those samples. These negative
observations are limited to the recorded core/pack/RADV/4x/trilinear condition;
the old Mac core/pack and exact website extraction phase are not reconstructed.
No specific historical renderer cause or prologue GPU fix is established.
Follow ADR-0004/0005; do not match GlideN64 geometry or add scene-specific policy.

The subsequent cold replay reproduced the separate Peach hair transition and
traced its cause to the CI4 palette-bank substitution below. The replacement
pack itself contains flat yellow hair; a color difference alone did not prove
this fault. Actual draw roles and binding provenance supply the diagnosis.

## CI4 palette-bank replacement provenance

Status: reproduced and corrected October 11, 2026; cold/arrival art recovery,
Star Rod control and feature-off parity pass; the required gate passes 54/54.

Paper Mario's shaded-sprite pipeline deliberately samples equal-offset CI4
tiles with different palette roles: bank1 is generated lighting, bank0 is sprite
art. Its two-cycle combiner multiplies those samples, then applies shade and
environment. Replacement bindings previously retained no palette-bank provenance;
equal-offset upload propagation and SetTile retention could put a bank0 art
replacement into the bank1 lighting slot. Draw-time bank0 compatibility could
also restore that wrong-role binding.

A protected pre-window-smash checkpoint and its confirm2/neutral20,
confirm2/neutral20, neutral180 suffix reproduce the fault in fresh processes.
The identified Peach back sprite is `2c603189edadae7d`, native 48×72,
replacement 210×315. Actual triangle location, both physical tile identities,
authored/generated TLUT loads and the combiner establish the roles: before the
fix, bank1 serves bank0 replacement art; bank0 changes from native to the same
replacement near the smash. The pack's back-hair art is itself flat yellow;
matching native brown is not the correctness criterion.

Bindings now preserve their lookup-source palette bank across aliases. Draws
reject mismatched CI4 bindings only for sampled tiles, before resolving misses;
the unequal-bank0 fallback is removed. CI8-to-CI4 reinterpretation normalizes
its source provenance to bank0, matching the existing lookup. SetTile's pending
load behavior remains unchanged.

Bank rejection alone visibly lost replacement art at a cold start. Each sampled
slot now resolves independently against its own palette and final texture view.
A missing load origin may be inferred only from peers with matching offset,
stride, format, size and native extent, whose known addresses all agree. This
copies an origin, never a replacement binding or key. Overlapping writes retire
known origins even when a tile has already fallen back to native. This is bounded
inference from existing per-tile tracking, not a complete TMEM generation model.

The final cold replay retains crisp Mario and Peach replacement art while
restoring generated modulation: Peach's hair is muted olive/gold and her dress
purple, rather than the earlier neon-yellow/pink. The rejected guard-only
candidate and its visible native-fallback regression remain separate evidence.
The arrival replay also retains HD actors through Bowser’s entrance, and the
unsplit Star Rod control preserves coherent connected body/cast/glow phases.
Four feature-off captures match the prior native condition exactly. No
scene-specific renderer policy or texture-pack change is involved.

Opt-in diagnostics expose both physical tiles and raw queued triangle/shade
setup. Sampled CRCs explicitly describe host-global TMEM and pending upload
count: those bytes are not a current-draw snapshot while uploads are queued.
Legacy RDRAM/TLUT compatibility keys are recorded separately; the CRC algorithms
are not interchangeable. Probe coverage includes TEXEL1 triangles without
widening sampled-exact replacement admission.

The full isolated build passes, the required owner gate passes 54/54 and closing
static review reports no actionable findings. An independent reviewer inspected
all 22 final captures; the emulator is stopped. Production-policy tests cover
changed/unchanged CI4 banks, aliases without
provenance, CI8 normalization, pending SetTile, unsampled neighbors, coherent
versus ambiguous origins and overlap/wraparound. They do not replace GPU replay
validation. Same-bank TLUT-content changes, other backends, every intermediate
frame and the historical Bowser cause remain unqualified.

An additive fresh-new-file opening through the Bowser tutorial and house wake
also retains coherent shaded actors in the inspected current-core castle poses,
and connected Bowser pieces through the sampled Star Rod cast/recovery. Matched
cold native/replacement confrontation controls show a single shaded Mario;
the older pre-fix control again shows bright unmodulated Peach art at the same
pre-window checkpoint. Its matched confrontation frame also shows dark,
overlaid-looking Mario art and bright Bowser/Peach/Kammy colors resembling the
reported symptom; the current fixed condition retains coherent shaded art.
This does not establish two geometric actor instances. The world profile has lighting flag 1; the conditional
alternate-viewport silhouette flag 2 is inactive, so that source branch does not
explain the reported stationary website duplicate. A separate zero-HP spinning
Mario trail occurs in both native and replacement controls and agrees with the
game's explicit defeat-blur sequence. These bounded observations justify no
additional renderer patch or fixture-authority promotion. The website's original
runtime and exact-frame cause remain unqualified.

## Paper Mario native RDRAM semantic decoding

Status: native word mapping corrected and bounded runtime decoding checked October 11, 2026.

The semantic decoder treated CPU field offsets as direct offsets into the
little-endian native RDRAM returned by `READ_CORE_MEMORY`. Its unaligned
`0x800740aa`/230-byte GameStatus snapshot omitted the native bytes containing
areaID and swapped neighboring halfwords. A battle sample with area 4, map 7
and entry 2 consequently appeared as area 512, map 2 and entry 7. Byte fields
and optional menu/window/transition fields had the same mapping defect.

Maintained producers now capture complete aligned words at `0x800740a8`/232
bytes. The decoder reverses each native word, slices the logical
`0x800740aa`/230-byte GameStatus range and reads CPU big-endian fields. The
current-menu capture widens from one byte to four. Optional byte/halfword
decoding follows the same mapping; aligned pointer/word values retain their
numeric meaning. GameMode flags use CPU offset 0; offset 2 is padding.
Source metadata declares the native format and decoder revision, preserves
raw-window hashes and records the separate logical slice.

The focused memory contract invokes the real decoder on distinct adjacent
fields, optional byte/halfword/word inputs and a captured native battle window.
The fixture contract now decodes raw kmr03 memory before admitting the matching
scene and rejecting wrong area/map/entry. Named scene admission additionally
requires the corrected decoder revision, native format and raw/logical layout;
legacy semantic JSON with matching labels remains unqualified and is rejected.
Callback-only title and generic world contracts do not acquire this scene-only
requirement. Legacy unaligned GameStatus and
single-byte menu snapshots, short ranges and malformed inputs fail closed;
missing area bytes are never guessed. These offline checks pass, but do not
establish current state compatibility or capture freshness.

Historical traces and semantic outputs remain unchanged and unqualified for
these fields unless independently attested. No authority was reminted, and no
renderer, transport, scorer or frozen input changed. Newly decoded runtime
scene evidence remains separate from renderer-authority promotion.

The corrected live kmr03 scenario reports area 6/map 5/entry 2 and fails its
unchanged area 0/kmr_03/entry 5 expectation. Decoder qualification passes; named
scene identity fails. Its prior passing named-scene result was false evidence.
The manifest and graph node are now scene-unqualified, retaining IDs, paths,
expected scene and historical state/capture hashes. The title-plus-960-frame
bootstrap is not a proven route to kmr03 and must not be used as such.
That fixture was retired October 15, 2026 in favor of an imported
`kmr_03 ENTRY_2` authority; see "Paper Mario fixture capture and scene
admission".

The PHRB authority wrapper also requires literal successful decoder qualification
and scene identity when aggregating that named fixture, including `--reuse`.
Its former reliance on a cached `passed:true` verdict could bypass the corrected
consumer. Focused wrapper-policy checks reject legacy/malformed/truthy verdicts
and retain callback-only title/file-select admission.

## Authority remint publication

Status: kmr03 helper verifies staged bytes before atomic publication October 11, 2026;
import staging added October 15, 2026.

The helper previously copied a bootstrap candidate over the authority before
verification. A failing scene or feature-off capture check left that output
overwritten, and a custom output could verify the runtime profile's default
instead. It now verifies an independently staged candidate, checks the canonical
scenario's selected-state hash/mode and pinned capture digest, and atomically
publishes only the verified bytes. Failure preserves the existing output.

Six focused offline CLI checks exercise verifier/digest failures, wrong selected
bytes/mode, changed candidate and successful publication through the real helper.
No actual remint, asset change or authority promotion was performed.

The retargeted `remint-paper-mario-kmr-03-entry-2-authority.sh` no longer
bootstraps. It stages a caller-supplied recorded state, requires its pinned
M64+SAVE body digest (RetroArch containers are unwrapped) and verifies through
the canonical load, settle and capture scenario. It also requires the scene
decoded by that run to equal the tracked profile's pinned area, map and entry.
Source, scene and capture pins come from the tracked runtime env, which a
caller's runtime override cannot relax. With the capture digest still pending,
the helper reports the observed digest and exits 3 without publishing.
`emu.support.kmr03_import_remint_publication_contract` covers source mismatch,
container identity, pending digest, wrong scene, relaxed override pins and the
earlier publication cases through the real helper, shell library and unwrapper.

## LoadTile source layout in compatibility draw keys

Status: corrected and qualified in bounded October 11, 2026 regression checks.

Mario Kart 64's stock-USA title draws its character illustration as 320×2
strips from a 320-wide RGBA16 source. Inclusive command bounds report 321×3,
the masks retain two rows and TMEM aligns rows to 648 bytes. Compatibility
lookup incorrectly hashed that wider view with TMEM stride and the uncropped
image base. The matching pack was present, but the HD title art stayed native.
Copyright strips have the same fault: source width 124 and stride 248 became
width 125 and stride 256.

A protected title checkpoint independently reproduces both prior miss keys
from native RDRAM. Source-width/crop/stride correction produces all 120 default
background strip keys and the two full copyright-strip keys in the selected
pack. Its final copyright row remains an unqualified native fallback; no key
is invented for it. These are identity proofs, not image correctness gates.

Equal-format/size, unsplit LoadTile uploads now retain their source crop,
image width and RDRAM row stride alongside the existing tracked image base.
Aliases copy that context; conflicting contexts cannot establish an inferred
origin, and re-points/overlapping writes retire it. Draw keys clamp width to
the source and retain masked height. A differing source-view extent retries
the exact draw key even when the raw upload had a replacement hit. Alignment
padding no longer counts as extra sampled rows; real extra rows and columns
still reject a binding. Split transfers and format/size reinterpretations
retain the prior compatibility path. LoadBlock/DXT and its legacy odd-CI4
packed-byte guard remain unchanged.

Bindings minted from a qualified source view carry that provenance. A later
same-offset stride or format/size change that loses the qualified view retires
only those bindings before an exact legacy lookup can retry. Pending raw-upload
and LoadBlock bindings retain their existing reshape policy. This closes the
648-to-640 stride transition that otherwise passes the legacy byte guard.

Production-policy tests cover independent CRC goldens for adjacent cropped
strips, unaligned copyright rows, odd unmasked heights, unsupported context,
split-tail qualification, conflicting aliases, exact-view retry and genuine
overruns and source-binding retirement after a stride change. No game-specific
dimension adjustment or pack edit is involved.

The final isolated candidate passed all 57 required tests. Fresh and cold MK64
title inspection supports the HD-art correction; six protected course starts
and a later Thwomp orientation remain coherent. Eight native controls match the
prior binary exactly with identical process/load histories. Independent visual
review found no introduced semantic defect in the 59-sample fresh Paper Mario
demo, its returned-title boundary and three cold starts, or two SM64 Boo phases.
Static closing review reported no actionable finding. Normal MK64 gameplay HUD,
every intervening frame, patched MK64DX and other graphics backends remain
unqualified. Installed runtimes were preserved.

## CI4 LoadTile transport padding drops a valid replacement

Status: corrected in source October 12, 2026; isolated Linux 4x candidate qualified.
Installed-runtime rollout remains separate.

Paper Mario's ordinary speech bubble uses an 8×64 CI4 center between two
32×64 caps. The standard tile macro transports the center as a four-byte-wide
CI8 source, with four-byte RDRAM rows and eight-byte TMEM alignment. The selected
pack contains the authored center. Live diagnostics show a successful CI4
reinterpretation upload followed by a draw-window rejection: padded TMEM bytes
were mistaken for extra sampled image data. The result was a native-gray center
between authored lavender caps, with a discontinuous border.

The qualified source view now admits the CI8-transport-to-CI4 case when both
formats are CI and the captured TMEM stride still matches. (October 15 widened
this to any same-format 8-to-4-bit transport; see the I/IA entry below.) Source width doubles
into CI4 texels; crop addresses and RDRAM strides remain byte units. A live
binding whose admission depends on this padding proof carries source-layout
provenance and retires when a later draw loses it. Aligned raw uploads and
LoadBlock retain their prior policy; reverse and unrelated reinterpretations
remain unsupported. There is no game, key, descriptor or pack override.

The production compatibility-view test covers the real CRC calculation, logical
window admission, genuine overruns, incompatible sizes/formats, raw overwrite
and later stride changes, including the earlier 648-to-640 retirement case.
All 57 required tests passed after the gameplay session stopped. Two independent
fresh candidate replays with debug disabled restore the authored fill and
continuous border, with matched CPU cues and original GPU screenshots inspected
independently. The matched native control is decoded-RGBA identical to the prior
native control. The selected package payload and game source establish the art;
a same-style GLide screenshot is not qualified for this scene. These checks do
not establish every palette, animation frame, game or graphics backend.

## Replacement addressing in native texel space

Status: corrected in source October 15, 2026; isolated Linux 4x candidate qualified.
Installed-runtime rollout remains separate.

Replacement sampling wrapped, mirrored and clamped in native texel space, then
scaled into the replacement and clamped each tap at the image edge. Two visible
defects followed. A wrapping tile clamped its bilinear taps at every native
period boundary instead of continuing through the repeat: SM64's Hazy Maze Cave
attract demo showed the wall backdrop (RGBA16 32×64 → 256×512) split into hard
blocks and the floor (32×32 → 256×256) in stepped diagonal bands, while native
and GLideN64 are continuous and the pack art tiles cleanly. A clamped tile used
the N64 clamp, which pins coordinates to the last native texel's origin, so every
replacement row inside that texel froze: Paper Mario's title logo (point-sampled,
clamped 200×2 RGBA32 strips with 800×8 replacements) repeated four output rows and
then jumped at each strip boundary.

The sampler now takes the tile-relative coordinate before clamp and mask, scales
it into the replacement and addresses each tap in replacement space with the
scaled period (`shaders/hires_addressing.h`; ADR-0010 rule 8). A clamped tile
holds the replacement texels of its first and last native texels, including
tiles that still wrap inside a multi-period clamped range. Point-sampled draws
map a native texel's [i, i + 1) footprint onto its replacement block; bilinear
draws keep the integer-centered convention. The copy pipe is unchanged.

`emu.unit.hires_addressing` covers wrap continuity across the period boundary,
mirror reflection, clamp limits and the strip case. A paired sweep of 35 phases
(Paper Mario field, prologue, attract storybook/demo, title; SM64 Hazy Maze Cave)
on the base and candidate cores shows feature-off output pixel-identical, the
three target defects removed, and point-sampled 2D art (dialogue boxes, title
text) moved about 2 output pixels onto its native silhouette. Independent visual
review flagged one class of change, traced at texture level to pack content: Peach's castle
ledge strips that sample the wall texture at its wrap boundary now blend into the
repaint's top rows instead of holding its dark bottom rows, because the repaint
does not reproduce the native border-row layout. A fresh-boot GLideN64 capture
of the same attract-demo frame (16264, balcony pillar on the same wall art) shows
the candidate's uniform band, not the base's dark streaks. GLideN64's own title
logo keeps a weaker strip seam that the candidate no longer has. The gameplay
castle phases themselves, other games and other backends remain unqualified in
GLideN64.

## 4-bit intensity LoadTile transport drops the first replaced draw

Status: corrected in source October 15, 2026; isolated Linux 4x candidate qualified.
Installed-runtime rollout remains separate.

The October 12 transport correction admitted 8-bit-to-4-bit LoadTile views only
for CI. The standard 4-bit tile macros load I4 and IA4 art the same way. Their
8-bit upload key hit the pack, then the draw-time formatsize guard dropped it
because the tile is 4-bit, so the first draw after each load rendered native and
later draws re-keyed and rendered the replacement. Meshes were therefore part
native, part replaced: Paper Mario's attract-demo Mt. Lavalava smoke and wall
showed hard-edged grey wedges, and entity shadows hit the same path (830 guard
drops in one 110-frame volcano window; 30 for shadows in a Toad Town pause).

The source view now qualifies any same-format 8-bit-to-4-bit transport, so the
draw re-keys at the 4-bit tile identity before the guard. Cross-format
reinterpretations keep the legacy path. The compatibility-view test covers I and
IA transport, the re-key trigger and the transported CRC. The same volcano
window records no guard drops and renders the smoke as continuous replacement
art; Paper Mario's attract "1" star now serves its replacement centered on the
native silhouette. Stencil transitions, pause arrows and other I4/IA4 users are
expected to benefit but were not separately captured.

Adjacent keying note: draw-time compatibility keys add a palette CRC only for
CI. GLideN64 palette-qualifies I/IA draws with TLUT enabled but then falls back to
the texture-only key (ADR-0010 rule 7), the form packs carry for non-CI art, so
both reach the same entries. Only palette-qualified I/IA pack entries would
differ; none is known.

## Jabo MD5 keys await runtime qualification

Status: implemented in source October 19, 2026; offline-verified only. No
emulator run has served a Jabo package.

The Jabo key (see [Jabo packs](../assets/TEXTURE_PACKS.md#jabo-packs)) was
derived offline from ROM data against real SM64, OoT, MK64 and GoldenEye
pack names. `emu.unit.hires_jabo_key` checks the runtime decoder against that
reference for every format on strided views and against one real SM64 name.
Six packs (SM64 Reloaded 2.6.0 Jabo HD, KMan's OoT Hi-Rez, Kerber2k's MK64
1.2, the AeroGauge DDS pack, a DKR pack and Road Rash 64 Remastered with
JPEGs) convert, inspect as version 8 and decode identically through the C++
provider.

A wrong key is a silent miss. Unverified cases:

- LoadTile strided sub-rectangles, views where the mask differs from the
  tile size, 4-bit transport loads, split RGBA32 loads and LoadBlock views;
- CI4 with a nonzero palette bank, CI with an IA16 TLUT, and the TMEM palette
  mirror's byte order, which was read from the load path rather than observed;
- views whose Rice texture CRC is zero (rows under four bytes), which the
  compat lane skips before the Jabo key is tried.

Jabo keys are computed only by the draw-time compat lane, so a compat CRC
override that disables that lane disables Jabo lookups too. Runtime
qualification should compare SM64 Reloaded's Jabo and GLideN64 releases in
the same scenes, then OoT Kokiri, the MK64 attract demo and player select
(strided), a GoldenEye pack and the AeroGauge DDS pack, serially at 4x.

## Interactive stop leaves RetroArch running after a QUIT timeout

Status: corrected in source October 15, 2026; display-free contracts only.

`retroarch_interactive_session.sh start` recorded the setsid leader as the
session process group, but GNU `timeout` called `setpgid` and moved itself and
RetroArch into a new group within that session. When `QUIT` timed out, `stop`
signaled only the leader's group (`flock`), so RetroArch kept running and kept
the runtime lock. The launch chain now uses `timeout --foreground`, which keeps
`timeout` and RetroArch in the recorded group. The group probes used by
`start`, `require_live_session` and the TTL watchdog now include the frontend,
and `stop`'s TERM/KILL reaches it. `stop` also signals every process in the
recorded session ID. This covers sessions launched before the change.

With `--foreground`, the TTL signal goes to RetroArch itself rather than the
group. `--kill-after` (`RETROARCH_TTL_KILL_AFTER_SECONDS`, default 10) now follows
TERM with KILL. Before, a wedged frontend that ignored TERM outlived its TTL.
`doctor --reap` is unchanged. The batch adapter launches RetroArch as a direct
child, so it does not have this group defect. However, after an unanswered
`QUIT` it sent only TERM and then waited without bound. It now follows the TERM
with KILL.

`emu.support.interactive_lifecycle_contract` runs the real
setsid/flock/timeout chain with a fake frontend that ignores `QUIT` and TERM.
It checks that `stop` leaves no frontend, `timeout` or leader process and that
the group TERM reached the frontend. It also checks that TTL expiry delivers TERM
and then kills the frontend. `emu.support.batch_lifecycle_contract` covers the
batch escalation. Against the previous adapters, the frontend survived `stop`
and TTL expiry, and the batch adapter hung. The contracts ran on Linux only; the
`setsid`-less macOS launch path received the same change but was not exercised.

## Interactive commands overlapping a running step

Status: enforced in the interactive adapter October 15, 2026; display-free
contract only.

`STEP_FRAME` clears the frontend's pause flag and acknowledges before its
frames run. A second `STEP_FRAME` overwrites the remaining count. `send` already
waited for the step target, but `input`, `screenshot`, `save-slot` and
`load-slot` did not take the bundle send lock. A step whose waiter was
interrupted or timed out left nothing to stop the next command from landing
mid-step. Each step now records its target frame in `BUNDLE/session.step-target`.
`send`, `input`, `screenshot`, `save-slot` and `load-slot` serialize on the
bundle lock. While a recorded step is still PLAYING below its target, they
refuse input set/clear, memory reads, screenshots, state saves/loads and further
steps. The step ends when the frontend pauses or reaches the target. `SET_PAUSE
ON` abandons it. The failure-path release in `input` is unchanged: it still
clears a held port after a failed or interrupted step wait, even mid-step.

`emu.support.interactive_lifecycle_contract` interrupts a running step and
checks that each guarded `send` command and `input` is refused before reaching
the frontend. After the step finishes, input lands at the target frame. A
concurrent `input --frames` waits for a running step instead of overwriting it.
Against the previous adapter, that overlap stalled the first step's waiter.

## Save-slot lands in the wrong slot

Status: mitigated in the interactive adapter October 19, 2026; display-free
contract only. The root cause in the frontend is not confirmed.

`save-slot` moves RetroArch's slot cursor with silent `STATE_SLOT_PLUS`/`MINUS`
keybind events and tracks the slot locally. During the October 19 on-mode
golden run, one save landed a slot short ("Save landed in an unexpected slot"),
so one slot command never took effect. The adapter verified the slot only after
saving, by which point the misdirected save had overwritten another
checkpoint. Before each save, it now copies the state files for the slots a lost
command could leave the cursor on. If the log names another slot, the adapter
restores that slot's previous file, or removes it if it did not exist, and
records the slot actually reached. It then retries with wider command spacing,
up to three attempts. A save that lands outside the backed-up range still
fails without retrying. An explicit save-to-slot frontend command would remove
the race and is not implemented.

`emu.support.interactive_lifecycle_contract` drops one slot command in a fake
frontend and checks that the overwritten slot is restored and the retry lands
in the requested slot. The previous adapter failed it.

## Savestate bridge to mupen64plus-next

Status: tool added October 15, 2026; three Paper Mario states qualified in both
directions on a Linux 4x candidate.

mupen64plus-next segfaulted on this core's states because our writer stores
`ai.last_read` inside the AI section of a version 1.0 state; its 1.0 parser read
every later field four bytes off (PC 0, shifted RDRAM) and over-read a fixed
1024-byte event queue. `tools/savestate_bridge.py to-gliden64` writes next's
version 1.1: the extra word removed, the queue padded, `using_tlb` set from
valid TLB entries, and event 0x800 (CART_INT here, RSP_DMA_EVT there) dropped.
With CP0 Status.FR clear, FPRs are repacked between this core's per-slot layout
and next's packed pairs. `to-parallel` reads next 1.0–1.9, restores `last_read`
from the 1.3+ extra block, rebuilds `next_vi` from the VI event, maps flashram
read/status/ID modes (locating the block past any Transfer Pak cartridge), drops
64DD events and returns SPECIAL_INT to count 0: next saves a half-wrap point
there, which stalled this core about 460 frames after load until normalized.
Both directions refuse states saved with the RSP running, an RSP DMA pending or
a flash erase/program in flight. `gliden64-reference-capture.sh --state`
converts, stages and captures in one command with RetroArch's save-directory
sorting pinned. `emu.support.savestate_bridge_contract` covers both layouts,
FPR repacking, flash mapping and refusals on synthetic states, checked against
both cores' literal sizes.

Toad Town, Goomba Village and an attract-demo state ran 600 frames in GlideN64
with the HD pack and matched this core's scene, poses, idle-HUD timing and title
return at +1/+3/+60/+600 frames; GlideN64's own re-saves converted back ran 600
frames here and matched again. RDRAM after one frame differed only in frame and
depth buffers (GlideN64 does not write them back), HLE-vs-LLE RSP output buffers
and a few timing words. A Super Mario 64 Hazy Maze Cave state (FR=0 with live
FPU values) matched at +101/+401 frames as well; an unrepacked control matched
too, so the FPR repacking rests on the two cores' code rather than a visible
failure at that save point.

Limits: version 1.1 has no field for `last_read` (0 after a round trip) and lets
next apply its own extra-state defaults, including about 1542 VI cycles per
scanline where this core uses 1500. Other games, states saved mid-RSP-task or
mid-flash-operation, and long-run drift are unqualified. Separately, this core's
writer stores the PI domain-1 timing registers in the domain-2 slots; the
bridge carries them unchanged.

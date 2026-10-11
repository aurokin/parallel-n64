# Open Technical Issues

This register holds technical details owned by `parallel-n64`.
Scheduling, priority, fleet rollout, eval operation, and gameplay research are
tracked by their owning systems.

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

Status: invalid captures and wrong named world scenes rejected October 11, 2026.

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

The PHRB authority wrapper also requires literal successful decoder qualification
and scene identity when aggregating that named fixture, including `--reuse`.
Its former reliance on a cached `passed:true` verdict could bypass the corrected
consumer. Focused wrapper-policy checks reject legacy/malformed/truthy verdicts
and retain callback-only title/file-select admission.

## Authority remint publication

Status: kmr03 helper verifies staged bytes before atomic publication October 11, 2026.

The helper previously copied a bootstrap candidate over the authority before
verification. A failing scene or feature-off capture check left that output
overwritten, and a custom output could verify the runtime profile's default
instead. It now verifies an independently staged candidate, checks the canonical
scenario's selected-state hash/mode and pinned capture digest, and atomically
publishes only the verified bytes. Failure preserves the existing output.

Six focused offline CLI checks exercise verifier/digest failures, wrong selected
bytes/mode, changed candidate and successful publication through the real helper.
No actual remint, asset change or authority promotion was performed.

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

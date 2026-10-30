# ADR-0023: Hi-res residency budget and descriptor retirement

- Status: Accepted (implementation; runtime pressure qualification pending)
- Date: 2026-10-30

## Decision

Hi-res loading stays synchronous. Admission uses the selected metadata's full
RGBA8 mip-chain size before decode. A fixed-capacity LRU slot table accounts
reservations and resident images; decode or upload failure releases its slot and
bytes and suppresses retries for that key until provider change. Admission
preflights all victims, so insufficient retired memory leaves residency unchanged.
Slot zero remains the white fallback; capacity is the minimum of entry count
plus one, the device's update-after-bind sampled-image limit, and 65,536.
The variable sampled-image descriptor layout is clamped to that device limit too.

A slot can be reused only after **eight unused frames**, on every host. Binding
and draw-state copying both touch it. The command ring is flushed and drained
before advancing the frame context, with an assertion that no buffered draw
crosses that boundary. Granite waits frame-context fences before reuse; eight
frames exceed the supported frame-context count plus one. Configurations with
more than seven contexts disable hi-res with a diagnostic, preserving the
constant retirement rule. Image destruction remains deferred by Granite, and
eviction clears any of the eight tile bindings still naming the slot.

Auto memory is half of the largest device-local heap for discrete GPUs
(512 MiB–4 GiB), one quarter for shared GPUs (256 MiB–2 GiB), or one quarter
on Android/iOS (256 MiB–1 GiB). Use `VK_EXT_memory_budget` heapBudget when
available, otherwise heap size. Fix the budget at pack load. The restart option
"Hi-res texture memory" overrides auto; `PARALLEL_RDP_HIRES_GPU_BUDGET_MB`
overrides the option, with zero retaining the test-only unlimited mode.

The GPL frontend reports pack, device, pressure and read-error notifications
once per content load. [ADR-0024](0024-pack-selection-identity.md) owns the
loaded line (pack count, ident and texture count) and missing-pack diagnostics;
this decision supplies the memory-budget suffix and retains the device,
rejection, pressure and read-error rows. Both use the same notification path. MIT renderer status and periodic/final residency logs
contain no libretro dependency. Regression on-mode runs pin 8 GiB and synchronous
loading and suppress frontend notifications in viewport captures.

## Consequences and verification

Pressure falls back to native art without changing resolution, selectors,
cursors or the native RDRAM pass (ADR-0020). This bounds computed resident image
bytes, not driver allocations or staging memory still awaiting GPU completion.
There is no PHRB change, size reduction, background loader or new runtime dependency.

The first resolved texture now wins even if descriptor loading fails for a
non-budget reason (an indexed asset at upload without a palette, an invalid
sampled identity, or a decode error): CI4-reinterpretation, CI-low32 and entry-CRC
fallbacks are no longer tried, so resolution stays independent of residency and
descriptor availability. The pinned 8 GiB runtime comparison against `a26cd5b3`
will determine whether these fallbacks need restoring.

`emu.unit.hires_residency` covers N64-14's multi-eviction and release-accounting
acceptance, the eight-frame guard, atomic refusal, deterministic order, failed
upload rollback, refusal without decode, capacity and auto-budget clamps.
Runtime qualification remains separate: feature-off and generous-budget parity,
strict RDRAM parity under 64/256 MiB pressure, synchronization validation,
long-session memory/teardown checks and paired throughput measurements.

Pack refusal leaves the compiled shader capability unchanged and initializes a
one-slot fallback pool; allocation failure retains the previous fallback binding.
These Vulkan allocation paths require a runtime smoke: in scratch builds, lower
the accepted frame-context limit and separately inject a pack-pool allocation
failure. Run a few frames with synchronization validation, checking the
unavailable notification and a valid set 3 binding without a crash. No offline
renderer test currently constructs the Vulkan device needed to exercise refusal.

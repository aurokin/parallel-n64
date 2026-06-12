# ADR-0009: Interactive adapter: stdin FIFO, no daemons; paused-step determinism

- Status: Accepted
- Date: 2026-06-10; beat-capture conventions 2026-06-12

## Context

Reaching mid-game scenes (for fixtures and GlideN64 reference comparisons) needs an
agent that can play. TAS practice shows frame-accurate input authority is what makes
deterministic replay work. Daemon/socket control planes and UI automation were
rejected: the failed attempt's orchestration lessons, and the research-phase
preference for a Unix agent socket, lost to a simpler model.

## Decision

1. **One live session, stdin FIFO, no daemons, no sockets.**
   `tools/adapters/retroarch_interactive_session.sh` keeps a single RetroArch
   session alive across agent turns (setsid process group, shared runtime flock,
   TTL watchdog). Control is machine-readable frontend commands plus frame-stamped
   internal input — never menu/UI automation. RetroArch changes stay additive and
   merge-friendly (tools/retroarch-patches/).
2. **Deterministic play = stay paused, step frames.** The game runs in real time
   between agent commands, so free-running sessions are exploration only.
   Paused + `input --frames N` stepped recipes proved bit-identical on replay.
3. **Beat-state conventions** (2026-06-12). A "beat" is a user-flagged in-game
   moment pinned by saving a state at sight during a live run. Save at sight —
   long-range replays (~6k–24k frames) through the attract montage diverge
   (AI-FIFO restore is best-effort), while short-range (<~400 frames) load+step
   recipes are bit-exact. The exact-shot recipe is paused load → `STEP_FRAME 3` →
   screenshot (`LOAD_STATE_SLOT_PAUSED` does not render the loaded frame). Under
   hi-res debug log flood, STEP_FRAME acks can vanish — send with tolerance and
   poll the status frame.

## Consequences

- Agent play is proven live (kmr_03 walked into battle; clean stop, zero orphans,
  TTL self-termination verified).
- User-saved F2 slots (`savestate_auto_index`) are the standard way to pin reported
  moments; the 17-beat glide-vs-parallel comparison rests on this.
- Adapter limits stand documented: the `load-slot` verb caps at slot 9 (raw
  `LOAD_STATE_SLOT_PAUSED N` for 10+); `--state-source` must contain the
  "ParaLLEl N64" core subdir.
- Session-driver scripts encoding these conventions live in the lab repo
  (ADR-0017).

## Evidence

- Commit a7fbf4ad; tools/adapters/retroarch_interactive_session.sh;
  PROJECT_NOTES.md 2026-06-10 adapter entry and 2026-06-12 exact-shot entry;
  montage-hunt/beat-states MANIFEST.md.

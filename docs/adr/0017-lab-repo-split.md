# ADR-0017: Session-lab tooling lives in the private parallel-n64-lab repo

- Status: Accepted
- Date: 2026-06-12

## Context

The 2026-06-10..12 campaign produced a layer of session-driving and forensics
scripts that encode hard-won methodology (flood-safe stepping, beat-state minting,
glide/parallel comparison arms, PHRB asset extraction, review-sheet building) but
lived in unversioned places: `/tmp` and the gitignored `artifacts/` tree. They are
not product tooling — no gate references them, they carry machine-specific
assumptions, and they change per experiment — but losing them would mean
re-deriving the methodology.

## Decision

- Session-lab tooling is preserved in a separate **private** repo:
  [github.com/aurokin/parallel-n64-lab](https://github.com/aurokin/parallel-n64-lab)
  (local checkout `/home/auro/code/parallel-n64-lab`), organized as `sessions/`
  (interactive-session drivers + shared `lib.sh`), `wrappers/` (gdb and slangmosh
  shims), `analysis/` (PHRB extraction, comparison sheets, alpha maps, strip
  stacking), and `notes/` (forensics findings).
- **Boundary rule:** product tooling — adapters, scenario runners, fixtures,
  converters, retroarch-patches, anything a test or plan references — stays in
  parallel-n64 and is gated by its profiles. The lab repo holds the experiment
  layer; it is never a correctness authority and nothing in parallel-n64 may
  depend on it.
- Scratch that is re-derivable from any debug run (raw key censuses, pointer
  files, superseded script versions) is not preserved.

## Consequences

- The methodology survives `/tmp` cleanup and evidence-bundle pruning.
- parallel-n64 stays free of machine-specific session scripts; the lab repo README
  carries the machine assumptions and per-script catalog.
- Future session scripts should be written in (or promoted to) the lab repo once
  they outlive their experiment.

## Evidence

- parallel-n64-lab commit d7d51e7 (initial import, 31 files); audit inventory of
  /tmp and artifacts tooling, 2026-06-12.

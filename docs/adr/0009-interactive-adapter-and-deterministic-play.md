# ADR-0009: Bounded Stdin Control And Paused Stepping

- Status: Accepted
- Date: 2026-06-10

## Context

Deterministic fixture control needs a live frontend session without adding a
service, port, or renderer meaning to RetroArch.

## Decision

- One RetroArch process is controlled through its stdin command table and a
  bundle-local FIFO.
- Interactive sessions use a process group, runtime lock, and TTL; they are not
  daemons.
- Deterministic paths remain paused and advance with explicit frame steps.
- State saves and screenshots wait for completion acknowledgements. RetroArch
  exposes a load-completion barrier, but the interactive `load-slot` helper has
  not adopted it yet; [IL-19](../ISSUE_LOG.md) tracks that exception.
- Free-running control is exploratory, not replay authority.

RetroArch changes remain generic and reconstructable from
[the patch set](../../tools/retroarch-patches/).

## Consequences

Portable adapters translate commands and record evidence. Long gameplay,
route research, and experimental automation stay outside the product
repository.

## Evidence

- `tools/adapters/retroarch_interactive_session.sh`;
- adapter support tests and anchored replay verification.

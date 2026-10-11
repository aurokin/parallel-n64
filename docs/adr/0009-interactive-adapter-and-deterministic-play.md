# ADR-0009: Bounded Stdin Control And Paused Stepping

- Status: Accepted, amended
- Date: 2026-06-10; load-barrier and frontend-ownership correction 2026-10-11

## Context

Deterministic fixture control needs a live frontend session without adding a
service, port, or renderer meaning to RetroArch.

## Decision

- One RetroArch process is controlled through its stdin command table and a
  bundle-local FIFO.
- Interactive sessions use a process group, runtime lock, and TTL; they are not
  daemons.
- Deterministic paths remain paused and advance with explicit frame steps.
- State saves and screenshots wait for completion acknowledgements. The
  interactive `load-slot` helper checks frontend capability, waits for
  `WAIT_LOAD_STATE DONE`, and checks the current attempt's load result before
  returning success. The barrier sequences task completion; it does not establish
  successful deserialization. [IL-19](../ISSUE_LOG.md#il-19-complete-load-slot-with-wait_load_state) records the implemented contract.
- Free-running control is exploratory, not replay authority.

RetroArch changes remain generic and are maintained in its `agent-control`
branch. [The historical patch set](../../tools/retroarch-patches/) reconstructs
the older stdin-control series, not the full maintained frontend.

## Consequences

Portable adapters translate commands and record evidence. Long gameplay,
route research, and experimental automation stay outside the product
repository.

## Evidence

- `tools/adapters/retroarch_interactive_session.sh`;
- adapter support tests and anchored replay verification.

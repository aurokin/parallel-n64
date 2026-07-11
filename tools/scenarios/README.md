# Scenario Runners

Scenario runners turn fixture manifests into deterministic evidence bundles.
They are renderer validation tools, not a gameplay notebook or fleet
orchestrator.

## Runtime Contract

- Steady state: load an authoritative state, settle three frames, then capture.
- Bootstrap control exists to mint or replace an authority, not to redefine a
  steady-state test.
- Run emulator-facing scenarios serially under the adapter's runtime lock.
- Keep save data, options, logs, captures, and generated state bundle-local.
- Record the actual ROM, state, pack, core, frontend, and configuration
  identities.

Authority modes:

- `authoritative` uses the canonical fixture state.
- `bootstrap` follows the declared parent and deterministic input.
- `auto` prefers authority and falls back to bootstrap.

## Portable Inputs

Runners accept their inputs through existing arguments, fixture fields, and
environment overrides. Supply the RetroArch binary, core, ROM, states, pack,
and output root explicitly when repository-relative staging is unavailable.
Do not introduce host-specific defaults.

Use `RUNTIME_ENV_OVERRIDE=/absolute/path/to/runtime.env` only for a
temporary experiment. Evidence must record the resolved values.

## Active Entry Points

| Purpose | Entrypoint |
|---------|------------|
| Paper Mario title | `paper-mario-title-screen.sh` |
| Paper Mario file select | `paper-mario-file-select.sh` |
| Paper Mario `kmr_03 ENTRY_5` | `paper-mario-kmr-03-entry-5.sh` |
| Paper Mario PHRB authority set | `paper-mario-phrb-authority-validation.sh` |
| Full-cache runtime wrapper | `paper-mario-full-cache-phrb-authority-validation.sh` |
| SM64/OoT boot breadth | `cross-game-hires-boot-validation.sh` |
| SM64/OoT fixed-state breadth | `cross-game-hires-savestate-fixture-validation.sh` |
| GlideN64 content reference | `gliden64-reference-capture.sh` |

Remint helpers and bounded research probes live beside these entrypoints. Read a
script's usage before invoking it rather than treating the directory as an
ordered workflow.

## Evidence Contract

Every tracked bundle records:

- authority mode, graph node, and lineage;
- authoritative, bootstrap, and active state identities;
- ROM and pack identity;
- configuration and post-load settle count;
- logs, capture, fallback reporting, and class-level hit evidence;
- stable semantic traces where available.

Hi-res-on validation uses class-level semantics and explicit visual judgment.
Capture digests are reserved for feature-off parity and authority remints.

## Reminting

Use dedicated `remint-*` helpers. A replacement authority must pass
the locked feature-off verification before promotion; an unverified remint is
review output only.

## GlideN64 Reference

`gliden64-reference-capture.sh` reaches a comparable scene on the
reference core. `compose-reference-pair.py` produces labeled review
images. These are content judgments only: never add image-similarity metrics or
GlideN64 digest gates.

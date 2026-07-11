# Agent Instructions

## Start Here

- [README.md](README.md)
- [Documentation index](docs/README.md)
- [Architecture decisions](docs/adr/README.md)
- [Open technical issues](docs/ISSUE_LOG.md)

## Commands

| Task | Command |
|------|---------|
| Build core | `./run-build.sh` |
| Run one test | `./run-tests.sh -R '<test-regex>'` |
| Required gate | `./run-tests.sh --profile emu-required` |
| Runtime gate | `./run-tests.sh --profile emu-runtime-conformance` |
| Clean profile build | `./run-tests.sh --clean --profile <profile>` |
| Rebuild live ParaLLEl core | `make -j4 -B HAVE_PARALLEL=1 parallel_n64_libretro.so` |

No package manager is used.

## Scope

- Renderer/core correctness, fixtures, scenarios, adapters, and evidence live
  here.
- Generic frontend control belongs in RetroArch.
- Eval scoring, fleet scheduling, and gameplay/TAS research stay outside this
  repository.
- Do not add a dependency on a private workspace or a machine-specific host.

## Renderer Invariants

- With hi-res and scaling off, preserve upstream-grade behavior.
- Use capture digests only for feature-off parity and authority remint checks.
- For hi-res-on behavior, use class-level semantics plus explicit visual
  judgment; never exact metadata counts or image-similarity gates.
- GlideN64 is a source/key/content oracle, not a numeric image target.
- Do not add scene-specific or descriptor-specific renderer overrides.
- Treat hashes as artifact identity, not runtime correctness.
- Run emulator-facing tests at 4x, serially.

## Inputs And Output

- ROMs, packs, states, binaries, and captures are local inputs, not tracked
  source.
- Generated workflow output belongs under `artifacts/`.
- Accept paths through existing arguments or environment variables; do not
  document personal roots as defaults.

## Commit Attribution

AI commits must identify the actual agent and provider with a
`Co-Authored-By` trailer.

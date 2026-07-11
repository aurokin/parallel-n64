# Architecture Decision Records

ADRs contain durable renderer, validation, and tooling decisions. Current
commands and status live in the root documentation; experimental chronology
lives in Git history and evidence bundles.

Status values:

- **Accepted** — currently in force.
- **Accepted, amended** — in force with a later correction in the same record.
- **Historical** — explains the present architecture but no longer sequences
  work.

## Index

| ADR | Decision | Status |
|-----|----------|--------|
| [0001](0001-reboot-and-prior-attempt-closure.md) | Close failed attempts and restart from explicit invariants | Historical |
| [0002](0002-protected-feature-off-property.md) | Feature-off remains upstream-grade | Accepted |
| [0003](0003-architecture-boundaries.md) | Renderer and repository boundaries | Accepted, amended |
| [0004](0004-hires-on-validation-methodology.md) | Hi-res-on validation methodology | Accepted |
| [0005](0005-gliden64-oracle-policy.md) | GlideN64 oracle policy | Accepted |
| [0006](0006-replacement-identity.md) | Rice-compatible draw-time identity | Accepted |
| [0007](0007-pack-pipeline-phrb-only.md) | PHRB-only runtime pack pipeline | Accepted |
| [0008](0008-fixture-and-evidence-authority.md) | Fixture and evidence authority | Accepted |
| [0009](0009-interactive-adapter-and-deterministic-play.md) | Bounded stdin control and paused stepping | Accepted |
| [0010](0010-replacement-sampling-semantics.md) | Replacement sampling semantics | Accepted, amended |
| [0011](0011-texrect-rasterization-policy.md) | Replacement-aware texrect policy | Accepted |
| [0012](0012-gliden64-reference-rig-and-txdump.md) | Reference rig and txDump interpretation | Accepted |
| [0013](0013-compat-keying-refinements.md) | Compat keying refinements | Accepted |
| [0014](0014-orig-dims-display-view-rebase.md) | Display-view original dimensions | Accepted |
| [0015](0015-pack-content-curation-boundary.md) | Miss taxonomy and pack-curation boundary | Accepted, amended |
| [0016](0016-test-surface-trim.md) | Behavior-backed test surface | Accepted |
| [0017](0017-lab-repo-split.md) | Experimental session tooling stays external | Accepted, amended |
| [0018](0018-hires-composition-pack-contract.md) | HLE pack composition contract | Accepted |

## Decision Spine

Read [0002](0002-protected-feature-off-property.md),
[0003](0003-architecture-boundaries.md),
[0005](0005-gliden64-oracle-policy.md),
[0006](0006-replacement-identity.md), and
[0004](0004-hires-on-validation-methodology.md) first. Consult the remaining
records for the subsystem being changed.

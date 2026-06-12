# Architecture Decision Records

Decision records for the hi-res texture replacement and scaling program. Each ADR
captures one durable decision: what was decided, why, and what evidence backs it.
The running narrative lives in [PROJECT_NOTES.md](/home/auro/code/parallel-n64/PROJECT_NOTES.md);
ADRs are the distilled, citable form.

Status values: **Accepted** (in force), **Superseded** (replaced — the header names
the replacement). When a decision is amended rather than replaced, the amendment is
recorded in the same ADR with its date.

## Index

| ADR | Title | Status |
|-----|-------|--------|
| [0001](0001-reboot-and-prior-attempt-closure.md) | Close Attempts A and B; reboot under the Reboot Plan | Accepted |
| [0002](0002-protected-feature-off-property.md) | Protected property: feature-off stays upstream-grade | Accepted |
| [0003](0003-architecture-boundaries.md) | Architecture boundaries: four systems, one provider seam, VI authority | Accepted |
| [0004](0004-hires-on-validation-methodology.md) | Hi-res-ON validation: class semantics, visual rubric, falsification first | Accepted |
| [0005](0005-gliden64-oracle-policy.md) | GlideN64 oracle policy: three sanctioned uses, three hard bans | Accepted |
| [0006](0006-replacement-identity.md) | Replacement identity: GlideN64-compat Rice-CRC lane primary, strict identity | Accepted |
| [0007](0007-pack-pipeline-phrb-only.md) | Pack pipeline: PHRB-only runtime, offline conversion, exact-variant sets | Accepted |
| [0008](0008-fixture-and-evidence-authority.md) | Savestate ladder authority and evidence bundles | Accepted |
| [0009](0009-interactive-adapter-and-deterministic-play.md) | Interactive adapter: stdin FIFO, no daemons; paused-step determinism | Accepted |
| [0010](0010-replacement-sampling-semantics.md) | Replacement sampling: sub-texel fp5, direct sampling, CPU mips, trilinear | Accepted |
| [0011](0011-texrect-rasterization-policy.md) | Texrect policy: native snap by default, evidence-narrowed exemptions | Accepted |
| [0012](0012-gliden64-reference-rig-and-txdump.md) | GlideN64 reference rig; txDump output is the MISS set | Accepted |
| [0013](0013-compat-keying-refinements.md) | Compat keying refinements: TLUT base shadow, bank-0 palette candidate | Accepted |
| [0014](0014-orig-dims-display-view-rebase.md) | Replacement orig dims follow GlideN64's display view | Accepted |
| [0015](0015-pack-content-curation-boundary.md) | Pack-content defects are curation; three-class miss taxonomy | Accepted |
| [0016](0016-test-surface-trim.md) | Trimmed test surface: behavior-backed lanes, loud failures | Accepted |
| [0017](0017-lab-repo-split.md) | Session-lab tooling lives in the private parallel-n64-lab repo | Accepted |

## Decision spine (read in this order for onboarding)

1. ADR-0001 (why the project looks like this) → ADR-0002 (the non-negotiable)
2. ADR-0003 (how the program is decomposed: four systems, provider seam, VI authority)
3. ADR-0005 + ADR-0006 (how identity and the oracle work)
4. ADR-0004 (how anything gets validated)
5. The rest as needed per subsystem.

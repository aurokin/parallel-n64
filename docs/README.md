# Documentation

## Architecture

- [Architecture decisions](adr/README.md) — durable renderer and validation
  policy. Read the decision spine first.
- [Texture pack tracking](../assets/TEXTURE_PACKS.md) — source, format, and
  conversion provenance.
- [Component provenance](COMPONENT_PROVENANCE.md) — recovered import notices
  and unresolved source/distribution questions; see also [LICENSES.md](../LICENSES.md).

## Verification

- [Emulator testing](EMU_TESTING.md) — profiles, runtime prerequisites, and
  skip policy.
- [Open technical issues](ISSUE_LOG.md) — verified or bounded owner-repository
  follow-ups only.
- [Evidence retention](EVIDENCE_RETENTION.md) — compact reproducible cases,
  temporary recording closure and explicit coverage limits.

## Operational Components

- [Fixtures](../tools/fixtures/)
- [Scenarios](../tools/scenarios/)
- [Adapters](../tools/adapters/)
- [RetroArch patch set](../tools/retroarch-patches/)
- [GlideN64 txDump patch](../tools/gliden64-txdump-patch/)
- [Savestate bridge to mupen64plus-next](../tools/savestate_bridge.py)
- [Shader toolchain patches](../tools/parallel-rdp-toolchain-patches/)
- [Generated artifacts](../artifacts/)

Completed plans, experiment notebooks, eval design, private topology, and
generated evidence are intentionally not active documentation. Git history
retains prior decision context; ADRs retain the conclusions still in force.

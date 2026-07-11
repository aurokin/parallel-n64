# Texture Pack Sources And Conversion

Texture packs and generated packages are local, untracked inputs. This file
records portable source provenance and the conversion contract; it does not
record which private host currently stores an artifact.

## Format Contract

- Runtime input is `.phrb` only.
- `tools/hts2phrb.py` converts GlideN64/GLideNHQ `.hts` or
  `.htc` caches offline.
- Rice-format PNG directories require a separate GlideN64 cache-generation
  step before conversion.
- ROMs, pack archives, extracted caches, and generated packages are never
  committed.
- Record the source URL and archive SHA-256 before promoting a new source.

## Source Catalog

| Game | Source | Cache format | Source URL | Archive SHA-256 |
|------|--------|--------------|------------|----------------|
| Paper Mario | Paper Mario Redone HD 4.0.1 | old-version `.hts` | Unknown; acquisition predates this ledger | Unknown; recover before redistribution |
| Super Mario 64 | SM64 Reloaded 2.6.0 HD | GLideNHQ `.hts` | [download](https://evilgames.eu/files/texture-packs/sm64-reloaded-v2.6.0-gliden64-hts-hd.7z) | `c641cdd7ff590ac3e5fd5936ad7cbc8af139bbd392276e14871a81cf075873e6` |
| Ocarina of Time | OoT Reloaded 11.0.0 HD | GLideNHQ `.hts` | [release page](https://evilgames.eu/texture-packs/oot-reloaded.htm) | `7a68eaf94e62d2059cca011d7a03fefb98f1fbc923809ff8b96dbb45bfa267d2` |
| Mario Kart 64 | MK64 Reloaded 2026.04.03 HD | GLideNHQ `.hts` | [release page](https://evilgames.eu/texture-packs/mk64-reloaded.htm) | `de63c1d640aad8dbad7701b47b5ba38ba522262d92668d028766e5c388f10bd0` |
| Majora's Mask | MM Reloaded 11.0.2 HD | GLideNHQ `.hts` | [download](https://evilgames.eu/files/texture-packs/mm-reloaded-v11.0.2-gliden64-hts-hd.7z) | `23e96dce91a41c13861685b43b51f2a588458262c756be55eec78d43d0e5854d` |

Formats keyed for a PC port, Ship of Harkinian, Citra, or another console are
not interchangeable with GlideN64/Rice identities.

## Observed Package Selection

The checked-in behavior currently differs by consumer:

- `tools/scenarios/lib/common.sh` checks the path-stable
  `local-pm64-exact-variant-set` package first, then zero-config. The current
  staged package at that stable path is byte-identical to curated-r2
  (`sha256: 388bce41777a45eecd7581fc03bf67c7c36512cc7d1dd786ddfb38a90964d71f`),
  despite the path not carrying the curation suffix.
- The Paper Mario runtime-conformance wrapper defaults to that same stable
  exact-variant-set path.
- Fixture YAML files still declare the zero-config package.
- External callers may provide another package explicitly.

This describes the code and current staged identity as they exist; it is not
authorization to change a runtime default. Because curation is not encoded in
the stable path, every evidence bundle must record the package hash actually
used.

## Paper Mario Curation

Two tracked exclusion reviews describe known pack-content removals:

- `tools/hires_pack_curation_pm64_exclusions.json` excludes the
  `fbebebeb` transparency-grid placeholder.
- `tools/hires_pack_curation_pm64_jungle_exclusions.json` excludes
  the `816a81b8` and `23ff3d81` overlay pair.

Apply both reviews to a fresh exact-variant-set conversion:

```sh
python3 tools/hires_pack_apply_exclusion_review.py \
  --loader-manifest artifacts/hts2phrb-review/local-pm64-exact-variant-set/loader-manifest.json \
  --exclusion-review tools/hires_pack_curation_pm64_exclusions.json \
  --exclusion-review tools/hires_pack_curation_pm64_jungle_exclusions.json \
  --output-dir artifacts/hts2phrb-review/local-pm64-curated
```

Run conversion commands from the repository root so relative source paths
resolve consistently. Use `python3 tools/hts2phrb.py --help` for the
current converter interface.

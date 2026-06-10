# Texture Pack Tracking

Status of hi-res texture packs for validation titles. The converter
(`tools/hts2phrb.py`) ingests **`.hts` / `.htc` only** — prefer releases that
ship one of those. PNG-only (Rice-format folder) distributions need a
GlideN64-side cache-generation pass first, which requires a working GlideN64
reference emulator (see task: GlideN64 reference vehicle).

## Rules

- Record **source URL + sha256** for every archive here at acquisition time.
- Archive the original download (and extracted `.hts`) to
  `/pluto/game/texture_packs/n64/` as cold storage before using it.
- Working copies live in `assets/` for runtime use. Never point runtime
  scenarios at the NFS mount.
- ROMs are NOT tracked here — full No-Intro set at `/pluto/game/rom/n64`.

## Packs

| Game | Pack | Format | Status | Source URL | sha256 |
|------|------|--------|--------|------------|--------|
| Paper Mario (USA) | Paper Mario Redone HD v4.0.1 (MasterKillua) | `.hts` (443MB) | **ON DISK** — `assets/PAPER MARIO_HIRESTEXTURES.hts`; original archive `assets/PMRHD-401-NWO.rar` (contains only the same `.hts` + README, no PNG sources) | (unrecorded — predates this file) | `PMRHD-401-NWO.rar`: record on next touch |
| Super Mario 64 | SM64 Redrawn (preferred) or Render96 HD | want `.hts`/`.htc` | **NEEDED** — previously on disk and proven through hts2phrb (2,530 entries), deleted without recorded source | TBD | TBD |
| Zelda: Ocarina of Time | OoT Reloaded (Admentus) or Henriko Magnifico OoT HD (GlideN64 edition) | want `.hts`/`.htc` | **NEEDED** — previously on disk and proven (43K entries, 8.9GB PHRB, streaming load) | TBD | TBD |
| Zelda: Majora's Mask | Henriko Magnifico MM HD 4K (GlideN64 edition) | want `.hts`/`.htc` | **LATER** — deliberate very-large-pack stress test after SM64/OoT are green | TBD | TBD |
| Mario Kart 64 | community HD Rice packs | Rice PNG | **OPTIONAL** — 2D billboard/sprite coverage alternative | TBD | TBD |

## Converted runtime packages (PHRB)

| Source pack | Package | Status |
|-------------|---------|--------|
| Paper Mario Redone HD | `artifacts/hts2phrb-review/local-pm64-zero-config/package.phrb` (405MB, zero-config compat, built 2026-06-10) | **ACTIVE** — referenced by all Paper Mario fixtures |

PHRB packages are regenerable from their `.hts` via
`tools/hts2phrb.py` (zero-config); the `.hts` originals are the assets to protect.

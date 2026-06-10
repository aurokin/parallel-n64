# Texture Pack Tracking

Status of hi-res texture packs for validation titles. The converter
(`tools/hts2phrb.py`) ingests **`.hts` / `.htc` only** — prefer releases that
ship one of those. PNG-only (Rice-format folder) distributions need a
GlideN64-side cache-generation pass first, which requires a working GlideN64
reference emulator (see task: GlideN64 reference vehicle). Both `.hts`
layouts are supported: old-version (config word at offset 0, e.g. Paper
Mario's `0x40a20000`) and new-version GLideNHQ (`TXCACHE_FORMAT_VERSION`
`0x08000000` at offset 0, config at offset 4 — all GhostlyDark Reloaded
packs).

## Rules

- Record **source URL + sha256** for every archive here at acquisition time.
- Archive the original download (and extracted `.hts`) to
  `/pluto/game/texture_packs/n64/` as cold storage before using it.
  (`/pluto` is currently mounted read-only on this box; originals remain on
  `koopa:/Users/auro/Downloads/` as interim cold storage until a writable
  path is available.)
- Working copies live in `assets/packs/` for runtime use. Never point runtime
  scenarios at the NFS mount.
- ROMs are NOT tracked here — full No-Intro set at `/pluto/game/rom/n64`;
  local runtime copies of SM64/OoT/MK64/MM (USA) staged in `assets/`.

## Packs

| Game | Pack | Format | Status | Source URL | sha256 |
|------|------|--------|--------|------------|--------|
| Paper Mario (USA) | Paper Mario Redone HD v4.0.1 (MasterKillua) | `.hts` (443MB) | **ON DISK** — `assets/PAPER MARIO_HIRESTEXTURES.hts`; original archive `assets/PMRHD-401-NWO.rar` (contains only the same `.hts` + README, no PNG sources) | (unrecorded — predates this file) | `PMRHD-401-NWO.rar`: record on next touch |
| Super Mario 64 | SM64 Reloaded v2.6.0 (GhostlyDark) HD | `.hts` (7z) | **DOWNLOADING** — `assets/packs/sm64-reloaded-v2.6.0-gliden64-hts-hd.7z`; record sha256 + entry count on completion | https://evilgames.eu/files/texture-packs/sm64-reloaded-v2.6.0-gliden64-hts-hd.7z | pending |
| Zelda: Ocarina of Time | OoT Reloaded v11.0.0 (GhostlyDark) HD | `.hts` (1.3GB 7z → 9.4GB) | **ON DISK** — `assets/packs/oot-reloaded-hts/THE LEGEND OF ZELDA_HIRESTEXTURES.hts`; 43,324 entries parsed (matches the previously-proven 43K-entry pack) | https://evilgames.eu/texture-packs/oot-reloaded.htm (fetched on koopa) | `7a68eaf94e62d2059cca011d7a03fefb98f1fbc923809ff8b96dbb45bfa267d2` |
| Zelda: Majora's Mask | MM Reloaded v11.0.2 (GhostlyDark) HD | `.hts` (7z) | **DOWNLOADING** — `assets/packs/mm-reloaded-v11.0.2-gliden64-hts-hd.7z`; this replaces the missing "MM GlideN64 edition" the plan was waiting on | https://evilgames.eu/files/texture-packs/mm-reloaded-v11.0.2-gliden64-hts-hd.7z | pending |
| Mario Kart 64 | MK64 Reloaded v2026.04.03 (GhostlyDark) HD | `.hts` (397MB 7z → 3.6GB) | **ON DISK** — `assets/packs/mk64-reloaded-hts/MARIOKART64_HIRESTEXTURES.hts`; 20,212 entries parsed | https://evilgames.eu/texture-packs/mk64-reloaded.htm (fetched on koopa) | `de63c1d640aad8dbad7701b47b5ba38ba522262d92668d028766e5c388f10bd0` |

## Rejected / inapplicable acquisitions

| Artifact | Verdict |
|----------|---------|
| `assets/packs/sm64redrawn-master.pak` (203MB, sha256 `4955ceaeb346c95136de1f4132ecbd543dff7d849aa783323020501ff420dfa3`) | **NOT INGESTIBLE** — sm64nx (Switch port) pack from the TechieAndroid/sm64redrawn project; textures keyed by sm64ex asset paths, not N64 checksums. The repo's PNGs target `res/gfx` of the PC port, so neither hts2phrb nor the GlideN64 mint-an-hts vehicle can use them. SM64 Reloaded (above) covers SM64 instead. |
| `koopa:/Users/auro/Downloads/MM 3D 4K 3.0b-1 (1080p)[.zip]` (1.7GB) | **NOT APPLICABLE** — Citra texture pack for Majora's Mask **3D** (3DS): `user/load/textures/<3DS title id>/` layout, `tex1_<WxH>_<hash>_<fmt>` naming, ReShade payload. Not an N64 pack. MM Reloaded (above) covers MM instead. |
| `koopa:/Users/auro/Downloads/oot-reloaded-v11.0.0-soh-o2r-hd.7z` (626MB) | **NOT NEEDED** — Ship of Harkinian `.o2r` resource archive (asset-path keyed); superseded by the completed GlideN64 `.hts` download of the same pack version. Left on koopa. |

## Converted runtime packages (PHRB)

| Source pack | Package | Status |
|-------------|---------|--------|
| Paper Mario Redone HD | `artifacts/hts2phrb-review/local-pm64-zero-config/package.phrb` (405MB, zero-config compat, built 2026-06-10) | **ACTIVE** — referenced by all Paper Mario fixtures |
| MK64 Reloaded HD | (pending conversion) | queued |
| OoT Reloaded HD | (pending conversion) | queued |

PHRB packages are regenerable from their `.hts` via
`tools/hts2phrb.py` (zero-config); the `.hts` originals are the assets to protect.

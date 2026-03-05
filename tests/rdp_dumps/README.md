# RDP Dump Corpus (Local)

This directory is the default local corpus location for `emu.dump.*` tests.

- Committed files: only `.gitkeep` and this README.
- Local dump files (`*.rdp`) are intentionally ignored by Git.

Quick flow:

```bash
./run-dump-tests.sh --provision-validator --capture-if-missing
```

This will:

1. Build `rdp-validate-dump` if it is not already available.
2. Capture one Angrylion dump if the corpus directory is empty.
3. Run `ctest -R emu.dump` through `run-tests.sh`.

Requirements:

- `parallel_n64_libretro.so` built with `HAVE_RDP_DUMP=1`
- RetroArch binary at `/home/auro/code/mupen/RetroArch-upstream/retroarch`
- ROM available at `/home/auro/code/n64_roms/Paper Mario (USA).zip` (default capture target)

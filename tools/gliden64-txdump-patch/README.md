# GlideN64 txDump patch (hash-coverage oracle)

One-line, env-gated patch for the mupen64plus-next clone at
`/home/auro/code/mupen64plus-libretro-nx` (base commit `98c1b0d`): setting
`GLN64_TXDUMP=1` in the core's environment turns on GLideNHQ texture dumping.

Rebuild:

```sh
cd /home/auro/code/mupen64plus-libretro-nx
git apply /home/auro/code/parallel-n64/tools/gliden64-txdump-patch/0001-env-gated-txdump.patch
make -j"$(nproc)"
# core: mupen64plus_next_libretro.so
```

Why: dumped filenames are GlideN64's own Rice CRC identities per drawn
texture — `<ROM NAME>#<texture_crc>#<fmt>#<size>[#<palette_crc>]_*.png` —
which makes the dump directory a coverage oracle for our GlideN64-compat
CRC lane: every checksum GlideN64 dumps for a scene is a checksum our
compat lane should be able to hit. Use it to diagnose upload/draw misses;
never as a pixel-accuracy target.

Verified live (2026-06-10): `GLN64_TXDUMP=1` with
`tools/scenarios/gliden64-reference-capture.sh --core <patched core>` dumps
to `<bundle>/system/Mupen64plus/texture_dump/<ROM NAME>/GLideNHQ/`.
Without the env var, behavior is unchanged.

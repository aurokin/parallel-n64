# GlideN64 txDump Patch

`0001-env-gated-txdump.patch` adds an opt-in
`GLN64_TXDUMP=1` switch to a mupen64plus-next checkout. It is a
hash-coverage oracle for the Rice-compatible identity lane, never a
pixel-accuracy target.

Set `MUPEN64PLUS_NEXT_ROOT` to a checkout based on commit
`98c1b0d`:

```sh
PATCH="$PWD/tools/gliden64-txdump-patch/0001-env-gated-txdump.patch"
git -C "$MUPEN64PLUS_NEXT_ROOT" apply --check "$PATCH"
git -C "$MUPEN64PLUS_NEXT_ROOT" apply "$PATCH"
make -C "$MUPEN64PLUS_NEXT_ROOT" -j"$(nproc)"
```

If forward `--check` fails, use `git apply --reverse --check`
to distinguish an already-applied patch from an incompatible checkout.

Run [`gliden64-reference-capture.sh`](../scenarios/gliden64-reference-capture.sh)
with the patched core and `GLN64_TXDUMP=1`. Dumps appear under the
bundle's RetroArch system directory.

Interpretation is deliberately narrow:

- with a pack, txDump output is GlideN64's miss set;
- without a pack, it records computed Rice identities;
- a dump match does not prove what GlideN64 served.

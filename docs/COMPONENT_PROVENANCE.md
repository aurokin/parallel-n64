# Recovered component provenance

Checked September 29, 2026 against source `45b754769fc7367e68a0062f874b2378dfa978ff`.
This records actual inherited declarations and restores missing notices. It
does not select an aggregate license, establish contributor rights or clear
binary redistribution. The three directories below have identical tracked
bytes at earlier audit source `8618dda61491907059e56a7fc4f5a9da646167ce`.

## Angrylion

The June 2014 import `5fb951a80034a0124c32bc63f6f30cc13378761a` included
credits and old MAME terms. Rename `0c9a8a51` retained them; replacement
`894ad160e3adf1256da3e443b2eac398f7195c77` deleted them in November 2018.
Notice deletion is not evidence of relicensing.

The restored `MAME License.txt` is exact historical Git blob
`622420419a88127f87c8ad8eadd96ce5c9293312`, SHA-256
`455a30b92aba9a8e17702e6458dae6ca78b7e90b83800182fba9c4285ec2d385`.
It matches the [pinned Plus upstream terms](https://github.com/ata4/angrylion-rdp-plus/blob/79809245115d7c29c07a8b4365c28a720c8b4e4c/MAME%20License.txt).
Restored `CREDITS.txt` is that upstream revision's blob
`74844dfd7c86b7d87cfae71cb87a082a7c1975fc`, SHA-256
`300b078c472496f83be57219395d8da427efcf2ac7e4ad60e8ea7c74ae7b6113`;
it retains the original attribution and adds the upstream helper credit.

These terms include noncommercial, complete-source and notice conditions.
Do not label this imported code GPL, MIT or public domain by adjacency.
Compatibility with other program components/builds remains unresolved.

Historical threaded import `755b8f82` has 41 same-path blobs matching Plus
`bcd99b8964623e6d42483cf814f5e41e6a761fcf`; core import `118cdfb8` has 21
matching Plus `79809245115d7c29c07a8b4365c28a720c8b4e4c` blobs. Later
files include local adaptations, including the thread scheduler. These are
compatible pinned snapshots, not uniquely proven upstream import tips or
grants for every later contribution. The first rasterizer/HatCat revisions
and a complete rights map remain unresolved.

## Glide API and renderer

Import `fce0b4277962bc0e6bc7227b40a3f29c13ea85c9` included the 3dfx 1999
notice in `Glitch64/inc/glide.h`; `dada61d1547f5742d4ae59cf5c4702243e9a95f7`
removed it while adapting the header. The original header blob
`f79f5cf072c51741f92af46096bdaae730c1abc8` matches the [pinned AE source](https://github.com/mupen64plus-ae/mupen64plus-ae/blob/6e357dd040ec13f050db3f8d57b79d5b20f53e15/jni/gles2glide64/src/Glitch64/inc/glide.h).
The original notice is restored at the current header's beginning and retained
in `glide2gl/NOTICE.glide`; implementation bytes are unchanged.

`glide2gl/LICENSE.glide` preserves the named 3DFX GLIDE Source Code General
Public License from [sezero/glide revision 2f226f0f](https://github.com/sezero/glide/blob/2f226f0f9225ce8ee83e6a4a7042981e719d19ee/LICENSE),
SHA-256 `a65ccd270f7b418cbe3afce7ca385560d4dc87adeff3c55dcd717c425c320161`.
This corroborates the declaration named by the imported header; it does not
prove that this complete text was bundled in the original AE import. Keep it
separate from GNU GPL and FXT1 compression terms. Its full requirements and
local modifications still need distribution review.

Headed renderer sources declare GPL-2-or-later. Several unheaded files are
splits of those sources: Ini (`51f8626f`), UCode (`f3dd85eb`), TexLoad
(`6a97249d`), and gDP/gSP interfaces. `ucode_f3dtexa.h` explicitly adapts
Project64 commit `03d86887d1a6d1e3c3cc4e45f6406e5596e5f8cd`, whose donor
source declares GPL-2-or-later. Refactoring does not remove inherited terms.

All three help HTML documents exactly match the historical AE snapshot above;
their document-specific reuse grants remain unresolved. `todo!.txt` has the
same import lineage plus local changes, without an independently established
text grant. Component adjacency does not resolve these exceptions.

## Rice exceptions

`RiceVideoLinux.ini` exactly matches upstream
`11410e62c06c4e913d1a032e342fbf61b1920ac3`; that component's LICENSES declares
GPL version 2, whereas headed code often declares GPL-2-or-later. Preserve
this distinction instead of assigning one blanket identifier.

`gDP_rice.cpp` (`1bc607fe`) and `gSP_rice.cpp` (`a52dcaeb`) relocate code from
headed GPL-2-or-later Rice sources. `arm_features.h` entered in `a5d229fa`,
whose message attributes notaz optimizations; its exact external source
revision and grant remain unresolved. Git attribution alone does not settle
all contribution rights.

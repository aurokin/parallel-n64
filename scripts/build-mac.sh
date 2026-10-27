#!/bin/bash
sh ./scripts/clean.sh
sh ./scripts/build-mac-intel.sh
mv ./paralleish_libretro.dylib ./paralleish_libretro_x64.dylib

sh ./scripts/clean.sh
sh ./scripts/build-mac-arm.sh
mv ./paralleish_libretro.dylib ./paralleish_libretro_arm64.dylib

lipo -create -output ./paralleish_libretro.dylib ./paralleish_libretro_x64.dylib ./paralleish_libretro_arm64.dylib
rm ./paralleish_libretro_x64.dylib
rm ./paralleish_libretro_arm64.dylib

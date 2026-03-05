#pragma once

#include "rdp_common.hpp"
#include "rdp_data_structures.hpp"

namespace RDP::detail
{
inline void decode_triangle_setup_words(TriangleSetup &setup,
                                        const uint32_t *words,
                                        bool copy_cycle,
                                        bool native_texture_lod)
{
	const bool flip = (words[0] & 0x800000u) != 0;
	const bool sign_dxhdy = (words[5] & 0x80000000u) != 0;
	const bool do_offset = flip == sign_dxhdy;

	setup.flags |= flip ? TRIANGLE_SETUP_FLIP_BIT : 0;
	setup.flags |= do_offset ? TRIANGLE_SETUP_DO_OFFSET_BIT : 0;
	setup.flags |= copy_cycle ? TRIANGLE_SETUP_SKIP_XFRAC_BIT : 0;
	setup.flags |= native_texture_lod ? TRIANGLE_SETUP_NATIVE_LOD_BIT : 0;

	setup.tile = (words[0] >> 16) & 63;

	setup.yl = sext<14>(words[0]);
	setup.ym = sext<14>(words[1] >> 16);
	setup.yh = sext<14>(words[1]);

	// Lower edge bit is ignored by hardware. Shift to preserve one extra subpixel bit.
	setup.xl = sext<28>(words[2]) >> 1;
	setup.xh = sext<28>(words[4]) >> 1;
	setup.xm = sext<28>(words[6]) >> 1;
	setup.dxldy = sext<28>(words[3] >> 2) >> 1;
	setup.dxhdy = sext<28>(words[5] >> 2) >> 1;
	setup.dxmdy = sext<28>(words[7] >> 2) >> 1;
}
}

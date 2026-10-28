// SPDX-License-Identifier: MIT

#ifndef HIRES_CUTOUT_POLICY_H_
#define HIRES_CUTOUT_POLICY_H_

// A one-cycle decal uses texture alpha to paint over SHADE, while its
// independent output alpha controls the whole surface. An empty decal must
// retain SHADE rather than punch a hole. Keep other pack cutouts unchanged;
// multiple samples/cycles need individual dependency tracking before they
// can safely share this exception. Uses the normalized mux selectors.
bool hires_replacement_cutout_kills_pixel(bool cutout, bool multi_cycle,
                                         bool secondary_sample, bool convert_one,
                                         u8x4 rgb, u8x4 alpha)
{
	bool shade_decal = rgb.x == RGB_MULADD_TEXEL0 &&
	                   rgb.y == RGB_MULSUB_SHADE &&
	                   rgb.z == RGB_MUL_TEXEL0_ALPHA &&
	                   rgb.w == RGB_ADD_SHADE;
	bool independent_alpha = alpha.x > ALPHA_ADDSUB_TEXEL1_ALPHA &&
	                         alpha.y > ALPHA_ADDSUB_TEXEL1_ALPHA &&
	                         alpha.z != ALPHA_MUL_TEXEL0_ALPHA &&
	                         alpha.z != ALPHA_MUL_TEXEL1_ALPHA &&
	                         alpha.w > ALPHA_ADDSUB_TEXEL1_ALPHA;
	return cutout && (multi_cycle || secondary_sample || convert_one ||
	                  !shade_decal || !independent_alpha);
}

#endif

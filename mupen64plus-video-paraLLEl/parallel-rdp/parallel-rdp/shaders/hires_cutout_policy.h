// SPDX-License-Identifier: MIT

#ifndef HIRES_CUTOUT_POLICY_H_
#define HIRES_CUTOUT_POLICY_H_

// A one-cycle decal uses texture alpha to paint over SHADE, while its
// independent output alpha controls the whole surface. An empty decal must
// retain SHADE rather than punch a hole. Keep other pack cutouts unchanged.
// Other composition classes have their own dependency checks below. Uses
// the normalized mux selectors.
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

// An opaque two-cycle material can use texture RGB as an intensity, then
// light the resulting color with SHADE. At zero intensity its constant color
// bias still describes a surface; replacement alpha must not punch through it.
// Limit this to one RGB sample, a pure shade modulation, and constant ONE
// output alpha. Product-only effect quads keep the pack cutout contract.
bool hires_shaded_intensity_preserves_surface(bool multi_cycle, bool secondary_sample, bool convert_one,
                                             u8x4 rgb0, u8x4 rgb1, u8x4 alpha1,
                                             u8x4 bias0, u8x4 sub1, u8x4 bias1)
{
	return multi_cycle && !secondary_sample && !convert_one &&
	       rgb0.x == RGB_MULADD_CONSTANT && rgb0.y == RGB_MULSUB_CONSTANT &&
	       rgb0.z == RGB_MUL_TEXEL0 && rgb0.w == RGB_ADD_CONSTANT &&
	       (bias0.x != 0 || bias0.y != 0 || bias0.z != 0) &&
	       rgb1.x == RGB_MULADD_COMBINED && rgb1.y == RGB_MULSUB_CONSTANT &&
	       rgb1.z == RGB_MUL_SHADE && rgb1.w == RGB_ADD_CONSTANT &&
	       sub1.x == 0 && sub1.y == 0 && sub1.z == 0 &&
	       bias1.x == 0 && bias1.y == 0 && bias1.z == 0 &&
	       // Equal normalized constant selectors can hold different values.
	       alpha1.x == ALPHA_ADDSUB_ONE && alpha1.y == ALPHA_ADDSUB_ONE &&
	       alpha1.w == ALPHA_ADDSUB_ONE;
}

#endif

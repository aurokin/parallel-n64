// SPDX-License-Identifier: MIT

#ifndef HIRES_ADDRESSING_H_
#define HIRES_ADDRESSING_H_

// Replacement images are addressed in their own texel space. Native N64
// addressing (clamp to the last texel's origin, wrap/mirror per native texel)
// would hold or skip whole runs of replacement texels: a clamped strip freezes
// every replacement row inside its last native texel, and a wrap taken before
// scaling clamps bilinear taps at the image edge instead of repeating it.
// Scalar int/float arithmetic keeps these helpers usable by both GLSL and the
// display-free C++ test.

const int HIRES_ADDRESS_CLAMP = 0;
const int HIRES_ADDRESS_WRAP = 1;
const int HIRES_ADDRESS_MIRROR = 2;

// Native masking repeats a tile every (1 << mask_bits) texels. A clamp bit
// stops that repeat only when the tile extent fits within one period;
// otherwise the hardware still wraps inside the clamped range.
int hires_address_mode(int mask_bits, bool clamp_bit, bool mirror_bit, int extent_texels)
{
	if (mask_bits == 0)
		return HIRES_ADDRESS_CLAMP;
	if (clamp_bit && extent_texels <= (1 << mask_bits))
		return HIRES_ADDRESS_CLAMP;
	return mirror_bit ? HIRES_ADDRESS_MIRROR : HIRES_ADDRESS_WRAP;
}

// Tile extent in native texels from 10.2 SetTileSize bounds.
int hires_address_extent(int lo, int hi)
{
	return (hi >> 2) - (lo >> 2) + 1;
}

// Replacement texels per native repeat period at one mip level. Draw-time
// rebasing keys orig_dim to the mask period whenever a tile wraps inside its
// extent, so the period is exact there; rounding only matters for coordinates
// past a narrower tile's own extent, where native sampling reads unloaded TMEM.
int hires_address_period(int mask_bits, int level_dim, int orig_dim)
{
	if (mask_bits == 0 || orig_dim <= 0)
		return level_dim;
	int period = int(float(1 << mask_bits) * float(level_dim) / float(orig_dim) + 0.5f);
	return period > 1 ? period : 1;
}

// Replacement-space position of a tile-relative native coordinate (s10.5,
// after tile shift, before the N64 clamp). texel_origin moves native texel i's
// footprint to start at i, and bias selects the tap convention (0.5 for
// bilinear texel centers, 0 for nearest). A clamped tile holds the replacement
// texels of its first and last native texels: the N64 clamp pins coordinates to
// the last texel's origin, which would freeze every replacement row inside it,
// and the next repeat's origin would wrap back to the first texel.
float hires_address_position(int coord_fp5, float texel_origin, float scale, float bias,
                             bool clamp_bit, int extent_texels)
{
	float position = (float(coord_fp5) * (1.0f / 32.0f) + texel_origin) * scale - bias;
	if (clamp_bit)
	{
		float last = float(extent_texels) * scale - 1.0f;
		last = last > 0.0f ? last : 0.0f;
		position = position < 0.0f ? 0.0f : (position > last ? last : position);
	}
	return position;
}

// Tap position for one axis of a replaced draw. native_bilinear selects the
// N64 texel footprint convention (bilinear centers on integers, point covers
// [i, i + 1)); linear_filter selects bilinear taps or a nearest fetch.
float hires_tap_position(int coord_fp5, bool native_bilinear, bool linear_filter, float scale,
                         bool clamp_bit, int extent_texels)
{
	return hires_address_position(coord_fp5, native_bilinear ? 0.5f : 0.0f, scale,
	                              linear_filter ? 0.5f : 0.0f, clamp_bit, extent_texels);
}

// Resolve one replacement texel index. GLSL leaves % undefined for negative
// operands, so negative indices are lifted by whole periods first.
int hires_address_texel(int texel, int level_dim, int period, int mode)
{
	if (mode != HIRES_ADDRESS_CLAMP)
	{
		int span = mode == HIRES_ADDRESS_MIRROR ? 2 * period : period;
		if (texel < 0)
			texel += span * ((span - 1 - texel) / span);
		texel %= span;
		if (mode == HIRES_ADDRESS_MIRROR && texel >= period)
			texel = span - 1 - texel;
	}
	return texel < 0 ? 0 : (texel > level_dim - 1 ? level_dim - 1 : texel);
}

#endif

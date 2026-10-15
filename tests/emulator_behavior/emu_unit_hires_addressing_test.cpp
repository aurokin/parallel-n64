#include <cmath>
#include <cstdlib>
#include <iostream>

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders/hires_addressing.h"

namespace
{
void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

void check_texel(int texel, int level_dim, int period, int mode, int expected, const char *message)
{
	const int actual = hires_address_texel(texel, level_dim, period, mode);
	if (actual != expected)
	{
		std::cerr << "FAIL: " << message << " texel " << texel << " expected " << expected
		          << " got " << actual << '\n';
		std::exit(1);
	}
}

// Bilinear tap pair of a native bilinear draw, composed as texture.h does.
void bilinear_taps(int coord_fp5, float scale, int level_dim, int period, int mode, int &p0, int &p1)
{
	const float position = hires_tap_position(coord_fp5, true, true, scale, false, level_dim);
	const int base = int(std::floor(position));
	p0 = hires_address_texel(base, level_dim, period, mode);
	p1 = hires_address_texel(base + 1, level_dim, period, mode);
}

// Nearest replacement texel of a point-sampled draw (texel i covers [i, i + 1)).
int point_texel(int coord_fp5, float scale, bool clamp_bit, int extent, int level_dim, int period, int mode)
{
	const float position = hires_tap_position(coord_fp5, false, false, scale, clamp_bit, extent);
	return hires_address_texel(int(std::floor(position)), level_dim, period, mode);
}
}

int main()
{
	check(hires_address_mode(0, false, false, 32) == HIRES_ADDRESS_CLAMP, "unmasked axis clamps");
	check(hires_address_mode(5, true, false, 32) == HIRES_ADDRESS_CLAMP,
	      "clamp bit within one period clamps");
	check(hires_address_mode(5, true, false, 64) == HIRES_ADDRESS_WRAP,
	      "clamp bit beyond one period still wraps inside the range");
	check(hires_address_mode(5, false, true, 32) == HIRES_ADDRESS_MIRROR, "mirror bit mirrors");

	check(hires_address_extent(0, 31 * 4) == 32 && hires_address_extent(2 * 4, 9 * 4 + 3) == 8,
	      "extent counts whole texels between 10.2 SetTileSize bounds");

	// Footprint conventions at 4x: native texel 3 spans replacement texels 12..15.
	// Point sampling covers [3, 4); bilinear centers it on 3 and blends with
	// its neighbors outside [2.5, 3.5).
	check(hires_tap_position(3 * 32, false, false, 4.0f, false, 32) == 12.0f,
	      "point-sampled nearest fetch starts the texel's block at its origin");
	check(hires_tap_position(3 * 32 + 16, true, true, 4.0f, false, 32) == 15.5f,
	      "bilinear taps reach the block edge half a native texel past the center");
	check(hires_tap_position(3 * 32, true, true, 4.0f, false, 32) == 13.5f,
	      "bilinear taps center the native texel on its replacement block");
	check(hires_tap_position(3 * 32, false, true, 4.0f, false, 32) == 11.5f,
	      "filtered point-sampled draws center taps on replacement texels");

	check(hires_address_period(5, 256, 32) == 256, "32-texel period scales to the 256-texel image");
	check(hires_address_period(5, 128, 32) == 128, "period follows the mip level");
	check(hires_address_period(5, 256, 64) == 128, "period smaller than the keyed image");
	check(hires_address_period(0, 200, 50) == 200, "unmasked period is the image");

	check_texel(256, 256, 256, HIRES_ADDRESS_WRAP, 0, "wrap past the right edge");
	check_texel(-1, 256, 256, HIRES_ADDRESS_WRAP, 255, "wrap before the left edge");
	check_texel(-257, 256, 256, HIRES_ADDRESS_WRAP, 255, "wrap several periods negative");
	check_texel(256, 256, 256, HIRES_ADDRESS_CLAMP, 255, "clamp past the right edge");
	check_texel(-3, 256, 256, HIRES_ADDRESS_CLAMP, 0, "clamp before the left edge");
	const int mirrored[] = { 0, 1, 2, 3, 3, 2, 1, 0, 0, 1 };
	for (int i = 0; i < 10; i++)
		check_texel(i, 4, 4, HIRES_ADDRESS_MIRROR, mirrored[i], "mirror alternates direction");
	check_texel(-1, 4, 4, HIRES_ADDRESS_MIRROR, 0, "mirror reflects before the left edge");
	check_texel(-5, 4, 4, HIRES_ADDRESS_MIRROR, 3, "mirror reflects several periods negative");

	// SM64 Hazy Maze Cave: a 32-texel wrapping tile with a 256-texel
	// replacement. Taps across the native wrap continue at the image start;
	// native-space wrap clamped them to the last column.
	int p0 = 0, p1 = 0;
	bilinear_taps(31 * 32 + 24, 8.0f, 256, 256, HIRES_ADDRESS_WRAP, p0, p1);
	check(p0 == 1 && p1 == 2, "taps past the wrap boundary continue at the image start");
	bilinear_taps(31 * 32 + 30, 8.0f, 256, 256, HIRES_ADDRESS_WRAP, p0, p1);
	check(p0 == 3 && p1 == 4, "wrap keeps advancing through the image start");
	bilinear_taps(-16, 8.0f, 256, 256, HIRES_ADDRESS_WRAP, p0, p1);
	check(p0 == 255 && p1 == 0, "wrap blends the last and first columns at the seam");

	// Paper Mario title logo: a clamped 2-row point-sampled strip with an
	// 8-row replacement. The N64 clamp pins the second row to its origin;
	// replacement addressing must still step through all four of its rows.
	for (int quarter = 0; quarter < 8; quarter++)
		check(point_texel(quarter * 8, 4.0f, true, 2, 8, 8, HIRES_ADDRESS_CLAMP) == quarter,
		      "clamped strip advances one replacement row per quarter texel");
	check(point_texel(3 * 32, 4.0f, true, 2, 8, 8, HIRES_ADDRESS_CLAMP) == 7,
	      "clamped strip holds its last replacement row past the extent");
	check(point_texel(-32, 4.0f, true, 2, 8, 8, HIRES_ADDRESS_CLAMP) == 0,
	      "clamped strip holds its first replacement row before the origin");
	check(hires_tap_position(-32, false, false, 1.0f, false, 2) == -1.0f,
	      "unclamped positions pass through for wrap/mirror addressing");

	// A clamped tile spanning two 32-texel repeats still wraps inside its
	// range, but past the extent it must hold the final repeat's last texel
	// rather than wrap to the next repeat's origin.
	check(hires_address_mode(5, true, false, 64) == HIRES_ADDRESS_WRAP, "multi-repeat clamp wraps inside");
	check(point_texel(40 * 32, 1.0f, true, 64, 32, 32, HIRES_ADDRESS_WRAP) == 8,
	      "multi-repeat clamp wraps inside its range");
	check(point_texel(70 * 32, 1.0f, true, 64, 32, 32, HIRES_ADDRESS_WRAP) == 31,
	      "multi-repeat clamp holds the final repeat's last texel past the extent");
	check(point_texel(64 * 32, 4.0f, true, 64, 128, 128, HIRES_ADDRESS_WRAP) == 127,
	      "multi-repeat clamp holds the last replacement texel at the exact extent");
	const float edge = hires_tap_position(70 * 32, true, true, 1.0f, true, 64);
	check(edge == 63.0f, "bilinear clamp centers on the last texel without a neighbor weight");
	check(hires_tap_position(0, true, true, 0.25f, true, 2) == 0.0f,
	      "a minified clamp limit cannot fall below the first texel");

	std::cout << "emu_unit_hires_addressing_test: PASS\n";
	return 0;
}

#include <cstdlib>
#include <iostream>

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_common.hpp"

// Supply GLSL's mux value type and selectors from the renderer's C++ enums;
// the included production header owns the fragment rejection decision.
struct u8x4
{
	int x, y, z, w;
};

constexpr int RGB_MULADD_TEXEL0 = static_cast<int>(RDP::RGBMulAdd::Texel0);
constexpr int RGB_MULSUB_SHADE = static_cast<int>(RDP::RGBMulSub::Shade);
constexpr int RGB_MUL_TEXEL0_ALPHA = static_cast<int>(RDP::RGBMul::Texel0Alpha);
constexpr int RGB_ADD_SHADE = static_cast<int>(RDP::RGBAdd::Shade);
constexpr int ALPHA_ADDSUB_TEXEL1_ALPHA = static_cast<int>(RDP::AlphaAddSub::Texel1Alpha);
constexpr int ALPHA_MUL_TEXEL0_ALPHA = static_cast<int>(RDP::AlphaMul::Texel0Alpha);
constexpr int ALPHA_MUL_TEXEL1_ALPHA = static_cast<int>(RDP::AlphaMul::Texel1Alpha);

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/shaders/hires_cutout_policy.h"

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
}

int main()
{
	// Captured normalized one-cycle Boo eye/mouth mux: 01040804.07070705.
	// Transparent decal texels must leave the shaded white face present.
	const u8x4 decal = { 1, 4, 8, 4 };
	const u8x4 environment_alpha = { 7, 7, 7, 5 };
	check(!hires_replacement_cutout_kills_pixel(true, false, false, false, decal, environment_alpha),
	      "empty decal must retain its shaded surface");
	check(!hires_replacement_cutout_kills_pixel(false, false, false, false, decal, environment_alpha),
	      "nonempty decal must remain present");

	// Ordinary sprite/effect RGB reads the texture directly, so an authored
	// empty repaint remains a cutout even when alpha comes from SHADE.
	const u8x4 sprite = { 15, 15, 31, 1 };
	const u8x4 shade_alpha = { 7, 7, 7, 4 };
	check(hires_replacement_cutout_kills_pixel(true, false, false, false, sprite, shade_alpha),
	      "empty sprite repaint must still leave the background untouched");
	check(!hires_replacement_cutout_kills_pixel(false, false, false, false, sprite, shade_alpha),
	      "native or nonempty sprite samples must not be rejected");
	u8x4 changed_decal = decal;
	changed_decal.w = 7;
	check(hires_replacement_cutout_kills_pixel(true, false, false, false, changed_decal, environment_alpha),
	      "multiplication without a shaded fallback must remain a cutout");
	changed_decal = decal;
	changed_decal.z = 9;
	check(hires_replacement_cutout_kills_pixel(true, false, false, false, changed_decal, environment_alpha),
	      "a different texture's alpha must not exempt an empty sample");

	for (int selector : { 1, 2 })
	{
		for (unsigned slot = 0; slot < 4; slot++)
		{
			u8x4 alpha = environment_alpha;
			int *fields[] = { &alpha.x, &alpha.y, &alpha.z, &alpha.w };
			*fields[slot] = selector;
			check(hires_replacement_cutout_kills_pixel(true, false, false, false, decal, alpha),
			      "texture-dependent output alpha must retain cutout behavior");
		}
	}
	check(hires_replacement_cutout_kills_pixel(true, true, false, false, decal, environment_alpha),
	      "two-cycle dependencies must not receive the one-cycle exception");
	check(hires_replacement_cutout_kills_pixel(true, false, true, false, decal, environment_alpha),
	      "an empty secondary sample must not be mistaken for the decal mask");
	check(hires_replacement_cutout_kills_pixel(true, false, false, true, decal, environment_alpha),
	      "converted texture inputs must retain existing behavior");

	std::cout << "emu_unit_hires_cutout_policy_test: PASS\n";
	return 0;
}

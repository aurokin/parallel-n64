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
constexpr int RGB_MULADD_COMBINED = static_cast<int>(RDP::RGBMulAdd::Combined);
constexpr int RGB_MULADD_CONSTANT = static_cast<int>(RDP::RGBMulAdd::Zero);
constexpr int RGB_MULSUB_SHADE = static_cast<int>(RDP::RGBMulSub::Shade);
constexpr int RGB_MULSUB_CONSTANT = static_cast<int>(RDP::RGBMulSub::Zero);
constexpr int RGB_MUL_TEXEL0 = static_cast<int>(RDP::RGBMul::Texel0);
constexpr int RGB_MUL_SHADE = static_cast<int>(RDP::RGBMul::Shade);
constexpr int RGB_MUL_TEXEL0_ALPHA = static_cast<int>(RDP::RGBMul::Texel0Alpha);
constexpr int RGB_ADD_SHADE = static_cast<int>(RDP::RGBAdd::Shade);
constexpr int RGB_ADD_CONSTANT = static_cast<int>(RDP::RGBAdd::Zero);
constexpr int ALPHA_ADDSUB_ONE = static_cast<int>(RDP::AlphaAddSub::One);
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

	// Captured normalized intensity material: (PRIM - ENV) * T0 + ENV,
	// then COMBINED * SHADE, with ONE output alpha. A zero-alpha replacement
	// sample still leaves the brown ENV=(40,10,0) surface under its lighting.
	const u8x4 intensity = { 8, 8, 1, 7 };
	const u8x4 lit = { 0, 8, 4, 7 };
	const u8x4 opaque = { 6, 6, 6, 6 };
	const u8x4 brown = { 40, 10, 0, 0 };
	const u8x4 zero = { 0, 0, 0, 0 };
	check(hires_shaded_intensity_preserves_surface(true, false, false, intensity, lit, opaque, brown, zero, zero),
	      "empty intensity sample must retain the opaque shaded color bias");
	check(!hires_shaded_intensity_preserves_surface(false, false, false, intensity, lit, opaque, brown, zero, zero),
	      "a one-cycle draw must not receive the two-cycle material exception");
	check(!hires_shaded_intensity_preserves_surface(true, true, false, intensity, lit, opaque, brown, zero, zero),
	      "another sampled tile needs its own dependency tracking");
	check(!hires_shaded_intensity_preserves_surface(true, false, true, intensity, lit, opaque, brown, zero, zero),
	      "converted samples must retain cutout behavior");
	check(!hires_shaded_intensity_preserves_surface(true, false, false, intensity, lit, opaque, zero, zero, zero),
	      "a product-only effect has no colored surface to preserve");
	check(!hires_shaded_intensity_preserves_surface(true, false, false, intensity, lit, shade_alpha, brown, zero, zero),
	      "a material with variable output alpha must retain cutout behavior");
	check(!hires_shaded_intensity_preserves_surface(true, false, false, intensity, lit, opaque, brown, brown, zero),
	      "subtraction in the second cycle is not pure surface lighting");
	check(!hires_shaded_intensity_preserves_surface(true, false, false, intensity, lit, opaque, brown, zero, brown),
	      "addition in the second cycle is not pure surface lighting");

	for (unsigned slot = 0; slot < 4; slot++)
	{
		u8x4 changed = intensity;
		int *fields[] = { &changed.x, &changed.y, &changed.z, &changed.w };
		*fields[slot] = slot == 2 ? 8 : 1;
		check(!hires_shaded_intensity_preserves_surface(true, false, false, changed, lit, opaque, brown, zero, zero),
		      "texture inputs outside the intensity multiplier must retain cutout behavior");
		changed = lit;
		*fields[slot] = slot == 0 ? 1 : 0;
		check(!hires_shaded_intensity_preserves_surface(true, false, false, intensity, changed, opaque, brown, zero, zero),
		      "the second cycle must exclusively modulate the combined surface by shade");
	}
	u8x4 variable_alpha = opaque;
	variable_alpha.x = variable_alpha.y = 7;
	check(!hires_shaded_intensity_preserves_surface(true, false, false, intensity, lit, variable_alpha, brown, zero, zero),
	      "equal normalized constant selectors may hold unequal primitive and environment alpha");
	variable_alpha = opaque;
	variable_alpha.x = 1;
	check(!hires_shaded_intensity_preserves_surface(true, false, false, intensity, lit, variable_alpha, brown, zero, zero),
	      "non-cancelling alpha arithmetic must retain cutout behavior");
	variable_alpha = opaque;
	variable_alpha.w = 1;
	check(!hires_shaded_intensity_preserves_surface(true, false, false, intensity, lit, variable_alpha, brown, zero, zero),
	      "texture alpha in the final add must retain cutout behavior");

	std::cout << "emu_unit_hires_cutout_policy_test: PASS\n";
	return 0;
}

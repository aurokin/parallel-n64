#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_debug_policy.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace RDP;
using namespace RDP::detail;

namespace
{
struct EnvGuard
{
	explicit EnvGuard(const char *name_)
		: name(name_)
	{
		const char *current = std::getenv(name);
		if (current)
		{
			had_value = true;
			value = current;
		}
	}

	~EnvGuard()
	{
		if (had_value)
			setenv(name, value.c_str(), 1);
		else
			unsetenv(name);
	}

	const char *name;
	bool had_value = false;
	std::string value;
};

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static std::array<uint32_t, 8> make_descs(uint32_t a, uint32_t b = 0xffffffffu)
{
	std::array<uint32_t, 8> descs = {};
	descs[0] = a;
	descs[1] = b;
	return descs;
}

static void test_no_env_means_no_overrides()
{
	EnvGuard suppress_draw("PARALLEL_HIRES_SUPPRESS_DRAW_DESC");
	EnvGuard clear_force("PARALLEL_HIRES_CLEAR_FORCE_BLEND_DESC");
	EnvGuard clear_multi("PARALLEL_HIRES_CLEAR_MULTI_CYCLE_DESC");
	EnvGuard clear_image("PARALLEL_HIRES_CLEAR_IMAGE_READ_DESC");
	EnvGuard force_image("PARALLEL_HIRES_FORCE_IMAGE_READ_DESC");
	EnvGuard clear_dither("PARALLEL_HIRES_CLEAR_DITHER_DESC");
	EnvGuard clear_depth_test("PARALLEL_HIRES_CLEAR_DEPTH_TEST_DESC");
	EnvGuard clear_depth_update("PARALLEL_HIRES_CLEAR_DEPTH_UPDATE_DESC");
	EnvGuard clear_color_on_cvg("PARALLEL_HIRES_CLEAR_COLOR_ON_CVG_DESC");
	EnvGuard clear_aa("PARALLEL_HIRES_CLEAR_AA_DESC");
	EnvGuard clear_alpha_test("PARALLEL_HIRES_CLEAR_ALPHA_TEST_DESC");
	EnvGuard force_native("PARALLEL_HIRES_FORCE_NATIVE_TEXRECT_DESC");
	EnvGuard force_upscaled("PARALLEL_HIRES_FORCE_UPSCALED_TEXRECT_DESC");
	EnvGuard blend_1a_memory("PARALLEL_HIRES_BLEND_1A_MEMORY_DESC");
	EnvGuard blend_2a_memory("PARALLEL_HIRES_BLEND_2A_MEMORY_DESC");
	EnvGuard blend_2b_memory_alpha("PARALLEL_HIRES_BLEND_2B_MEMORY_ALPHA_DESC");
	EnvGuard blend_1b_zero("PARALLEL_HIRES_BLEND_1B_ZERO_DESC");
	EnvGuard blend_en_on("PARALLEL_HIRES_FORCE_BLEND_EN_ON_DESC");
	EnvGuard blend_en_off("PARALLEL_HIRES_FORCE_BLEND_EN_OFF_DESC");
	EnvGuard cvg_wrap_on("PARALLEL_HIRES_FORCE_CVG_WRAP_ON_DESC");
	EnvGuard cvg_wrap_off("PARALLEL_HIRES_FORCE_CVG_WRAP_OFF_DESC");
	EnvGuard blend_shift_zero("PARALLEL_HIRES_FORCE_BLEND_SHIFT_ZERO_DESC");
	EnvGuard blend_shift_max("PARALLEL_HIRES_FORCE_BLEND_SHIFT_MAX_DESC");
	EnvGuard pixel_alpha_full("PARALLEL_HIRES_FORCE_PIXEL_ALPHA_FULL_DESC");
	EnvGuard pixel_alpha_zero("PARALLEL_HIRES_FORCE_PIXEL_ALPHA_ZERO_DESC");
	EnvGuard cycle0_rgb_texel0("PARALLEL_HIRES_FORCE_CYCLE0_RGB_TEXEL0_DESC");
	EnvGuard cycle0_rgb_shade("PARALLEL_HIRES_FORCE_CYCLE0_RGB_SHADE_DESC");
	EnvGuard cycle0_rgb_full("PARALLEL_HIRES_FORCE_CYCLE0_RGB_FULL_DESC");
	EnvGuard cycle0_rgb_zero("PARALLEL_HIRES_FORCE_CYCLE0_RGB_ZERO_DESC");
	EnvGuard cycle1_rgb_combined("PARALLEL_HIRES_FORCE_CYCLE1_RGB_COMBINED_DESC");
	EnvGuard cycle1_rgb_texel0("PARALLEL_HIRES_FORCE_CYCLE1_RGB_TEXEL0_DESC");
	EnvGuard cycle1_rgb_full("PARALLEL_HIRES_FORCE_CYCLE1_RGB_FULL_DESC");
	EnvGuard cycle1_rgb_zero("PARALLEL_HIRES_FORCE_CYCLE1_RGB_ZERO_DESC");
	EnvGuard cycle0_alpha_texel0("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_TEXEL0_DESC");
	EnvGuard cycle0_alpha_shade("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_SHADE_DESC");
	EnvGuard cycle0_alpha_full("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_FULL_DESC");
	EnvGuard cycle0_alpha_zero("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_ZERO_DESC");
	EnvGuard match_raster("PARALLEL_HIRES_MATCH_RASTER_FLAGS");
	EnvGuard match_c0_a("PARALLEL_HIRES_MATCH_C0_A");
	EnvGuard match_shade("PARALLEL_HIRES_MATCH_SHADE");
	unsetenv(suppress_draw.name);
	unsetenv(clear_force.name);
	unsetenv(clear_multi.name);
	unsetenv(clear_image.name);
	unsetenv(force_image.name);
	unsetenv(clear_dither.name);
	unsetenv(clear_depth_test.name);
	unsetenv(clear_depth_update.name);
	unsetenv(clear_color_on_cvg.name);
	unsetenv(clear_aa.name);
	unsetenv(clear_alpha_test.name);
	unsetenv(force_native.name);
	unsetenv(force_upscaled.name);
	unsetenv(blend_1a_memory.name);
	unsetenv(blend_2a_memory.name);
	unsetenv(blend_2b_memory_alpha.name);
	unsetenv(blend_1b_zero.name);
	unsetenv(blend_en_on.name);
	unsetenv(blend_en_off.name);
	unsetenv(cvg_wrap_on.name);
	unsetenv(cvg_wrap_off.name);
	unsetenv(blend_shift_zero.name);
	unsetenv(blend_shift_max.name);
	unsetenv(pixel_alpha_full.name);
	unsetenv(pixel_alpha_zero.name);
	unsetenv(cycle0_rgb_texel0.name);
	unsetenv(cycle0_rgb_shade.name);
	unsetenv(cycle0_rgb_full.name);
	unsetenv(cycle0_rgb_zero.name);
	unsetenv(cycle1_rgb_combined.name);
	unsetenv(cycle1_rgb_texel0.name);
	unsetenv(cycle1_rgb_full.name);
	unsetenv(cycle1_rgb_zero.name);
	unsetenv(cycle0_alpha_texel0.name);
	unsetenv(cycle0_alpha_shade.name);
	unsetenv(cycle0_alpha_full.name);
	unsetenv(cycle0_alpha_zero.name);
	unsetenv(match_raster.name);
	unsetenv(match_c0_a.name);
	unsetenv(match_shade.name);

	auto descs = make_descs(25u, 40u);
	auto overrides = derive_hires_debug_draw_overrides(descs, 2);
	check(!overrides.suppress_draw, "suppress_draw should default off");
	check(!overrides.clear_force_blend, "clear_force_blend should default off");
	check(!overrides.clear_multi_cycle, "clear_multi_cycle should default off");
	check(!overrides.clear_image_read, "clear_image_read should default off");
	check(!overrides.force_image_read, "force_image_read should default off");
	check(!overrides.clear_blend_dither, "clear_blend_dither should default off");
	check(!overrides.clear_depth_test, "clear_depth_test should default off");
	check(!overrides.clear_depth_update, "clear_depth_update should default off");
	check(!overrides.clear_color_on_coverage, "clear_color_on_coverage should default off");
	check(!overrides.clear_aa, "clear_aa should default off");
	check(!overrides.clear_alpha_test, "clear_alpha_test should default off");
	check(!overrides.force_native_texrect, "force_native_texrect should default off");
	check(!overrides.force_upscaled_texrect, "force_upscaled_texrect should default off");
	check(!overrides.force_blend_1a_memory, "force_blend_1a_memory should default off");
	check(!overrides.force_blend_1b_zero, "force_blend_1b_zero should default off");
	check(!overrides.force_blend_2a_memory, "force_blend_2a_memory should default off");
	check(!overrides.force_blend_2b_memory_alpha, "force_blend_2b_memory_alpha should default off");
	check(!overrides.force_blend_en_on, "force_blend_en_on should default off");
	check(!overrides.force_blend_en_off, "force_blend_en_off should default off");
	check(!overrides.force_coverage_wrap_on, "force_coverage_wrap_on should default off");
	check(!overrides.force_coverage_wrap_off, "force_coverage_wrap_off should default off");
	check(!overrides.force_blend_shift_zero, "force_blend_shift_zero should default off");
	check(!overrides.force_blend_shift_max, "force_blend_shift_max should default off");
	check(!overrides.force_pixel_alpha_full, "force_pixel_alpha_full should default off");
	check(!overrides.force_pixel_alpha_zero, "force_pixel_alpha_zero should default off");
	check(!overrides.force_cycle0_rgb_texel0, "force_cycle0_rgb_texel0 should default off");
	check(!overrides.force_cycle0_rgb_shade, "force_cycle0_rgb_shade should default off");
	check(!overrides.force_cycle0_rgb_full, "force_cycle0_rgb_full should default off");
	check(!overrides.force_cycle0_rgb_zero, "force_cycle0_rgb_zero should default off");
	check(!overrides.force_cycle1_rgb_combined, "force_cycle1_rgb_combined should default off");
	check(!overrides.force_cycle1_rgb_texel0, "force_cycle1_rgb_texel0 should default off");
	check(!overrides.force_cycle1_rgb_full, "force_cycle1_rgb_full should default off");
	check(!overrides.force_cycle1_rgb_zero, "force_cycle1_rgb_zero should default off");
	check(!overrides.force_cycle0_alpha_texel0, "force_cycle0_alpha_texel0 should default off");
	check(!overrides.force_cycle0_alpha_shade, "force_cycle0_alpha_shade should default off");
	check(!overrides.force_cycle0_alpha_full, "force_cycle0_alpha_full should default off");
	check(!overrides.force_cycle0_alpha_zero, "force_cycle0_alpha_zero should default off");

	auto subtype = derive_hires_debug_subtype_match();
	check(!hires_debug_subtype_match_active(subtype), "subtype match should default inactive");
}

static void test_descriptor_lists_match_any_bound_replacement()
{
	EnvGuard suppress_draw("PARALLEL_HIRES_SUPPRESS_DRAW_DESC");
	EnvGuard clear_force("PARALLEL_HIRES_CLEAR_FORCE_BLEND_DESC");
	EnvGuard clear_image("PARALLEL_HIRES_CLEAR_IMAGE_READ_DESC");
	EnvGuard force_image("PARALLEL_HIRES_FORCE_IMAGE_READ_DESC");
	EnvGuard clear_dither("PARALLEL_HIRES_CLEAR_DITHER_DESC");
	EnvGuard clear_depth("PARALLEL_HIRES_CLEAR_DEPTH_TEST_DESC");
	EnvGuard blend_1a_memory("PARALLEL_HIRES_BLEND_1A_MEMORY_DESC");
	EnvGuard blend_2b_one("PARALLEL_HIRES_BLEND_2B_ONE_DESC");
	EnvGuard blend_en_on("PARALLEL_HIRES_FORCE_BLEND_EN_ON_DESC");
	EnvGuard blend_1b_zero("PARALLEL_HIRES_BLEND_1B_ZERO_DESC");
	EnvGuard pixel_alpha_full("PARALLEL_HIRES_FORCE_PIXEL_ALPHA_FULL_DESC");
	EnvGuard cycle0_rgb_texel0("PARALLEL_HIRES_FORCE_CYCLE0_RGB_TEXEL0_DESC");
	EnvGuard cycle0_rgb_zero("PARALLEL_HIRES_FORCE_CYCLE0_RGB_ZERO_DESC");
	EnvGuard cycle1_rgb_combined("PARALLEL_HIRES_FORCE_CYCLE1_RGB_COMBINED_DESC");
	EnvGuard cycle1_rgb_zero("PARALLEL_HIRES_FORCE_CYCLE1_RGB_ZERO_DESC");
	EnvGuard cycle0_alpha_texel0("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_TEXEL0_DESC");
	EnvGuard cycle0_alpha_zero("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_ZERO_DESC");
	EnvGuard match_raster("PARALLEL_HIRES_MATCH_RASTER_FLAGS");
	EnvGuard match_c0_a("PARALLEL_HIRES_MATCH_C0_A");
	EnvGuard match_shade("PARALLEL_HIRES_MATCH_SHADE");
	setenv(suppress_draw.name, "40", 1);
	setenv(clear_force.name, "41, 88", 1);
	setenv(clear_image.name, "25", 1);
	setenv(force_image.name, "40", 1);
	setenv(clear_dither.name, "999,40", 1);
	setenv(clear_depth.name, "40", 1);
	setenv(blend_1a_memory.name, "40", 1);
	setenv(blend_2b_one.name, "88", 1);
	setenv(blend_en_on.name, "25", 1);
	setenv(blend_1b_zero.name, "25", 1);
	setenv(pixel_alpha_full.name, "40", 1);
	setenv(cycle0_rgb_texel0.name, "40", 1);
	setenv(cycle0_rgb_zero.name, "25", 1);
	setenv(cycle1_rgb_combined.name, "40", 1);
	setenv(cycle1_rgb_zero.name, "25", 1);
	setenv(cycle0_alpha_texel0.name, "40", 1);
	setenv(cycle0_alpha_zero.name, "25", 1);
	setenv(match_raster.name, "0x21844108", 1);
	setenv(match_c0_a.name, "7,7,7,1", 1);
	setenv(match_shade.name, "255,255,255,255", 1);

	auto descs = make_descs(25u, 40u);
	auto overrides = derive_hires_debug_draw_overrides(descs, 2);
	check(overrides.suppress_draw, "matching suppress_draw descriptor should trigger");
	check(!overrides.clear_force_blend, "non-matching clear_force_blend descriptor should not trigger");
	check(overrides.clear_image_read, "matching clear_image_read descriptor should trigger");
	check(overrides.force_image_read, "matching force_image_read descriptor should trigger");
	check(overrides.clear_blend_dither, "matching clear_blend_dither descriptor should trigger");
	check(overrides.clear_depth_test, "matching clear_depth_test descriptor should trigger");
	check(overrides.force_blend_1a_memory, "matching blend_1a_memory descriptor should trigger");
	check(overrides.force_blend_1b_zero, "matching blend_1b_zero descriptor should trigger");
	check(!overrides.force_blend_2b_one, "non-matching blend_2b_one descriptor should not trigger");
	check(overrides.force_blend_en_on, "matching force_blend_en_on descriptor should trigger");
	check(overrides.force_pixel_alpha_full, "matching force_pixel_alpha_full should trigger");
	check(overrides.force_cycle0_rgb_texel0, "matching force_cycle0_rgb_texel0 should trigger");
	check(overrides.force_cycle0_rgb_zero, "matching force_cycle0_rgb_zero should trigger");
	check(overrides.force_cycle1_rgb_combined, "matching force_cycle1_rgb_combined should trigger");
	check(overrides.force_cycle1_rgb_zero, "matching force_cycle1_rgb_zero should trigger");
	check(overrides.force_cycle0_alpha_texel0, "matching force_cycle0_alpha_texel0 should trigger");
	check(overrides.force_cycle0_alpha_zero, "matching force_cycle0_alpha_zero should trigger");

	auto subtype = derive_hires_debug_subtype_match();
	check(hires_debug_subtype_match_active(subtype), "subtype match should become active");
	check(subtype.has_raw_raster_flags && subtype.raw_raster_flags == 0x21844108u,
	      "subtype raster match should parse hex");
	check(subtype.has_c0_alpha &&
	      subtype.c0_alpha == std::array<uint8_t, 4>{ 7u, 7u, 7u, 1u },
	      "subtype c0_a match should parse tuple");
	check(subtype.has_shade &&
	      subtype.shade == std::array<uint8_t, 4>{ 255u, 255u, 255u, 255u },
	      "subtype shade match should parse tuple");
}

static void test_descriptor_wildcard_matches_without_bound_descs()
{
	EnvGuard suppress_draw("PARALLEL_HIRES_SUPPRESS_DRAW_DESC");
	EnvGuard clear_force("PARALLEL_HIRES_CLEAR_FORCE_BLEND_DESC");
	setenv(suppress_draw.name, "*", 1);
	setenv(clear_force.name, "*", 1);

	std::array<uint32_t, 8> descs = {};
	auto overrides = derive_hires_debug_draw_overrides(descs, 0);
	check(overrides.suppress_draw, "wildcard suppress_draw should apply without bound descriptors");
	check(overrides.clear_force_blend, "wildcard descriptor match should apply without bound descriptors");
}

static void test_subtype_filter_blocks_nonmatching_overrides()
{
	EnvGuard clear_force("PARALLEL_HIRES_CLEAR_FORCE_BLEND_DESC");
	EnvGuard match_raster("PARALLEL_HIRES_MATCH_RASTER_FLAGS");
	EnvGuard match_c0_a("PARALLEL_HIRES_MATCH_C0_A");
	EnvGuard match_shade("PARALLEL_HIRES_MATCH_SHADE");
	setenv(clear_force.name, "68", 1);
	setenv(match_raster.name, "0x21844108", 1);
	setenv(match_c0_a.name, "7,7,7,1", 1);
	setenv(match_shade.name, "255,255,255,255", 1);

	auto descs = make_descs(68u);
	auto overrides = derive_hires_debug_draw_overrides(descs, 1);
	auto subtype = derive_hires_debug_subtype_match();

	StaticRasterizationState normalized = {};
	normalized.combiner[0].alpha.muladd = AlphaAddSub::Zero;
	normalized.combiner[0].alpha.mulsub = AlphaAddSub::Zero;
	normalized.combiner[0].alpha.mul = AlphaMul::Zero;
	normalized.combiner[0].alpha.add = AlphaAddSub::Texel0Alpha;
	AttributeSetup attr = {};
	attr.r = 255 << 16;
	attr.g = 255 << 16;
	attr.b = 255 << 16;
	attr.a = 255 << 16;

	auto matched = filter_hires_debug_draw_overrides(overrides, subtype, 0x21844108u, normalized, attr);
	check(matched.clear_force_blend, "matching subtype should preserve overrides");

	normalized.combiner[0].alpha.muladd = AlphaAddSub::Texel0Alpha;
	normalized.combiner[0].alpha.mul = AlphaMul::ShadeAlpha;
	normalized.combiner[0].alpha.add = AlphaAddSub::CombinedAlpha;
	auto filtered = filter_hires_debug_draw_overrides(overrides, subtype, 0x21844108u, normalized, attr);
	check(!filtered.clear_force_blend, "nonmatching subtype should clear overrides");

	normalized.combiner[0].alpha.muladd = AlphaAddSub::Zero;
	normalized.combiner[0].alpha.mul = AlphaMul::Zero;
	normalized.combiner[0].alpha.add = AlphaAddSub::Texel0Alpha;
	attr.r = 193 << 16;
	auto wrong_shade = filter_hires_debug_draw_overrides(overrides, subtype, 0x21844108u, normalized, attr);
	check(!wrong_shade.clear_force_blend, "wrong shade should clear overrides");

	attr.r = 255 << 16;
	auto wrong_raster = filter_hires_debug_draw_overrides(overrides, subtype, 0x01804108u, normalized, attr);
	check(!wrong_raster.clear_force_blend, "wrong raster should clear overrides");
}

static void test_subtype_filter_can_force_suppress_without_descriptors()
{
	EnvGuard suppress_desc("PARALLEL_HIRES_SUPPRESS_DRAW_DESC");
	EnvGuard suppress_match("PARALLEL_HIRES_SUPPRESS_MATCHED_DRAW");
	EnvGuard match_raster("PARALLEL_HIRES_MATCH_RASTER_FLAGS");
	unsetenv(suppress_desc.name);
	setenv(suppress_match.name, "1", 1);
	setenv(match_raster.name, "0x21840010", 1);

	std::array<uint32_t, 8> descs = {};
	auto overrides = derive_hires_debug_draw_overrides(descs, 0);
	auto subtype = derive_hires_debug_subtype_match();

	StaticRasterizationState normalized = {};
	AttributeSetup attr = {};

	auto matched = filter_hires_debug_draw_overrides(overrides, subtype, 0x21840010u, normalized, attr);
	check(matched.suppress_draw, "matched subtype should force suppress_draw without bound descriptors");

	auto wrong = filter_hires_debug_draw_overrides(overrides, subtype, 0x21844118u, normalized, attr);
	check(!wrong.suppress_draw, "nonmatching subtype should not force suppress_draw");
}

static void test_apply_overrides_mutates_expected_state_bits()
{
	EnvGuard clear_force("PARALLEL_HIRES_CLEAR_FORCE_BLEND_DESC");
	EnvGuard clear_multi("PARALLEL_HIRES_CLEAR_MULTI_CYCLE_DESC");
	EnvGuard clear_depth_test("PARALLEL_HIRES_CLEAR_DEPTH_TEST_DESC");
	EnvGuard clear_depth_update("PARALLEL_HIRES_CLEAR_DEPTH_UPDATE_DESC");
	EnvGuard clear_color_on_cvg("PARALLEL_HIRES_CLEAR_COLOR_ON_CVG_DESC");
	EnvGuard clear_aa("PARALLEL_HIRES_CLEAR_AA_DESC");
	EnvGuard clear_alpha_test("PARALLEL_HIRES_CLEAR_ALPHA_TEST_DESC");
	EnvGuard force_native("PARALLEL_HIRES_FORCE_NATIVE_TEXRECT_DESC");
	EnvGuard blend_1a_memory("PARALLEL_HIRES_BLEND_1A_MEMORY_DESC");
	EnvGuard blend_1b_shade_alpha("PARALLEL_HIRES_BLEND_1B_SHADE_ALPHA_DESC");
	EnvGuard blend_1b_zero("PARALLEL_HIRES_BLEND_1B_ZERO_DESC");
	EnvGuard blend_2a_memory("PARALLEL_HIRES_BLEND_2A_MEMORY_DESC");
	EnvGuard blend_2b_memory_alpha("PARALLEL_HIRES_BLEND_2B_MEMORY_ALPHA_DESC");
	EnvGuard blend_en_on("PARALLEL_HIRES_FORCE_BLEND_EN_ON_DESC");
	EnvGuard cvg_wrap_off("PARALLEL_HIRES_FORCE_CVG_WRAP_OFF_DESC");
	EnvGuard blend_shift_zero("PARALLEL_HIRES_FORCE_BLEND_SHIFT_ZERO_DESC");
	EnvGuard pixel_alpha_full("PARALLEL_HIRES_FORCE_PIXEL_ALPHA_FULL_DESC");
	EnvGuard pixel_alpha_zero("PARALLEL_HIRES_FORCE_PIXEL_ALPHA_ZERO_DESC");
	EnvGuard cycle0_rgb_texel0("PARALLEL_HIRES_FORCE_CYCLE0_RGB_TEXEL0_DESC");
	EnvGuard cycle0_rgb_shade("PARALLEL_HIRES_FORCE_CYCLE0_RGB_SHADE_DESC");
	EnvGuard cycle0_rgb_full("PARALLEL_HIRES_FORCE_CYCLE0_RGB_FULL_DESC");
	EnvGuard cycle0_rgb_zero("PARALLEL_HIRES_FORCE_CYCLE0_RGB_ZERO_DESC");
	EnvGuard cycle1_rgb_combined("PARALLEL_HIRES_FORCE_CYCLE1_RGB_COMBINED_DESC");
	EnvGuard cycle1_rgb_texel0("PARALLEL_HIRES_FORCE_CYCLE1_RGB_TEXEL0_DESC");
	EnvGuard cycle1_rgb_full("PARALLEL_HIRES_FORCE_CYCLE1_RGB_FULL_DESC");
	EnvGuard cycle1_rgb_zero("PARALLEL_HIRES_FORCE_CYCLE1_RGB_ZERO_DESC");
	EnvGuard cycle0_alpha_texel0("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_TEXEL0_DESC");
	EnvGuard cycle0_alpha_shade("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_SHADE_DESC");
	EnvGuard cycle0_alpha_full("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_FULL_DESC");
	EnvGuard cycle0_alpha_zero("PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_ZERO_DESC");
	setenv(clear_force.name, "25", 1);
	setenv(clear_multi.name, "25", 1);
	setenv(clear_depth_test.name, "25", 1);
	setenv(clear_depth_update.name, "25", 1);
	setenv(clear_color_on_cvg.name, "25", 1);
	setenv(clear_aa.name, "25", 1);
	setenv(clear_alpha_test.name, "25", 1);
	setenv(force_native.name, "25", 1);
	setenv(blend_1a_memory.name, "25", 1);
	setenv(blend_1b_shade_alpha.name, "25", 1);
	setenv(blend_1b_zero.name, "41", 1);
	setenv(blend_2a_memory.name, "25", 1);
	setenv(blend_2b_memory_alpha.name, "25", 1);
	setenv(blend_en_on.name, "25", 1);
	setenv(cvg_wrap_off.name, "25", 1);
	setenv(blend_shift_zero.name, "25", 1);
	setenv(pixel_alpha_full.name, "25", 1);
	setenv(pixel_alpha_zero.name, "25", 1);
	setenv(cycle0_rgb_texel0.name, "25", 1);
	setenv(cycle0_rgb_shade.name, "25", 1);
	setenv(cycle0_rgb_full.name, "25", 1);
	setenv(cycle0_rgb_zero.name, "25", 1);
	setenv(cycle1_rgb_combined.name, "25", 1);
	setenv(cycle1_rgb_texel0.name, "25", 1);
	setenv(cycle1_rgb_full.name, "25", 1);
	setenv(cycle1_rgb_zero.name, "25", 1);
	setenv(cycle0_alpha_texel0.name, "25", 1);
	setenv(cycle0_alpha_shade.name, "25", 1);
	setenv(cycle0_alpha_full.name, "25", 1);
	setenv(cycle0_alpha_zero.name, "25", 1);
	setenv(cycle0_alpha_full.name, "25", 1);
	setenv(cycle0_alpha_zero.name, "25", 1);

	auto descs = make_descs(25u);
	auto overrides = derive_hires_debug_draw_overrides(descs, 1);

	TriangleSetup setup = {};
	setup.flags = 0;
	StaticRasterizationFlags raster = RASTERIZATION_MULTI_CYCLE_BIT |
	                                  RASTERIZATION_COPY_BIT |
	                                  RASTERIZATION_AA_BIT |
	                                  RASTERIZATION_ALPHA_TEST_BIT |
	                                  RASTERIZATION_ALPHA_TEST_DITHER_BIT;
	uint32_t static_dither = 0;
	DepthBlendFlags depth = DEPTH_BLEND_FORCE_BLEND_BIT |
	                        DEPTH_BLEND_DEPTH_TEST_BIT |
	                        DEPTH_BLEND_DEPTH_UPDATE_BIT |
	                        DEPTH_BLEND_MULTI_CYCLE_BIT |
	                        DEPTH_BLEND_IMAGE_READ_ENABLE_BIT |
	                        DEPTH_BLEND_COLOR_ON_COVERAGE_BIT |
	                        DEPTH_BLEND_AA_BIT |
	                        DEPTH_BLEND_DITHER_ENABLE_BIT;
	DepthBlendState depth_state = {};
	depth_state.blend_cycles[0].blend_1a = BlendMode1A::PixelColor;
	depth_state.blend_cycles[0].blend_1b = BlendMode1B::PixelAlpha;
	depth_state.blend_cycles[0].blend_2a = BlendMode2A::PixelColor;
	depth_state.blend_cycles[0].blend_2b = BlendMode2B::InvPixelAlpha;
	depth_state.blend_cycles[1] = depth_state.blend_cycles[0];
	apply_hires_debug_draw_overrides(overrides, setup, raster, static_dither, depth, depth_state);

	check((setup.flags & TRIANGLE_SETUP_DISABLE_UPSCALING_BIT) != 0,
	      "force_native_texrect should disable upscaling");
	check((raster & RASTERIZATION_MULTI_CYCLE_BIT) == 0,
	      "clear_multi_cycle should clear raster multi-cycle");
	check((depth & DEPTH_BLEND_FORCE_BLEND_BIT) == 0,
	      "clear_force_blend should clear depth force-blend");
	check((depth & DEPTH_BLEND_MULTI_CYCLE_BIT) == 0,
	      "clear_multi_cycle should clear depth multi-cycle");
	check((depth & DEPTH_BLEND_DEPTH_TEST_BIT) == 0,
	      "clear_depth_test should clear depth test");
	check((depth & DEPTH_BLEND_DEPTH_UPDATE_BIT) == 0,
	      "clear_depth_update should clear depth update");
	check((depth & DEPTH_BLEND_COLOR_ON_COVERAGE_BIT) == 0,
	      "clear_color_on_coverage should clear color-on-coverage");
	check((depth & DEPTH_BLEND_AA_BIT) == 0,
	      "clear_aa should clear depth AA bit");
	check((depth & DEPTH_BLEND_IMAGE_READ_ENABLE_BIT) != 0,
	      "unrequested image-read bit should remain set");
	check((depth & DEPTH_BLEND_DITHER_ENABLE_BIT) != 0,
	      "unrequested dither bit should remain set");
	check((raster & RASTERIZATION_AA_BIT) == 0,
	      "clear_aa should clear raster AA bit");
	check((raster & RASTERIZATION_ALPHA_TEST_BIT) == 0,
	      "clear_alpha_test should clear raster alpha test bit");
	check((raster & RASTERIZATION_ALPHA_TEST_DITHER_BIT) == 0,
	      "clear_alpha_test should clear raster alpha test dither bit");
	check(depth_state.blend_cycles[0].blend_1a == BlendMode1A::MemoryColor,
	      "blend_1a override should force memory color");
	check(depth_state.blend_cycles[0].blend_1b == BlendMode1B::ShadeAlpha,
	      "blend_1b override should force shade alpha");
	check(depth_state.blend_cycles[0].blend_2a == BlendMode2A::MemoryColor,
	      "blend_2a override should force memory color");
	check(depth_state.blend_cycles[0].blend_2b == BlendMode2B::MemoryAlpha,
	      "blend_2b override should force memory alpha");
	check(depth_state.blend_cycles[1].blend_2b == BlendMode2B::MemoryAlpha,
	      "blend overrides should apply to both cycles");
	check((depth_state.padding[0] & HIRES_DBDBG_FORCE_BLEND_EN_ON_BIT) != 0,
	      "blend_en_on should set debug padding bit");
	check((depth_state.padding[0] & HIRES_DBDBG_FORCE_CVG_WRAP_OFF_BIT) != 0,
	      "cvg_wrap_off should set debug padding bit");
	check((depth_state.padding[0] & HIRES_DBDBG_FORCE_BLEND_SHIFT_ZERO_BIT) != 0,
	      "blend_shift_zero should set debug padding bit");
	check((depth_state.padding[0] & HIRES_DBDBG_FORCE_PIXEL_ALPHA_FULL_BIT) != 0,
	      "pixel_alpha_full should set debug padding bit");
	check((depth_state.padding[0] & HIRES_DBDBG_FORCE_PIXEL_ALPHA_ZERO_BIT) != 0,
	      "pixel_alpha_zero should set debug padding bit");
	check((depth_state.padding[1] & 0xffu) == 0,
	      "cycle0 combiner probes should not use depth blend padding");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE0_RGB_TEXEL0_BIT) != 0,
	      "cycle0_rgb_texel0 should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE0_RGB_SHADE_BIT) != 0,
	      "cycle0_rgb_shade should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE0_RGB_FULL_BIT) != 0,
	      "cycle0_rgb_full should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE0_RGB_ZERO_BIT) != 0,
	      "cycle0_rgb_zero should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE1_RGB_COMBINED_BIT) != 0,
	      "cycle1_rgb_combined should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE1_RGB_TEXEL0_BIT) != 0,
	      "cycle1_rgb_texel0 should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE1_RGB_FULL_BIT) != 0,
	      "cycle1_rgb_full should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE1_RGB_ZERO_BIT) != 0,
	      "cycle1_rgb_zero should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_FULL_BIT) != 0,
	      "cycle0_alpha_full should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_ZERO_BIT) != 0,
	      "cycle0_alpha_zero should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_TEXEL0_BIT) != 0,
	      "cycle0_alpha_texel0 should set combiner dither debug bit");
	check((static_dither & HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_SHADE_BIT) != 0,
	      "cycle0_alpha_shade should set combiner dither debug bit");
}

static void test_force_upscaled_texrect_wins_last()
{
	EnvGuard force_native("PARALLEL_HIRES_FORCE_NATIVE_TEXRECT_DESC");
	EnvGuard force_upscaled("PARALLEL_HIRES_FORCE_UPSCALED_TEXRECT_DESC");
	setenv(force_native.name, "25", 1);
	setenv(force_upscaled.name, "25", 1);

	auto descs = make_descs(25u);
	auto overrides = derive_hires_debug_draw_overrides(descs, 1);

	TriangleSetup setup = {};
	setup.flags = TRIANGLE_SETUP_DISABLE_UPSCALING_BIT;
	StaticRasterizationFlags raster = 0;
	uint32_t static_dither = 0;
	DepthBlendFlags depth = 0;
	DepthBlendState depth_state = {};
	apply_hires_debug_draw_overrides(overrides, setup, raster, static_dither, depth, depth_state);

	check((setup.flags & TRIANGLE_SETUP_DISABLE_UPSCALING_BIT) == 0,
	      "force_upscaled_texrect should win over force_native_texrect");
}
}

int main()
{
	test_no_env_means_no_overrides();
	test_descriptor_lists_match_any_bound_replacement();
	test_descriptor_wildcard_matches_without_bound_descs();
	test_subtype_filter_blocks_nonmatching_overrides();
	test_subtype_filter_can_force_suppress_without_descriptors();
	test_apply_overrides_mutates_expected_state_bits();
	test_force_upscaled_texrect_wins_last();
	std::cout << "emu_unit_hires_debug_policy_test: PASS" << std::endl;
	return 0;
}

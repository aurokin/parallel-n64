#pragma once

#include "rdp_data_structures.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace RDP
{
namespace detail
{
struct HiresDebugDrawOverrides
{
	bool clear_force_blend = false;
	bool clear_multi_cycle = false;
	bool clear_image_read = false;
	bool clear_blend_dither = false;
	bool clear_depth_test = false;
	bool clear_depth_update = false;
	bool clear_color_on_coverage = false;
	bool clear_aa = false;
	bool clear_alpha_test = false;
	bool force_native_texrect = false;
	bool force_upscaled_texrect = false;
	bool force_blend_1a_memory = false;
	bool force_blend_1a_pixel = false;
	bool force_blend_1b_shade_alpha = false;
	bool force_blend_1b_pixel_alpha = false;
	bool force_blend_2a_memory = false;
	bool force_blend_2a_pixel = false;
	bool force_blend_2b_memory_alpha = false;
	bool force_blend_2b_inv_pixel_alpha = false;
	bool force_blend_2b_one = false;
	bool force_blend_2b_zero = false;
};

inline bool hires_debug_desc_list_matches_value(const char *env, uint32_t value)
{
	if (!env || !*env)
		return false;

	const char *ptr = env;
	while (*ptr)
	{
		while (*ptr == ',' || *ptr == ' ' || *ptr == '\t')
			ptr++;
		if (!*ptr)
			break;

		char *end = nullptr;
		unsigned long parsed = std::strtoul(ptr, &end, 10);
		if (end == ptr)
			break;
		if (parsed == value)
			return true;
		ptr = end;
	}

	return false;
}

template <size_t N>
inline bool hires_debug_desc_list_matches_any(const std::array<uint32_t, N> &descs,
                                              size_t count,
                                              const char *env_name)
{
	if (count == 0)
		return false;

	const char *env = std::getenv(env_name);
	if (!env || !*env)
		return false;

	for (size_t i = 0; i < count; i++)
	{
		if (hires_debug_desc_list_matches_value(env, descs[i]))
			return true;
	}

	return false;
}

template <size_t N>
inline HiresDebugDrawOverrides derive_hires_debug_draw_overrides(const std::array<uint32_t, N> &descs,
                                                                 size_t count)
{
	HiresDebugDrawOverrides overrides = {};
	overrides.clear_force_blend = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_FORCE_BLEND_DESC");
	overrides.clear_multi_cycle = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_MULTI_CYCLE_DESC");
	overrides.clear_image_read = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_IMAGE_READ_DESC");
	overrides.clear_blend_dither = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_DITHER_DESC");
	overrides.clear_depth_test = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_DEPTH_TEST_DESC");
	overrides.clear_depth_update = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_DEPTH_UPDATE_DESC");
	overrides.clear_color_on_coverage = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_COLOR_ON_CVG_DESC");
	overrides.clear_aa = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_AA_DESC");
	overrides.clear_alpha_test = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_ALPHA_TEST_DESC");
	overrides.force_native_texrect = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_NATIVE_TEXRECT_DESC");
	overrides.force_upscaled_texrect = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_UPSCALED_TEXRECT_DESC");
	overrides.force_blend_1a_memory = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_1A_MEMORY_DESC");
	overrides.force_blend_1a_pixel = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_1A_PIXEL_DESC");
	overrides.force_blend_1b_shade_alpha = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_1B_SHADE_ALPHA_DESC");
	overrides.force_blend_1b_pixel_alpha = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_1B_PIXEL_ALPHA_DESC");
	overrides.force_blend_2a_memory = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2A_MEMORY_DESC");
	overrides.force_blend_2a_pixel = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2A_PIXEL_DESC");
	overrides.force_blend_2b_memory_alpha = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2B_MEMORY_ALPHA_DESC");
	overrides.force_blend_2b_inv_pixel_alpha = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2B_INV_PIXEL_ALPHA_DESC");
	overrides.force_blend_2b_one = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2B_ONE_DESC");
	overrides.force_blend_2b_zero = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2B_ZERO_DESC");
	return overrides;
}

inline void apply_hires_debug_draw_overrides(const HiresDebugDrawOverrides &overrides,
                                             TriangleSetup &setup,
                                             StaticRasterizationFlags &raster_flags,
                                             DepthBlendFlags &depth_blend_flags,
                                             DepthBlendState &depth_blend_state)
{
	if (overrides.force_native_texrect)
		setup.flags |= TRIANGLE_SETUP_DISABLE_UPSCALING_BIT;
	if (overrides.force_upscaled_texrect)
		setup.flags &= ~TRIANGLE_SETUP_DISABLE_UPSCALING_BIT;
	if (overrides.clear_force_blend)
		depth_blend_flags &= ~DEPTH_BLEND_FORCE_BLEND_BIT;
	if (overrides.clear_multi_cycle)
	{
		raster_flags &= ~RASTERIZATION_MULTI_CYCLE_BIT;
		depth_blend_flags &= ~DEPTH_BLEND_MULTI_CYCLE_BIT;
	}
	if (overrides.clear_image_read)
		depth_blend_flags &= ~DEPTH_BLEND_IMAGE_READ_ENABLE_BIT;
	if (overrides.clear_blend_dither)
		depth_blend_flags &= ~DEPTH_BLEND_DITHER_ENABLE_BIT;
	if (overrides.clear_depth_test)
		depth_blend_flags &= ~DEPTH_BLEND_DEPTH_TEST_BIT;
	if (overrides.clear_depth_update)
		depth_blend_flags &= ~DEPTH_BLEND_DEPTH_UPDATE_BIT;
	if (overrides.clear_color_on_coverage)
		depth_blend_flags &= ~DEPTH_BLEND_COLOR_ON_COVERAGE_BIT;
	if (overrides.clear_aa)
	{
		raster_flags &= ~RASTERIZATION_AA_BIT;
		depth_blend_flags &= ~DEPTH_BLEND_AA_BIT;
	}
	if (overrides.clear_alpha_test)
		raster_flags &= ~(RASTERIZATION_ALPHA_TEST_BIT | RASTERIZATION_ALPHA_TEST_DITHER_BIT);
	for (auto &cycle : depth_blend_state.blend_cycles)
	{
		if (overrides.force_blend_1a_memory)
			cycle.blend_1a = BlendMode1A::MemoryColor;
		if (overrides.force_blend_1a_pixel)
			cycle.blend_1a = BlendMode1A::PixelColor;
		if (overrides.force_blend_1b_shade_alpha)
			cycle.blend_1b = BlendMode1B::ShadeAlpha;
		if (overrides.force_blend_1b_pixel_alpha)
			cycle.blend_1b = BlendMode1B::PixelAlpha;
		if (overrides.force_blend_2a_memory)
			cycle.blend_2a = BlendMode2A::MemoryColor;
		if (overrides.force_blend_2a_pixel)
			cycle.blend_2a = BlendMode2A::PixelColor;
		if (overrides.force_blend_2b_memory_alpha)
			cycle.blend_2b = BlendMode2B::MemoryAlpha;
		if (overrides.force_blend_2b_inv_pixel_alpha)
			cycle.blend_2b = BlendMode2B::InvPixelAlpha;
		if (overrides.force_blend_2b_one)
			cycle.blend_2b = BlendMode2B::One;
		if (overrides.force_blend_2b_zero)
			cycle.blend_2b = BlendMode2B::Zero;
	}
}
}
}

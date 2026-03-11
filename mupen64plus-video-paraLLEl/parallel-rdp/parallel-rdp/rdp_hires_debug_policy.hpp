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
	return overrides;
}

inline void apply_hires_debug_draw_overrides(const HiresDebugDrawOverrides &overrides,
                                             TriangleSetup &setup,
                                             StaticRasterizationFlags &raster_flags,
                                             DepthBlendFlags &depth_blend_flags)
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
}
}
}

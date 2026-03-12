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
	bool force_blend_1b_zero = false;
	bool force_blend_2a_memory = false;
	bool force_blend_2a_pixel = false;
	bool force_blend_2b_memory_alpha = false;
	bool force_blend_2b_inv_pixel_alpha = false;
	bool force_blend_2b_one = false;
	bool force_blend_2b_zero = false;
	bool force_blend_en_on = false;
	bool force_blend_en_off = false;
	bool force_coverage_wrap_on = false;
	bool force_coverage_wrap_off = false;
	bool force_blend_shift_zero = false;
	bool force_blend_shift_max = false;
	bool force_pixel_alpha_full = false;
	bool force_pixel_alpha_zero = false;
	bool force_cycle0_alpha_full = false;
	bool force_cycle0_alpha_zero = false;
	bool force_cycle0_alpha_texel0 = false;
	bool force_cycle0_alpha_shade = false;
	bool force_cycle0_rgb_full = false;
	bool force_cycle0_rgb_zero = false;
	bool force_cycle0_rgb_texel0 = false;
	bool force_cycle0_rgb_shade = false;
};

struct HiresDebugSubtypeMatch
{
	bool has_raw_raster_flags = false;
	uint32_t raw_raster_flags = 0;
	bool has_c0_alpha = false;
	std::array<uint8_t, 4> c0_alpha = {};
};

enum HiresDepthBlendDebugBit : uint8_t
{
	HIRES_DBDBG_FORCE_BLEND_EN_ON_BIT = 1 << 0,
	HIRES_DBDBG_FORCE_BLEND_EN_OFF_BIT = 1 << 1,
	HIRES_DBDBG_FORCE_CVG_WRAP_ON_BIT = 1 << 2,
	HIRES_DBDBG_FORCE_CVG_WRAP_OFF_BIT = 1 << 3,
	HIRES_DBDBG_FORCE_BLEND_SHIFT_ZERO_BIT = 1 << 4,
	HIRES_DBDBG_FORCE_BLEND_SHIFT_MAX_BIT = 1 << 5,
	HIRES_DBDBG_FORCE_PIXEL_ALPHA_FULL_BIT = 1 << 6,
	HIRES_DBDBG_FORCE_PIXEL_ALPHA_ZERO_BIT = 1 << 7
};

enum HiresCombinerDitherDebugBit : uint32_t
{
	HIRES_CMBDBG_FORCE_CYCLE0_RGB_TEXEL0_BIT = 1u << 24u,
	HIRES_CMBDBG_FORCE_CYCLE0_RGB_SHADE_BIT = 1u << 25u,
	HIRES_CMBDBG_FORCE_CYCLE0_RGB_FULL_BIT = 1u << 26u,
	HIRES_CMBDBG_FORCE_CYCLE0_RGB_ZERO_BIT = 1u << 27u,
	HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_TEXEL0_BIT = 1u << 28u,
	HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_SHADE_BIT = 1u << 29u,
	HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_FULL_BIT = 1u << 30u,
	HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_ZERO_BIT = 1u << 31u
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

inline bool hires_debug_parse_u32_env(const char *env_name, uint32_t &value)
{
	const char *env = std::getenv(env_name);
	if (!env || !*env)
		return false;

	char *end = nullptr;
	unsigned long parsed = std::strtoul(env, &end, 0);
	if (end == env)
		return false;
	value = static_cast<uint32_t>(parsed);
	return true;
}

inline bool hires_debug_parse_u8x4_env(const char *env_name, std::array<uint8_t, 4> &value)
{
	const char *env = std::getenv(env_name);
	if (!env || !*env)
		return false;

	const char *ptr = env;
	for (size_t i = 0; i < value.size(); i++)
	{
		while (*ptr == ',' || *ptr == ' ' || *ptr == '\t')
			ptr++;
		if (!*ptr)
			return false;

		char *end = nullptr;
		unsigned long parsed = std::strtoul(ptr, &end, 0);
		if (end == ptr || parsed > 255u)
			return false;
		value[i] = static_cast<uint8_t>(parsed);
		ptr = end;
	}

	return true;
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
	overrides.force_blend_1b_zero = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_1B_ZERO_DESC");
	overrides.force_blend_2a_memory = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2A_MEMORY_DESC");
	overrides.force_blend_2a_pixel = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2A_PIXEL_DESC");
	overrides.force_blend_2b_memory_alpha = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2B_MEMORY_ALPHA_DESC");
	overrides.force_blend_2b_inv_pixel_alpha = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2B_INV_PIXEL_ALPHA_DESC");
	overrides.force_blend_2b_one = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2B_ONE_DESC");
	overrides.force_blend_2b_zero = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_BLEND_2B_ZERO_DESC");
	overrides.force_blend_en_on = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_BLEND_EN_ON_DESC");
	overrides.force_blend_en_off = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_BLEND_EN_OFF_DESC");
	overrides.force_coverage_wrap_on = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CVG_WRAP_ON_DESC");
	overrides.force_coverage_wrap_off = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CVG_WRAP_OFF_DESC");
	overrides.force_blend_shift_zero = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_BLEND_SHIFT_ZERO_DESC");
	overrides.force_blend_shift_max = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_BLEND_SHIFT_MAX_DESC");
	overrides.force_pixel_alpha_full = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_PIXEL_ALPHA_FULL_DESC");
	overrides.force_pixel_alpha_zero = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_PIXEL_ALPHA_ZERO_DESC");
	overrides.force_cycle0_alpha_texel0 = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_TEXEL0_DESC");
	overrides.force_cycle0_alpha_shade = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_SHADE_DESC");
	overrides.force_cycle0_alpha_full = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_FULL_DESC");
	overrides.force_cycle0_alpha_zero = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE0_ALPHA_ZERO_DESC");
	overrides.force_cycle0_rgb_texel0 = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE0_RGB_TEXEL0_DESC");
	overrides.force_cycle0_rgb_shade = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE0_RGB_SHADE_DESC");
	overrides.force_cycle0_rgb_full = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE0_RGB_FULL_DESC");
	overrides.force_cycle0_rgb_zero = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE0_RGB_ZERO_DESC");
	return overrides;
}

inline HiresDebugSubtypeMatch derive_hires_debug_subtype_match()
{
	HiresDebugSubtypeMatch match = {};
	match.has_raw_raster_flags = hires_debug_parse_u32_env("PARALLEL_HIRES_MATCH_RASTER_FLAGS",
	                                                       match.raw_raster_flags);
	match.has_c0_alpha = hires_debug_parse_u8x4_env("PARALLEL_HIRES_MATCH_C0_A",
	                                                match.c0_alpha);
	return match;
}

inline bool hires_debug_subtype_match_active(const HiresDebugSubtypeMatch &match)
{
	return match.has_raw_raster_flags || match.has_c0_alpha;
}

inline bool hires_debug_subtype_matches(const HiresDebugSubtypeMatch &match,
                                        uint32_t raw_raster_flags,
                                        const StaticRasterizationState &normalized)
{
	if (match.has_raw_raster_flags && match.raw_raster_flags != raw_raster_flags)
		return false;

	if (match.has_c0_alpha)
	{
		const auto &alpha = normalized.combiner[0].alpha;
		if (match.c0_alpha[0] != static_cast<uint8_t>(alpha.muladd) ||
		    match.c0_alpha[1] != static_cast<uint8_t>(alpha.mulsub) ||
		    match.c0_alpha[2] != static_cast<uint8_t>(alpha.mul) ||
		    match.c0_alpha[3] != static_cast<uint8_t>(alpha.add))
			return false;
	}

	return true;
}

inline HiresDebugDrawOverrides filter_hires_debug_draw_overrides(const HiresDebugDrawOverrides &overrides,
                                                                 const HiresDebugSubtypeMatch &match,
                                                                 uint32_t raw_raster_flags,
                                                                 const StaticRasterizationState &normalized)
{
	if (!hires_debug_subtype_match_active(match))
		return overrides;
	if (hires_debug_subtype_matches(match, raw_raster_flags, normalized))
		return overrides;
	return {};
}

inline void apply_hires_debug_draw_overrides(const HiresDebugDrawOverrides &overrides,
                                             TriangleSetup &setup,
                                             StaticRasterizationFlags &raster_flags,
                                             uint32_t &static_dither,
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
		if (overrides.force_blend_1b_zero)
			cycle.blend_1b = BlendMode1B::Zero;
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
	depth_blend_state.padding[0] = 0;
	if (overrides.force_blend_en_on)
		depth_blend_state.padding[0] |= HIRES_DBDBG_FORCE_BLEND_EN_ON_BIT;
	if (overrides.force_blend_en_off)
		depth_blend_state.padding[0] |= HIRES_DBDBG_FORCE_BLEND_EN_OFF_BIT;
	if (overrides.force_coverage_wrap_on)
		depth_blend_state.padding[0] |= HIRES_DBDBG_FORCE_CVG_WRAP_ON_BIT;
	if (overrides.force_coverage_wrap_off)
		depth_blend_state.padding[0] |= HIRES_DBDBG_FORCE_CVG_WRAP_OFF_BIT;
	if (overrides.force_blend_shift_zero)
		depth_blend_state.padding[0] |= HIRES_DBDBG_FORCE_BLEND_SHIFT_ZERO_BIT;
	if (overrides.force_blend_shift_max)
		depth_blend_state.padding[0] |= HIRES_DBDBG_FORCE_BLEND_SHIFT_MAX_BIT;
	if (overrides.force_pixel_alpha_full)
		depth_blend_state.padding[0] |= HIRES_DBDBG_FORCE_PIXEL_ALPHA_FULL_BIT;
	if (overrides.force_pixel_alpha_zero)
		depth_blend_state.padding[0] |= HIRES_DBDBG_FORCE_PIXEL_ALPHA_ZERO_BIT;
	static_dither &= ~(HIRES_CMBDBG_FORCE_CYCLE0_RGB_TEXEL0_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_RGB_SHADE_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_RGB_FULL_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_RGB_ZERO_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_TEXEL0_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_SHADE_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_FULL_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_ZERO_BIT);
	if (overrides.force_cycle0_rgb_texel0)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE0_RGB_TEXEL0_BIT;
	if (overrides.force_cycle0_rgb_shade)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE0_RGB_SHADE_BIT;
	if (overrides.force_cycle0_rgb_full)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE0_RGB_FULL_BIT;
	if (overrides.force_cycle0_rgb_zero)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE0_RGB_ZERO_BIT;
	if (overrides.force_cycle0_alpha_texel0)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_TEXEL0_BIT;
	if (overrides.force_cycle0_alpha_shade)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_SHADE_BIT;
	if (overrides.force_cycle0_alpha_full)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_FULL_BIT;
	if (overrides.force_cycle0_alpha_zero)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_ZERO_BIT;
}
}
}

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
	bool suppress_draw = false;
	bool clear_force_blend = false;
	bool clear_multi_cycle = false;
	bool clear_image_read = false;
	bool force_image_read = false;
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
	bool force_cycle1_rgb_combined = false;
	bool force_cycle1_rgb_texel0 = false;
	bool force_cycle1_rgb_full = false;
	bool force_cycle1_rgb_zero = false;
	bool force_cycle1_alpha_texel0 = false;
	bool force_cycle1_alpha_shade = false;
	bool force_cycle1_alpha_full = false;
	bool force_cycle1_alpha_zero = false;
};

struct HiresDebugSubtypeMatch
{
	bool has_raw_raster_flags = false;
	uint32_t raw_raster_flags = 0;
	bool has_c0_alpha = false;
	std::array<uint8_t, 4> c0_alpha = {};
	bool has_shade = false;
	std::array<uint8_t, 4> shade = {};
	bool has_screen_y_min = false;
	uint32_t screen_y_min = 0;
	bool has_screen_y_max = false;
	uint32_t screen_y_max = 0;
	bool has_screen_x_min = false;
	uint32_t screen_x_min = 0;
	bool has_screen_x_max = false;
	uint32_t screen_x_max = 0;
	bool has_st_s_min = false;
	int32_t st_s_min = 0;
	bool has_st_s_max = false;
	int32_t st_s_max = 0;
	bool has_st_t_min = false;
	int32_t st_t_min = 0;
	bool has_st_t_max = false;
	int32_t st_t_max = 0;
	bool has_call_modulus = false;
	uint32_t call_modulus = 0;
	bool has_call_remainder = false;
	uint32_t call_remainder = 0;
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

enum HiresDepthBlendDebugBit1 : uint8_t
{
	HIRES_DBDBG1_FORCE_CYCLE1_ALPHA_TEXEL0_BIT = 1 << 0,
	HIRES_DBDBG1_FORCE_CYCLE1_ALPHA_SHADE_BIT = 1 << 1,
	HIRES_DBDBG1_FORCE_CYCLE1_ALPHA_FULL_BIT = 1 << 2,
	HIRES_DBDBG1_FORCE_CYCLE1_ALPHA_ZERO_BIT = 1 << 3
};

enum HiresCombinerDitherDebugBit : uint32_t
{
	HIRES_CMBDBG_FORCE_CYCLE1_RGB_COMBINED_BIT = 1u << 20u,
	HIRES_CMBDBG_FORCE_CYCLE1_RGB_TEXEL0_BIT = 1u << 21u,
	HIRES_CMBDBG_FORCE_CYCLE1_RGB_FULL_BIT = 1u << 22u,
	HIRES_CMBDBG_FORCE_CYCLE1_RGB_ZERO_BIT = 1u << 23u,
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

inline bool hires_debug_desc_list_matches_all(const char *env)
{
	if (!env || !*env)
		return false;

	while (*env == ',' || *env == ' ' || *env == '\t')
		env++;

	return *env == '*';
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

inline bool hires_debug_parse_i32_env(const char *env_name, int32_t &value)
{
	const char *env = std::getenv(env_name);
	if (!env || !*env)
		return false;

	char *end = nullptr;
	long parsed = std::strtol(env, &end, 0);
	if (end == env)
		return false;
	value = static_cast<int32_t>(parsed);
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

inline bool hires_debug_env_enabled(const char *env_name)
{
	const char *env = std::getenv(env_name);
	return env && *env && !(env[0] == '0' && env[1] == '\0');
}

template <size_t N>
inline bool hires_debug_desc_list_matches_any(const std::array<uint32_t, N> &descs,
                                              size_t count,
                                              const char *env_name)
{
	const char *env = std::getenv(env_name);
	if (!env || !*env)
		return false;
	if (hires_debug_desc_list_matches_all(env))
		return true;
	if (count == 0)
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
	overrides.suppress_draw = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_SUPPRESS_DRAW_DESC");
	overrides.clear_force_blend = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_FORCE_BLEND_DESC");
	overrides.clear_multi_cycle = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_MULTI_CYCLE_DESC");
	overrides.clear_image_read = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_CLEAR_IMAGE_READ_DESC");
	overrides.force_image_read = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_IMAGE_READ_DESC");
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
	overrides.force_cycle1_rgb_combined = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE1_RGB_COMBINED_DESC");
	overrides.force_cycle1_rgb_texel0 = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE1_RGB_TEXEL0_DESC");
	overrides.force_cycle1_rgb_full = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE1_RGB_FULL_DESC");
	overrides.force_cycle1_rgb_zero = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE1_RGB_ZERO_DESC");
	overrides.force_cycle1_alpha_texel0 = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE1_ALPHA_TEXEL0_DESC");
	overrides.force_cycle1_alpha_shade = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE1_ALPHA_SHADE_DESC");
	overrides.force_cycle1_alpha_full = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE1_ALPHA_FULL_DESC");
	overrides.force_cycle1_alpha_zero = hires_debug_desc_list_matches_any(descs, count, "PARALLEL_HIRES_FORCE_CYCLE1_ALPHA_ZERO_DESC");
	return overrides;
}

inline HiresDebugSubtypeMatch derive_hires_debug_subtype_match()
{
	HiresDebugSubtypeMatch match = {};
	match.has_raw_raster_flags = hires_debug_parse_u32_env("PARALLEL_HIRES_MATCH_RASTER_FLAGS",
	                                                       match.raw_raster_flags);
	match.has_c0_alpha = hires_debug_parse_u8x4_env("PARALLEL_HIRES_MATCH_C0_A",
	                                                match.c0_alpha);
	match.has_shade = hires_debug_parse_u8x4_env("PARALLEL_HIRES_MATCH_SHADE",
	                                             match.shade);
	match.has_screen_y_min = hires_debug_parse_u32_env("PARALLEL_HIRES_MATCH_SCREEN_Y_MIN",
	                                                   match.screen_y_min);
	match.has_screen_y_max = hires_debug_parse_u32_env("PARALLEL_HIRES_MATCH_SCREEN_Y_MAX",
	                                                   match.screen_y_max);
	match.has_screen_x_min = hires_debug_parse_u32_env("PARALLEL_HIRES_MATCH_SCREEN_X_MIN",
	                                                   match.screen_x_min);
	match.has_screen_x_max = hires_debug_parse_u32_env("PARALLEL_HIRES_MATCH_SCREEN_X_MAX",
	                                                   match.screen_x_max);
	match.has_st_s_min = hires_debug_parse_i32_env("PARALLEL_HIRES_MATCH_ST_S_MIN",
	                                               match.st_s_min);
	match.has_st_s_max = hires_debug_parse_i32_env("PARALLEL_HIRES_MATCH_ST_S_MAX",
	                                               match.st_s_max);
	match.has_st_t_min = hires_debug_parse_i32_env("PARALLEL_HIRES_MATCH_ST_T_MIN",
	                                               match.st_t_min);
	match.has_st_t_max = hires_debug_parse_i32_env("PARALLEL_HIRES_MATCH_ST_T_MAX",
	                                               match.st_t_max);
	match.has_call_modulus = hires_debug_parse_u32_env("PARALLEL_HIRES_MATCH_CALL_MODULUS",
	                                                   match.call_modulus);
	match.has_call_remainder = hires_debug_parse_u32_env("PARALLEL_HIRES_MATCH_CALL_REMAINDER",
	                                                     match.call_remainder);
	return match;
}

inline bool hires_debug_subtype_match_active(const HiresDebugSubtypeMatch &match)
{
	return match.has_raw_raster_flags || match.has_c0_alpha || match.has_shade ||
	       match.has_screen_y_min || match.has_screen_y_max ||
	       match.has_screen_x_min || match.has_screen_x_max ||
	       match.has_st_s_min || match.has_st_s_max ||
	       match.has_st_t_min || match.has_st_t_max ||
	       match.has_call_modulus || match.has_call_remainder;
}

inline bool hires_debug_subtype_matches(const HiresDebugSubtypeMatch &match,
                                        uint32_t raw_raster_flags,
                                        const StaticRasterizationState &normalized,
                                        const AttributeSetup &attr,
                                        uint64_t draw_call_index,
                                        int32_t st_s,
                                        int32_t st_t,
                                        bool has_screen_bounds,
                                        uint32_t screen_x0,
                                        uint32_t screen_x1,
                                        uint32_t screen_y0,
                                        uint32_t screen_y1)
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

	if (match.has_shade)
	{
		if (match.shade[0] != static_cast<uint8_t>((attr.r >> 16) & 0xff) ||
		    match.shade[1] != static_cast<uint8_t>((attr.g >> 16) & 0xff) ||
		    match.shade[2] != static_cast<uint8_t>((attr.b >> 16) & 0xff) ||
		    match.shade[3] != static_cast<uint8_t>((attr.a >> 16) & 0xff))
			return false;
	}

	if ((match.has_screen_y_min || match.has_screen_y_max) && !has_screen_bounds)
		return false;
	if ((match.has_screen_x_min || match.has_screen_x_max) && !has_screen_bounds)
		return false;
	if (match.has_screen_x_min && screen_x0 < match.screen_x_min)
		return false;
	if (match.has_screen_x_max && screen_x1 > match.screen_x_max)
		return false;
	if (match.has_screen_y_min && screen_y0 < match.screen_y_min)
		return false;
	if (match.has_screen_y_max && screen_y1 > match.screen_y_max)
		return false;
	if (match.has_st_s_min && st_s < match.st_s_min)
		return false;
	if (match.has_st_s_max && st_s > match.st_s_max)
		return false;
	if (match.has_st_t_min && st_t < match.st_t_min)
		return false;
	if (match.has_st_t_max && st_t > match.st_t_max)
		return false;
	if (match.has_call_modulus)
	{
		if (match.call_modulus == 0)
			return false;
		const uint32_t remainder = uint32_t(draw_call_index % match.call_modulus);
		if (match.has_call_remainder)
		{
			if (remainder != match.call_remainder)
				return false;
		}
	}
	else if (match.has_call_remainder)
	{
		return false;
	}

	return true;
}

inline HiresDebugDrawOverrides filter_hires_debug_draw_overrides(const HiresDebugDrawOverrides &overrides,
                                                                 const HiresDebugSubtypeMatch &match,
                                                                 uint32_t raw_raster_flags,
                                                                 const StaticRasterizationState &normalized,
                                                                 const AttributeSetup &attr,
                                                                 uint64_t draw_call_index = 0,
                                                                 int32_t st_s = 0,
                                                                 int32_t st_t = 0,
                                                                 bool has_screen_bounds = false,
                                                                 uint32_t screen_x0 = 0,
                                                                 uint32_t screen_x1 = 0,
                                                                 uint32_t screen_y0 = 0,
                                                                 uint32_t screen_y1 = 0)
{
	if (!hires_debug_subtype_match_active(match))
		return overrides;
	if (hires_debug_subtype_matches(match, raw_raster_flags, normalized, attr, draw_call_index, st_s, st_t,
	                                has_screen_bounds, screen_x0, screen_x1, screen_y0, screen_y1))
	{
		auto filtered = overrides;
		if (hires_debug_env_enabled("PARALLEL_HIRES_SUPPRESS_MATCHED_DRAW"))
			filtered.suppress_draw = true;
		return filtered;
	}
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
	if (overrides.force_image_read)
		depth_blend_flags |= DEPTH_BLEND_IMAGE_READ_ENABLE_BIT;
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
	depth_blend_state.padding[1] = 0;
	if (overrides.force_cycle1_alpha_texel0)
		depth_blend_state.padding[1] |= HIRES_DBDBG1_FORCE_CYCLE1_ALPHA_TEXEL0_BIT;
	if (overrides.force_cycle1_alpha_shade)
		depth_blend_state.padding[1] |= HIRES_DBDBG1_FORCE_CYCLE1_ALPHA_SHADE_BIT;
	if (overrides.force_cycle1_alpha_full)
		depth_blend_state.padding[1] |= HIRES_DBDBG1_FORCE_CYCLE1_ALPHA_FULL_BIT;
	if (overrides.force_cycle1_alpha_zero)
		depth_blend_state.padding[1] |= HIRES_DBDBG1_FORCE_CYCLE1_ALPHA_ZERO_BIT;
	static_dither &= ~(HIRES_CMBDBG_FORCE_CYCLE1_RGB_COMBINED_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE1_RGB_TEXEL0_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE1_RGB_FULL_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE1_RGB_ZERO_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_RGB_TEXEL0_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_RGB_SHADE_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_RGB_FULL_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_RGB_ZERO_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_TEXEL0_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_SHADE_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_FULL_BIT |
	                   HIRES_CMBDBG_FORCE_CYCLE0_ALPHA_ZERO_BIT);
	if (overrides.force_cycle1_rgb_combined)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE1_RGB_COMBINED_BIT;
	if (overrides.force_cycle1_rgb_texel0)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE1_RGB_TEXEL0_BIT;
	if (overrides.force_cycle1_rgb_full)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE1_RGB_FULL_BIT;
	if (overrides.force_cycle1_rgb_zero)
		static_dither |= HIRES_CMBDBG_FORCE_CYCLE1_RGB_ZERO_BIT;
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

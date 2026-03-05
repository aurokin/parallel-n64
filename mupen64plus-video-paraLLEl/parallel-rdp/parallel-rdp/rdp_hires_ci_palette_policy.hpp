#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "rdp_common.hpp"
#include "texture_keying.hpp"

namespace RDP
{
namespace detail
{
struct HiresCiPaletteCrcCandidates
{
	uint32_t values[3] = {};
	uint32_t count = 0;
};

inline uint32_t compute_hires_ci_palette_crc_for_entries(TextureSize size,
                                                         uint32_t palette,
                                                         const uint8_t *cpu_rdram,
                                                         size_t rdram_size,
                                                         uint32_t src_base_addr,
                                                         uint32_t key_width_pixels,
                                                         uint32_t key_height_pixels,
                                                         uint32_t row_stride_bytes,
                                                         const uint8_t *tlut_shadow,
                                                         size_t tlut_shadow_size,
                                                         bool tlut_shadow_valid,
                                                         uint32_t min_entries,
                                                         bool full_bank)
{
	if (!tlut_shadow_valid || !cpu_rdram || !tlut_shadow || rdram_size == 0 || tlut_shadow_size < 512)
		return 0;

	if (size == TextureSize::Bpp8)
	{
		uint32_t entries = 0;
		if (full_bank)
			entries = 256u;
		else
		{
			const uint32_t cimax = compute_ci8_max_index(cpu_rdram, rdram_size, src_base_addr,
			                                              key_width_pixels, key_height_pixels, row_stride_bytes);
			entries = std::min<uint32_t>(std::max<uint32_t>(cimax + 1, min_entries), 256u);
		}
		return rice_crc32_wrapped(tlut_shadow, tlut_shadow_size, 0, entries, 1, 2, 512);
	}

	if (size == TextureSize::Bpp4)
	{
		uint32_t entries = 0;
		if (full_bank)
			entries = 16u;
		else
		{
			const uint32_t cimax = compute_ci4_max_index(cpu_rdram, rdram_size, src_base_addr,
			                                              key_width_pixels, key_height_pixels, row_stride_bytes);
			entries = std::min<uint32_t>(std::max<uint32_t>(cimax + 1, min_entries), 16u);
		}
		const uint32_t bank = std::min<uint32_t>(palette, 15u);
		return rice_crc32_wrapped(tlut_shadow, tlut_shadow_size, bank * 32, entries, 1, 2, 32);
	}

	return 0;
}

inline uint32_t compute_hires_ci_palette_crc(TextureSize size,
                                             uint32_t palette,
                                             const uint8_t *cpu_rdram,
                                             size_t rdram_size,
                                             uint32_t src_base_addr,
                                             uint32_t key_width_pixels,
                                             uint32_t key_height_pixels,
                                             uint32_t row_stride_bytes,
                                             const uint8_t *tlut_shadow,
                                             size_t tlut_shadow_size,
                                             bool tlut_shadow_valid)
{
	return compute_hires_ci_palette_crc_for_entries(
			size,
			palette,
			cpu_rdram,
			rdram_size,
			src_base_addr,
			key_width_pixels,
			key_height_pixels,
			row_stride_bytes,
			tlut_shadow,
			tlut_shadow_size,
			tlut_shadow_valid,
			1u,
			false);
}

inline HiresCiPaletteCrcCandidates compute_hires_ci_palette_crc_candidates(
		TextureSize size,
		uint32_t palette,
		const uint8_t *cpu_rdram,
		size_t rdram_size,
		uint32_t src_base_addr,
		uint32_t key_width_pixels,
		uint32_t key_height_pixels,
		uint32_t row_stride_bytes,
		const uint8_t *tlut_shadow,
		size_t tlut_shadow_size,
		bool tlut_shadow_valid)
{
	HiresCiPaletteCrcCandidates out = {};
	if (size != TextureSize::Bpp4 && size != TextureSize::Bpp8)
		return out;

	const uint32_t primary = compute_hires_ci_palette_crc_for_entries(
			size,
			palette,
			cpu_rdram,
			rdram_size,
			src_base_addr,
			key_width_pixels,
			key_height_pixels,
			row_stride_bytes,
			tlut_shadow,
			tlut_shadow_size,
			tlut_shadow_valid,
			1u,
			false);
	const uint32_t min2 = compute_hires_ci_palette_crc_for_entries(
			size,
			palette,
			cpu_rdram,
			rdram_size,
			src_base_addr,
			key_width_pixels,
			key_height_pixels,
			row_stride_bytes,
			tlut_shadow,
			tlut_shadow_size,
			tlut_shadow_valid,
			2u,
			false);
	const uint32_t full = compute_hires_ci_palette_crc_for_entries(
			size,
			palette,
			cpu_rdram,
			rdram_size,
			src_base_addr,
			key_width_pixels,
			key_height_pixels,
			row_stride_bytes,
			tlut_shadow,
			tlut_shadow_size,
			tlut_shadow_valid,
			1u,
			true);

	const uint32_t candidates[3] = { primary, min2, full };
	for (uint32_t candidate : candidates)
	{
		bool seen = false;
		for (uint32_t i = 0; i < out.count; i++)
		{
			if (out.values[i] == candidate)
			{
				seen = true;
				break;
			}
		}
		if (!seen && out.count < 3u)
			out.values[out.count++] = candidate;
	}

	return out;
}
}
}

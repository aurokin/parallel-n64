// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "rdp_common.hpp"
#include "texture_keying.hpp"

namespace RDP
{
namespace detail
{
inline uint32_t compute_hires_ci_palette_entry_count(TextureSize size,
                                                     const uint8_t *cpu_rdram,
                                                     size_t rdram_size,
                                                     uint32_t src_base_addr,
                                                     uint32_t key_width_pixels,
                                                     uint32_t key_height_pixels,
                                                     uint32_t row_stride_bytes)
{
	if (!cpu_rdram || rdram_size == 0)
		return 0;

	if (size == TextureSize::Bpp8)
	{
		const uint32_t cimax = compute_ci8_max_index(cpu_rdram, rdram_size, src_base_addr,
		                                              key_width_pixels, key_height_pixels, row_stride_bytes);
		return std::min<uint32_t>(cimax + 1, 256u);
	}

	if (size == TextureSize::Bpp4)
	{
		const uint32_t cimax = compute_ci4_max_index(cpu_rdram, rdram_size, src_base_addr,
		                                              key_width_pixels, key_height_pixels, row_stride_bytes);
		return std::min<uint32_t>(cimax + 1, 16u);
	}

	return 0;
}

inline uint32_t compute_hires_ci_palette_crc_for_entries(TextureSize size,
                                                         uint32_t palette,
                                                         const uint8_t *tlut_shadow,
                                                         size_t tlut_shadow_size,
                                                         bool tlut_shadow_valid,
                                                         uint32_t entries)
{
	if (!tlut_shadow_valid || !tlut_shadow || tlut_shadow_size < 512)
		return 0;

	if (size == TextureSize::Bpp8)
	{
		entries = std::min<uint32_t>(entries, 256u);
		if (entries == 0)
			return 0;
		return rice_crc32_wrapped(tlut_shadow, tlut_shadow_size, 0, entries, 1, 2, 512);
	}

	if (size == TextureSize::Bpp4)
	{
		entries = std::min<uint32_t>(entries, 16u);
		if (entries == 0)
			return 0;
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
	const uint32_t entries = compute_hires_ci_palette_entry_count(
			size,
			cpu_rdram,
			rdram_size,
			src_base_addr,
			key_width_pixels,
			key_height_pixels,
			row_stride_bytes);
	return compute_hires_ci_palette_crc_for_entries(
			size,
			palette,
			tlut_shadow,
			tlut_shadow_size,
			tlut_shadow_valid,
			entries);
}

}
}

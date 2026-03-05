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
	if (!tlut_shadow_valid || !cpu_rdram || !tlut_shadow || rdram_size == 0 || tlut_shadow_size < 512)
		return 0;

	if (size == TextureSize::Bpp8)
	{
		const uint32_t cimax = compute_ci8_max_index(cpu_rdram, rdram_size, src_base_addr,
		                                              key_width_pixels, key_height_pixels, row_stride_bytes);
		const uint32_t entries = std::min<uint32_t>(cimax + 1, 256u);
		return rice_crc32_wrapped(tlut_shadow, tlut_shadow_size, 0, entries, 1, 2, 512);
	}

	if (size == TextureSize::Bpp4)
	{
		const uint32_t cimax = compute_ci4_max_index(cpu_rdram, rdram_size, src_base_addr,
		                                              key_width_pixels, key_height_pixels, row_stride_bytes);
		const uint32_t entries = std::min<uint32_t>(cimax + 1, 16u);
		const uint32_t bank = std::min<uint32_t>(palette, 15u);
		return rice_crc32_wrapped(tlut_shadow, tlut_shadow_size, bank * 32, entries, 1, 2, 32);
	}

	return 0;
}
}
}

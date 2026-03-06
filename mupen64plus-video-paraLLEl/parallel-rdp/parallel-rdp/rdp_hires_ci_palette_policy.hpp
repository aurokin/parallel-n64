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
	uint32_t values[6] = {};
	uint32_t count = 0;
};

inline uint32_t compute_hires_ci_palette_entry_count(TextureSize size,
                                                     const uint8_t *cpu_rdram,
                                                     size_t rdram_size,
                                                     uint32_t src_base_addr,
                                                     uint32_t key_width_pixels,
                                                     uint32_t key_height_pixels,
                                                     uint32_t row_stride_bytes,
                                                     uint32_t min_entries,
                                                     bool full_bank)
{
	if (size == TextureSize::Bpp8)
	{
		if (full_bank)
			return 256u;
		const uint32_t cimax = compute_ci8_max_index(cpu_rdram, rdram_size, src_base_addr,
		                                             key_width_pixels, key_height_pixels, row_stride_bytes);
		return std::min<uint32_t>(std::max<uint32_t>(cimax + 1, min_entries), 256u);
	}

	if (size == TextureSize::Bpp4)
	{
		if (full_bank)
			return 16u;
		const uint32_t cimax = compute_ci4_max_index(cpu_rdram, rdram_size, src_base_addr,
		                                             key_width_pixels, key_height_pixels, row_stride_bytes);
		return std::min<uint32_t>(std::max<uint32_t>(cimax + 1, min_entries), 16u);
	}

	return 0u;
}

inline uint32_t compute_hires_ci4_palette_base_offset_bytes(uint32_t palette,
                                                            bool alt_crc_layout)
{
	const uint32_t bank = std::min<uint32_t>(palette, 15u);
	return bank * (alt_crc_layout ? 64u : 32u);
}

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
                                                         bool full_bank,
                                                         bool ci4_alt_crc_layout = false)
{
	if (!tlut_shadow_valid || !cpu_rdram || !tlut_shadow || rdram_size == 0)
		return 0;

	if (size == TextureSize::Bpp8)
	{
		if (tlut_shadow_size < 512)
			return 0;
		const uint32_t entries = compute_hires_ci_palette_entry_count(
				size,
				cpu_rdram,
				rdram_size,
				src_base_addr,
				key_width_pixels,
				key_height_pixels,
				row_stride_bytes,
				min_entries,
				full_bank);
		if (entries == 0)
			return 0;
		if (entries > 256u)
			return 0;
		return rice_crc32_wrapped(tlut_shadow, tlut_shadow_size, 0, entries, 1, 2, 512);
	}

	if (size == TextureSize::Bpp4)
	{
		if (tlut_shadow_size < 512)
			return 0;
		const uint32_t entries = compute_hires_ci_palette_entry_count(
				size,
				cpu_rdram,
				rdram_size,
				src_base_addr,
				key_width_pixels,
				key_height_pixels,
				row_stride_bytes,
				min_entries,
				full_bank);
		if (entries == 0)
			return 0;
		if (entries > 16u)
			return 0;

		const uint32_t base_offset = compute_hires_ci4_palette_base_offset_bytes(palette, ci4_alt_crc_layout);
		const uint32_t palette_bytes = entries * 2u;
		if (base_offset >= tlut_shadow_size || palette_bytes > (tlut_shadow_size - base_offset))
			return 0;

		return rice_crc32_wrapped(tlut_shadow, tlut_shadow_size, base_offset, entries, 1, 2, 32);
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
			false,
			false);
}

inline void append_hires_ci_palette_candidate(HiresCiPaletteCrcCandidates &out,
                                              uint32_t candidate)
{
	for (uint32_t i = 0; i < out.count; i++)
	{
		if (out.values[i] == candidate)
			return;
	}

	if (out.count < uint32_t(sizeof(out.values) / sizeof(out.values[0])))
		out.values[out.count++] = candidate;
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
			false,
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
			false,
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
			true,
			false);

	append_hires_ci_palette_candidate(out, primary);
	append_hires_ci_palette_candidate(out, min2);
	append_hires_ci_palette_candidate(out, full);

	if (size == TextureSize::Bpp4)
	{
		const uint32_t alt_primary = compute_hires_ci_palette_crc_for_entries(
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
				false,
				true);
		const uint32_t alt_min2 = compute_hires_ci_palette_crc_for_entries(
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
				false,
				true);
		const uint32_t alt_full = compute_hires_ci_palette_crc_for_entries(
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
				true,
				true);

		append_hires_ci_palette_candidate(out, alt_primary);
		append_hires_ci_palette_candidate(out, alt_min2);
		append_hires_ci_palette_candidate(out, alt_full);
	}

	return out;
}
}
}

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include "rdp_common.hpp"
#include "rdp_hires_key_state_policy.hpp"
#include "rdp_hires_runtime_policy.hpp"

namespace RDP
{
namespace detail
{
enum class HiresLookupBirthFamily : uint8_t
{
	SameFormatsizeOwnerTile = 0,
	SameFormatsizeAliasTile,
	CrossFormatsizeOwnerTile,
	CrossFormatsizeAliasTile
};

struct HiresLookupBirthPattern
{
	uint16_t load_formatsize = 0;
	uint16_t lookup_formatsize = 0;
	uint16_t key_width = 0;
	uint16_t key_height = 0;
};

struct HiresReinterpretationBirthPatternPolicy
{
	const HiresLookupBirthPattern *patterns = nullptr;
	size_t pattern_count = 0;
};

inline HiresLookupBirthPattern make_hires_lookup_birth_pattern(uint16_t load_formatsize,
                                                               uint16_t lookup_formatsize,
                                                               uint16_t key_width,
                                                               uint16_t key_height)
{
	HiresLookupBirthPattern pattern = {};
	pattern.load_formatsize = load_formatsize;
	pattern.lookup_formatsize = lookup_formatsize;
	pattern.key_width = key_width;
	pattern.key_height = key_height;
	return pattern;
}

inline HiresReinterpretationBirthPatternPolicy make_hires_reinterpretation_birth_pattern_policy(
		const HiresLookupBirthPattern *patterns,
		size_t pattern_count)
{
	HiresReinterpretationBirthPatternPolicy policy = {};
	policy.patterns = patterns;
	policy.pattern_count = pattern_count;
	return policy;
}

inline uint8_t hires_lookup_birth_family_bit(HiresLookupBirthFamily family)
{
	return uint8_t(1u << unsigned(family));
}

inline bool is_hires_lookup_birth_cross_formatsize(const HiresLookupBirthSignature &signature)
{
	return signature.load_formatsize != signature.lookup_formatsize;
}

inline bool is_hires_lookup_birth_owner_tile(const HiresLookupBirthSignature &signature)
{
	return signature.lookup_tile_index == 0;
}

inline HiresLookupBirthFamily classify_hires_lookup_birth_family(const HiresLookupBirthSignature &signature)
{
	if (is_hires_lookup_birth_cross_formatsize(signature))
	{
		return is_hires_lookup_birth_owner_tile(signature) ?
		               HiresLookupBirthFamily::CrossFormatsizeOwnerTile :
		               HiresLookupBirthFamily::CrossFormatsizeAliasTile;
	}

	return is_hires_lookup_birth_owner_tile(signature) ?
	               HiresLookupBirthFamily::SameFormatsizeOwnerTile :
	               HiresLookupBirthFamily::SameFormatsizeAliasTile;
}

inline bool should_accept_hires_reinterpretation_birth_family(const HiresLookupModePolicy &policy,
                                                              const HiresLookupBirthSignature &signature)
{
	const auto family = classify_hires_lookup_birth_family(signature);
	return (policy.reinterpretation_birth_family_mask & hires_lookup_birth_family_bit(family)) != 0;
}

inline bool matches_hires_lookup_birth_pattern(const HiresLookupBirthSignature &signature,
                                               const HiresLookupBirthPattern &pattern)
{
	return signature.load_formatsize == pattern.load_formatsize &&
	       signature.lookup_formatsize == pattern.lookup_formatsize &&
	       signature.key_width == pattern.key_width &&
	       signature.key_height == pattern.key_height;
}

inline HiresReinterpretationBirthPatternPolicy resolve_hires_reinterpretation_birth_pattern_policy(
		const HiresLookupModePolicy &policy)
{
	static const HiresLookupBirthPattern narrow_paper_mario_patterns[] = {
		make_hires_lookup_birth_pattern(0x300u, 0x300u, 32u, 32u),
		make_hires_lookup_birth_pattern(0x202u, 0x02u, 16u, 16u),
		make_hires_lookup_birth_pattern(0x202u, 0x02u, 32u, 16u),
	};
	static const HiresLookupBirthPattern narrow_same_formatsize_32x32_patterns[] = {
		make_hires_lookup_birth_pattern(0x300u, 0x300u, 32u, 32u),
	};
	static const HiresLookupBirthPattern narrow_cross_formatsize_16x16_patterns[] = {
		make_hires_lookup_birth_pattern(0x202u, 0x02u, 16u, 16u),
	};
	static const HiresLookupBirthPattern narrow_cross_formatsize_32x16_patterns[] = {
		make_hires_lookup_birth_pattern(0x202u, 0x02u, 32u, 16u),
	};
	static const HiresLookupBirthPattern narrow_same_32x32_cross_16x16_patterns[] = {
		make_hires_lookup_birth_pattern(0x300u, 0x300u, 32u, 32u),
		make_hires_lookup_birth_pattern(0x202u, 0x02u, 16u, 16u),
	};
	static const HiresLookupBirthPattern narrow_same_32x32_cross_32x16_patterns[] = {
		make_hires_lookup_birth_pattern(0x300u, 0x300u, 32u, 32u),
		make_hires_lookup_birth_pattern(0x202u, 0x02u, 32u, 16u),
	};

	switch (policy.reinterpretation_birth_pattern_mode)
	{
	case HiresReinterpretationBirthPatternMode::AllowAll:
		return {};

	case HiresReinterpretationBirthPatternMode::NarrowPaperMarioProbe:
		return make_hires_reinterpretation_birth_pattern_policy(
				narrow_paper_mario_patterns,
				sizeof(narrow_paper_mario_patterns) / sizeof(narrow_paper_mario_patterns[0]));

	case HiresReinterpretationBirthPatternMode::NarrowSameFormatsize32x32Probe:
		return make_hires_reinterpretation_birth_pattern_policy(
				narrow_same_formatsize_32x32_patterns,
				sizeof(narrow_same_formatsize_32x32_patterns) / sizeof(narrow_same_formatsize_32x32_patterns[0]));

	case HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize16x16Probe:
		return make_hires_reinterpretation_birth_pattern_policy(
				narrow_cross_formatsize_16x16_patterns,
				sizeof(narrow_cross_formatsize_16x16_patterns) / sizeof(narrow_cross_formatsize_16x16_patterns[0]));

	case HiresReinterpretationBirthPatternMode::NarrowCrossFormatsize32x16Probe:
		return make_hires_reinterpretation_birth_pattern_policy(
				narrow_cross_formatsize_32x16_patterns,
				sizeof(narrow_cross_formatsize_32x16_patterns) / sizeof(narrow_cross_formatsize_32x16_patterns[0]));

	case HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross16x16Probe:
		return make_hires_reinterpretation_birth_pattern_policy(
				narrow_same_32x32_cross_16x16_patterns,
				sizeof(narrow_same_32x32_cross_16x16_patterns) / sizeof(narrow_same_32x32_cross_16x16_patterns[0]));

	case HiresReinterpretationBirthPatternMode::NarrowSame32x32Cross32x16Probe:
		return make_hires_reinterpretation_birth_pattern_policy(
				narrow_same_32x32_cross_32x16_patterns,
				sizeof(narrow_same_32x32_cross_32x16_patterns) / sizeof(narrow_same_32x32_cross_32x16_patterns[0]));

	default:
		return {};
	}
}

inline bool should_accept_hires_reinterpretation_birth_pattern(const HiresLookupModePolicy &policy,
                                                               const HiresLookupBirthSignature &signature)
{
	const auto pattern_policy = resolve_hires_reinterpretation_birth_pattern_policy(policy);
	if (pattern_policy.pattern_count == 0 || pattern_policy.patterns == nullptr)
		return true;

	for (size_t i = 0; i < pattern_policy.pattern_count; i++)
		if (matches_hires_lookup_birth_pattern(signature, pattern_policy.patterns[i]))
			return true;

	return false;
}

inline bool hires_rdram_view_valid(const void *cpu_rdram, size_t rdram_size)
{
	return cpu_rdram && rdram_size && ((rdram_size & (rdram_size - 1)) == 0);
}

inline bool should_update_tlut_shadow(bool rdram_view_ok, bool is_tlut_mode)
{
	return rdram_view_ok && is_tlut_mode;
}

inline bool should_run_hires_lookup(bool rdram_view_ok,
                                    bool has_replacement_provider,
                                    bool is_tlut_mode,
                                    uint32_t key_width_pixels,
                                    uint32_t key_height_pixels)
{
	return rdram_view_ok &&
	       has_replacement_provider &&
	       !is_tlut_mode &&
	       key_width_pixels > 0 &&
	       key_height_pixels > 0;
}

inline bool should_try_hires_ci_palette_candidates(TextureFormat fmt,
                                                   TextureSize size,
                                                   bool tlut_shadow_valid)
{
	return tlut_shadow_valid &&
	       fmt == TextureFormat::CI &&
	       (size == TextureSize::Bpp4 || size == TextureSize::Bpp8);
}

inline bool should_accept_hires_ci_ambiguous_fallback(bool allow_without_palette_match,
                                                      uint32_t preferred_palette_hint,
                                                      bool matched_preferred_palette)
{
	return allow_without_palette_match ||
	       preferred_palette_hint == 0 ||
	       matched_preferred_palette;
}

inline bool should_try_hires_ci_low32_fallback(const HiresLookupModePolicy &policy)
{
	return policy.allow_ci_low32;
}

inline bool should_try_hires_tile_mask_fallback(const HiresLookupModePolicy &policy, bool is_tile_mode)
{
	return policy.allow_tile_mask && is_tile_mode;
}

inline bool should_try_hires_tile_stride_fallback(const HiresLookupModePolicy &policy, bool is_tile_mode)
{
	return policy.allow_tile_stride && is_tile_mode;
}

inline bool should_try_hires_block_tile_fallback(const HiresLookupModePolicy &policy, bool is_block_mode)
{
	return policy.allow_block_tile && is_block_mode;
}

inline bool should_try_hires_block_shape_fallback(const HiresLookupModePolicy &policy, bool is_block_mode)
{
	return policy.allow_block_shape && is_block_mode;
}

inline uint32_t compute_hires_key_base_addr(uint32_t tex_addr,
                                            uint32_t tex_width,
                                            uint32_t key_start_x,
                                            uint32_t key_start_y,
                                            TextureSize size,
                                            bool is_load_block_mode)
{
	if (is_load_block_mode)
		return tex_addr;

	const uint32_t pixel_offset = tex_width * key_start_y + key_start_x;
	const uint32_t size_bits = unsigned(size);
	if (size_bits == 0)
		return tex_addr + (pixel_offset >> 1);
	return tex_addr + (pixel_offset << (size_bits - 1u));
}

inline uint32_t compute_hires_block_probe_base_addr(uint32_t tex_addr,
                                                    uint32_t probe_width,
                                                    uint32_t probe_start_x,
                                                    uint32_t probe_start_y,
                                                    TextureSize probe_size)
{
	return compute_hires_key_base_addr(
			tex_addr,
			probe_width,
			probe_start_x,
			probe_start_y,
			probe_size,
			false);
}

inline uint32_t compute_hires_texture_row_bytes(uint32_t width_pixels,
                                                TextureSize size)
{
	switch (size)
	{
	case TextureSize::Bpp4:
		return (width_pixels + 1u) >> 1u;
	case TextureSize::Bpp8:
		return width_pixels;
	case TextureSize::Bpp16:
		return width_pixels << 1u;
	case TextureSize::Bpp32:
		return width_pixels << 2u;
	default:
		return 0;
	}
}

inline uint32_t compute_hires_texture_total_bytes(uint32_t width_pixels,
                                                  uint32_t height_pixels,
                                                  TextureSize size)
{
	return compute_hires_texture_row_bytes(width_pixels, size) * height_pixels;
}

inline uint32_t compute_hires_block_reinterpret_height(uint32_t total_bytes,
                                                       uint32_t width_pixels,
                                                       TextureSize size)
{
	const uint32_t row_bytes = compute_hires_texture_row_bytes(width_pixels, size);
	if (row_bytes == 0 || total_bytes == 0 || (total_bytes % row_bytes) != 0)
		return 0;

	return total_bytes / row_bytes;
}

inline uint32_t derive_hires_tile_lookup_dim(uint32_t raw_dim,
                                             uint8_t tile_mask,
                                             uint32_t max_dim)
{
	uint32_t dim = raw_dim;
	if (tile_mask != 0)
	{
		const uint32_t mask_dim = 1u << std::min<unsigned>(tile_mask, 10u);
		dim = std::min(dim, mask_dim);
	}
	if (max_dim != 0)
		dim = std::min(dim, max_dim);
	return dim;
}

inline uint32_t hires_calculate_dxt(uint32_t txl2words)
{
	if (txl2words == 0)
		return 1;
	return (2048u + txl2words - 1u) / txl2words;
}

inline uint32_t hires_txl2words(uint32_t width_pixels, TextureSize size)
{
	switch (size)
	{
	case TextureSize::Bpp4:
		return std::max(1u, width_pixels / 16u);
	case TextureSize::Bpp8:
		return std::max(1u, width_pixels / 8u);
	case TextureSize::Bpp16:
		return std::max(1u, width_pixels / 4u);
	case TextureSize::Bpp32:
		return std::max(1u, width_pixels / 2u);
	default:
		return 1u;
	}
}

inline uint32_t hires_reverse_dxt(uint32_t dxt,
                                  uint32_t load_width_pixels,
                                  uint32_t target_width_pixels,
                                  TextureSize target_size)
{
	if (dxt == 0x800u)
		return 1u;
	if (dxt <= 1u)
		return dxt;

	uint32_t low = 2047u / dxt;
	if (hires_calculate_dxt(low) > dxt)
		low++;
	const uint32_t high = 2047u / (dxt - 1u);
	if (low == high)
		return low;

	const uint32_t target_words = hires_txl2words(target_width_pixels, target_size);
	for (uint32_t i = low; i <= high; i++)
	{
		if (target_words == i)
			return i;
	}

	(void)load_width_pixels;
	return (low + high) / 2u;
}

inline uint32_t compute_hires_block_row_stride_bytes(uint32_t dxt,
                                                     uint32_t load_width_pixels,
                                                     uint32_t target_width_pixels,
                                                     TextureSize target_size,
                                                     uint32_t stride_from_tile_bytes)
{
	if (dxt == 0)
		return stride_from_tile_bytes;

	uint32_t stride_words = dxt;
	if (dxt > 1u)
		stride_words = hires_reverse_dxt(dxt, load_width_pixels, target_width_pixels, target_size);

	return stride_words << 3;
}

inline uint32_t compute_hires_width_from_row_stride(uint32_t row_stride_bytes,
                                                    TextureSize size)
{
	if (row_stride_bytes == 0)
		return 0;

	switch (size)
	{
	case TextureSize::Bpp4:
		return row_stride_bytes << 1u;
	case TextureSize::Bpp8:
		return row_stride_bytes;
	case TextureSize::Bpp16:
		return std::max(1u, row_stride_bytes >> 1u);
	case TextureSize::Bpp32:
		return std::max(1u, row_stride_bytes >> 2u);
	default:
		return 0;
	}
}

inline void record_hires_lookup_result(bool hit,
                                       uint64_t &lookup_total,
                                       uint64_t &lookup_hits,
                                       uint64_t &lookup_misses)
{
	lookup_total++;
	if (hit)
		lookup_hits++;
	else
		lookup_misses++;
}

inline bool did_hires_lookup_bind_descriptor(bool provider_hit, uint32_t descriptor_index)
{
	return provider_hit && hires_descriptor_index_valid(descriptor_index);
}

inline void record_hires_lookup_binding_result(bool provider_hit,
                                               bool descriptor_bound,
                                               uint64_t &lookup_total,
                                               uint64_t &provider_hits,
                                               uint64_t &provider_misses,
                                               uint64_t &descriptor_bound_hits,
                                               uint64_t &descriptor_unbound_hits)
{
	lookup_total++;
	if (provider_hit)
		provider_hits++;
	else
		provider_misses++;

	if (descriptor_bound)
		descriptor_bound_hits++;
	else if (provider_hit)
		descriptor_unbound_hits++;
}
}
}

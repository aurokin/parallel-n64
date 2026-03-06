#pragma once

#include <algorithm>
#include <cstdint>

namespace RDP
{
namespace detail
{
enum class HiresFilterMode : uint8_t
{
	Nearest = 0,
	Linear = 1,
	Trilinear = 2,
};

enum class HiresSrgbMode : uint8_t
{
	Auto = 0,
	On = 1,
	Off = 2,
};

inline HiresFilterMode sanitize_hires_filter_mode(unsigned mode)
{
	switch (mode)
	{
	case 0:
		return HiresFilterMode::Nearest;
	case 2:
		return HiresFilterMode::Trilinear;
	default:
		return HiresFilterMode::Linear;
	}
}

inline HiresSrgbMode sanitize_hires_srgb_mode(unsigned mode)
{
	switch (mode)
	{
	case 1:
		return HiresSrgbMode::On;
	case 2:
		return HiresSrgbMode::Off;
	default:
		return HiresSrgbMode::Auto;
	}
}

inline bool hires_filter_uses_mipmaps(HiresFilterMode mode)
{
	return mode == HiresFilterMode::Trilinear;
}

inline bool resolve_hires_upload_srgb(HiresSrgbMode mode, bool replacement_srgb)
{
	switch (mode)
	{
	case HiresSrgbMode::On:
		return true;
	case HiresSrgbMode::Off:
		return false;
	case HiresSrgbMode::Auto:
	default:
		return replacement_srgb;
	}
}

inline uint16_t pack_hires_copy_rgba5551(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	const uint16_t alpha_bit = (a != 0) ? 1u : 0u;
	return static_cast<uint16_t>(
			((uint16_t(r) & 0xf8u) << 8u) |
			((uint16_t(g) & 0xf8u) << 3u) |
			((uint16_t(b) & 0xf8u) >> 2u) |
			alpha_bit);
}

inline uint32_t derive_hires_tile_span_texels(uint32_t lo, uint32_t hi)
{
	return (((hi >> 2) - (lo >> 2)) + 1u) & 0xfffu;
}

inline uint32_t derive_hires_mask_span_texels(uint8_t mask)
{
	if (mask == 0)
		return 0;
	const uint8_t clamped_mask = std::min<uint8_t>(mask, 11u);
	return 1u << clamped_mask;
}

inline uint32_t select_hires_sampling_orig_dim(uint32_t key_dim,
                                               uint32_t tile_lo,
                                               uint32_t tile_hi,
                                               uint8_t tile_mask)
{
	uint32_t dim = key_dim;

	const uint32_t tile_dim = derive_hires_tile_span_texels(tile_lo, tile_hi);
	if (tile_dim != 0 && (dim == 0 || tile_dim < dim))
		dim = tile_dim;

	const uint32_t mask_dim = derive_hires_mask_span_texels(tile_mask);
	if (mask_dim != 0 && (dim == 0 || mask_dim < dim))
		dim = mask_dim;

	return dim != 0 ? dim : 1u;
}
}
}

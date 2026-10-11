#pragma once

#include "rdp_common.hpp"

namespace RDP
{
namespace detail
{
struct HiresCI4PaletteBinding
{
	bool valid = false;
	uint8_t palette = 0;
};

// CI8 load descriptors do not select a 16-entry palette bank. If their
// byte extent is re-keyed as CI4, its lookup uses the bank-zero palette.
inline uint8_t hires_ci4_lookup_palette(TextureSize source_size, uint32_t source_palette)
{
	return source_size == TextureSize::Bpp4 ? uint8_t(source_palette & 15u) : 0;
}

inline HiresCI4PaletteBinding make_hires_ci4_palette_binding(uint16_t formatsize, uint32_t lookup_palette)
{
	HiresCI4PaletteBinding binding;
	binding.valid = formatsize == (uint16_t(TextureFormat::CI) | (uint16_t(TextureSize::Bpp4) << 8));
	binding.palette = uint8_t(lookup_palette & 15u);
	return binding;
}

// Check lookup provenance at the live draw, rather than the intermediate
// SetTile descriptor that may still predate a propagated texture load.
inline bool hires_ci4_binding_matches(const HiresCI4PaletteBinding &binding,
                                     TextureFormat format, TextureSize size,
                                     uint32_t palette, bool sampled)
{
	return !sampled || format != TextureFormat::CI || size != TextureSize::Bpp4 ||
	       (binding.valid && binding.palette == uint8_t(palette & 15u));
}

}
}

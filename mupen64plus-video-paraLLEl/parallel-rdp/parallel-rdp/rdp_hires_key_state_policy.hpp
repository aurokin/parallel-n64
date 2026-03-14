#pragma once

#include <algorithm>
#include <cstdint>
#include "rdp_hires_runtime_policy.hpp"

namespace RDP
{
namespace detail
{
inline uint64_t compose_hires_checksum64(uint32_t texture_crc, uint32_t palette_crc)
{
	return (uint64_t(palette_crc) << 32) | uint64_t(texture_crc);
}

inline uint16_t clamp_hires_dimension_u16(uint32_t dim)
{
	return static_cast<uint16_t>(std::min<uint32_t>(dim, 0xffffu));
}

template <typename TileState>
inline auto write_hires_lookup_tile_source(TileState &state,
                                           HiresLookupSource lookup_source,
                                           int) -> decltype(state.lookup_source = lookup_source, void())
{
	state.lookup_source = lookup_source;
}

template <typename TileState>
inline void write_hires_lookup_tile_source(TileState &,
                                           HiresLookupSource,
                                           long)
{
}

template <typename TileState>
inline void write_hires_lookup_tile_state(TileState &state,
                                          bool hit,
                                          uint64_t checksum64,
                                          uint16_t formatsize,
                                          uint32_t orig_w,
                                          uint32_t orig_h,
                                          uint32_t vk_image_index = 0xffffffffu,
                                          uint32_t repl_w = 0,
                                          uint32_t repl_h = 0,
                                          bool has_mips = false,
                                          bool allow_tile_sampling_expansion = true,
                                          HiresLookupSource lookup_source = HiresLookupSource::None)
{
	state.valid = true;
	state.hit = hit;
	state.checksum64 = checksum64;
	state.formatsize = formatsize;
	state.orig_w = clamp_hires_dimension_u16(orig_w);
	state.orig_h = clamp_hires_dimension_u16(orig_h);
	state.vk_image_index = vk_image_index;
	state.repl_w = clamp_hires_dimension_u16(repl_w);
	state.repl_h = clamp_hires_dimension_u16(repl_h);
	state.has_mips = has_mips;
	state.allow_tile_sampling_expansion = allow_tile_sampling_expansion;
	write_hires_lookup_tile_source(state, lookup_source, 0);
}
}
}

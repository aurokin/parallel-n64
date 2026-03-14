#pragma once

#include "rdp_common.hpp"
#include "rdp_hires_key_state_policy.hpp"
#include "rdp_hires_runtime_policy.hpp"

namespace RDP
{
namespace detail
{
template <typename ReplacementTileStateType>
inline HiresLookupBirthSignature read_hires_lookup_tile_provenance(const ReplacementTileStateType &state)
{
	return make_hires_lookup_birth_signature(
			state.source_load_tile_index,
			state.source_load_formatsize,
			state.source_lookup_tile_index,
			state.source_lookup_formatsize,
			state.source_key_width,
			state.source_key_height);
}

template <typename ReplacementTileStateType>
inline bool is_hires_cross_formatsize_16x16_100x100_consumer_candidate(const ReplacementTileStateType &state,
                                                                       const TileInfo &tile)
{
	if (!hires_descriptor_index_valid(tile.replacement.repl_desc_index))
		return false;

	const auto birth = read_hires_lookup_tile_provenance(state);
	return birth.load_formatsize == 0x202u &&
	       birth.lookup_formatsize == 0x02u &&
	       birth.key_width == 16u &&
	       birth.key_height == 16u &&
	       tile.replacement.repl_w == 100u &&
	       tile.replacement.repl_h == 100u;
}

template <typename ReplacementTileStateType>
inline bool should_consume_hires_replacement_for_draw(const HiresLookupModePolicy &policy,
                                                      uint32_t raw_raster_flags,
                                                      const ReplacementTileStateType &state,
                                                      const TileInfo &tile)
{
	if (policy.consumer_pattern_mode != HiresConsumerPatternMode::CrossFormatsize16x16PrimaryPhaseOnlyProbe)
		return true;

	if (!is_hires_cross_formatsize_16x16_100x100_consumer_candidate(state, tile))
		return true;

	return raw_raster_flags == 0x21864010u;
}

template <typename ReplacementTileStateType, size_t NumTiles>
inline void apply_hires_draw_consumer_policy(const HiresLookupModePolicy &policy,
                                             uint32_t raw_raster_flags,
                                             const ReplacementTileStateType (&replacement_states)[NumTiles],
                                             TileInfo (&draw_tiles)[NumTiles])
{
	for (size_t i = 0; i < NumTiles; i++)
	{
		if (!should_consume_hires_replacement_for_draw(policy, raw_raster_flags, replacement_states[i], draw_tiles[i]))
			draw_tiles[i].replacement = {};
	}
}
}
}

#pragma once

#include "rdp_common.hpp"
#include "rdp_hires_key_state_policy.hpp"
#include "rdp_hires_ownership_policy.hpp"
#include "rdp_hires_runtime_policy.hpp"

namespace RDP
{
namespace detail
{

enum class HiresConsumerArchetype : uint8_t
{
	None = 0,
	CrossFormatsize16x16PrimaryAliasConsumer,
	CrossFormatsize16x16SecondaryAliasConsumer,
	CrossFormatsize16x16PrimaryOwnerLike,
	CrossFormatsize16x16SecondaryOwnerLike,
	CrossFormatsize32x16PendingProducer,
	CrossFormatsize32x16AliasConsumer,
	SameFormatsize32x32AliasConsumer,
	OtherReplacement
};

inline const char *hires_consumer_archetype_name(HiresConsumerArchetype archetype)
{
	switch (archetype)
	{
	case HiresConsumerArchetype::None:
		return "none";
	case HiresConsumerArchetype::CrossFormatsize16x16PrimaryAliasConsumer:
		return "cross16x16_primary_alias";
	case HiresConsumerArchetype::CrossFormatsize16x16SecondaryAliasConsumer:
		return "cross16x16_secondary_alias";
	case HiresConsumerArchetype::CrossFormatsize16x16PrimaryOwnerLike:
		return "cross16x16_primary_owner";
	case HiresConsumerArchetype::CrossFormatsize16x16SecondaryOwnerLike:
		return "cross16x16_secondary_owner";
	case HiresConsumerArchetype::CrossFormatsize32x16PendingProducer:
		return "cross32x16_pending";
	case HiresConsumerArchetype::CrossFormatsize32x16AliasConsumer:
		return "cross32x16_alias";
	case HiresConsumerArchetype::SameFormatsize32x32AliasConsumer:
		return "same32x32_alias";
	default:
		return "other";
	}
}

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

template <typename ReplacementTileStateType, typename TileInfoType>
inline bool is_hires_cross_formatsize_16x16_100x100_consumer_candidate(const ReplacementTileStateType &state,
                                                                       const TileInfoType &tile)
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

template <typename ReplacementTileStateType, typename TileInfoType>
inline bool is_hires_cross_formatsize_32x16_512x256_consumer_candidate(const ReplacementTileStateType &state,
                                                                       const TileInfoType &tile)
{
	if (!hires_descriptor_index_valid(tile.replacement.repl_desc_index))
		return false;

	const auto birth = read_hires_lookup_tile_provenance(state);
	return birth.load_formatsize == 0x202u &&
	       birth.lookup_formatsize == 0x02u &&
	       birth.key_width == 32u &&
	       birth.key_height == 16u &&
	       tile.replacement.repl_w == 512u &&
	       tile.replacement.repl_h == 256u;
}

template <typename ReplacementTileStateType, typename TileInfoType>
inline bool is_hires_same_formatsize_32x32_320x320_consumer_candidate(const ReplacementTileStateType &state,
                                                                      const TileInfoType &tile)
{
	if (!hires_descriptor_index_valid(tile.replacement.repl_desc_index))
		return false;

	const auto birth = read_hires_lookup_tile_provenance(state);
	return birth.load_formatsize == 0x300u &&
	       birth.lookup_formatsize == 0x300u &&
	       birth.key_width == 32u &&
	       birth.key_height == 32u &&
	       tile.replacement.repl_w == 320u &&
	       tile.replacement.repl_h == 320u;
}

template <typename ReplacementTileStateType, typename TileInfoType>
inline HiresConsumerArchetype classify_hires_consumer_archetype(uint32_t raw_raster_flags,
                                                                HiresDrawOwnershipClass draw_ownership_class,
                                                                const ReplacementTileStateType &state,
                                                                const TileInfoType &tile)
{
	if (!hires_descriptor_index_valid(tile.replacement.repl_desc_index))
		return HiresConsumerArchetype::None;

	if (is_hires_cross_formatsize_16x16_100x100_consumer_candidate(state, tile))
	{
		const bool primary_phase = raw_raster_flags == 0x21864010u;
		if (draw_ownership_class == HiresDrawOwnershipClass::AliasConsumer ||
		    draw_ownership_class == HiresDrawOwnershipClass::DescriptorlessConsumer)
		{
			return primary_phase ?
				HiresConsumerArchetype::CrossFormatsize16x16PrimaryAliasConsumer :
				HiresConsumerArchetype::CrossFormatsize16x16SecondaryAliasConsumer;
		}
		return primary_phase ?
			HiresConsumerArchetype::CrossFormatsize16x16PrimaryOwnerLike :
			HiresConsumerArchetype::CrossFormatsize16x16SecondaryOwnerLike;
	}

	if (is_hires_cross_formatsize_32x16_512x256_consumer_candidate(state, tile))
	{
		if (state.lookup_source == HiresLookupSource::PendingBlockRetry)
			return HiresConsumerArchetype::CrossFormatsize32x16PendingProducer;
		if (state.lookup_source == HiresLookupSource::AliasPropagated ||
		    state.lookup_source == HiresLookupSource::BlockTile ||
		    state.origin_lookup_source == HiresLookupSource::BlockTile ||
		    draw_ownership_class == HiresDrawOwnershipClass::AliasConsumer ||
		    draw_ownership_class == HiresDrawOwnershipClass::DescriptorlessConsumer)
		{
			return HiresConsumerArchetype::CrossFormatsize32x16AliasConsumer;
		}
	}

	if (is_hires_same_formatsize_32x32_320x320_consumer_candidate(state, tile) &&
	    (state.lookup_source == HiresLookupSource::AliasPropagated ||
	     state.lookup_source == HiresLookupSource::BlockTile ||
	     state.origin_lookup_source == HiresLookupSource::BlockTile ||
	     draw_ownership_class == HiresDrawOwnershipClass::AliasConsumer))
	{
		return HiresConsumerArchetype::SameFormatsize32x32AliasConsumer;
	}

	return HiresConsumerArchetype::OtherReplacement;
}

template <typename ReplacementTileStateType>
inline bool should_consume_hires_replacement_for_draw(const HiresLookupModePolicy &policy,
                                                      uint32_t raw_raster_flags,
                                                      HiresDrawOwnershipClass draw_ownership_class,
                                                      const ReplacementTileStateType &state,
                                                      const TileInfo &tile)
{
	const auto archetype = classify_hires_consumer_archetype(raw_raster_flags, draw_ownership_class, state, tile);

	if (policy.cross_formatsize_32x16_source_filter != HiresCrossFormatsize32x16SourceFilter::AllowAll)
	{
		if (policy.cross_formatsize_32x16_source_filter == HiresCrossFormatsize32x16SourceFilter::PendingOnly)
		{
			if (archetype != HiresConsumerArchetype::CrossFormatsize32x16PendingProducer &&
			    archetype != HiresConsumerArchetype::CrossFormatsize32x16AliasConsumer)
				return true;
			return archetype == HiresConsumerArchetype::CrossFormatsize32x16PendingProducer;
		}
		else if (policy.cross_formatsize_32x16_source_filter == HiresCrossFormatsize32x16SourceFilter::AliasOnly)
		{
			if (archetype != HiresConsumerArchetype::CrossFormatsize32x16PendingProducer &&
			    archetype != HiresConsumerArchetype::CrossFormatsize32x16AliasConsumer)
				return true;
			return archetype == HiresConsumerArchetype::CrossFormatsize32x16AliasConsumer;
		}
	}

	if (policy.cross_formatsize_32x16_draw_ownership_mask != 0xffu &&
	    is_hires_cross_formatsize_32x16_512x256_consumer_candidate(state, tile))
	{
		return (policy.cross_formatsize_32x16_draw_ownership_mask &
		        hires_draw_ownership_class_mask_bit(draw_ownership_class)) != 0;
	}

	if (!policy.restrict_cross_formatsize_16x16_to_primary_phase)
		return true;

	if (archetype != HiresConsumerArchetype::CrossFormatsize16x16PrimaryAliasConsumer &&
	    archetype != HiresConsumerArchetype::CrossFormatsize16x16SecondaryAliasConsumer &&
	    archetype != HiresConsumerArchetype::CrossFormatsize16x16PrimaryOwnerLike &&
	    archetype != HiresConsumerArchetype::CrossFormatsize16x16SecondaryOwnerLike)
		return true;

	return archetype == HiresConsumerArchetype::CrossFormatsize16x16PrimaryAliasConsumer ||
	       archetype == HiresConsumerArchetype::CrossFormatsize16x16PrimaryOwnerLike;
}

template <typename ReplacementTileStateType, size_t NumTiles>
inline void apply_hires_draw_consumer_policy(const HiresLookupModePolicy &policy,
                                             uint32_t raw_raster_flags,
                                             HiresDrawOwnershipClass draw_ownership_class,
                                             const ReplacementTileStateType (&replacement_states)[NumTiles],
                                             TileInfo (&draw_tiles)[NumTiles])
{
	for (size_t i = 0; i < NumTiles; i++)
	{
		if (!should_consume_hires_replacement_for_draw(policy, raw_raster_flags, draw_ownership_class,
		                                               replacement_states[i], draw_tiles[i]))
			draw_tiles[i].replacement = {};
	}
}
}
}

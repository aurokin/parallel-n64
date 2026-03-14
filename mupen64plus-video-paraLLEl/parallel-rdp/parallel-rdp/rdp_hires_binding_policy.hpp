#pragma once

#include "rdp_hires_key_state_policy.hpp"
#include "rdp_hires_lookup_policy.hpp"
#include "rdp_hires_shader_policy.hpp"
#include "rdp_hires_tile_alias_policy.hpp"
#include "texture_replacement.hpp"

namespace RDP
{
namespace detail
{
enum class HiresBindingPolicyMode : uint8_t
{
	OwnerTileOnly = 0,
	PropagateAliasGroup
};

inline HiresBindingPolicyMode resolve_hires_binding_policy_mode(bool propagate_alias_group)
{
	return propagate_alias_group ? HiresBindingPolicyMode::PropagateAliasGroup :
	                               HiresBindingPolicyMode::OwnerTileOnly;
}

struct HiresLookupBindingDecision
{
	bool provider_hit = false;
	uint64_t checksum64 = 0;
	uint16_t load_formatsize = 0;
	uint16_t lookup_formatsize = 0;
	uint32_t lookup_tile_index = 0;
	uint32_t load_tile_index = 0;
	uint32_t lookup_key_width = 0;
	uint32_t lookup_key_height = 0;
	uint32_t sampling_orig_w = 0;
	uint32_t sampling_orig_h = 0;
	uint32_t descriptor_index = hires_invalid_descriptor_index();
	uint32_t repl_w = 0;
	uint32_t repl_h = 0;
	bool has_mips = false;
	bool allow_tile_sampling_expansion = true;
	HiresLookupSource lookup_source = HiresLookupSource::None;
};

template <typename TileInfoType>
inline HiresLookupBindingDecision build_hires_lookup_binding_decision(unsigned load_tile_index,
                                                                      uint16_t load_formatsize,
                                                                      unsigned lookup_tile_index,
                                                                      uint16_t lookup_formatsize,
                                                                      uint32_t lookup_key_width,
                                                                      uint32_t lookup_key_height,
                                                                      uint64_t checksum64,
                                                                      bool provider_hit,
                                                                      const ReplacementMeta &repl_meta,
                                                                      bool allow_tile_sampling_expansion,
                                                                      HiresLookupSource lookup_source,
                                                                      const TileInfoType &lookup_tile)
{
	HiresLookupBindingDecision decision = {};
	decision.provider_hit = provider_hit;
	decision.checksum64 = checksum64;
	decision.load_formatsize = load_formatsize;
	decision.lookup_formatsize = lookup_formatsize;
	decision.lookup_tile_index = lookup_tile_index;
	decision.load_tile_index = load_tile_index;
	decision.lookup_key_width = lookup_key_width;
	decision.lookup_key_height = lookup_key_height;
	decision.sampling_orig_w = select_hires_sampling_orig_width_for_tile(lookup_key_width, lookup_tile);
	decision.sampling_orig_h = select_hires_sampling_orig_height_for_tile(lookup_key_height, lookup_tile);
	decision.descriptor_index = repl_meta.vk_image_index;
	decision.repl_w = repl_meta.repl_w;
	decision.repl_h = repl_meta.repl_h;
	decision.has_mips = repl_meta.has_mips;
	decision.allow_tile_sampling_expansion = allow_tile_sampling_expansion;
	decision.lookup_source = lookup_source;
	return decision;
}

template <typename TileInfoType, typename ReplacementTileStateType, size_t NumTiles>
inline void apply_hires_lookup_binding_decision(const HiresLookupBindingDecision &decision,
                                                HiresBindingPolicyMode policy_mode,
                                                TileInfoType (&tile_infos)[NumTiles],
                                                ReplacementTileStateType (&tile_states)[NumTiles],
                                                uint64_t &alias_binding_applications)
{
	auto &repl_state = tile_states[decision.lookup_tile_index];
	write_hires_lookup_tile_state(
			repl_state,
			decision.provider_hit,
			decision.checksum64,
			decision.lookup_formatsize,
			decision.sampling_orig_w,
			decision.sampling_orig_h,
			decision.descriptor_index,
			decision.repl_w,
			decision.repl_h,
			decision.has_mips,
			decision.allow_tile_sampling_expansion,
			decision.lookup_source);
	write_hires_lookup_tile_provenance(
			repl_state,
			decision.load_tile_index,
			decision.load_formatsize,
			decision.lookup_tile_index,
			decision.lookup_formatsize,
			decision.lookup_key_width,
			decision.lookup_key_height,
			0);

	if (policy_mode == HiresBindingPolicyMode::PropagateAliasGroup)
	{
		propagate_hires_alias_group_binding(decision.lookup_tile_index, tile_infos, tile_states);

		for (unsigned alias_tile = 0; alias_tile < NumTiles; alias_tile++)
		{
			if (alias_tile != decision.lookup_tile_index &&
			    !should_apply_hires_propagated_binding(tile_infos[decision.lookup_tile_index].meta,
			                                           tile_infos[alias_tile].meta))
				continue;
			apply_hires_tile_replacement_binding(tile_infos[alias_tile], tile_states[alias_tile]);
			if (alias_tile != decision.lookup_tile_index)
				alias_binding_applications++;
		}
	}
	else
	{
		apply_hires_tile_replacement_binding(tile_infos[decision.lookup_tile_index], tile_states[decision.lookup_tile_index]);
	}
}
}
}

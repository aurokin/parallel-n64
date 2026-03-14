#pragma once

#include "rdp_common.hpp"
#include "rdp_hires_key_state_policy.hpp"
#include "rdp_hires_runtime_policy.hpp"
#include "rdp_hires_sampling_policy.hpp"

#include <cstddef>

namespace RDP
{
namespace detail
{
inline bool should_alias_hires_tile_binding(const TileMeta &source_meta,
                                            const TileMeta &target_meta)
{
	return source_meta.offset == target_meta.offset &&
	       source_meta.stride == target_meta.stride &&
	       source_meta.fmt == target_meta.fmt &&
	       source_meta.size == target_meta.size &&
	       source_meta.palette == target_meta.palette;
}

inline bool should_invalidate_hires_binding_on_load(const TileMeta &source_meta,
                                                    const TileMeta &target_meta)
{
	// Any load into the same TMEM base can invalidate previous replacement bindings,
	// even if descriptor fields (size/format/palette) changed in-between uploads.
	return source_meta.offset == target_meta.offset;
}

inline bool should_alias_hires_load_binding(const TileMeta &source_meta,
                                            const TileMeta &target_meta)
{
	// LOAD_BLOCK/LOAD_TILE commonly upload through one tile descriptor (often tile 7)
	// before sampling through another descriptor that points at the same TMEM base.
	// Allow offset-based aliasing so replacements follow this remap path.
	return source_meta.offset == target_meta.offset;
}

inline bool should_apply_hires_propagated_binding(const TileMeta &source_meta,
                                                  const TileMeta &target_meta)
{
	return should_alias_hires_tile_binding(source_meta, target_meta) ||
	       should_alias_hires_load_binding(source_meta, target_meta);
}

inline bool should_propagate_hires_alias_group_binding(bool strict_lookup)
{
	return !strict_lookup;
}

template <typename ReplacementTileStateType>
inline bool hires_tile_state_is_bindable(const ReplacementTileStateType &state)
{
	return state.valid &&
	       state.hit &&
	       hires_descriptor_index_valid(state.vk_image_index) &&
	       state.orig_w > 0 && state.orig_h > 0 &&
	       state.repl_w > 0 && state.repl_h > 0;
}

template <typename TileInfoType, typename ReplacementTileStateType, size_t NumTiles>
inline int find_hires_alias_source_tile(unsigned dst_tile,
                                        const TileInfoType (&tile_infos)[NumTiles],
                                        const ReplacementTileStateType (&tile_states)[NumTiles])
{
	const auto &dst_meta = tile_infos[dst_tile].meta;
	for (unsigned i = 0; i < NumTiles; i++)
	{
		if (i == dst_tile)
			continue;
		if (!hires_tile_state_is_bindable(tile_states[i]))
			continue;
		if (should_apply_hires_propagated_binding(tile_infos[i].meta, dst_meta))
			return int(i);
	}

	return -1;
}

template <typename TileInfoType, typename ReplacementTileStateType, size_t NumTiles>
inline void invalidate_hires_alias_group(unsigned owner_tile,
                                         const TileInfoType (&tile_infos)[NumTiles],
                                         ReplacementTileStateType (&tile_states)[NumTiles])
{
	const auto &owner_meta = tile_infos[owner_tile].meta;
	for (unsigned i = 0; i < NumTiles; i++)
	{
		if (i == owner_tile)
			continue;
		if (should_alias_hires_tile_binding(tile_infos[i].meta, owner_meta))
			tile_states[i] = {};
	}
}

template <typename TileInfoType, typename ReplacementTileStateType, size_t NumTiles>
inline void invalidate_hires_load_binding_group(unsigned owner_tile,
                                                const TileInfoType (&tile_infos)[NumTiles],
                                                ReplacementTileStateType (&tile_states)[NumTiles])
{
	const auto &owner_meta = tile_infos[owner_tile].meta;
	for (unsigned i = 0; i < NumTiles; i++)
	{
		if (i == owner_tile)
			continue;
		if (should_invalidate_hires_binding_on_load(tile_infos[i].meta, owner_meta))
			tile_states[i] = {};
	}
}

template <typename TileInfoType, typename ReplacementTileStateType, size_t NumTiles>
inline void propagate_hires_alias_group_binding(unsigned owner_tile,
                                                const TileInfoType (&tile_infos)[NumTiles],
                                                ReplacementTileStateType (&tile_states)[NumTiles])
{
	if (!hires_tile_state_is_bindable(tile_states[owner_tile]))
		return;

	const auto &owner_meta = tile_infos[owner_tile].meta;
	for (unsigned i = 0; i < NumTiles; i++)
	{
		if (i == owner_tile)
			continue;
		if (should_apply_hires_propagated_binding(owner_meta, tile_infos[i].meta))
		{
			tile_states[i] = tile_states[owner_tile];
			write_hires_lookup_tile_source(tile_states[i], HiresLookupSource::AliasPropagated, 0);
			write_hires_lookup_tile_origin_source(
					tile_states[i],
					read_hires_lookup_tile_origin_source(tile_states[owner_tile], 0),
					0);
			if (tile_states[i].allow_tile_sampling_expansion &&
			    !should_alias_hires_tile_binding(owner_meta, tile_infos[i].meta) &&
			    should_alias_hires_load_binding(owner_meta, tile_infos[i].meta))
			{
				tile_states[i].orig_w = clamp_hires_dimension_u16(std::max<uint32_t>(
						tile_states[i].orig_w,
						select_hires_sampling_orig_width_for_tile(0u, tile_infos[i])));
				tile_states[i].orig_h = clamp_hires_dimension_u16(std::max<uint32_t>(
						tile_states[i].orig_h,
						select_hires_sampling_orig_height_for_tile(0u, tile_infos[i])));
			}
		}
	}
}
}
}

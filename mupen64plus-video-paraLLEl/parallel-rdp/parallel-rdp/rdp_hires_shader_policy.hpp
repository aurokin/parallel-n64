#pragma once

#include "rdp_hires_key_state_policy.hpp"
#include "rdp_hires_runtime_policy.hpp"
#include "rdp_hires_sampling_policy.hpp"

#include <cstdint>

namespace RDP
{
namespace detail
{
inline bool should_enable_hires_shader_path(bool has_replacement_provider,
                                            bool registry_ready)
{
	return has_replacement_provider && registry_ready;
}

inline bool should_bind_hires_descriptor_set(bool hires_shader_path_enabled,
                                             bool has_bindless_pool)
{
	return hires_shader_path_enabled && has_bindless_pool;
}

inline bool should_rebuild_hires_shader_bank(bool has_shader_bank,
                                             bool runtime_shader_dir_enabled,
                                             bool previous_hires_shader_define,
                                             bool next_hires_shader_define)
{
	return has_shader_bank &&
	       !runtime_shader_dir_enabled &&
	       (previous_hires_shader_define != next_hires_shader_define);
}

template <typename TileInfoType>
inline void clear_hires_tile_replacement_binding(TileInfoType &tile)
{
	tile.replacement.repl_orig_w = 0;
	tile.replacement.repl_orig_h = 0;
	tile.replacement.repl_w = 0;
	tile.replacement.repl_h = 0;
	tile.replacement.repl_desc_index = hires_invalid_descriptor_index();
}

template <typename TileInfoType>
inline bool hires_tile_binding_has_informative_width(const TileInfoType &tile)
{
	return tile.size.slo != 0 || tile.size.shi != 0 || tile.meta.mask_s != 0;
}

template <typename TileInfoType>
inline bool hires_tile_binding_has_informative_height(const TileInfoType &tile)
{
	return tile.size.tlo != 0 || tile.size.thi != 0 || tile.meta.mask_t != 0;
}

template <typename TileInfoType, typename ReplacementTileStateType>
inline uint16_t resolve_hires_tile_replacement_orig_width(const TileInfoType &tile,
                                                          const ReplacementTileStateType &state)
{
	if (!state.allow_tile_sampling_expansion && state.orig_w != 0)
		return state.orig_w;

	uint32_t orig_w = 0;
	if (hires_tile_binding_has_informative_width(tile))
		orig_w = select_hires_sampling_orig_width_for_tile(0u, tile);
	if (orig_w == 0)
		orig_w = state.orig_w;
	return clamp_hires_dimension_u16(std::max(orig_w, 1u));
}

template <typename TileInfoType, typename ReplacementTileStateType>
inline uint16_t resolve_hires_tile_replacement_orig_height(const TileInfoType &tile,
                                                           const ReplacementTileStateType &state)
{
	if (!state.allow_tile_sampling_expansion && state.orig_h != 0)
		return state.orig_h;

	uint32_t orig_h = 0;
	if (hires_tile_binding_has_informative_height(tile))
		orig_h = select_hires_sampling_orig_height_for_tile(0u, tile);
	if (orig_h == 0)
		orig_h = state.orig_h;
	return clamp_hires_dimension_u16(std::max(orig_h, 1u));
}

template <typename TileInfoType, typename ReplacementTileStateType>
inline void apply_hires_tile_replacement_binding(TileInfoType &tile,
                                                 const ReplacementTileStateType &state)
{
	if (state.hit &&
	    hires_descriptor_index_valid(state.vk_image_index) &&
	    state.orig_w > 0 && state.orig_h > 0 &&
	    state.repl_w > 0 && state.repl_h > 0)
	{
		if (state.vk_image_index >= 65u && state.vk_image_index <= 72u)
		{
			tile.replacement.repl_orig_w = state.orig_w;
			tile.replacement.repl_orig_h = state.orig_h;
		}
		else
		{
			tile.replacement.repl_orig_w = resolve_hires_tile_replacement_orig_width(tile, state);
			tile.replacement.repl_orig_h = resolve_hires_tile_replacement_orig_height(tile, state);
		}
		tile.replacement.repl_w = state.repl_w;
		tile.replacement.repl_h = state.repl_h;
		tile.replacement.repl_desc_index = pack_hires_shader_descriptor_index(state.vk_image_index, state.has_mips);
	}
	else
		clear_hires_tile_replacement_binding(tile);
}
}
}

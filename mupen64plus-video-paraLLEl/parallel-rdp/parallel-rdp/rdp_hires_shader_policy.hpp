#pragma once

#include "rdp_hires_runtime_policy.hpp"

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

template <typename TileInfoType>
inline void clear_hires_tile_replacement_binding(TileInfoType &tile)
{
	tile.replacement.repl_orig_w = 0;
	tile.replacement.repl_orig_h = 0;
	tile.replacement.repl_w = 0;
	tile.replacement.repl_h = 0;
	tile.replacement.repl_desc_index = hires_invalid_descriptor_index();
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
		tile.replacement.repl_orig_w = state.orig_w;
		tile.replacement.repl_orig_h = state.orig_h;
		tile.replacement.repl_w = state.repl_w;
		tile.replacement.repl_h = state.repl_h;
		tile.replacement.repl_desc_index = state.vk_image_index;
	}
	else
		clear_hires_tile_replacement_binding(tile);
}
}
}

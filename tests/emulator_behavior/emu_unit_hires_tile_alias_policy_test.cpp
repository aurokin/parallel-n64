#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_tile_alias_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace RDP;
using namespace RDP::detail;

namespace
{
struct TileInfo
{
	TileMeta meta = {};
	struct
	{
		uint16_t repl_orig_w = 0;
		uint16_t repl_orig_h = 0;
		uint16_t repl_w = 0;
		uint16_t repl_h = 0;
		uint32_t repl_desc_index = 0;
	} replacement;
};

struct ReplacementTileState
{
	uint64_t checksum64 = 0;
	uint16_t formatsize = 0;
	uint16_t orig_w = 0;
	uint16_t orig_h = 0;
	uint16_t repl_w = 0;
	uint16_t repl_h = 0;
	uint32_t vk_image_index = hires_invalid_descriptor_index();
	bool valid = false;
	bool hit = false;
	bool has_mips = false;
};

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static TileMeta make_meta(uint32_t offset, uint32_t stride, TextureFormat fmt, TextureSize size, uint8_t palette)
{
	TileMeta meta = {};
	meta.offset = offset;
	meta.stride = stride;
	meta.fmt = fmt;
	meta.size = size;
	meta.palette = palette;
	return meta;
}

static ReplacementTileState make_bindable_state(uint32_t desc)
{
	ReplacementTileState state = {};
	state.valid = true;
	state.hit = true;
	state.orig_w = 32;
	state.orig_h = 16;
	state.repl_w = 128;
	state.repl_h = 64;
	state.vk_image_index = desc;
	return state;
}

static void test_should_alias_hires_tile_binding_contract()
{
	auto a = make_meta(0x180, 0x40, TextureFormat::CI, TextureSize::Bpp8, 3);
	auto b = make_meta(0x180, 0x40, TextureFormat::CI, TextureSize::Bpp8, 3);
	check(should_alias_hires_tile_binding(a, b), "matching TMEM descriptor should alias");

	b.offset++;
	check(!should_alias_hires_tile_binding(a, b), "offset mismatch should not alias");
	b = make_meta(0x180, 0x44, TextureFormat::CI, TextureSize::Bpp8, 3);
	check(!should_alias_hires_tile_binding(a, b), "stride mismatch should not alias");
	b = make_meta(0x180, 0x40, TextureFormat::RGBA, TextureSize::Bpp8, 3);
	check(!should_alias_hires_tile_binding(a, b), "format mismatch should not alias");
	b = make_meta(0x180, 0x40, TextureFormat::CI, TextureSize::Bpp16, 3);
	check(!should_alias_hires_tile_binding(a, b), "size mismatch should not alias");
	b = make_meta(0x180, 0x40, TextureFormat::CI, TextureSize::Bpp8, 2);
	check(!should_alias_hires_tile_binding(a, b), "palette mismatch should not alias");
}

static void test_should_invalidate_hires_binding_on_load_contract()
{
	auto load_meta = make_meta(0x1c0, 0x40, TextureFormat::CI, TextureSize::Bpp8, 0);
	auto same_offset_diff_desc = make_meta(0x1c0, 0x20, TextureFormat::RGBA, TextureSize::Bpp16, 3);
	auto different_offset = make_meta(0x1c4, 0x40, TextureFormat::CI, TextureSize::Bpp8, 0);

	check(should_invalidate_hires_binding_on_load(load_meta, same_offset_diff_desc),
	      "load invalidation should trigger on shared TMEM offset regardless of descriptor fields");
	check(!should_invalidate_hires_binding_on_load(load_meta, different_offset),
	      "load invalidation should ignore tiles with different TMEM offsets");
}

static void test_should_alias_hires_load_binding_contract()
{
	auto load_meta = make_meta(0x220, 0x00, TextureFormat::CI, TextureSize::Bpp16, 0);
	auto sample_meta_same_offset = make_meta(0x220, 0x40, TextureFormat::RGBA, TextureSize::Bpp16, 0);
	auto sample_meta_other_offset = make_meta(0x224, 0x40, TextureFormat::RGBA, TextureSize::Bpp16, 0);

	check(should_alias_hires_load_binding(load_meta, sample_meta_same_offset),
	      "load alias should match shared TMEM offset regardless of descriptor fields");
	check(!should_alias_hires_load_binding(load_meta, sample_meta_other_offset),
	      "load alias should reject different TMEM offsets");
}

static void test_find_hires_alias_source_tile_contract()
{
	constexpr unsigned NumTiles = 8;
	TileInfo tiles[NumTiles] = {};
	ReplacementTileState states[NumTiles] = {};

	tiles[7].meta = make_meta(0x200, 0x40, TextureFormat::CI, TextureSize::Bpp8, 0);
	states[7] = make_bindable_state(42);

	tiles[0].meta = make_meta(0x200, 0x40, TextureFormat::CI, TextureSize::Bpp8, 0);
	int source = find_hires_alias_source_tile(0, tiles, states);
	check(source == 7, "tile 0 should alias tile 7 when TMEM descriptor matches and source is bindable");

	states[7].hit = false;
	source = find_hires_alias_source_tile(0, tiles, states);
	check(source == -1, "source tile with miss state should not alias");

	states[7] = make_bindable_state(42);
	states[7].vk_image_index = hires_invalid_descriptor_index();
	source = find_hires_alias_source_tile(0, tiles, states);
	check(source == -1, "source tile without valid descriptor should not alias");

	states[7] = make_bindable_state(42);
	tiles[0].meta = make_meta(0x200, 0x20, TextureFormat::RGBA, TextureSize::Bpp16, 3);
	source = find_hires_alias_source_tile(0, tiles, states);
	check(source == -1, "descriptor-only tile updates should not reuse shared-offset alias bindings");

	tiles[0].meta = make_meta(0x208, 0x40, TextureFormat::CI, TextureSize::Bpp8, 0);
	source = find_hires_alias_source_tile(0, tiles, states);
	check(source == -1, "different-offset tile should not alias any source");
}

static void test_hires_tile_state_is_bindable_contract()
{
	auto state = make_bindable_state(9);
	check(hires_tile_state_is_bindable(state), "valid hit state with descriptor and dimensions should be bindable");

	state.valid = false;
	check(!hires_tile_state_is_bindable(state), "invalid state should not be bindable");
	state = make_bindable_state(9);
	state.hit = false;
	check(!hires_tile_state_is_bindable(state), "miss state should not be bindable");
	state = make_bindable_state(9);
	state.orig_w = 0;
	check(!hires_tile_state_is_bindable(state), "zero original width should not be bindable");
}

static void test_invalidate_hires_alias_group_contract()
{
	constexpr unsigned NumTiles = 8;
	TileInfo tiles[NumTiles] = {};
	ReplacementTileState states[NumTiles] = {};

	tiles[7].meta = make_meta(0x200, 0x40, TextureFormat::CI, TextureSize::Bpp8, 0);
	states[7] = make_bindable_state(17);

	tiles[0].meta = make_meta(0x200, 0x40, TextureFormat::CI, TextureSize::Bpp8, 0);
	states[0] = make_bindable_state(18);

	tiles[3].meta = make_meta(0x204, 0x40, TextureFormat::CI, TextureSize::Bpp8, 0);
	states[3] = make_bindable_state(19);

	invalidate_hires_alias_group(7, tiles, states);

	check(states[7].hit, "owner tile state should be preserved during invalidation");
	check(!states[0].valid && !states[0].hit,
	      "matching alias state should be invalidated");
	check(states[3].valid && states[3].hit,
	      "non-matching alias state should remain intact");
}

static void test_invalidate_hires_load_binding_group_contract()
{
	constexpr unsigned NumTiles = 8;
	TileInfo tiles[NumTiles] = {};
	ReplacementTileState states[NumTiles] = {};

	tiles[7].meta = make_meta(0x300, 0x20, TextureFormat::RGBA, TextureSize::Bpp16, 0);
	states[7] = make_bindable_state(27);

	tiles[0].meta = make_meta(0x300, 0x40, TextureFormat::CI, TextureSize::Bpp8, 4);
	states[0] = make_bindable_state(28);

	tiles[3].meta = make_meta(0x304, 0x20, TextureFormat::RGBA, TextureSize::Bpp16, 0);
	states[3] = make_bindable_state(29);

	invalidate_hires_load_binding_group(7, tiles, states);

	check(states[7].hit, "owner tile state should be preserved during load invalidation");
	check(!states[0].valid && !states[0].hit,
	      "shared-offset tile should be invalidated even when descriptor fields differ");
	check(states[3].valid && states[3].hit,
	      "different-offset tile should remain intact after load invalidation");
}

static void test_propagate_hires_alias_group_binding_contract()
{
	constexpr unsigned NumTiles = 8;
	TileInfo tiles[NumTiles] = {};
	ReplacementTileState states[NumTiles] = {};

	tiles[7].meta = make_meta(0x280, 0x40, TextureFormat::RGBA, TextureSize::Bpp16, 0);
	states[7] = make_bindable_state(55);

	tiles[0].meta = make_meta(0x280, 0x40, TextureFormat::RGBA, TextureSize::Bpp16, 0);
	tiles[1].meta = make_meta(0x280, 0x40, TextureFormat::RGBA, TextureSize::Bpp16, 0);
	tiles[2].meta = make_meta(0x284, 0x40, TextureFormat::RGBA, TextureSize::Bpp16, 0);
	tiles[3].meta = make_meta(0x280, 0x10, TextureFormat::CI, TextureSize::Bpp8, 4);

	propagate_hires_alias_group_binding(7, tiles, states);

	check(states[0].vk_image_index == 55 && states[0].hit,
	      "matching alias tile should inherit owner replacement state");
	check(states[1].vk_image_index == 55 && states[1].hit,
	      "all matching alias tiles should inherit owner replacement state");
	check(states[3].vk_image_index == 55 && states[3].hit,
	      "shared-offset tile should inherit owner replacement state via load alias fallback");
	check(states[2].vk_image_index == hires_invalid_descriptor_index() && !states[2].hit,
	      "non-alias tile should remain unchanged");

	states[7].hit = false;
	states[0] = {};
	propagate_hires_alias_group_binding(7, tiles, states);
	check(states[0].vk_image_index == hires_invalid_descriptor_index() && !states[0].hit,
	      "unbound owner state should not be propagated");
}

}

int main()
{
	test_should_alias_hires_tile_binding_contract();
	test_should_invalidate_hires_binding_on_load_contract();
	test_should_alias_hires_load_binding_contract();
	test_find_hires_alias_source_tile_contract();
	test_hires_tile_state_is_bindable_contract();
	test_invalidate_hires_alias_group_contract();
	test_invalidate_hires_load_binding_group_contract();
	test_propagate_hires_alias_group_binding_contract();

	std::cout << "emu_unit_hires_tile_alias_policy_test: PASS" << std::endl;
	return 0;
}

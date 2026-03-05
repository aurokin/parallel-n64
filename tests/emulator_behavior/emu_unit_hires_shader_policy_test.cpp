#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_shader_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace RDP::detail;

namespace
{
struct TileInfo
{
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
	bool hit = false;
	uint16_t orig_w = 0;
	uint16_t orig_h = 0;
	uint16_t repl_w = 0;
	uint16_t repl_h = 0;
	uint32_t vk_image_index = hires_invalid_descriptor_index();
};

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static void test_clear_hires_tile_replacement_binding_contract()
{
	TileInfo tile = {};
	tile.replacement.repl_orig_w = 64;
	tile.replacement.repl_orig_h = 32;
	tile.replacement.repl_w = 256;
	tile.replacement.repl_h = 128;
	tile.replacement.repl_desc_index = 77;

	clear_hires_tile_replacement_binding(tile);

	check(tile.replacement.repl_orig_w == 0 && tile.replacement.repl_orig_h == 0,
	      "clear should reset original dimensions");
	check(tile.replacement.repl_w == 0 && tile.replacement.repl_h == 0,
	      "clear should reset replacement dimensions");
	check(tile.replacement.repl_desc_index == hires_invalid_descriptor_index(),
	      "clear should reset descriptor index to invalid sentinel");
}

static void test_apply_hires_tile_replacement_binding_hit_contract()
{
	TileInfo tile = {};
	ReplacementTileState state = {};
	state.hit = true;
	state.orig_w = 32;
	state.orig_h = 64;
	state.repl_w = 128;
	state.repl_h = 256;
	state.vk_image_index = 15;

	apply_hires_tile_replacement_binding(tile, state);

	check(tile.replacement.repl_orig_w == 32 && tile.replacement.repl_orig_h == 64,
	      "apply-hit should copy original dimensions");
	check(tile.replacement.repl_w == 128 && tile.replacement.repl_h == 256,
	      "apply-hit should copy replacement dimensions");
	check(tile.replacement.repl_desc_index == 15,
	      "apply-hit should copy descriptor index");
}

static void test_apply_hires_tile_replacement_binding_invalid_contract()
{
	TileInfo tile = {};
	ReplacementTileState state = {};
	state.hit = true;
	state.orig_w = 32;
	state.orig_h = 64;
	state.repl_w = 128;
	state.repl_h = 256;
	state.vk_image_index = hires_invalid_descriptor_index();

	apply_hires_tile_replacement_binding(tile, state);
	check(tile.replacement.repl_desc_index == hires_invalid_descriptor_index(),
	      "invalid descriptor should clear replacement binding");

	state.vk_image_index = 22;
	state.repl_w = 0;
	apply_hires_tile_replacement_binding(tile, state);
	check(tile.replacement.repl_desc_index == hires_invalid_descriptor_index(),
	      "zero replacement dimensions should clear replacement binding");
}

static void test_shader_enable_and_bind_gate_contract()
{
	check(!should_enable_hires_shader_path(false, false), "shader path should be disabled with no provider");
	check(!should_enable_hires_shader_path(true, false), "shader path should be disabled when registry is not ready");
	check(should_enable_hires_shader_path(true, true), "shader path should be enabled when provider and registry are ready");

	check(!should_bind_hires_descriptor_set(false, true), "bindless descriptor set should not bind when shader path is disabled");
	check(!should_bind_hires_descriptor_set(true, false), "bindless descriptor set should not bind without pool");
	check(should_bind_hires_descriptor_set(true, true), "bindless descriptor set should bind when shader path is enabled and pool exists");
}
}

int main()
{
	test_clear_hires_tile_replacement_binding_contract();
	test_apply_hires_tile_replacement_binding_hit_contract();
	test_apply_hires_tile_replacement_binding_invalid_contract();
	test_shader_enable_and_bind_gate_contract();

	std::cout << "emu_unit_hires_shader_policy_test: PASS" << std::endl;
	return 0;
}

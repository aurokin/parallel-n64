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
		uint32_t slo = 0;
		uint32_t shi = 0;
		uint32_t tlo = 0;
		uint32_t thi = 0;
	} size;
	struct
	{
		uint8_t mask_s = 0;
		uint8_t mask_t = 0;
	} meta;
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
	bool has_mips = false;
	bool allow_tile_sampling_expansion = true;
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
	state.has_mips = true;

	apply_hires_tile_replacement_binding(tile, state);

	check(tile.replacement.repl_orig_w == 32 && tile.replacement.repl_orig_h == 64,
	      "apply-hit should copy original dimensions");
	check(tile.replacement.repl_w == 128 && tile.replacement.repl_h == 256,
	      "apply-hit should copy replacement dimensions");
	check(unpack_hires_shader_descriptor_index(tile.replacement.repl_desc_index) == 15,
	      "apply-hit should preserve descriptor index in packed value");
	check(hires_shader_descriptor_has_mips(tile.replacement.repl_desc_index),
	      "apply-hit should preserve mip flag in packed descriptor");
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
	state.has_mips = true;
	state.repl_w = 0;
	apply_hires_tile_replacement_binding(tile, state);
	check(tile.replacement.repl_desc_index == hires_invalid_descriptor_index(),
	      "zero replacement dimensions should clear replacement binding");
}

static void test_apply_hires_tile_replacement_binding_no_mips_contract()
{
	TileInfo tile = {};
	ReplacementTileState state = {};
	state.hit = true;
	state.orig_w = 16;
	state.orig_h = 16;
	state.repl_w = 64;
	state.repl_h = 64;
	state.vk_image_index = 9;
	state.has_mips = false;

	apply_hires_tile_replacement_binding(tile, state);
	check(unpack_hires_shader_descriptor_index(tile.replacement.repl_desc_index) == 9,
	      "descriptor index should remain addressable when mips are disabled");
	check(!hires_shader_descriptor_has_mips(tile.replacement.repl_desc_index),
	      "packed descriptor should not set mip flag when mips are disabled");
}

static void test_apply_hires_tile_replacement_binding_uses_tile_sampling_domain_contract()
{
	TileInfo tile = {};
	tile.size.shi = (16u - 1u) << 2u;
	tile.size.thi = (32u - 1u) << 2u;
	tile.meta.mask_s = 4;
	tile.meta.mask_t = 5;

	ReplacementTileState state = {};
	state.hit = true;
	state.orig_w = 4;
	state.orig_h = 32;
	state.repl_w = 1024;
	state.repl_h = 2048;
	state.vk_image_index = 37;

	apply_hires_tile_replacement_binding(tile, state);

	check(tile.replacement.repl_orig_w == 16 && tile.replacement.repl_orig_h == 32,
	      "bind-time domain should follow the live tile sample span when it is known");
}

static void test_apply_hires_tile_replacement_binding_respects_subset_tile_sampling_domain_contract()
{
	TileInfo tile = {};
	tile.size.shi = (8u - 1u) << 2u;
	tile.size.thi = (16u - 1u) << 2u;

	ReplacementTileState state = {};
	state.hit = true;
	state.orig_w = 32;
	state.orig_h = 64;
	state.repl_w = 512;
	state.repl_h = 1024;
	state.vk_image_index = 11;

	apply_hires_tile_replacement_binding(tile, state);

	check(tile.replacement.repl_orig_w == 8 && tile.replacement.repl_orig_h == 16,
	      "bind-time domain should shrink to the live tile sample span when it is narrower");
}

static void test_apply_hires_tile_replacement_binding_falls_back_without_tile_sampling_domain_contract()
{
	TileInfo tile = {};

	ReplacementTileState state = {};
	state.hit = true;
	state.orig_w = 32;
	state.orig_h = 64;
	state.repl_w = 128;
	state.repl_h = 256;
	state.vk_image_index = 19;

	apply_hires_tile_replacement_binding(tile, state);

	check(tile.replacement.repl_orig_w == 32 && tile.replacement.repl_orig_h == 64,
	      "bind-time domain should fall back to lookup dimensions when tile sampling is not known yet");
}

static void test_apply_hires_tile_replacement_binding_can_lock_lookup_dimensions_contract()
{
	TileInfo tile = {};
	tile.size.shi = (16u - 1u) << 2u;
	tile.size.thi = (32u - 1u) << 2u;
	tile.meta.mask_s = 4;
	tile.meta.mask_t = 5;

	ReplacementTileState state = {};
	state.hit = true;
	state.orig_w = 4;
	state.orig_h = 32;
	state.repl_w = 1024;
	state.repl_h = 2048;
	state.vk_image_index = 23;
	state.allow_tile_sampling_expansion = false;

	apply_hires_tile_replacement_binding(tile, state);

	check(tile.replacement.repl_orig_w == 4 && tile.replacement.repl_orig_h == 32,
	      "bind-time domain should stay locked to lookup dimensions when expansion is disabled");
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

static void test_hires_shader_bank_rebuild_gate_contract()
{
	check(!should_rebuild_hires_shader_bank(false, false, false, true),
	      "shader bank rebuild should not happen without an initialized bank");
	check(!should_rebuild_hires_shader_bank(true, true, false, true),
	      "runtime shader dir path should not require shader bank rebuild");
	check(!should_rebuild_hires_shader_bank(true, false, false, false),
	      "shader bank rebuild should not happen when define state is unchanged");
	check(should_rebuild_hires_shader_bank(true, false, false, true),
	      "shader bank rebuild should happen when hires define toggles on");
	check(should_rebuild_hires_shader_bank(true, false, true, false),
	      "shader bank rebuild should happen when hires define toggles off");
}
}

int main()
{
	test_clear_hires_tile_replacement_binding_contract();
	test_apply_hires_tile_replacement_binding_hit_contract();
	test_apply_hires_tile_replacement_binding_invalid_contract();
	test_apply_hires_tile_replacement_binding_no_mips_contract();
	test_apply_hires_tile_replacement_binding_uses_tile_sampling_domain_contract();
	test_apply_hires_tile_replacement_binding_respects_subset_tile_sampling_domain_contract();
	test_apply_hires_tile_replacement_binding_falls_back_without_tile_sampling_domain_contract();
	test_apply_hires_tile_replacement_binding_can_lock_lookup_dimensions_contract();
	test_shader_enable_and_bind_gate_contract();
	test_hires_shader_bank_rebuild_gate_contract();

	std::cout << "emu_unit_hires_shader_policy_test: PASS" << std::endl;
	return 0;
}

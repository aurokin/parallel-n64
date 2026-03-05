#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_key_state_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace RDP::detail;

namespace
{
struct TileState
{
	bool valid = false;
	bool hit = false;
	uint64_t checksum64 = 0;
	uint16_t formatsize = 0;
	uint16_t orig_w = 0;
	uint16_t orig_h = 0;
	uint16_t repl_w = 0;
	uint16_t repl_h = 0;
	uint32_t vk_image_index = 0xffffffffu;
};

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static void test_compose_hires_checksum64_contract()
{
	const uint32_t texture_crc = 0x11223344u;
	const uint32_t palette_crc = 0xaabbccddu;
	const uint64_t checksum = compose_hires_checksum64(texture_crc, palette_crc);

	check((checksum & 0xffffffffull) == texture_crc,
	      "checksum64 low word should store texture crc");
	check(((checksum >> 32) & 0xffffffffull) == palette_crc,
	      "checksum64 high word should store palette crc");
}

static void test_clamp_hires_dimension_u16_contract()
{
	check(clamp_hires_dimension_u16(0) == 0, "zero dimension clamp mismatch");
	check(clamp_hires_dimension_u16(4096) == 4096, "in-range dimension clamp mismatch");
	check(clamp_hires_dimension_u16(0xffff) == 0xffff, "u16 max dimension clamp mismatch");
	check(clamp_hires_dimension_u16(0x1ffff) == 0xffff, "overflow dimension should clamp to u16 max");
}

static void test_write_hires_lookup_tile_state_contract()
{
	TileState state = {};
	write_hires_lookup_tile_state(state,
	                              true,
	                              0x1234567887654321ull,
	                              0x2201,
	                              80000,
	                              300,
	                              41,
	                              2048,
	                              70000);

	check(state.valid, "tile state should be marked valid");
	check(state.hit, "tile state hit should mirror lookup result");
	check(state.checksum64 == 0x1234567887654321ull, "tile state checksum mismatch");
	check(state.formatsize == 0x2201, "tile state formatsize mismatch");
	check(state.orig_w == 0xffff, "tile state width should clamp to u16 max");
	check(state.orig_h == 300, "tile state height should keep in-range values");
	check(state.vk_image_index == 41u, "tile state descriptor index mismatch");
	check(state.repl_w == 2048, "tile replacement width mismatch");
	check(state.repl_h == 0xffff, "tile replacement height should clamp to u16 max");
}

static void test_write_hires_lookup_tile_state_overwrites_previous_values()
{
	TileState state = {};
	state.valid = true;
	state.hit = true;
	state.checksum64 = 0xffffffffffffffffull;
	state.formatsize = 0xffff;
	state.orig_w = 0xffff;
	state.orig_h = 0xffff;
	state.repl_w = 0xffff;
	state.repl_h = 0xffff;
	state.vk_image_index = 17;

	write_hires_lookup_tile_state(state,
	                              false,
	                              0,
	                              0x0102,
	                              16,
	                              32);

	check(state.valid, "tile state should remain valid after overwrite");
	check(!state.hit, "tile state hit flag should update on overwrite");
	check(state.checksum64 == 0, "tile state checksum should overwrite previous value");
	check(state.formatsize == 0x0102, "tile state formatsize should overwrite previous value");
	check(state.orig_w == 16 && state.orig_h == 32, "tile state dimensions should overwrite previous values");
	check(state.vk_image_index == 0xffffffffu, "tile descriptor index should default to invalid sentinel");
	check(state.repl_w == 0 && state.repl_h == 0, "tile replacement dimensions should default to zero");
}
}

int main()
{
	test_compose_hires_checksum64_contract();
	test_clamp_hires_dimension_u16_contract();
	test_write_hires_lookup_tile_state_contract();
	test_write_hires_lookup_tile_state_overwrites_previous_values();

	std::cout << "emu_unit_hires_key_state_policy_test: PASS" << std::endl;
	return 0;
}

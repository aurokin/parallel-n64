#include "rdp_common.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace RDP;

namespace
{
static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}
}

int main()
{
	check(VI_V_SYNC_NTSC == 525u, "VI_V_SYNC_NTSC mismatch");
	check(VI_V_SYNC_PAL == 625u, "VI_V_SYNC_PAL mismatch");
	check(VI_H_OFFSET_NTSC == 108u, "VI_H_OFFSET_NTSC mismatch");
	check(VI_H_OFFSET_PAL == 128u, "VI_H_OFFSET_PAL mismatch");
	check(VI_V_OFFSET_NTSC == 34u, "VI_V_OFFSET_NTSC mismatch");
	check(VI_V_OFFSET_PAL == 44u, "VI_V_OFFSET_PAL mismatch");
	check(VI_SCANOUT_WIDTH == 640, "VI_SCANOUT_WIDTH mismatch");

	check(make_vi_start_register(0x3ffu, 0x155u) == 0x03ff0155u, "make_vi_start_register packing mismatch");
	check(make_vi_start_register(0x7ffu, 0x4ffu) == 0x03ff00ffu,
	      "make_vi_start_register masking mismatch");

	check(make_vi_scale_register(0xabcdu, 0x1234u) == 0x02340bcdu, "make_vi_scale_register packing mismatch");
	check(make_vi_scale_register(0x1fffu, 0x1fffu) == 0x0fff0fffu,
	      "make_vi_scale_register masking mismatch");

	check(make_default_v_start() == make_vi_start_register(VI_V_OFFSET_NTSC, VI_V_OFFSET_NTSC + 224u * 2u),
	      "make_default_v_start mismatch");
	check(make_default_h_start() == make_vi_start_register(VI_H_OFFSET_NTSC, VI_H_OFFSET_NTSC + VI_SCANOUT_WIDTH),
	      "make_default_h_start mismatch");

	check((VI_CONTROL_AA_MODE_MASK & VI_CONTROL_DITHER_FILTER_ENABLE_BIT) == 0u,
	      "VI control mask overlap mismatch");
	check((VI_CONTROL_TYPE_MASK & VI_CONTROL_AA_MODE_MASK) == 0u,
	      "VI control type/mode overlap mismatch");

	std::cout << "emu_conformance_vi_register_contract_test: PASS" << std::endl;
	return 0;
}

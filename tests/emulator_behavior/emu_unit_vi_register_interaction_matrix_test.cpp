#include "vi_scanout_policy.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace RDP;
using namespace RDP::detail;

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

static std::array<uint32_t, unsigned(VIRegister::Count)> make_default_regs()
{
	std::array<uint32_t, unsigned(VIRegister::Count)> regs = {};
	regs[unsigned(VIRegister::Control)] = VI_CONTROL_TYPE_RGBA5551_BIT;
	regs[unsigned(VIRegister::Origin)] = 0x2000u;
	regs[unsigned(VIRegister::Width)] = 320u;
	regs[unsigned(VIRegister::VSync)] = VI_V_SYNC_NTSC;
	regs[unsigned(VIRegister::HStart)] = make_vi_start_register(VI_H_OFFSET_NTSC, VI_H_OFFSET_NTSC + 320u);
	regs[unsigned(VIRegister::VStart)] = make_vi_start_register(VI_V_OFFSET_NTSC, VI_V_OFFSET_NTSC + 480u);
	regs[unsigned(VIRegister::XScale)] = make_vi_scale_register(1024u, 0u);
	regs[unsigned(VIRegister::YScale)] = make_vi_scale_register(1024u, 0u);
	regs[unsigned(VIRegister::VCurrentLine)] = 0u;
	return regs;
}

static void test_left_clamp_and_vertical_start_adjustment()
{
	auto regs = make_default_regs();
	regs[unsigned(VIRegister::HStart)] = make_vi_start_register(50u, 300u);
	regs[unsigned(VIRegister::VStart)] = make_vi_start_register(10u, 410u);
	regs[unsigned(VIRegister::XScale)] = make_vi_scale_register(512u, 100u);
	regs[unsigned(VIRegister::YScale)] = make_vi_scale_register(768u, 60u);
	regs[unsigned(VIRegister::VCurrentLine)] = 0x123u;

	auto decoded = decode_vi_registers(regs.data());
	check(decoded.left_clamp, "left clamp should trigger when HStart is left of NTSC offset");
	check(!decoded.right_clamp, "right clamp should not trigger in left-clamp-only case");
	check(decoded.h_start == 0, "h_start should be clamped to zero");
	check(decoded.h_res == 192, "h_res should shrink by amount clamped from the left");
	check(decoded.v_start == 0, "v_start should clamp to zero when it goes negative");
	check(decoded.x_start == 29796, "x_start should be adjusted to preserve sampling origin");
	check(decoded.y_start == 9276, "y_start should be adjusted to preserve sampling origin");
	check(decoded.max_x == 125, "max_x mismatch after left clamp");
	check(decoded.max_y == 159, "max_y mismatch after v_start clamp");
	check(decoded.v_current_line == 1, "v_current_line parity decode mismatch");
}

static void test_right_clamp_and_scanout_range_generation()
{
	auto regs = make_default_regs();
	regs[unsigned(VIRegister::HStart)] = make_vi_start_register(VI_H_OFFSET_NTSC + 600u, VI_H_OFFSET_NTSC + 900u);
	regs[unsigned(VIRegister::VStart)] = make_vi_start_register(VI_V_OFFSET_NTSC + 20u, VI_V_OFFSET_NTSC + 220u);
	regs[unsigned(VIRegister::Width)] = 640u;
	regs[unsigned(VIRegister::Origin)] = 0x18000u;

	auto decoded = decode_vi_registers(regs.data());
	check(!decoded.left_clamp, "left clamp should not trigger in right clamp case");
	check(decoded.right_clamp, "right clamp should trigger when scanout exceeds visible width");
	check(decoded.h_start == 600, "right clamp h_start mismatch");
	check(decoded.h_res == 40, "right clamp h_res mismatch");
	check(decoded.max_x == 40, "right clamp max_x mismatch");
	check(decoded.max_y == 100, "right clamp max_y mismatch");

	unsigned offset = 0;
	unsigned length = 0;
	compute_scanout_memory_range(decoded, offset, length);
	check(offset == 95740u, "right-clamp scanout offset mismatch");
	check(length == 134492u, "right-clamp scanout length mismatch");
}

static void test_pal_offsets_without_clamping()
{
	auto regs = make_default_regs();
	regs[unsigned(VIRegister::VSync)] = VI_V_SYNC_PAL;
	regs[unsigned(VIRegister::HStart)] = make_vi_start_register(VI_H_OFFSET_PAL + 20u, VI_H_OFFSET_PAL + 340u);
	regs[unsigned(VIRegister::VStart)] = make_vi_start_register(VI_V_OFFSET_PAL + 10u, VI_V_OFFSET_PAL + 410u);

	auto decoded = decode_vi_registers(regs.data());
	check(decoded.is_pal, "PAL detection should be enabled when VSync is PAL");
	check(!decoded.left_clamp, "PAL case should not left-clamp when start offset is in range");
	check(!decoded.right_clamp, "PAL case should not right-clamp when width is in range");
	check(decoded.h_start == 20, "PAL horizontal offset decode mismatch");
	check(decoded.v_start == 5, "PAL vertical offset decode mismatch");
	check(decoded.h_res == 320, "PAL h_res mismatch");
	check(decoded.v_res == 200, "PAL v_res mismatch");
	check(decoded.max_x == 320, "PAL max_x mismatch");
	check(decoded.max_y == 200, "PAL max_y mismatch");
}

static void test_invalid_horizontal_resolution_yields_empty_range()
{
	auto regs = make_default_regs();
	regs[unsigned(VIRegister::HStart)] = make_vi_start_register(VI_H_OFFSET_NTSC + 300u, VI_H_OFFSET_NTSC + 200u);
	regs[unsigned(VIRegister::Origin)] = 0x4000u;

	auto decoded = decode_vi_registers(regs.data());
	check(decoded.h_res < 0, "test setup should create negative h_res");

	unsigned offset = 123u;
	unsigned length = 456u;
	compute_scanout_memory_range(decoded, offset, length);
	check(offset == 0u && length == 0u,
	      "invalid horizontal resolution should result in empty scanout range");
}
}

int main()
{
	test_left_clamp_and_vertical_start_adjustment();
	test_right_clamp_and_scanout_range_generation();
	test_pal_offsets_without_clamping();
	test_invalid_horizontal_resolution_yields_empty_range();
	std::cout << "emu_unit_vi_register_interaction_matrix_test: PASS" << std::endl;
	return 0;
}

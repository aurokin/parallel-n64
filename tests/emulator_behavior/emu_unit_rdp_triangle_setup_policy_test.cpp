#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_triangle_setup_policy.hpp"

#include <array>
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

static void test_flag_decode_and_tile_mapping()
{
	TriangleSetup setup = {};
	setup.flags = TRIANGLE_SETUP_INTERLACE_FIELD_BIT;

	std::array<uint32_t, 8> words = {};
	words[0] = 0x800000u | (0x2au << 16) | 0x0001u;
	words[5] = 0x80000000u;

	detail::decode_triangle_setup_words(setup, words.data(), true, true);

	check((setup.flags & TRIANGLE_SETUP_INTERLACE_FIELD_BIT) != 0,
	      "existing flags should be preserved");
	check((setup.flags & TRIANGLE_SETUP_FLIP_BIT) != 0,
	      "flip flag should be set");
	check((setup.flags & TRIANGLE_SETUP_DO_OFFSET_BIT) != 0,
	      "do_offset should be set when flip and sign(dxhdy) match");
	check((setup.flags & TRIANGLE_SETUP_SKIP_XFRAC_BIT) != 0,
	      "copy-cycle should set skip-xfrac");
	check((setup.flags & TRIANGLE_SETUP_NATIVE_LOD_BIT) != 0,
	      "native texture LOD should set native-lod flag");
	check(setup.tile == 0x2au, "tile decode mismatch");
	check(setup.yl == 1, "yl decode mismatch");
}

static void test_do_offset_combinations()
{
	std::array<uint32_t, 8> words = {};

	TriangleSetup flip_no_sign = {};
	words[0] = 0x800000u;
	words[5] = 0x00000000u;
	detail::decode_triangle_setup_words(flip_no_sign, words.data(), false, false);
	check((flip_no_sign.flags & TRIANGLE_SETUP_FLIP_BIT) != 0,
	      "flip flag should be set");
	check((flip_no_sign.flags & TRIANGLE_SETUP_DO_OFFSET_BIT) == 0,
	      "do_offset should clear when flip and sign(dxhdy) differ");

	TriangleSetup no_flip_no_sign = {};
	words[0] = 0x000000u;
	words[5] = 0x00000000u;
	detail::decode_triangle_setup_words(no_flip_no_sign, words.data(), false, false);
	check((no_flip_no_sign.flags & TRIANGLE_SETUP_FLIP_BIT) == 0,
	      "flip flag should be clear");
	check((no_flip_no_sign.flags & TRIANGLE_SETUP_DO_OFFSET_BIT) != 0,
	      "do_offset should set when flip and sign(dxhdy) both clear");
}

static void test_subpixel_and_sign_extension_decode()
{
	TriangleSetup setup = {};

	std::array<uint32_t, 8> words = {};
	words[0] = 0x3fffu;
	words[1] = (0x0002u << 16) | 0x3ffeu;
	words[2] = 0x07ffffffu;
	words[3] = 0x00000008u;
	words[4] = 0x08000000u;
	words[5] = 0xe0000000u;
	words[6] = 0x00000002u;
	words[7] = 0x00000010u;

	detail::decode_triangle_setup_words(setup, words.data(), false, false);

	check(setup.yl == -1, "yl sign-extension mismatch");
	check(setup.ym == 2, "ym sign-extension mismatch");
	check(setup.yh == -2, "yh sign-extension mismatch");

	check(setup.xl == 67108863, "xl subpixel decode mismatch");
	check(setup.xh == -67108864, "xh subpixel decode mismatch");
	check(setup.xm == 1, "xm subpixel decode mismatch");
	check(setup.dxldy == 1, "dxldy subpixel decode mismatch");
	check(setup.dxhdy == -67108864, "dxhdy subpixel decode mismatch");
	check(setup.dxmdy == 2, "dxmdy subpixel decode mismatch");
}
}

int main()
{
	test_flag_decode_and_tile_mapping();
	test_do_offset_combinations();
	test_subpixel_and_sign_extension_decode();
	std::cout << "emu_unit_rdp_triangle_setup_policy_test: PASS" << std::endl;
	return 0;
}

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_scissor_policy.hpp"

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

static void test_scissor_coordinate_decode_masks_to_12_bits()
{
	ScissorState scissor = {};
	StaticRasterizationState state = {};

	const uint32_t word0 = (0x234u << 12) | 0x567u;
	const uint32_t word1 = (0x9abu << 12) | 0xdefu;

	detail::apply_set_scissor_words(scissor, state, word0, word1);

	check(scissor.xlo == 0x234u, "xlo should decode to low 12 bits");
	check(scissor.ylo == 0x567u, "ylo should decode to low 12 bits");
	check(scissor.xhi == 0x9abu, "xhi should decode to low 12 bits");
	check(scissor.yhi == 0xdefu, "yhi should decode to low 12 bits");
}

static void test_interlace_bits_toggle_cleanly()
{
	ScissorState scissor = {};
	StaticRasterizationState state = {};
	state.flags = RASTERIZATION_INTERLACE_FIELD_BIT | RASTERIZATION_INTERLACE_KEEP_ODD_BIT;

	// Clear both interlace bits.
	detail::apply_set_scissor_words(scissor, state, 0u, 0u);
	check((state.flags & RASTERIZATION_INTERLACE_FIELD_BIT) == 0u,
	      "interlace-field bit should clear when bit 25 is unset");
	check((state.flags & RASTERIZATION_INTERLACE_KEEP_ODD_BIT) == 0u,
	      "interlace-keep-odd bit should clear when bit 24 is unset");

	// Set only field bit.
	detail::apply_set_scissor_words(scissor, state, 0u, (1u << 25));
	check((state.flags & RASTERIZATION_INTERLACE_FIELD_BIT) != 0u,
	      "interlace-field bit should set when bit 25 is set");
	check((state.flags & RASTERIZATION_INTERLACE_KEEP_ODD_BIT) == 0u,
	      "interlace-keep-odd bit should remain clear when bit 24 is unset");

	// Set only keep-odd bit.
	detail::apply_set_scissor_words(scissor, state, 0u, (1u << 24));
	check((state.flags & RASTERIZATION_INTERLACE_FIELD_BIT) == 0u,
	      "interlace-field bit should clear when bit 25 is unset");
	check((state.flags & RASTERIZATION_INTERLACE_KEEP_ODD_BIT) != 0u,
	      "interlace-keep-odd bit should set when bit 24 is set");
}

static void test_scissor_bounds_are_not_implicitly_reordered()
{
	ScissorState scissor = {};
	StaticRasterizationState state = {};

	const uint32_t word0 = (300u << 12) | 400u;
	const uint32_t word1 = (100u << 12) | 200u;
	detail::apply_set_scissor_words(scissor, state, word0, word1);

	check(scissor.xlo == 300u && scissor.xhi == 100u,
	      "policy should preserve raw x bounds without normalization");
	check(scissor.ylo == 400u && scissor.yhi == 200u,
	      "policy should preserve raw y bounds without normalization");
}
}

int main()
{
	test_scissor_coordinate_decode_masks_to_12_bits();
	test_interlace_bits_toggle_cleanly();
	test_scissor_bounds_are_not_implicitly_reordered();
	std::cout << "emu_unit_rdp_scissor_policy_test: PASS" << std::endl;
	return 0;
}

// Independently written from behavior specifications for MIT provenance.
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_ci_palette_policy.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

static void check(bool condition, const char *description)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << description << '\n';
		std::exit(1);
	}
}

static void rice_vectors()
{
	std::array<uint8_t, 16> memory;
	for (unsigned i = 0; i < memory.size(); ++i)
		memory[i] = uint8_t(i);
	struct Vector { uint32_t base, width, height, size, stride, expected; };
	const Vector vectors[] = {
		{0, 4, 1, 1, 4, 0x06040200u},
		{0, 8, 1, 1, 8, 0x76645200u},
		{0, 8, 2, 1, 8, 0x6b46e407u},
		{0, 4, 2, 1, 4, 0x6e4c2a18u},
		{0, 5, 1, 1, 5, 0x08060400u},
		{14, 4, 1, 1, 4, 0x02001e1cu},
		{0, 3, 1, 1, 3, 0},
		{0, 8, 1, 0, 4, 0x06040200u},
		{0, 2, 1, 2, 4, 0x06040200u},
		{0, 1, 1, 3, 4, 0x06040200u},
		{0xfffffffeu, 4, 1, 1, 4, 0x02001e1cu},
	};
	for (const auto &v : vectors)
		check(RDP::rice_crc32_wrapped(memory.data(), memory.size(), v.base,
		      v.width, v.height, v.size, v.stride) == v.expected, "Rice specification vector");
	check(RDP::rice_crc32_wrapped(nullptr, 16, 0, 4, 1, 1, 4) == 0, "Rice null memory");
	check(RDP::rice_crc32_wrapped(memory.data(), 0, 0, 4, 1, 1, 4) == 0, "Rice empty memory");
	check(RDP::rice_crc32_wrapped(memory.data(), 16, 0, 0, 1, 1, 4) == 0, "Rice zero width");
	check(RDP::rice_crc32_wrapped(memory.data(), 16, 0, 4, 0, 1, 4) == 0, "Rice zero height");
	check(RDP::rice_crc32_wrapped(memory.data(), 16, 0, 1, 1, 2, 32) == 0, "one-entry palette prefix");
	memory.fill(255);
	check(RDP::rice_crc32_wrapped(memory.data(), 16, 0, 8, 2, 1, 8) == 0xffffbcadu,
	      "Rice unsigned overflow vector");
}

static void index_vectors()
{
	const std::array<uint8_t, 4> memory = {0x12, 0x34, 0xf0, 0};
	check(RDP::compute_ci4_max_index(memory.data(), 4, 0, 3, 1, 2) == 4, "CI4 rounds odd width up");
	check(RDP::compute_ci4_max_index(memory.data(), 4, 0, 2, 1, 2) == 2, "CI4 width two scans one byte");
	const std::array<uint8_t, 4> odd = {0x12, 0x3f, 0xf0, 0};
	check(RDP::compute_ci4_max_index(odd.data(), 4, 0, 3, 1, 2) == 15, "CI4 includes unused low nibble");
	check(RDP::compute_ci4_max_index(memory.data(), 4, 3, 3, 2, 3) == 15, "CI4 row and address wrap");
	check(RDP::compute_ci8_max_index(memory.data(), 4, 0, 2, 1, 2) == 0x34, "CI8 scans width bytes");
	check(RDP::compute_ci8_max_index(memory.data(), 4, 3, 2, 2, 3) == 0xf0, "CI8 row and address wrap");
	check(RDP::compute_ci4_max_index(nullptr, 4, 0, 3, 1, 2) == 0, "CI4 null memory");
	check(RDP::compute_ci8_max_index(memory.data(), 0, 0, 2, 1, 2) == 0, "CI8 empty memory");
}

int main()
{
	rice_vectors();
	index_vectors();
	return 0;
}

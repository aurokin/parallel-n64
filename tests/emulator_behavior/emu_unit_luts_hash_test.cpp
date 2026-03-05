#include "luts.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace RDP;

namespace
{
static uint64_t fnv1a64(const uint8_t *data, size_t size)
{
	uint64_t h = 1469598103934665603ull;
	for (size_t i = 0; i < size; i++)
	{
		h ^= uint64_t(data[i]);
		h *= 1099511628211ull;
	}
	return h;
}

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
	check(sizeof(blender_lut) == 0x8000u, "blender_lut size mismatch");
	check(sizeof(gamma_table) == (256u + 256u * 64u), "gamma_table size mismatch");

	const uint64_t blender_hash = fnv1a64(blender_lut, sizeof(blender_lut));
	const uint64_t gamma_hash = fnv1a64(gamma_table, sizeof(gamma_table));

	check(blender_hash == 0x57af05c993c5e92cull, "blender_lut hash mismatch");
	check(gamma_hash == 0x9a335da6dc81f083ull, "gamma_table hash mismatch");

	std::cout << "emu_unit_luts_hash_test: PASS" << std::endl;
	return 0;
}

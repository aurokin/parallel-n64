#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_strong_hash.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace RDP;
using namespace RDP::detail;

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main(int argc, char **argv)
{
	// Optional private A148 vectors are supplied by the caller, never embedded
	// in the source tree: FILE WIDTH HEIGHT EXPECTED_HEX (RGBA16 packed rows).
	if (argc == 5)
	{
		std::ifstream file(argv[1], std::ios::binary);
		check(bool(file), "open source vector");
		std::vector<uint8_t> input((std::istreambuf_iterator<char>(file)), {});
		const auto width = uint32_t(std::stoul(argv[2])), height = uint32_t(std::stoul(argv[3]));
		check(input.size() == size_t(width) * height * 2, "vector dimensions");
		std::vector<uint8_t> memory(1u << 17), packed;
		check(input.size() <= memory.size() - 16, "vector exceeds bounded staging buffer");
		std::copy(input.begin(), input.end(), memory.begin() + 16);
		check(pack_hires_strong_rows(memory.data(), memory.size(), 16, width, height, 2, width * 2, packed), "pack vector");
		check(hires_strong_crc32(packed.data(), packed.size()) == std::stoul(argv[4], nullptr, 16), "A148 pinned upstream hash");
		return 0;
	}
	check(argc == 1, "arguments");
	std::vector<uint8_t> memory(4096), packed;
	for (size_t i = 0; i < memory.size(); i++)
		memory[i] = uint8_t(i * 37 + (i >> 3) + 11);

	// Expected folds independently computed with the pinned upstream algorithm.
	check(pack_hires_strong_rows(memory.data(), memory.size(), 32, 5, 3, 1, 11, packed), "strided rows");
	check(hires_strong_crc32(packed.data(), packed.size()) == 0x55aa2b5f, "stride padding excluded, five active bytes retained");
	HiresStrongHashMemo memo;
	const auto original = memo.resolve(123, packed, packed.size());
	check(memo.resolve(123, packed, packed.size()) == original && memo.computations == 1 && memo.hits == 1, "unchanged source is memoized");
	packed[0] ^= 1;
	check(memo.resolve(123, packed, packed.size()) != original && memo.computations == 2, "Rice-equal changed source retires memo");
	packed.clear();
	check(pack_hires_strong_rows(memory.data(), memory.size(), 32, 3, 3, 1, 7, packed), "tiny rows");
	check(hires_strong_crc32(packed.data(), packed.size()) == 0xc9dac541, "tiny rows read preceding byte");
	packed.clear();
	check(pack_hires_strong_rows(memory.data(), memory.size(), 4094, 4, 2, 1, 7, packed), "wrapped rows");
	check(hires_strong_crc32(packed.data(), packed.size()) == 0x6ef5096d, "RDRAM wrap");
	packed.clear();
	check(pack_hires_strong_rows(memory.data(), memory.size(), 0, 1, 1, 0, 1, packed), "zero-active-byte row");
	check(packed == std::vector<uint8_t>(memory.end() - 4, memory.end()), "pre-address read wraps at memory start");
	check(!pack_hires_strong_rows(memory.data(), memory.size(), 0, 4096, 4096, 3, 16384, packed), "oversized staging rejected");

	uint8_t palette[512];
	for (unsigned i = 0; i < 512; i++)
		palette[i] = uint8_t(i * 13 + 5);
	// CI4 odd width: upstream scans only floor(width/2) bytes per row.
	for (unsigned y = 0; y < 3; y++)
	{
		for (unsigned x = 0; x < 4; x++)
			memory[32 + y * 8 + x] = uint8_t((x << 4) | (x + 1));
		memory[32 + y * 8 + 4] = 0xff;
	}
	HiresCompatView view = {32, 9, 3, 8, true};
	size_t texture_bytes = 0;
	check(pack_hires_strong_source(memory.data(), memory.size(), view, TextureFormat::CI, TextureSize::Bpp4,
	                              palette, 3, packed, texture_bytes), "CI4 source");
	check(texture_bytes == 12 && packed.size() == 22, "CI4 cimax=4 prefix is five entries");
	const uint64_t ci4 = memo.resolve(456, packed, texture_bytes);
	check(uint32_t(ci4) == hires_strong_crc32(packed.data(), 12) && uint32_t(ci4 >> 32) == hires_strong_crc32(palette + 96, 10), "CI4 composition and bank");
	palette[96] ^= 1;
	check(pack_hires_strong_source(memory.data(), memory.size(), view, TextureFormat::CI, TextureSize::Bpp4,
	                              palette, 3, packed, texture_bytes), "changed palette");
	check(memo.resolve(456, packed, texture_bytes) != ci4, "palette change retires memo with equal Rice identity");
	check(!pack_hires_strong_source(memory.data(), memory.size(), view, TextureFormat::CI, TextureSize::Bpp4,
	                               nullptr, 3, packed, texture_bytes), "missing palette rejected");
	view = {32, 3, 1, 8, true};
	memory[32] = 0; memory[33] = 1; memory[34] = 2;
	check(pack_hires_strong_source(memory.data(), memory.size(), view, TextureFormat::CI, TextureSize::Bpp8,
	                              palette, 0, packed, texture_bytes), "CI8 tiny source");
	check(texture_bytes == 4 && packed.size() == 10, "CI8 prefix includes cimax+1 entries");
	const uint64_t ci8 = memo.resolve(789, packed, texture_bytes);
	check(uint32_t(ci8 >> 32) == hires_strong_crc32(palette, 6), "CI8 palette composition");
	memory[32] = memory[33] = memory[34] = 0;
	check(pack_hires_strong_source(memory.data(), memory.size(), view, TextureFormat::CI, TextureSize::Bpp8,
	                              palette, 0, packed, texture_bytes), "one-entry palette");
	check(packed.size() == 8, "tiny palette is bounded");
	check(packed[4] == palette[510] && packed[5] == palette[511] && packed[6] == palette[0] && packed[7] == palette[1], "tiny palette pre-address wrap");
	return 0;
}

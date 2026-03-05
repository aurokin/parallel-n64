#include "rdp_common.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <type_traits>

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
	check(std::is_enum<Op>::value, "Op must be an enum");
	check(uint32_t(Op::SyncFull) == 0x29u, "SyncFull opcode mismatch");
	check(uint32_t(Op::TextureRectangle) == 0x24u, "TextureRectangle opcode mismatch");
	check(uint32_t(Op::SetColorImage) == 0x3fu, "SetColorImage opcode mismatch");
	check(uint32_t(Op::MetaIdle) == 3u, "MetaIdle opcode mismatch");

	check(uint32_t(TextureFormat::RGBA) == 0u, "TextureFormat::RGBA mismatch");
	check(uint32_t(TextureFormat::CI) == 2u, "TextureFormat::CI mismatch");
	check(uint32_t(TextureSize::Bpp4) == 0u, "TextureSize::Bpp4 mismatch");
	check(uint32_t(TextureSize::Bpp32) == 3u, "TextureSize::Bpp32 mismatch");

	check(uint32_t(RGBDitherMode::Off) == 3u, "RGBDitherMode::Off mismatch");
	check(uint32_t(AlphaDitherMode::Off) == 3u, "AlphaDitherMode::Off mismatch");
	check(uint32_t(CycleType::Fill) == 3u, "CycleType::Fill mismatch");

	check(uint32_t(TILE_INFO_CLAMP_S_BIT) == 1u, "TILE_INFO_CLAMP_S_BIT mismatch");
	check(uint32_t(TILE_INFO_MIRROR_T_BIT) == 8u, "TILE_INFO_MIRROR_T_BIT mismatch");

	std::cout << "emu_unit_rdp_common_test: PASS" << std::endl;
	return 0;
}

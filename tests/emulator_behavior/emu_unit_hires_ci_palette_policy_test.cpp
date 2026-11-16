#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_ci_palette_policy.hpp"
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_texture_layout.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace
{
using namespace RDP;

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static void test_ci8_raw_palette_crc_matches_legacy_contract()
{
	std::array<uint8_t, 512> tlut_shadow = {};
	for (uint32_t i = 0; i < tlut_shadow.size(); i++)
		tlut_shadow[i] = uint8_t((i * 37u + 13u) & 0xffu);

	const std::array<uint8_t, 8> ci8_texels = { 0u, 3u, 7u, 1u, 2u, 6u, 4u, 5u };
	const uint32_t entries = RDP::detail::compute_hires_ci_palette_entry_count(
		TextureSize::Bpp8,
		ci8_texels.data(),
		ci8_texels.size(),
		0,
		4,
		2,
		4);
	check(entries == 8u, "CI8 entry count should follow cimax + 1");

	const uint32_t palette_crc = RDP::detail::compute_hires_ci_palette_crc(
		TextureSize::Bpp8,
		0,
		ci8_texels.data(),
		ci8_texels.size(),
		0,
		4,
		2,
		4,
		tlut_shadow.data(),
		tlut_shadow.size(),
		true);
	const uint32_t expected_crc = rice_crc32_wrapped(
		tlut_shadow.data(),
		tlut_shadow.size(),
		0,
		entries,
		1,
		2,
		512);
	check(palette_crc == expected_crc, "CI8 raw palette CRC should match legacy raw TLUT bytes");


}

static void test_ci4_raw_palette_crc_matches_legacy_contract()
{
	std::array<uint8_t, 512> tlut_shadow = {};
	for (uint32_t i = 0; i < tlut_shadow.size(); i++)
		tlut_shadow[i] = uint8_t((i * 19u + 7u) & 0xffu);

	const std::array<uint8_t, 4> ci4_texels = {
		uint8_t((0x1u << 4u) | 0x7u),
		uint8_t((0x2u << 4u) | 0x5u),
		uint8_t((0x6u << 4u) | 0x0u),
		uint8_t((0x4u << 4u) | 0x3u),
	};

	const uint32_t palette = 5u;
	const uint32_t entries = RDP::detail::compute_hires_ci_palette_entry_count(
		TextureSize::Bpp4,
		ci4_texels.data(),
		ci4_texels.size(),
		0,
		4,
		2,
		2);
	check(entries == 8u, "CI4 entry count should follow cimax + 1");

	const uint32_t palette_crc = RDP::detail::compute_hires_ci_palette_crc(
		TextureSize::Bpp4,
		palette,
		ci4_texels.data(),
		ci4_texels.size(),
		0,
		4,
		2,
		2,
		tlut_shadow.data(),
		tlut_shadow.size(),
		true);
	const uint32_t expected_crc = rice_crc32_wrapped(
		tlut_shadow.data(),
		tlut_shadow.size(),
		palette * 32u,
		entries,
		1,
		2,
		32);
	check(palette_crc == expected_crc, "CI4 raw palette CRC should match legacy banked raw TLUT bytes");


}

static void test_ci4_row_bytes_round_up_for_odd_width()
{
	check(RDP::detail::compute_hires_texture_row_bytes(0, TextureSize::Bpp4) == 0u,
	      "CI4 row bytes should keep zero-width textures at zero bytes");
	check(RDP::detail::compute_hires_texture_row_bytes(1, TextureSize::Bpp4) == 1u,
	      "CI4 row bytes should round width=1 up to one byte");
	check(RDP::detail::compute_hires_texture_row_bytes(3, TextureSize::Bpp4) == 2u,
	      "CI4 row bytes should round odd widths up instead of truncating");
	check(RDP::detail::compute_hires_texture_row_bytes(5, TextureSize::Bpp4) == 3u,
	      "CI4 row bytes should keep rounding odd widths up");
	check(RDP::detail::compute_hires_texture_row_bytes(3, TextureSize::Bpp8) == 3u,
	      "CI8 row bytes should stay width-sized");
}

static void test_ci4_odd_width_stride_reaches_second_row_palette_indices()
{
	const std::array<uint8_t, 4> ci4_texels = {
		uint8_t((0x1u << 4u) | 0x2u),
		uint8_t((0x3u << 4u) | 0x0u),
		uint8_t((0x4u << 4u) | 0x5u),
		uint8_t((0xfu << 4u) | 0x0u),
	};

	const uint32_t row_stride = RDP::detail::compute_hires_texture_row_bytes(3u, TextureSize::Bpp4);
	check(row_stride == 2u, "CI4 odd-width textures should reserve two bytes per row");


	const uint32_t entries = RDP::detail::compute_hires_ci_palette_entry_count(
		TextureSize::Bpp4,
		ci4_texels.data(),
		ci4_texels.size(),
		0,
		3,
		2,
		row_stride);
	check(entries == 16u, "CI4 odd-width entry count should reflect second-row max index coverage");
}

}

int main()
{
	test_ci8_raw_palette_crc_matches_legacy_contract();
	test_ci4_raw_palette_crc_matches_legacy_contract();
	test_ci4_row_bytes_round_up_for_odd_width();
	test_ci4_odd_width_stride_reaches_second_row_palette_indices();

	std::cout << "emu_unit_hires_ci_palette_policy_test: PASS" << std::endl;
	return 0;
}

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_ci_palette_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

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

static std::vector<uint8_t> make_tlut_shadow()
{
	std::vector<uint8_t> tlut(512);
	for (uint32_t i = 0; i < tlut.size(); i++)
		tlut[i] = static_cast<uint8_t>((i * 13u + 7u) & 0xffu);
	return tlut;
}

static void test_invalid_input_guard_matrix()
{
	const std::vector<uint8_t> rdram(64, 0);
	const auto tlut = make_tlut_shadow();

	check(compute_hires_ci_palette_crc(TextureSize::Bpp8, 0,
	                                   nullptr, rdram.size(),
	                                   0, 8, 1, 8,
	                                   tlut.data(), tlut.size(), true) == 0,
	      "null rdram should return zero palette crc");

	check(compute_hires_ci_palette_crc(TextureSize::Bpp8, 0,
	                                   rdram.data(), 0,
	                                   0, 8, 1, 8,
	                                   tlut.data(), tlut.size(), true) == 0,
	      "zero rdram size should return zero palette crc");

	check(compute_hires_ci_palette_crc(TextureSize::Bpp8, 0,
	                                   rdram.data(), rdram.size(),
	                                   0, 8, 1, 8,
	                                   nullptr, tlut.size(), true) == 0,
	      "null tlut should return zero palette crc");

	check(compute_hires_ci_palette_crc(TextureSize::Bpp8, 0,
	                                   rdram.data(), rdram.size(),
	                                   0, 8, 1, 8,
	                                   tlut.data(), 128, true) == 0,
	      "short tlut shadow should return zero palette crc");

	check(compute_hires_ci_palette_crc(TextureSize::Bpp8, 0,
	                                   rdram.data(), rdram.size(),
	                                   0, 8, 1, 8,
	                                   tlut.data(), tlut.size(), false) == 0,
	      "invalid tlut shadow flag should return zero palette crc");

	check(compute_hires_ci_palette_crc(TextureSize::Bpp16, 0,
	                                   rdram.data(), rdram.size(),
	                                   0, 8, 1, 8,
	                                   tlut.data(), tlut.size(), true) == 0,
	      "non-CI texture sizes should return zero palette crc");

	auto candidates = compute_hires_ci_palette_crc_candidates(
			TextureSize::Bpp16,
			0,
			rdram.data(),
			rdram.size(),
			0,
			8,
			1,
			8,
			tlut.data(),
			tlut.size(),
			true);
	check(candidates.count == 0, "non-CI palette candidate set should be empty");
}

static void test_ci8_palette_crc_contract()
{
	std::vector<uint8_t> rdram(128, 0);
	rdram[3] = 0x01;
	rdram[5] = 0x03;
	rdram[7] = 0x02;
	const auto tlut = make_tlut_shadow();

	const uint32_t actual = compute_hires_ci_palette_crc(TextureSize::Bpp8, 0,
	                                                     rdram.data(), rdram.size(),
	                                                     0, 8, 1, 8,
	                                                     tlut.data(), tlut.size(), true);
	const uint32_t expected = rice_crc32_wrapped(tlut.data(), tlut.size(), 0, 4, 1, 2, 512);
	check(actual == expected, "CI8 palette crc contract mismatch");
}

static void test_ci4_palette_crc_contract_and_bank_clamp()
{
	std::vector<uint8_t> rdram(128, 0);
	rdram[0] = 0xe2;
	rdram[1] = 0x47;
	rdram[2] = 0xa1;
	const auto tlut = make_tlut_shadow();

	const uint32_t actual = compute_hires_ci_palette_crc(TextureSize::Bpp4, 31,
	                                                     rdram.data(), rdram.size(),
	                                                     0, 6, 1, 3,
	                                                     tlut.data(), tlut.size(), true);
	const uint32_t expected = rice_crc32_wrapped(tlut.data(), tlut.size(), 15u * 32u, 15, 1, 2, 32);
	check(actual == expected, "CI4 palette crc contract mismatch or palette-bank clamp mismatch");

	rdram[0] = 0xf0;
	const uint32_t actual_max = compute_hires_ci_palette_crc(TextureSize::Bpp4, 0,
	                                                         rdram.data(), rdram.size(),
	                                                         0, 2, 1, 1,
	                                                         tlut.data(), tlut.size(), true);
	const uint32_t expected_max = rice_crc32_wrapped(tlut.data(), tlut.size(), 0, 16, 1, 2, 32);
	check(actual_max == expected_max, "CI4 max-index should clamp to 16 palette entries");
}

static void test_ci_palette_crc_candidates_include_fallbacks()
{
	std::vector<uint8_t> rdram(128, 0);
	const auto tlut = make_tlut_shadow();

	const auto candidates = compute_hires_ci_palette_crc_candidates(
			TextureSize::Bpp8,
			0,
			rdram.data(),
			rdram.size(),
			0,
			8,
			1,
			8,
			tlut.data(),
			tlut.size(),
			true);

	check(candidates.count >= 2, "CI8 candidates should include fallback variants");
	check(candidates.values[0] == 0u, "CI8 primary candidate should preserve legacy zero-CRC behavior");
	bool has_nonzero = false;
	for (uint32_t i = 1; i < candidates.count; i++)
	{
		if (candidates.values[i] != 0u)
			has_nonzero = true;
	}
	check(has_nonzero, "CI8 fallback candidates should include non-zero CRC options");
}

static void test_ci_palette_crc_candidates_dedupe_when_equal()
{
	std::vector<uint8_t> rdram(128, 0xff);
	const auto tlut = make_tlut_shadow();

	const auto candidates = compute_hires_ci_palette_crc_candidates(
			TextureSize::Bpp4,
			5,
			rdram.data(),
			rdram.size(),
			0,
			2,
			1,
			1,
			tlut.data(),
			tlut.size(),
			true);

	for (uint32_t i = 0; i < candidates.count; i++)
	{
		for (uint32_t j = i + 1; j < candidates.count; j++)
			check(candidates.values[i] != candidates.values[j], "candidate set should be deduplicated");
	}
}
}

int main()
{
	test_invalid_input_guard_matrix();
	test_ci8_palette_crc_contract();
	test_ci4_palette_crc_contract_and_bank_clamp();
	test_ci_palette_crc_candidates_include_fallbacks();
	test_ci_palette_crc_candidates_dedupe_when_equal();

	std::cout << "emu_unit_hires_ci_palette_policy_test: PASS" << std::endl;
	return 0;
}

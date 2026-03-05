#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_tlut_shadow_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

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

static std::vector<uint8_t> make_rdram(size_t size)
{
	std::vector<uint8_t> data(size);
	for (size_t i = 0; i < size; i++)
		data[i] = static_cast<uint8_t>((i * 17u + 11u) & 0xffu);
	return data;
}

static void test_full_tlut_update_from_base_offset()
{
	auto rdram = make_rdram(4096);
	std::vector<uint8_t> shadow(512, 0);
	bool valid = false;

	auto result = update_hires_tlut_shadow(
			shadow.data(), shadow.size(), valid,
			rdram.data(), rdram.size(),
			123u, 512u, hires_tlut_shadow_base_offset_bytes());

	check(result.updated, "full update should be marked updated");
	check(result.copied_bytes == 512u, "full update should copy full shadow size");
	check(result.shadow_offset == 0u, "full update should begin at shadow offset 0");
	check(valid, "full update should mark shadow valid");

	for (uint32_t i = 0; i < 512u; i++)
		check(shadow[i] == rdram[(123u + i) & uint32_t(rdram.size() - 1)], "full update contents mismatch");
}

static void test_partial_update_preserves_existing_shadow()
{
	auto rdram = make_rdram(4096);
	std::vector<uint8_t> shadow(512, 0xee);
	bool valid = true;

	const uint32_t dst_offset = 96u;
	auto result = update_hires_tlut_shadow(
			shadow.data(), shadow.size(), valid,
			rdram.data(), rdram.size(),
			200u, 32u, hires_tlut_shadow_base_offset_bytes() + dst_offset);

	check(result.updated, "partial update should be marked updated");
	check(result.copied_bytes == 32u, "partial update should copy requested bytes");
	check(result.shadow_offset == dst_offset, "partial update shadow offset mismatch");
	check(valid, "partial update should keep shadow valid");

	for (uint32_t i = 0; i < 512u; i++)
	{
		if (i >= dst_offset && i < dst_offset + 32u)
			check(shadow[i] == rdram[(200u + (i - dst_offset)) & uint32_t(rdram.size() - 1)], "partial updated range mismatch");
		else
			check(shadow[i] == 0xee, "partial update should preserve untouched bytes");
	}
}

static void test_first_partial_update_zero_fills_rest()
{
	auto rdram = make_rdram(4096);
	std::vector<uint8_t> shadow(512, 0xaa);
	bool valid = false;

	const uint32_t dst_offset = 32u;
	auto result = update_hires_tlut_shadow(
			shadow.data(), shadow.size(), valid,
			rdram.data(), rdram.size(),
			500u, 16u, hires_tlut_shadow_base_offset_bytes() + dst_offset);

	check(result.updated, "first partial update should be marked updated");
	check(result.copied_bytes == 16u, "first partial update copied bytes mismatch");
	check(result.shadow_offset == dst_offset, "first partial update offset mismatch");
	check(valid, "first partial update should mark shadow valid");

	for (uint32_t i = 0; i < 512u; i++)
	{
		if (i >= dst_offset && i < dst_offset + 16u)
			check(shadow[i] == rdram[(500u + (i - dst_offset)) & uint32_t(rdram.size() - 1)], "first partial updated bytes mismatch");
		else
			check(shadow[i] == 0u, "first partial update should zero untouched bytes");
	}
}

static void test_clipped_overlap_and_out_of_range_paths()
{
	auto rdram = make_rdram(4096);
	std::vector<uint8_t> shadow(512, 0);
	bool valid = false;

	auto clipped = update_hires_tlut_shadow(
			shadow.data(), shadow.size(), valid,
			rdram.data(), rdram.size(),
			1000u, 32u, hires_tlut_shadow_base_offset_bytes() - 16u);

	check(clipped.updated, "clipped overlap should update");
	check(clipped.copied_bytes == 16u, "clipped overlap should copy only in-range bytes");
	check(clipped.shadow_offset == 0u, "clipped overlap should start at shadow offset 0");
	for (uint32_t i = 0; i < 16u; i++)
		check(shadow[i] == rdram[(1000u + 16u + i) & uint32_t(rdram.size() - 1)], "clipped overlap contents mismatch");

	std::vector<uint8_t> before = shadow;
	auto skipped = update_hires_tlut_shadow(
			shadow.data(), shadow.size(), valid,
			rdram.data(), rdram.size(),
			300u, 32u, hires_tlut_shadow_base_offset_bytes() + uint32_t(shadow.size()) + 4u);

	check(!skipped.updated, "out-of-range upload should not update shadow");
	check(skipped.copied_bytes == 0u, "out-of-range upload should copy zero bytes");
	check(shadow == before, "out-of-range upload should not mutate shadow");
}

static void test_invalid_input_guard_matrix()
{
	auto rdram = make_rdram(64);
	std::vector<uint8_t> shadow(16, 0);
	bool valid = false;

	check(!update_hires_tlut_shadow(nullptr, shadow.size(), valid,
	                                rdram.data(), rdram.size(),
	                                0u, 8u, hires_tlut_shadow_base_offset_bytes()).updated,
	      "null shadow must be ignored");

	check(!update_hires_tlut_shadow(shadow.data(), shadow.size(), valid,
	                                nullptr, rdram.size(),
	                                0u, 8u, hires_tlut_shadow_base_offset_bytes()).updated,
	      "null rdram must be ignored");

	check(!update_hires_tlut_shadow(shadow.data(), 0u, valid,
	                                rdram.data(), rdram.size(),
	                                0u, 8u, hires_tlut_shadow_base_offset_bytes()).updated,
	      "zero shadow size must be ignored");

	check(!update_hires_tlut_shadow(shadow.data(), shadow.size(), valid,
	                                rdram.data(), 0u,
	                                0u, 8u, hires_tlut_shadow_base_offset_bytes()).updated,
	      "zero rdram size must be ignored");

	check(!update_hires_tlut_shadow(shadow.data(), shadow.size(), valid,
	                                rdram.data(), rdram.size(),
	                                0u, 0u, hires_tlut_shadow_base_offset_bytes()).updated,
	      "zero update size must be ignored");
}
}

int main()
{
	test_full_tlut_update_from_base_offset();
	test_partial_update_preserves_existing_shadow();
	test_first_partial_update_zero_fills_rest();
	test_clipped_overlap_and_out_of_range_paths();
	test_invalid_input_guard_matrix();

	std::cout << "emu_unit_hires_tlut_shadow_policy_test: PASS" << std::endl;
	return 0;
}

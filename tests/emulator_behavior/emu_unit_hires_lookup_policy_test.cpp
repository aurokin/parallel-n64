#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_lookup_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

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

static void test_hires_rdram_view_valid_matrix()
{
	const uint8_t backing[8] = {};
	check(!hires_rdram_view_valid(nullptr, 8), "null rdram pointer should be invalid");
	check(!hires_rdram_view_valid(backing, 0), "zero rdram size should be invalid");
	check(!hires_rdram_view_valid(backing, 3), "non-power-of-two rdram size should be invalid");
	check(hires_rdram_view_valid(backing, 8), "power-of-two rdram view should be valid");
}

static void test_tlut_shadow_gate()
{
	check(should_update_tlut_shadow(true, true), "tlut shadow should run when rdram view is valid and mode is TLUT");
	check(!should_update_tlut_shadow(false, true), "tlut shadow should not run with invalid rdram view");
	check(!should_update_tlut_shadow(true, false), "tlut shadow should not run outside TLUT mode");
}

static void test_hires_lookup_fast_path_gate()
{
	check(!should_run_hires_lookup(false, true, false, 64, 64), "lookup should not run with invalid rdram view");
	check(!should_run_hires_lookup(true, false, false, 64, 64), "lookup should not run when provider is unavailable");
	check(!should_run_hires_lookup(true, true, true, 64, 64), "lookup should not run in TLUT mode");
	check(!should_run_hires_lookup(true, true, false, 0, 64), "lookup should not run with zero width");
	check(!should_run_hires_lookup(true, true, false, 64, 0), "lookup should not run with zero height");
	check(should_run_hires_lookup(true, true, false, 64, 64), "lookup should run when all prerequisites are met");
}

static void test_lookup_counter_updates()
{
	uint64_t total = 10;
	uint64_t hits = 4;
	uint64_t misses = 6;

	record_hires_lookup_result(true, total, hits, misses);
	check(total == 11 && hits == 5 && misses == 6, "hit counter update mismatch");

	record_hires_lookup_result(false, total, hits, misses);
	check(total == 12 && hits == 5 && misses == 7, "miss counter update mismatch");
}

static void test_descriptor_binding_result_updates()
{
	uint64_t total = 0;
	uint64_t provider_hits = 0;
	uint64_t provider_misses = 0;
	uint64_t descriptor_bound_hits = 0;
	uint64_t descriptor_unbound_hits = 0;

	check(did_hires_lookup_bind_descriptor(true, 7), "valid descriptor should count as bound on provider hit");
	check(!did_hires_lookup_bind_descriptor(true, 0xffffffffu), "invalid descriptor should not count as bound");
	check(!did_hires_lookup_bind_descriptor(false, 7), "provider miss should never count as descriptor bound");

	record_hires_lookup_binding_result(
			true,
			did_hires_lookup_bind_descriptor(true, 5),
			total,
			provider_hits,
			provider_misses,
			descriptor_bound_hits,
			descriptor_unbound_hits);
	check(total == 1 && provider_hits == 1 && provider_misses == 0,
	      "provider hit accounting mismatch");
	check(descriptor_bound_hits == 1 && descriptor_unbound_hits == 0,
	      "descriptor bound accounting mismatch");

	record_hires_lookup_binding_result(
			true,
			did_hires_lookup_bind_descriptor(true, 0xffffffffu),
			total,
			provider_hits,
			provider_misses,
			descriptor_bound_hits,
			descriptor_unbound_hits);
	check(total == 2 && provider_hits == 2 && provider_misses == 0,
	      "provider hit accounting mismatch on unbound descriptor");
	check(descriptor_bound_hits == 1 && descriptor_unbound_hits == 1,
	      "descriptor unbound accounting mismatch");

	record_hires_lookup_binding_result(
			false,
			did_hires_lookup_bind_descriptor(false, 3),
			total,
			provider_hits,
			provider_misses,
			descriptor_bound_hits,
			descriptor_unbound_hits);
	check(total == 3 && provider_hits == 2 && provider_misses == 1,
	      "provider miss accounting mismatch");
	check(descriptor_bound_hits == 1 && descriptor_unbound_hits == 1,
	      "descriptor counters should not change on provider miss");
}
}

int main()
{
	test_hires_rdram_view_valid_matrix();
	test_tlut_shadow_gate();
	test_hires_lookup_fast_path_gate();
	test_lookup_counter_updates();
	test_descriptor_binding_result_updates();

	std::cout << "emu_unit_hires_lookup_policy_test: PASS" << std::endl;
	return 0;
}

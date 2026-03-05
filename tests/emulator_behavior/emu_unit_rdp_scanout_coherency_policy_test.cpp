#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_scanout_coherency_policy.hpp"

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

static void test_host_coherent_path_flushes_only()
{
	std::vector<int> order;
	unsigned resolve_calls = 0;
	unsigned range_calls = 0;

	run_scanout_coherency_sequence(
			true,
			[&]() {
				order.push_back(1);
			},
			[&](unsigned &, unsigned &) {
				range_calls++;
				order.push_back(2);
			},
			[&](unsigned, unsigned) {
				resolve_calls++;
				order.push_back(3);
			});

	check(order.size() == 1 && order[0] == 1, "host-coherent path should only flush");
	check(range_calls == 0, "host-coherent path should skip range computation");
	check(resolve_calls == 0, "host-coherent path should skip coherency resolve");
}

static void test_non_host_coherent_path_orders_flush_then_range_then_resolve()
{
	std::vector<int> order;
	unsigned resolved_offset = 0;
	unsigned resolved_length = 0;

	run_scanout_coherency_sequence(
			false,
			[&]() {
				order.push_back(1);
			},
			[&](unsigned &offset, unsigned &length) {
				offset = 0x1200u;
				length = 0x3400u;
				order.push_back(2);
			},
			[&](unsigned offset, unsigned length) {
				resolved_offset = offset;
				resolved_length = length;
				order.push_back(3);
			});

	check(order.size() == 3, "non-host-coherent path should execute all three steps");
	for (size_t i = 0; i < order.size(); i++)
		check(order[i] == int(i + 1), "non-host-coherent step order mismatch");
	check(resolved_offset == 0x1200u, "resolved offset mismatch");
	check(resolved_length == 0x3400u, "resolved length mismatch");
}

static void test_non_host_coherent_path_allows_empty_ranges()
{
	unsigned resolve_calls = 0;
	unsigned resolved_offset = 1;
	unsigned resolved_length = 1;

	run_scanout_coherency_sequence(
			false,
			[]() {},
			[](unsigned &offset, unsigned &length) {
				offset = 0;
				length = 0;
			},
			[&](unsigned offset, unsigned length) {
				resolve_calls++;
				resolved_offset = offset;
				resolved_length = length;
			});

	check(resolve_calls == 1, "resolve should still run for empty ranges");
	check(resolved_offset == 0u && resolved_length == 0u,
	      "empty range should be forwarded exactly");
}
}

int main()
{
	test_host_coherent_path_flushes_only();
	test_non_host_coherent_path_orders_flush_then_range_then_resolve();
	test_non_host_coherent_path_allows_empty_ranges();
	std::cout << "emu_unit_rdp_scanout_coherency_policy_test: PASS" << std::endl;
	return 0;
}

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_registry_policy.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void check(bool condition, const char *message)
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
	constexpr uint32_t invalid = 0xffffffffu;
	size_t bytes = 0;
	uint32_t next = 1;
	auto allocate = [&](size_t incoming, size_t budget, uint32_t capacity = 10) {
		return RDP::detail::allocate_hires_registry_descriptor(bytes, incoming, budget, next, capacity);
	};

	check(allocate(30, 100) == 1 && allocate(30, 100) == 2 && allocate(30, 100) == 3,
	      "successful admissions must use distinct stable descriptors");
	check(allocate(60, 100) == invalid, "budget overflow must reject instead of replacing a resident");
	check(bytes == 90 && next == 4, "rejection must preserve existing resident accounting and slots");
	check(allocate(101, 100) == invalid && bytes == 90 && next == 4,
	      "oversized rejection must preserve resident accounting and slots");
	check(allocate(10, 100) == 4 && bytes == 100 && next == 5,
	      "exact fit must admit and account once");
	check(allocate(1, 100) == invalid && bytes == 100 && next == 5,
	      "full budget must reject new images while preserving existing bindings");
	check(allocate(1000, 0) == 5 && bytes == 1100 && next == 6,
	      "zero budget must retain unlimited behavior");
	check(allocate(1, 0, 6) == invalid && bytes == 1100 && next == 6,
	      "descriptor exhaustion must not consume byte budget or slots");

	bytes = std::numeric_limits<size_t>::max() - 3;
	check(allocate(7, std::numeric_limits<size_t>::max()) == invalid && next == 6,
	      "budget comparison must avoid resident plus incoming overflow");
	check(bytes == std::numeric_limits<size_t>::max() - 3,
	      "overflow rejection must preserve existing byte accounting");

	std::cout << "emu_unit_hires_registry_budget_test: PASS" << std::endl;
	return 0;
}

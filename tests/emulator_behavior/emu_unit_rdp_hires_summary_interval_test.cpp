#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_summary_interval.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using RDP::detail::HiresSummaryIntervalClock;
using RDP::detail::parse_hires_summary_interval_frames;

static void check(bool condition, const char *message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static void test_interval_parsing()
{
	check(parse_hires_summary_interval_frames(nullptr) == 0, "unset env should disable periodic summaries");
	check(parse_hires_summary_interval_frames("") == 0, "empty env should disable periodic summaries");
	check(parse_hires_summary_interval_frames("0") == 0, "env=0 should disable periodic summaries");
	check(parse_hires_summary_interval_frames("-60") == 0, "negative env should disable periodic summaries");
	check(parse_hires_summary_interval_frames("garbage") == 0, "non-numeric env should disable periodic summaries");
	check(parse_hires_summary_interval_frames("60") == 60, "env=60 should select a 60-frame interval");
	check(parse_hires_summary_interval_frames("0x10") == 16, "hex env should parse like other hi-res env vars");
	check(parse_hires_summary_interval_frames("99999999999") == UINT32_MAX, "oversized env should saturate");
}

static std::vector<uint64_t> fired_frames(uint32_t interval, unsigned presented)
{
	HiresSummaryIntervalClock clock = {};
	clock.interval = interval;
	std::vector<uint64_t> fired;
	for (unsigned i = 0; i < presented; i++)
		if (clock.advance())
			fired.push_back(clock.frames);
	return fired;
}

static void test_disabled_clock_never_fires_or_counts()
{
	HiresSummaryIntervalClock clock = {};
	for (unsigned i = 0; i < 1000; i++)
		check(!clock.advance(), "disabled clock should never fire");
	check(clock.frames == 0, "disabled clock should not count frames");
}

static void test_clock_fires_on_interval_boundaries()
{
	check(fired_frames(3, 10) == std::vector<uint64_t>{ 3, 6, 9 },
	      "interval=3 should fire on frames 3, 6, 9 and never on frame 0");
	check(fired_frames(1, 4) == std::vector<uint64_t>{ 1, 2, 3, 4 }, "interval=1 should fire every frame");
	check(fired_frames(60, 59).empty(), "a run shorter than the interval should not fire");
}
}

int main()
{
	test_interval_parsing();
	test_disabled_clock_never_fires_or_counts();
	test_clock_fires_on_interval_boundaries();
	std::cout << "emu_unit_rdp_hires_summary_interval_test: PASS" << std::endl;
	return 0;
}

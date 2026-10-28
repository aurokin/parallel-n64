// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <cstdlib>

namespace RDP::detail
{

// Parses PARALLEL_RDP_HIRES_SUMMARY_INTERVAL_FRAMES. Unset, non-numeric, zero,
// and negative values disable periodic summaries; oversized values saturate.
inline uint32_t parse_hires_summary_interval_frames(const char *env)
{
	if (!env)
		return 0;

	const long long value = strtoll(env, nullptr, 0);
	if (value <= 0)
		return 0;
	if (static_cast<unsigned long long>(value) > UINT32_MAX)
		return UINT32_MAX;
	return uint32_t(value);
}

// Counts presented frames and decides when a periodic hi-res summary is due.
// With interval == 0 the clock is inert: it never counts and never fires, so the
// per-frame cost of the disabled path is a single compare.
struct HiresSummaryIntervalClock
{
	uint32_t interval = 0;
	// Presented frames observed since the clock was armed. Reported as frame=K.
	uint64_t frames = 0;

	// Records one presented frame. Returns true on every interval-th frame.
	bool advance()
	{
		if (interval == 0)
			return false;
		frames++;
		return frames % interval == 0;
	}
};
}

#pragma once

#include <cstdlib>
#include "vi_scaling_mode.hpp"

namespace RDP::detail
{
struct VIScaleSamplingPolicyInput
{
	unsigned scaling_mode = VI_SCALING_MODE_ACCURATE;
	unsigned experimental_vi = VI_EXPERIMENTAL_OVERRIDE_AUTO;
	unsigned scaling_factor = 1;
	bool vi_scale = true;
};

struct VIScaleSamplingPolicy
{
	bool use_documented_source_mapping = false;
	unsigned subpixel_grid = 1;
};

inline VIScaleSamplingPolicy derive_vi_scale_sampling_policy(const VIScaleSamplingPolicyInput &in)
{
	VIScaleSamplingPolicy out = {};
	bool enable_experimental_vi = false;
	switch (in.experimental_vi)
	{
	case VI_EXPERIMENTAL_OVERRIDE_ENABLED:
		enable_experimental_vi = true;
		break;
	case VI_EXPERIMENTAL_OVERRIDE_DISABLED:
		enable_experimental_vi = false;
		break;
	default:
		enable_experimental_vi = false;
		break;
	}

	if (in.vi_scale && enable_experimental_vi && in.scaling_factor > 1)
	{
		out.use_documented_source_mapping = true;
	}

	return out;
}
}

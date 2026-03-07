#pragma once

#include "vi_scaling_mode.hpp"

namespace RDP::detail
{
struct VIScaleSamplingPolicyInput
{
	unsigned scaling_mode = VI_SCALING_MODE_ACCURATE;
	unsigned scaling_factor = 1;
	bool vi_scale = true;
};

struct VIScaleSamplingPolicy
{
	bool use_subpixel_reconstruction = false;
	unsigned subpixel_grid = 1;
};

inline VIScaleSamplingPolicy derive_vi_scale_sampling_policy(const VIScaleSamplingPolicyInput &in)
{
	VIScaleSamplingPolicy out = {};

	if (in.vi_scale && in.scaling_mode == VI_SCALING_MODE_EXPERIMENTAL && in.scaling_factor > 1)
	{
		out.use_subpixel_reconstruction = true;
		out.subpixel_grid = 2;
	}

	return out;
}
}

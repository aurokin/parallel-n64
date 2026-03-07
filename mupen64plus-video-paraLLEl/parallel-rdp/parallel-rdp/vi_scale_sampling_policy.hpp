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
	unsigned source_y_add_bias = 0;
	int source_y_base_bias = 0;
	unsigned source_x_add_bias = 0;
	int source_x_base_bias = 0;
};

inline VIScaleSamplingPolicy derive_vi_scale_sampling_policy(const VIScaleSamplingPolicyInput &in)
{
	VIScaleSamplingPolicy out = {};

	if (in.vi_scale && in.scaling_mode == VI_SCALING_MODE_EXPERIMENTAL && in.scaling_factor > 1)
	{
		out.use_subpixel_reconstruction = true;
		out.subpixel_grid = 2;

		// Current 4x Paper Mario oracle work shows a meaningful source-domain improvement
		// when the experimental path slightly reduces the Y step fed into the VI scale shader.
		if (in.scaling_factor == 4)
		{
			out.source_y_add_bias = 29;
			out.source_y_base_bias = 0;
			out.source_x_add_bias = 17;
			out.source_x_base_bias = 0;
		}
	}

	return out;
}
}

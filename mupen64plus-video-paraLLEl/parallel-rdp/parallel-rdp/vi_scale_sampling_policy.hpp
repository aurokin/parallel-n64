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
	bool use_subpixel_reconstruction = false;
	bool use_derived_source_y_biases = false;
	unsigned subpixel_grid = 1;
	unsigned source_y_add_bias = 0;
	int source_y_base_bias = 0;
	int source_y_line_base_upper_bias = 0;
	int source_y_line_base_lower_bias = 0;
	unsigned source_x_add_bias = 0;
	int source_x_base_bias = 0;
	int phase1_source_y_bias = 0;
	int phase1_lower_source_y_bias = 0;
	int phase3_source_x_bias = 0;
	int phase3_source_y_bias = 0;
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
		out.use_subpixel_reconstruction = true;
		out.subpixel_grid = 2;

		// Current 4x Paper Mario oracle work shows a meaningful source-domain improvement
		// when the experimental path slightly reduces the Y step fed into the VI scale shader.
		if (in.scaling_factor == 4)
		{
			out.source_x_base_bias = 0;
			out.use_derived_source_y_biases = true;
		}
	}

	if (const char *env = std::getenv("PARALLEL_VI_SOURCE_Y_ADD_BIAS"))
		out.source_y_add_bias = unsigned(std::strtoul(env, nullptr, 0));
	if (const char *env = std::getenv("PARALLEL_VI_SOURCE_Y_BASE_BIAS"))
		out.source_y_base_bias = int(std::strtol(env, nullptr, 0));
	if (const char *env = std::getenv("PARALLEL_VI_SOURCE_Y_LINE_BASE_UPPER_BIAS"))
		out.source_y_line_base_upper_bias = int(std::strtol(env, nullptr, 0));
	if (const char *env = std::getenv("PARALLEL_VI_SOURCE_Y_LINE_BASE_LOWER_BIAS"))
		out.source_y_line_base_lower_bias = int(std::strtol(env, nullptr, 0));
	if (const char *env = std::getenv("PARALLEL_VI_SOURCE_X_ADD_BIAS"))
		out.source_x_add_bias = unsigned(std::strtoul(env, nullptr, 0));
	if (const char *env = std::getenv("PARALLEL_VI_SOURCE_X_BASE_BIAS"))
		out.source_x_base_bias = int(std::strtol(env, nullptr, 0));
	if (const char *env = std::getenv("PARALLEL_VI_PHASE1_Y_BIAS"))
	{
		out.use_derived_source_y_biases = false;
		out.phase1_source_y_bias = int(std::strtol(env, nullptr, 0));
	}
	if (const char *env = std::getenv("PARALLEL_VI_PHASE1_LOWER_Y_BIAS"))
	{
		out.use_derived_source_y_biases = false;
		out.phase1_lower_source_y_bias = int(std::strtol(env, nullptr, 0));
	}
	if (const char *env = std::getenv("PARALLEL_VI_PHASE3_X_BIAS"))
		out.phase3_source_x_bias = int(std::strtol(env, nullptr, 0));
	if (const char *env = std::getenv("PARALLEL_VI_PHASE3_Y_BIAS"))
	{
		out.use_derived_source_y_biases = false;
		out.phase3_source_y_bias = int(std::strtol(env, nullptr, 0));
	}

	return out;
}
}

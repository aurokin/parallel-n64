#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/vi_scale_sampling_policy.hpp"

#include <cstdlib>
#include <iostream>

using namespace RDP;
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

static void test_accurate_mode_keeps_single_sample_path()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_ACCURATE;
	in.experimental_vi = VI_EXPERIMENTAL_OVERRIDE_AUTO;
	in.scaling_factor = 4;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(!policy.use_documented_source_mapping, "accurate mode should keep the baseline VI mapping path");
	check(policy.subpixel_grid == 1u, "accurate mode should keep the single-sample grid");
}

static void test_experimental_mode_is_inert_at_native_scale()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.experimental_vi = VI_EXPERIMENTAL_OVERRIDE_AUTO;
	in.scaling_factor = 1;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(!policy.use_documented_source_mapping, "native-scale experimental mode should stay inert");
	check(policy.subpixel_grid == 1u, "native-scale experimental mode should keep the single-sample grid");
}

static void test_experimental_mode_auto_keeps_vi_path_disabled_when_upscaled()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.experimental_vi = VI_EXPERIMENTAL_OVERRIDE_AUTO;
	in.scaling_factor = 4;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(!policy.use_documented_source_mapping, "auto VI override should keep VI accuracy improvements disabled");
	check(policy.subpixel_grid == 1u, "auto VI override should keep the single-sample grid");
}

static void test_explicit_vi_enabled_enables_documented_mapping_when_upscaled()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.experimental_vi = VI_EXPERIMENTAL_OVERRIDE_ENABLED;
	in.scaling_factor = 4;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(policy.use_documented_source_mapping, "enabled VI override should enable documented VI mapping when upscaled");
	check(policy.subpixel_grid == 1u, "enabled VI override should keep the baseline sample grid");
}

static void test_experimental_mode_respects_disabled_vi_scaling()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.experimental_vi = VI_EXPERIMENTAL_OVERRIDE_AUTO;
	in.scaling_factor = 4;
	in.vi_scale = false;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(!policy.use_documented_source_mapping, "disabled VI scale should bypass documented VI mapping");
	check(policy.subpixel_grid == 1u, "disabled VI scale should keep the single-sample grid");
}

static void test_non_4x_experimental_mode_uses_documented_mapping_when_upscaled()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.experimental_vi = VI_EXPERIMENTAL_OVERRIDE_ENABLED;
	in.scaling_factor = 8;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(policy.use_documented_source_mapping, "documented VI mapping should apply at any upscale factor when enabled");
	check(policy.subpixel_grid == 1u, "documented VI mapping should keep the baseline sample grid");
}

static void test_explicit_vi_override_splits_from_scaling_mode()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_ACCURATE;
	in.experimental_vi = VI_EXPERIMENTAL_OVERRIDE_ENABLED;
	in.scaling_factor = 4;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(policy.use_documented_source_mapping, "enabled VI override should force documented VI mapping");

	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.experimental_vi = VI_EXPERIMENTAL_OVERRIDE_DISABLED;
	policy = derive_vi_scale_sampling_policy(in);
	check(!policy.use_documented_source_mapping, "disabled VI override should suppress documented VI mapping");
}
}

int main()
{
	test_accurate_mode_keeps_single_sample_path();
	test_experimental_mode_is_inert_at_native_scale();
	test_experimental_mode_auto_keeps_vi_path_disabled_when_upscaled();
	test_explicit_vi_enabled_enables_documented_mapping_when_upscaled();
	test_experimental_mode_respects_disabled_vi_scaling();
	test_non_4x_experimental_mode_uses_documented_mapping_when_upscaled();
	test_explicit_vi_override_splits_from_scaling_mode();
	std::cout << "emu_unit_vi_scale_sampling_policy_test: PASS" << std::endl;
	return 0;
}

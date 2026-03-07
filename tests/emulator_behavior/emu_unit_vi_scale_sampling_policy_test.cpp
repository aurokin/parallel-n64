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
	in.scaling_factor = 4;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(!policy.use_subpixel_reconstruction, "accurate mode should keep the baseline sampling path");
	check(policy.subpixel_grid == 1u, "accurate mode should keep the single-sample grid");
	check(policy.source_y_add_bias == 0u, "accurate mode should not bias source y_add");
	check(policy.source_y_base_bias == 0, "accurate mode should not bias source y base");
	check(policy.source_x_add_bias == 0u, "accurate mode should not bias source x_add");
	check(policy.source_x_base_bias == 0, "accurate mode should not bias source x base");
}

static void test_experimental_mode_is_inert_at_native_scale()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.scaling_factor = 1;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(!policy.use_subpixel_reconstruction, "experimental mode should stay inert at native scale");
	check(policy.subpixel_grid == 1u, "native-scale experimental mode should keep the single-sample grid");
	check(policy.source_y_add_bias == 0u, "native-scale experimental mode should not bias source y_add");
	check(policy.source_y_base_bias == 0, "native-scale experimental mode should not bias source y base");
	check(policy.source_x_add_bias == 0u, "native-scale experimental mode should not bias source x_add");
	check(policy.source_x_base_bias == 0, "native-scale experimental mode should not bias source x base");
}

static void test_experimental_mode_enables_subpixel_reconstruction_when_upscaled()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.scaling_factor = 4;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(policy.use_subpixel_reconstruction, "experimental mode should enable subpixel reconstruction when upscaled");
	check(policy.subpixel_grid == 2u, "experimental mode should request a 2x2 subpixel grid");
	check(policy.source_y_add_bias == 29u, "4x experimental mode should bias source y_add");
	check(policy.source_y_base_bias == 0, "4x experimental mode should keep zero source y base bias by default");
	check(policy.source_x_add_bias == 17u, "4x experimental mode should bias source x_add");
	check(policy.source_x_base_bias == 0, "4x experimental mode should keep zero source x base bias");
}

static void test_experimental_mode_respects_disabled_vi_scaling()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.scaling_factor = 4;
	in.vi_scale = false;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(!policy.use_subpixel_reconstruction, "disabled VI scale should bypass experimental sampling");
	check(policy.subpixel_grid == 1u, "disabled VI scale should keep the single-sample grid");
	check(policy.source_y_add_bias == 0u, "disabled VI scale should not bias source y_add");
	check(policy.source_y_base_bias == 0, "disabled VI scale should not bias source y base");
	check(policy.source_x_add_bias == 0u, "disabled VI scale should not bias source x_add");
	check(policy.source_x_base_bias == 0, "disabled VI scale should not bias source x base");
}

static void test_non_4x_experimental_mode_keeps_zero_source_y_add_bias()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.scaling_factor = 8;
	in.vi_scale = true;

	auto policy = derive_vi_scale_sampling_policy(in);
	check(policy.use_subpixel_reconstruction, "experimental mode should still use subpixel reconstruction at 8x");
	check(policy.subpixel_grid == 2u, "8x experimental mode should keep the 2x2 subpixel grid");
	check(policy.source_y_add_bias == 0u, "unvalidated non-4x path should keep zero source y_add bias");
	check(policy.source_y_base_bias == 0, "unvalidated non-4x path should keep zero source y base bias");
	check(policy.source_x_add_bias == 0u, "unvalidated non-4x path should keep zero source x_add bias");
	check(policy.source_x_base_bias == 0, "unvalidated non-4x path should keep zero source x base bias");
}

static void test_env_overrides_replace_default_biases()
{
	VIScaleSamplingPolicyInput in = {};
	in.scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.scaling_factor = 4;
	in.vi_scale = true;

	setenv("PARALLEL_VI_SOURCE_Y_ADD_BIAS", "31", 1);
	setenv("PARALLEL_VI_SOURCE_Y_BASE_BIAS", "512", 1);
	setenv("PARALLEL_VI_SOURCE_X_ADD_BIAS", "19", 1);
	setenv("PARALLEL_VI_SOURCE_X_BASE_BIAS", "-64", 1);

	auto policy = derive_vi_scale_sampling_policy(in);
	check(policy.source_y_add_bias == 31u, "env override should replace source y_add bias");
	check(policy.source_y_base_bias == 512, "env override should replace source y base bias");
	check(policy.source_x_add_bias == 19u, "env override should replace source x_add bias");
	check(policy.source_x_base_bias == -64, "env override should replace source x base bias");

	unsetenv("PARALLEL_VI_SOURCE_Y_ADD_BIAS");
	unsetenv("PARALLEL_VI_SOURCE_Y_BASE_BIAS");
	unsetenv("PARALLEL_VI_SOURCE_X_ADD_BIAS");
	unsetenv("PARALLEL_VI_SOURCE_X_BASE_BIAS");
}
}

int main()
{
	test_accurate_mode_keeps_single_sample_path();
	test_experimental_mode_is_inert_at_native_scale();
	test_experimental_mode_enables_subpixel_reconstruction_when_upscaled();
	test_experimental_mode_respects_disabled_vi_scaling();
	test_non_4x_experimental_mode_keeps_zero_source_y_add_bias();
	test_env_overrides_replace_default_biases();
	std::cout << "emu_unit_vi_scale_sampling_policy_test: PASS" << std::endl;
	return 0;
}

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
}
}

int main()
{
	test_accurate_mode_keeps_single_sample_path();
	test_experimental_mode_is_inert_at_native_scale();
	test_experimental_mode_enables_subpixel_reconstruction_when_upscaled();
	test_experimental_mode_respects_disabled_vi_scaling();
	std::cout << "emu_unit_vi_scale_sampling_policy_test: PASS" << std::endl;
	return 0;
}

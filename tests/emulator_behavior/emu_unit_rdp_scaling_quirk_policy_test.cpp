#include "mupen64plus-video-paraLLEl/rdp_scaling_quirk_policy.hpp"

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

static void test_accurate_mode_preserves_requested_native_tex_rect()
{
	ScalingQuirkPolicyInput in = {};
	in.vi_scaling_mode = VI_SCALING_MODE_ACCURATE;
	in.upscaling_factor = 4;
	in.native_tex_rect = true;

	auto policy = derive_scaling_quirk_policy(in);
	check(policy.effective_native_tex_rect,
	      "accurate mode should preserve native tex-rect when requested");

	in.native_tex_rect = false;
	policy = derive_scaling_quirk_policy(in);
	check(!policy.effective_native_tex_rect,
	      "accurate mode should preserve a disabled native tex-rect request");
}

static void test_experimental_mode_only_overrides_tex_rect_when_upscaled()
{
	ScalingQuirkPolicyInput in = {};
	in.vi_scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.upscaling_factor = 1;
	in.native_tex_rect = true;

	auto policy = derive_scaling_quirk_policy(in);
	check(policy.effective_native_tex_rect,
	      "experimental mode should stay inert at native scale");

	in.upscaling_factor = 4;
	policy = derive_scaling_quirk_policy(in);
	check(!policy.effective_native_tex_rect,
	      "experimental mode should force tex-rect upscaling when VI upscaling is active");
}
}

int main()
{
	test_accurate_mode_preserves_requested_native_tex_rect();
	test_experimental_mode_only_overrides_tex_rect_when_upscaled();
	std::cout << "emu_unit_rdp_scaling_quirk_policy_test: PASS" << std::endl;
	return 0;
}

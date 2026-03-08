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
	in.experimental_texrect = VI_EXPERIMENTAL_OVERRIDE_AUTO;
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

static void test_experimental_mode_auto_forces_native_tex_rect_when_upscaled()
{
	ScalingQuirkPolicyInput in = {};
	in.vi_scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.experimental_texrect = VI_EXPERIMENTAL_OVERRIDE_AUTO;
	in.upscaling_factor = 1;
	in.native_tex_rect = true;

	auto policy = derive_scaling_quirk_policy(in);
	check(policy.effective_native_tex_rect,
	      "experimental mode should stay inert at native scale");

	in.upscaling_factor = 4;
	policy = derive_scaling_quirk_policy(in);
	check(policy.effective_native_tex_rect,
	      "experimental mode should keep native tex-rect enabled when VI upscaling is active");

	in.native_tex_rect = false;
	policy = derive_scaling_quirk_policy(in);
	check(policy.effective_native_tex_rect,
	      "experimental mode should force native tex-rect on when VI upscaling is active");
}

static void test_explicit_texrect_override_splits_from_scaling_mode()
{
	ScalingQuirkPolicyInput in = {};
	in.vi_scaling_mode = VI_SCALING_MODE_ACCURATE;
	in.experimental_texrect = VI_EXPERIMENTAL_OVERRIDE_ENABLED;
	in.upscaling_factor = 4;
	in.native_tex_rect = false;

	auto policy = derive_scaling_quirk_policy(in);
	check(policy.effective_native_tex_rect,
	      "enabled texrect override should force native tex-rect in accurate mode");

	in.vi_scaling_mode = VI_SCALING_MODE_EXPERIMENTAL;
	in.experimental_texrect = VI_EXPERIMENTAL_OVERRIDE_DISABLED;
	policy = derive_scaling_quirk_policy(in);
	check(!policy.effective_native_tex_rect,
	      "disabled texrect override should suppress experimental tex-rect forcing");
}
}

int main()
{
	test_accurate_mode_preserves_requested_native_tex_rect();
	test_experimental_mode_auto_forces_native_tex_rect_when_upscaled();
	test_explicit_texrect_override_splits_from_scaling_mode();
	std::cout << "emu_unit_rdp_scaling_quirk_policy_test: PASS" << std::endl;
	return 0;
}

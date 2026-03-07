#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/vi_scanout_flow_policy.hpp"

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

static VIScanoutFlowPolicyInput make_default_input()
{
	VIScanoutFlowPolicyInput in = {};
	in.status = VI_CONTROL_TYPE_RGBA5551_BIT |
	            VI_CONTROL_AA_MODE_RESAMP_EXTRA_BIT |
	            VI_CONTROL_GAMMA_DITHER_ENABLE_BIT |
	            VI_CONTROL_DIVOT_ENABLE_BIT |
	            VI_CONTROL_DITHER_FILTER_ENABLE_BIT;
	in.vi_aa = true;
	in.vi_scale = true;
	in.vi_serrate = true;
	in.vi_dither_filter = true;
	in.vi_divot_filter = true;
	in.vi_gamma_dither = true;
	in.previous_frame_blank = false;
	in.persist_frame_on_invalid_input = false;
	in.horizontal_input_valid = true;
	in.upscale_deinterlacing = true;
	in.frame_count = 10;
	in.last_valid_frame_count = 8;
	in.scaling_factor = 4;
	in.downscale_steps = 1;
	return in;
}

static void test_option_masking_rewrites_vi_meta_bits_from_status_and_options()
{
	auto in = make_default_input();
	auto policy = derive_vi_scanout_flow_policy(in);

	check((policy.option_masked_status & VI_CONTROL_AA_MODE_MASK) == 0,
	      "AA mode bits should be cleared after deriving meta bits");
	check((policy.option_masked_status & VI_CONTROL_META_AA_BIT) != 0,
	      "AA meta bit should remain enabled for AA-capable modes");
	check((policy.option_masked_status & VI_CONTROL_META_SCALE_BIT) != 0,
	      "Scale meta bit should remain enabled for bilinear-capable modes");
	check((policy.option_masked_status & VI_CONTROL_DIVOT_ENABLE_BIT) != 0,
	      "Divot bit should survive when option is enabled");
	check((policy.option_masked_status & VI_CONTROL_DITHER_FILTER_ENABLE_BIT) != 0,
	      "Dither filter bit should survive when option is enabled");
	check((policy.option_masked_status & VI_CONTROL_GAMMA_DITHER_ENABLE_BIT) != 0,
	      "Gamma dither bit should survive when option is enabled");
}

static void test_disabled_vi_features_strip_bits_and_block_meta_flags()
{
	auto in = make_default_input();
	in.status = VI_CONTROL_TYPE_RGBA5551_BIT |
	            VI_CONTROL_AA_MODE_RESAMP_REPLICATE_BIT |
	            VI_CONTROL_SERRATE_BIT |
	            VI_CONTROL_GAMMA_DITHER_ENABLE_BIT |
	            VI_CONTROL_DIVOT_ENABLE_BIT |
	            VI_CONTROL_DITHER_FILTER_ENABLE_BIT;
	in.vi_aa = false;
	in.vi_scale = false;
	in.vi_serrate = false;
	in.vi_dither_filter = false;
	in.vi_divot_filter = false;
	in.vi_gamma_dither = false;

	auto policy = derive_vi_scanout_flow_policy(in);
	check((policy.option_masked_status & VI_CONTROL_META_AA_BIT) == 0,
	      "AA meta bit should be cleared when VI AA is disabled");
	check((policy.option_masked_status & VI_CONTROL_META_SCALE_BIT) == 0,
	      "Scale meta bit should be cleared when VI scale is disabled");
	check((policy.option_masked_status & VI_CONTROL_SERRATE_BIT) == 0,
	      "Serrate bit should be cleared when serrate option is disabled");
	check((policy.option_masked_status & VI_CONTROL_DIVOT_ENABLE_BIT) == 0,
	      "Divot bit should be cleared when option is disabled");
	check((policy.option_masked_status & VI_CONTROL_DITHER_FILTER_ENABLE_BIT) == 0,
	      "Dither filter bit should be cleared when option is disabled");
	check((policy.option_masked_status & VI_CONTROL_GAMMA_DITHER_ENABLE_BIT) == 0,
	      "Gamma dither bit should be cleared when option is disabled");
}

static void test_resamp_only_mode_keeps_scale_without_enabling_aa_meta()
{
	auto in = make_default_input();
	in.status = VI_CONTROL_TYPE_RGBA5551_BIT | VI_CONTROL_AA_MODE_RESAMP_ONLY_BIT;

	auto policy = derive_vi_scanout_flow_policy(in);
	check((policy.option_masked_status & VI_CONTROL_META_AA_BIT) == 0,
	      "AA meta bit should stay disabled for resample-only mode");
	check((policy.option_masked_status & VI_CONTROL_META_SCALE_BIT) != 0,
	      "Scale meta bit should stay enabled for resample-only mode");
}

static void test_blank_frame_policy_resets_history_and_skips_repeated_blank()
{
	auto in = make_default_input();
	in.status = 0;

	auto first_blank = derive_vi_scanout_flow_policy(in);
	check(first_blank.blank_frame, "blank frame should be detected when VI type is zero");
	check(first_blank.reset_previous_scanout, "blank frame should reset previous scanout");
	check(!first_blank.skip_repeated_blank_frame, "first blank frame should not be skipped");
	check((first_blank.processing_status & VI_CONTROL_TYPE_RGBA5551_BIT) != 0,
	      "processing status should force RGBA5551 type for downstream stages");

	in.previous_frame_blank = true;
	auto repeated_blank = derive_vi_scanout_flow_policy(in);
	check(repeated_blank.skip_repeated_blank_frame, "repeated blank frame should be skipped");
}

static void test_invalid_input_persistence_only_applies_in_short_window()
{
	auto in = make_default_input();
	in.horizontal_input_valid = false;
	in.persist_frame_on_invalid_input = true;
	in.frame_count = 20;
	in.last_valid_frame_count = 17;

	auto persisted = derive_vi_scanout_flow_policy(in);
	check(persisted.persist_previous_on_invalid_input,
	      "invalid input should persist previous frame inside the short grace window");

	in.last_valid_frame_count = 16;
	auto expired = derive_vi_scanout_flow_policy(in);
	check(!expired.persist_previous_on_invalid_input,
	      "invalid input persistence should expire after the grace window");

	in.horizontal_input_valid = true;
	auto valid = derive_vi_scanout_flow_policy(in);
	check(!valid.persist_previous_on_invalid_input,
	      "valid input should never request persistence");
}

static void test_downscale_and_deinterlace_follow_scaling_and_serrate_state()
{
	auto in = make_default_input();
	in.status |= VI_CONTROL_SERRATE_BIT;
	in.upscale_deinterlacing = true;
	in.scaling_factor = 8;
	in.downscale_steps = 2;

	auto policy = derive_vi_scanout_flow_policy(in);
	check(policy.should_downscale, "downscale should run when scale factor exceeds one and steps are configured");
	check(policy.post_downscale_scaling_factor == 2u,
	      "post-downscale scaling factor should match the deinterlace input domain");
	check(policy.should_upscale_deinterlace,
	      "upscale deinterlace should run when serrate remains enabled");

	in.downscale_steps = 5;
	policy = derive_vi_scanout_flow_policy(in);
	check(policy.post_downscale_scaling_factor == 1u,
	      "post-downscale scaling factor should clamp at one");

	in.scaling_factor = 1;
	in.downscale_steps = 1;
	policy = derive_vi_scanout_flow_policy(in);
	check(!policy.should_downscale, "downscale should not run at native scale");

	in.scaling_factor = 4;
	in.upscale_deinterlacing = false;
	policy = derive_vi_scanout_flow_policy(in);
	check(!policy.should_upscale_deinterlace,
	      "upscale deinterlace should not run when weave mode is requested");
}
}

int main()
{
	test_option_masking_rewrites_vi_meta_bits_from_status_and_options();
	test_disabled_vi_features_strip_bits_and_block_meta_flags();
	test_resamp_only_mode_keeps_scale_without_enabling_aa_meta();
	test_blank_frame_policy_resets_history_and_skips_repeated_blank();
	test_invalid_input_persistence_only_applies_in_short_window();
	test_downscale_and_deinterlace_follow_scaling_and_serrate_state();
	std::cout << "emu_unit_vi_scanout_flow_policy_test: PASS" << std::endl;
	return 0;
}

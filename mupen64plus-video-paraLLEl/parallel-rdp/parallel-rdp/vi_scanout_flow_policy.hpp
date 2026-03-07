#pragma once

#include <algorithm>

#include "rdp_common.hpp"

namespace RDP::detail
{
struct VIScanoutFlowPolicyInput
{
	uint32_t status = 0;
	bool vi_aa = true;
	bool vi_scale = true;
	bool vi_serrate = true;
	bool vi_dither_filter = true;
	bool vi_divot_filter = true;
	bool vi_gamma_dither = true;
	bool previous_frame_blank = false;
	bool persist_frame_on_invalid_input = false;
	bool horizontal_input_valid = true;
	bool upscale_deinterlacing = true;
	uint32_t frame_count = 0;
	uint32_t last_valid_frame_count = 0;
	unsigned scaling_factor = 1;
	unsigned downscale_steps = 0;
};

struct VIScanoutFlowPolicy
{
	uint32_t option_masked_status = 0;
	uint32_t processing_status = 0;
	bool blank_frame = false;
	bool skip_repeated_blank_frame = false;
	bool reset_previous_scanout = false;
	bool persist_previous_on_invalid_input = false;
	bool should_downscale = false;
	bool should_upscale_deinterlace = false;
	unsigned post_downscale_scaling_factor = 1;
};

inline VIScanoutFlowPolicy derive_vi_scanout_flow_policy(const VIScanoutFlowPolicyInput &in)
{
	VIScanoutFlowPolicy out = {};
	out.option_masked_status = in.status;

	if (!in.vi_serrate)
		out.option_masked_status &= ~VI_CONTROL_SERRATE_BIT;

	bool status_is_aa = (out.option_masked_status & VI_CONTROL_AA_MODE_MASK) < VI_CONTROL_AA_MODE_RESAMP_ONLY_BIT;
	bool status_is_bilinear =
			(out.option_masked_status & VI_CONTROL_AA_MODE_MASK) < VI_CONTROL_AA_MODE_RESAMP_REPLICATE_BIT;

	status_is_aa = status_is_aa && in.vi_aa;
	status_is_bilinear = status_is_bilinear && in.vi_scale;

	out.option_masked_status &= ~(VI_CONTROL_AA_MODE_MASK | VI_CONTROL_META_AA_BIT | VI_CONTROL_META_SCALE_BIT);
	if (status_is_aa)
		out.option_masked_status |= VI_CONTROL_META_AA_BIT;
	if (status_is_bilinear)
		out.option_masked_status |= VI_CONTROL_META_SCALE_BIT;

	if (!in.vi_gamma_dither)
		out.option_masked_status &= ~VI_CONTROL_GAMMA_DITHER_ENABLE_BIT;
	if (!in.vi_divot_filter)
		out.option_masked_status &= ~VI_CONTROL_DIVOT_ENABLE_BIT;
	if (!in.vi_dither_filter)
		out.option_masked_status &= ~VI_CONTROL_DITHER_FILTER_ENABLE_BIT;

	out.blank_frame = (out.option_masked_status & VI_CONTROL_TYPE_RGBA5551_BIT) == 0;
	out.skip_repeated_blank_frame = out.blank_frame && in.previous_frame_blank;
	out.reset_previous_scanout = out.blank_frame;

	out.persist_previous_on_invalid_input =
			!in.horizontal_input_valid &&
			in.persist_frame_on_invalid_input &&
			(in.frame_count - in.last_valid_frame_count < 4);

	out.processing_status = out.option_masked_status | VI_CONTROL_TYPE_RGBA5551_BIT;
	out.should_downscale = in.downscale_steps != 0 && in.scaling_factor > 1;
	out.post_downscale_scaling_factor = std::max(1u, in.scaling_factor >> in.downscale_steps);
	out.should_upscale_deinterlace =
			(out.option_masked_status & VI_CONTROL_SERRATE_BIT) != 0 && in.upscale_deinterlacing;

	return out;
}
}

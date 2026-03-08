#ifndef PARALLEL_RDP_SCALING_QUIRK_POLICY_HPP
#define PARALLEL_RDP_SCALING_QUIRK_POLICY_HPP

#include "parallel-rdp/parallel-rdp/vi_scaling_mode.hpp"

namespace RDP
{
namespace detail
{
struct ScalingQuirkPolicyInput
{
	unsigned vi_scaling_mode = VI_SCALING_MODE_ACCURATE;
	unsigned experimental_texrect = VI_EXPERIMENTAL_OVERRIDE_AUTO;
	unsigned upscaling_factor = 1;
	bool native_tex_rect = true;
	bool hires_textures_enabled = false;
};

struct ScalingQuirkPolicy
{
	bool effective_native_tex_rect = true;
};

inline ScalingQuirkPolicy derive_scaling_quirk_policy(const ScalingQuirkPolicyInput &in)
{
	ScalingQuirkPolicy out = {};
	out.effective_native_tex_rect = in.native_tex_rect;
	bool enable_experimental_texrect = false;
	switch (in.experimental_texrect)
	{
	case VI_EXPERIMENTAL_OVERRIDE_ENABLED:
		enable_experimental_texrect = true;
		break;
	case VI_EXPERIMENTAL_OVERRIDE_DISABLED:
		enable_experimental_texrect = false;
		break;
	default:
		enable_experimental_texrect = in.vi_scaling_mode == VI_SCALING_MODE_EXPERIMENTAL;
		break;
	}
	if (enable_experimental_texrect && in.upscaling_factor > 1 && !in.hires_textures_enabled)
		out.effective_native_tex_rect = true;
	return out;
}
}
}

#endif

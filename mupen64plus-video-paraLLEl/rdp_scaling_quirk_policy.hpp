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
	unsigned upscaling_factor = 1;
	bool native_tex_rect = true;
};

struct ScalingQuirkPolicy
{
	bool effective_native_tex_rect = true;
};

inline ScalingQuirkPolicy derive_scaling_quirk_policy(const ScalingQuirkPolicyInput &in)
{
	ScalingQuirkPolicy out = {};
	out.effective_native_tex_rect = in.native_tex_rect;
	return out;
}
}
}

#endif

#pragma once

namespace RDP
{
namespace detail
{
struct HiresDescriptorFeatureSupport
{
	bool descriptor_indexing = false;
	bool runtime_descriptor_array = false;
	bool sampled_image_array_non_uniform_indexing = false;
	bool descriptor_binding_variable_descriptor_count = false;
	bool descriptor_binding_partially_bound = false;
	bool descriptor_binding_update_after_bind = false;
};

enum class HiresDescriptorRequirement
{
	Supported,
	MissingDescriptorIndexing,
	MissingRuntimeDescriptorArray,
	MissingSampledImageArrayNonUniformIndexing,
	MissingDescriptorBindingVariableDescriptorCount,
	MissingDescriptorBindingPartiallyBound,
	MissingDescriptorBindingUpdateAfterBind,
};

inline HiresDescriptorRequirement validate_hires_descriptor_support(const HiresDescriptorFeatureSupport &support)
{
	if (!support.descriptor_indexing)
		return HiresDescriptorRequirement::MissingDescriptorIndexing;
	if (!support.runtime_descriptor_array)
		return HiresDescriptorRequirement::MissingRuntimeDescriptorArray;
	if (!support.sampled_image_array_non_uniform_indexing)
		return HiresDescriptorRequirement::MissingSampledImageArrayNonUniformIndexing;
	if (!support.descriptor_binding_variable_descriptor_count)
		return HiresDescriptorRequirement::MissingDescriptorBindingVariableDescriptorCount;
	if (!support.descriptor_binding_partially_bound)
		return HiresDescriptorRequirement::MissingDescriptorBindingPartiallyBound;
	if (!support.descriptor_binding_update_after_bind)
		return HiresDescriptorRequirement::MissingDescriptorBindingUpdateAfterBind;
	return HiresDescriptorRequirement::Supported;
}

inline bool should_enable_hires_after_capability_check(bool requested,
                                                       HiresDescriptorRequirement requirement)
{
	return requested && requirement == HiresDescriptorRequirement::Supported;
}
}
}

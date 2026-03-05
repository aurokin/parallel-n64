#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Vulkan
{
namespace detail
{
constexpr uint16_t slangmosh_reflection_magic_v6[] = { 'G', 'R', 'A', 6 };

struct SlangmoshDescriptorSetLayoutV6
{
	uint32_t sampled_image_mask = 0;
	uint32_t storage_image_mask = 0;
	uint32_t uniform_buffer_mask = 0;
	uint32_t storage_buffer_mask = 0;
	uint32_t rtas_mask = 0;
	uint32_t sampled_texel_buffer_mask = 0;
	uint32_t storage_texel_buffer_mask = 0;
	uint32_t input_attachment_mask = 0;
	uint32_t sampler_mask = 0;
	uint32_t separate_image_mask = 0;
	uint32_t fp_mask = 0;
	uint32_t immutable_sampler_mask = 0;
	uint8_t array_size[32] = {};
};

struct SlangmoshResourceLayoutV6
{
	SlangmoshDescriptorSetLayoutV6 sets[4] = {};
	uint32_t input_mask = 0;
	uint32_t output_mask = 0;
	uint32_t push_constant_size = 0;
	uint32_t spec_constant_mask = 0;
	uint32_t bindless_set_mask = 0;
};

static_assert(sizeof(SlangmoshDescriptorSetLayoutV6) == 80, "Unexpected slangmosh v6 descriptor-set layout size.");
static_assert(sizeof(SlangmoshResourceLayoutV6) == 340, "Unexpected slangmosh v6 resource layout size.");

inline size_t slangmosh_resource_layout_v6_serialized_size()
{
	return sizeof(slangmosh_reflection_magic_v6) + sizeof(SlangmoshResourceLayoutV6);
}

inline bool parse_slangmosh_resource_layout_v6(const uint8_t *data,
                                               size_t size,
                                               SlangmoshResourceLayoutV6 *out)
{
	if (!data || !out)
		return false;

	if (size != slangmosh_resource_layout_v6_serialized_size())
		return false;

	if (std::memcmp(data, slangmosh_reflection_magic_v6, sizeof(slangmosh_reflection_magic_v6)) != 0)
		return false;

	std::memcpy(out, data + sizeof(slangmosh_reflection_magic_v6), sizeof(*out));
	return true;
}

inline uint32_t binding_mask_for_limit(unsigned num_bindings)
{
	if (num_bindings >= 32u)
		return 0xffffffffu;
	else if (num_bindings == 0u)
		return 0u;
	else
		return (1u << num_bindings) - 1u;
}

inline uint32_t combined_set_binding_mask(const SlangmoshDescriptorSetLayoutV6 &set)
{
	return set.sampled_image_mask |
	       set.storage_image_mask |
	       set.uniform_buffer_mask |
	       set.storage_buffer_mask |
	       set.rtas_mask |
	       set.sampled_texel_buffer_mask |
	       set.storage_texel_buffer_mask |
	       set.input_attachment_mask |
	       set.sampler_mask |
	       set.separate_image_mask |
	       set.fp_mask |
	       set.immutable_sampler_mask;
}

inline bool set_uses_high_bindings(const SlangmoshDescriptorSetLayoutV6 &set,
                                   unsigned num_bindings)
{
	const uint32_t allowed = binding_mask_for_limit(num_bindings);
	const uint32_t high = ~allowed;
	if (high == 0u)
		return false;

	return (combined_set_binding_mask(set) & high) != 0u;
}

inline bool set_uses_unsupported_features(const SlangmoshDescriptorSetLayoutV6 &set)
{
	return set.rtas_mask != 0 || set.storage_texel_buffer_mask != 0;
}
}
}

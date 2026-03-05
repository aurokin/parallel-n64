#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

namespace RDP::detail
{
enum class DeviceSupportRequirement
{
	Supported,
	MissingStorage16Bit,
	MissingStorage8Bit,
};

enum class SmallTypesDriverPolicy
{
	Allow,
	DisableAmdProprietary,
	DisableAmdOpenSource,
	DisableNvidiaProprietary,
	DisableIntelProprietaryWindows,
};

struct RendererEnvOverrides
{
	bool has_timestamp_override = false;
	bool timestamp_enabled = false;
	bool has_ubershader_override = false;
	bool ubershader_enabled = false;
	bool has_force_sync_override = false;
	bool force_sync_enabled = false;
	bool has_subgroup_override = false;
	bool allow_subgroup = true;
	bool has_small_types_override = false;
	bool allow_small_types = true;
	bool forces_small_types = false;
};

inline bool parse_positive_env_value(const char *value)
{
	return value && strtol(value, nullptr, 0) > 0;
}

inline RendererEnvOverrides derive_renderer_env_overrides(
		const char *bench_env,
		const char *ubershader_env,
		const char *force_sync_env,
		const char *subgroup_env,
		const char *small_types_env)
{
	RendererEnvOverrides env = {};

	env.has_timestamp_override = bench_env != nullptr;
	env.timestamp_enabled = parse_positive_env_value(bench_env);

	env.has_ubershader_override = ubershader_env != nullptr;
	env.ubershader_enabled = parse_positive_env_value(ubershader_env);

	env.has_force_sync_override = force_sync_env != nullptr;
	env.force_sync_enabled = parse_positive_env_value(force_sync_env);

	env.has_subgroup_override = subgroup_env != nullptr;
	env.allow_subgroup = subgroup_env ? parse_positive_env_value(subgroup_env) : true;

	env.has_small_types_override = small_types_env != nullptr;
	env.allow_small_types = small_types_env ? parse_positive_env_value(small_types_env) : true;
	env.forces_small_types = env.has_small_types_override;

	return env;
}

inline DeviceSupportRequirement validate_device_support_requirements(bool has_storage_16bit_ssbo,
                                                                     bool has_storage_8bit_ssbo)
{
	if (!has_storage_16bit_ssbo)
		return DeviceSupportRequirement::MissingStorage16Bit;
	if (!has_storage_8bit_ssbo)
		return DeviceSupportRequirement::MissingStorage8Bit;
	return DeviceSupportRequirement::Supported;
}

inline SmallTypesDriverPolicy small_types_driver_policy(uint32_t driver_id)
{
	switch (driver_id)
	{
	case VK_DRIVER_ID_AMD_PROPRIETARY_KHR:
		return SmallTypesDriverPolicy::DisableAmdProprietary;
	case VK_DRIVER_ID_AMD_OPEN_SOURCE_KHR:
	case VK_DRIVER_ID_MESA_RADV_KHR:
		return SmallTypesDriverPolicy::DisableAmdOpenSource;
	case VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR:
		return SmallTypesDriverPolicy::DisableNvidiaProprietary;
	case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS_KHR:
		return SmallTypesDriverPolicy::DisableIntelProprietaryWindows;
	default:
		return SmallTypesDriverPolicy::Allow;
	}
}

inline bool allow_small_types_after_driver_policy(bool allow_small_types,
                                                   bool forces_small_types,
                                                   bool supports_driver_properties,
                                                   uint32_t driver_id)
{
	if (!allow_small_types)
		return false;
	if (!supports_driver_properties || forces_small_types)
		return true;
	return small_types_driver_policy(driver_id) == SmallTypesDriverPolicy::Allow;
}

inline bool enable_small_integer_arithmetic(bool allow_small_types,
                                            bool shader_int16,
                                            bool shader_int8)
{
	return allow_small_types && shader_int16 && shader_int8;
}

inline bool enable_subgroup_tile_binning(bool allow_subgroup,
                                         VkSubgroupFeatureFlags supported_operations,
                                         VkShaderStageFlags supported_stages,
                                         bool supports_minimum_subgroup_size_32,
                                         uint32_t subgroup_size)
{
	constexpr VkSubgroupFeatureFlags required =
			VK_SUBGROUP_FEATURE_BALLOT_BIT |
			VK_SUBGROUP_FEATURE_BASIC_BIT |
			VK_SUBGROUP_FEATURE_VOTE_BIT |
			VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;

	return allow_subgroup &&
	       (supported_operations & required) == required &&
	       (supported_stages & VK_SHADER_STAGE_COMPUTE_BIT) != 0 &&
	       supports_minimum_subgroup_size_32 &&
	       subgroup_size <= 64;
}
}

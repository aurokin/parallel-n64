#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_device_capability_policy.hpp"

#include <cstdlib>
#include <iostream>

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

static void test_validate_device_support_requirements()
{
	using RDP::detail::DeviceSupportRequirement;
	using RDP::detail::validate_device_support_requirements;

	check(validate_device_support_requirements(true, true) == DeviceSupportRequirement::Supported,
	      "expected supported when 16-bit and 8-bit storage are available");
	check(validate_device_support_requirements(false, true) == DeviceSupportRequirement::MissingStorage16Bit,
	      "expected missing-16bit requirement");
	check(validate_device_support_requirements(true, false) == DeviceSupportRequirement::MissingStorage8Bit,
	      "expected missing-8bit requirement");
}

static void test_small_types_driver_policy_matrix()
{
	using RDP::detail::SmallTypesDriverPolicy;
	using RDP::detail::small_types_driver_policy;
	constexpr uint32_t unknown_driver_id = 0u;

	check(small_types_driver_policy(VK_DRIVER_ID_AMD_PROPRIETARY_KHR) ==
	              SmallTypesDriverPolicy::DisableAmdProprietary,
	      "AMD proprietary should disable small types");
	check(small_types_driver_policy(VK_DRIVER_ID_AMD_OPEN_SOURCE_KHR) ==
	              SmallTypesDriverPolicy::DisableAmdOpenSource,
	      "AMD open-source should disable small types");
	check(small_types_driver_policy(VK_DRIVER_ID_MESA_RADV_KHR) ==
	              SmallTypesDriverPolicy::DisableAmdOpenSource,
	      "Mesa RADV should disable small types");
	check(small_types_driver_policy(VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR) ==
	              SmallTypesDriverPolicy::DisableNvidiaProprietary,
	      "NVIDIA proprietary should disable small types");
	check(small_types_driver_policy(VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS_KHR) ==
	              SmallTypesDriverPolicy::DisableIntelProprietaryWindows,
	      "Intel proprietary Windows should disable small types");
	check(small_types_driver_policy(unknown_driver_id) == SmallTypesDriverPolicy::Allow,
	      "non-special-case drivers should keep small types allowed");
}

static void test_allow_small_types_after_driver_policy()
{
	using RDP::detail::allow_small_types_after_driver_policy;
	constexpr uint32_t unknown_driver_id = 0u;

	check(!allow_small_types_after_driver_policy(false, false, true, unknown_driver_id),
	      "disabled small types should remain disabled");
	check(!allow_small_types_after_driver_policy(true, false, true, VK_DRIVER_ID_AMD_PROPRIETARY_KHR),
	      "driver policy should disable small types when not forced");
	check(allow_small_types_after_driver_policy(true, true, true, VK_DRIVER_ID_AMD_PROPRIETARY_KHR),
	      "forced small types should bypass driver policy");
	check(allow_small_types_after_driver_policy(true, false, false, VK_DRIVER_ID_AMD_PROPRIETARY_KHR),
	      "driver policy should be skipped when driver properties are unavailable");
}

static void test_enable_small_integer_arithmetic()
{
	using RDP::detail::enable_small_integer_arithmetic;

	check(enable_small_integer_arithmetic(true, true, true),
	      "small integer arithmetic should enable with allow+int16+int8");
	check(!enable_small_integer_arithmetic(false, true, true),
	      "small integer arithmetic should disable when small types disallowed");
	check(!enable_small_integer_arithmetic(true, false, true),
	      "small integer arithmetic should disable without shaderInt16");
	check(!enable_small_integer_arithmetic(true, true, false),
	      "small integer arithmetic should disable without shaderInt8");
}

static void test_enable_subgroup_tile_binning()
{
	using RDP::detail::enable_subgroup_tile_binning;

	constexpr VkSubgroupFeatureFlags required_ops =
			VK_SUBGROUP_FEATURE_BALLOT_BIT |
			VK_SUBGROUP_FEATURE_BASIC_BIT |
			VK_SUBGROUP_FEATURE_VOTE_BIT |
			VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;

	check(enable_subgroup_tile_binning(true, required_ops, VK_SHADER_STAGE_COMPUTE_BIT, true, 32),
	      "subgroup tile binning should enable when all requirements are met");
	check(!enable_subgroup_tile_binning(false, required_ops, VK_SHADER_STAGE_COMPUTE_BIT, true, 32),
	      "subgroup tile binning should disable when subgroup use is overridden off");
	check(!enable_subgroup_tile_binning(true, required_ops & ~VK_SUBGROUP_FEATURE_VOTE_BIT,
	                                    VK_SHADER_STAGE_COMPUTE_BIT, true, 32),
	      "subgroup tile binning should disable when required subgroup operations are missing");
	check(!enable_subgroup_tile_binning(true, required_ops, 0, true, 32),
	      "subgroup tile binning should disable when compute stage support is missing");
	check(!enable_subgroup_tile_binning(true, required_ops, VK_SHADER_STAGE_COMPUTE_BIT, false, 32),
	      "subgroup tile binning should disable when minimum subgroup size support is missing");
	check(!enable_subgroup_tile_binning(true, required_ops, VK_SHADER_STAGE_COMPUTE_BIT, true, 128),
	      "subgroup tile binning should disable for subgroup sizes above 64");
}

static void test_derive_renderer_env_overrides()
{
	using RDP::detail::derive_renderer_env_overrides;

	auto env = derive_renderer_env_overrides(nullptr, nullptr, nullptr, nullptr, nullptr);
	check(!env.has_timestamp_override, "timestamp override should be absent by default");
	check(!env.has_ubershader_override, "ubershader override should be absent by default");
	check(!env.has_force_sync_override, "force sync override should be absent by default");
	check(!env.has_subgroup_override && env.allow_subgroup,
	      "subgroup should default to allowed when unset");
	check(!env.has_small_types_override && env.allow_small_types && !env.forces_small_types,
	      "small types should default to allowed and unforced when unset");

	env = derive_renderer_env_overrides("1", "0", "3", "0", "-1");
	check(env.has_timestamp_override && env.timestamp_enabled,
	      "timestamp override should parse positive values as enabled");
	check(env.has_ubershader_override && !env.ubershader_enabled,
	      "ubershader override should parse zero as disabled");
	check(env.has_force_sync_override && env.force_sync_enabled,
	      "force sync override should parse positive values as enabled");
	check(env.has_subgroup_override && !env.allow_subgroup,
	      "subgroup override should parse zero as disabled");
	check(env.has_small_types_override && !env.allow_small_types && env.forces_small_types,
	      "small types override should parse negative values as disabled and forced");
}
}

int main()
{
	test_validate_device_support_requirements();
	test_small_types_driver_policy_matrix();
	test_allow_small_types_after_driver_policy();
	test_enable_small_integer_arithmetic();
	test_enable_subgroup_tile_binning();
	test_derive_renderer_env_overrides();
	std::cout << "emu_unit_rdp_device_capability_policy_test: PASS" << std::endl;
	return 0;
}

#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_hires_runtime_policy.hpp"
#include "mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

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

static void test_cache_path_resolution_precedence()
{
	check(resolve_hires_cache_path("/explicit/path", "/env/path") == "/explicit/path",
	      "explicit cache path should take precedence over env");
	check(resolve_hires_cache_path("", "/env/path") == "/env/path",
	      "env cache path should be used when explicit path is empty");
	check(resolve_hires_cache_path("", nullptr).empty(),
	      "cache path should resolve to empty string when explicit/env are absent");
}

static void test_should_attempt_hires_cache_load_matrix()
{
	check(!should_attempt_hires_cache_load(false, "/cache"),
	      "disabled path should never attempt cache load");
	check(!should_attempt_hires_cache_load(true, nullptr),
	      "null path should not attempt cache load");
	check(!should_attempt_hires_cache_load(true, ""),
	      "empty path should not attempt cache load");
	check(should_attempt_hires_cache_load(true, "/cache"),
	      "enabled + non-empty path should attempt cache load");
}

static void test_classify_hires_configure_outcome_matrix()
{
	check(classify_hires_configure_outcome(false, "/cache", true) == HiresConfigureOutcome::Disabled,
	      "disabled outcome mismatch");
	check(classify_hires_configure_outcome(true, nullptr, true) == HiresConfigureOutcome::MissingPath,
	      "missing-path outcome mismatch (null)");
	check(classify_hires_configure_outcome(true, "", true) == HiresConfigureOutcome::MissingPath,
	      "missing-path outcome mismatch (empty)");
	check(classify_hires_configure_outcome(true, "/cache", false) == HiresConfigureOutcome::LoadFailed,
	      "load-failed outcome mismatch");
	check(classify_hires_configure_outcome(true, "/cache", true) == HiresConfigureOutcome::LoadSucceeded,
	      "load-succeeded outcome mismatch");

	check(!should_attach_hires_provider(HiresConfigureOutcome::Disabled),
	      "disabled outcome should not attach provider");
	check(!should_attach_hires_provider(HiresConfigureOutcome::MissingPath),
	      "missing-path outcome should not attach provider");
	check(!should_attach_hires_provider(HiresConfigureOutcome::LoadFailed),
	      "load-failed outcome should not attach provider");
	check(should_attach_hires_provider(HiresConfigureOutcome::LoadSucceeded),
	      "load-succeeded outcome should attach provider");
}

static void test_descriptor_index_sentinel_contract()
{
	check(hires_invalid_descriptor_index() == 0xffffffffu,
	      "invalid descriptor sentinel mismatch");
	check(!hires_descriptor_index_valid(hires_invalid_descriptor_index()),
	      "invalid descriptor sentinel should be rejected");
	check(hires_descriptor_index_valid(0u), "descriptor index zero should be considered valid");
	check(hires_descriptor_index_valid(17u), "descriptor index 17 should be considered valid");

	check(hires_shader_descriptor_mipmap_bit() == (1u << 30),
	      "shader descriptor mipmap bit mismatch");
	check(hires_shader_descriptor_index_mask() == ((1u << 30) - 1u),
	      "shader descriptor index mask mismatch");

	const uint32_t packed_nomips = pack_hires_shader_descriptor_index(17u, false);
	check(unpack_hires_shader_descriptor_index(packed_nomips) == 17u,
	      "packed descriptor should preserve index when mips are disabled");
	check(!hires_shader_descriptor_has_mips(packed_nomips),
	      "packed descriptor should not report mip flag when disabled");

	const uint32_t packed_mips = pack_hires_shader_descriptor_index(17u, true);
	check(unpack_hires_shader_descriptor_index(packed_mips) == 17u,
	      "packed descriptor should preserve index when mips are enabled");
	check(hires_shader_descriptor_has_mips(packed_mips),
	      "packed descriptor should report mip flag when enabled");
	check(pack_hires_shader_descriptor_index(hires_invalid_descriptor_index(), true) == hires_invalid_descriptor_index(),
	      "invalid descriptor should remain invalid when packed");

	const RDP::ReplacementMeta meta = {};
	check(meta.vk_image_index == hires_invalid_descriptor_index(),
	      "ReplacementMeta default descriptor index should match invalid sentinel");
}
}

int main()
{
	test_cache_path_resolution_precedence();
	test_should_attempt_hires_cache_load_matrix();
	test_classify_hires_configure_outcome_matrix();
	test_descriptor_index_sentinel_contract();
	std::cout << "emu_unit_hires_runtime_policy_test: PASS" << std::endl;
	return 0;
}

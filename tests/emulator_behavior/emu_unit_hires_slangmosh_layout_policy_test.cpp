#include "mupen64plus-video-paraLLEl/parallel-rdp/vulkan/resource_layout_policy.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace Vulkan::detail;

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

static SlangmoshResourceLayoutV6 make_layout_payload_v6()
{
	SlangmoshResourceLayoutV6 encoded = {};
	encoded.sets[0].sampled_image_mask = 0x0003u;
	encoded.sets[0].uniform_buffer_mask = 0x0010u;
	encoded.sets[0].rtas_mask = 0x0004u;
	encoded.sets[0].sampled_texel_buffer_mask = 0x0020u;
	encoded.sets[0].storage_texel_buffer_mask = 0x0040u;
	encoded.sets[0].array_size[0] = 1;
	encoded.sets[0].array_size[1] = 2;
	encoded.input_mask = 0x5u;
	encoded.output_mask = 0x2u;
	encoded.push_constant_size = 64u;
	encoded.spec_constant_mask = 0x3u;
	encoded.bindless_set_mask = 0x1u;
	return encoded;
}

static SlangmoshResourceLayoutV4 make_layout_payload_v4()
{
	SlangmoshResourceLayoutV4 encoded = {};
	encoded.sets[0].sampled_image_mask = 0x0003u;
	encoded.sets[0].uniform_buffer_mask = 0x0010u;
	encoded.sets[0].sampled_texel_buffer_mask = 0x0020u;
	encoded.sets[0].storage_texel_buffer_mask = 0x0040u;
	encoded.sets[0].array_size[0] = 1;
	encoded.sets[0].array_size[1] = 2;
	encoded.input_mask = 0x5u;
	encoded.output_mask = 0x2u;
	encoded.push_constant_size = 64u;
	encoded.spec_constant_mask = 0x3u;
	encoded.bindless_set_mask = 0x1u;
	return encoded;
}

static void fill_payload_v6(std::vector<uint8_t> &payload,
                            const SlangmoshResourceLayoutV6 &encoded)
{
	payload.resize(slangmosh_resource_layout_v6_serialized_size());
	std::memcpy(payload.data(), slangmosh_reflection_magic_v6, sizeof(slangmosh_reflection_magic_v6));
	std::memcpy(payload.data() + sizeof(slangmosh_reflection_magic_v6), &encoded, sizeof(encoded));
}

static void fill_payload_v4(std::vector<uint8_t> &payload,
                            const SlangmoshResourceLayoutV4 &encoded)
{
	payload.resize(slangmosh_resource_layout_v6_serialized_size());
	std::memcpy(payload.data(), slangmosh_reflection_magic_v4, sizeof(slangmosh_reflection_magic_v4));
	std::memcpy(payload.data() + sizeof(slangmosh_reflection_magic_v4), &encoded, sizeof(encoded));
}

static void assert_v6_layout_common_matches(const SlangmoshResourceLayoutV6 &parsed)
{
	check(parsed.sets[0].sampled_image_mask == 0x0003u,
	      "parsed sampled-image mask mismatch");
	check(parsed.sets[0].uniform_buffer_mask == 0x0010u,
	      "parsed uniform-buffer mask mismatch");
	check(parsed.sets[0].sampled_texel_buffer_mask == 0x0020u,
	      "parsed sampled-texel-buffer mask mismatch");
	check(parsed.sets[0].storage_texel_buffer_mask == 0x0040u,
	      "parsed storage-texel-buffer mask mismatch");
	check(parsed.sets[0].array_size[1] == 2u,
	      "parsed array-size mismatch");
	check(parsed.push_constant_size == 64u,
	      "parsed push-constant size mismatch");
	check(parsed.bindless_set_mask == 0x1u,
	      "parsed bindless set mask mismatch");
}

static void test_parse_slangmosh_resource_layout_v6_success()
{
	const SlangmoshResourceLayoutV6 encoded = make_layout_payload_v6();
	std::vector<uint8_t> payload;
	fill_payload_v6(payload, encoded);

	SlangmoshResourceLayoutV6 parsed = {};
	check(parse_slangmosh_resource_layout_v6(payload.data(), payload.size(), &parsed),
	      "v6 reflection payload should parse");
	assert_v6_layout_common_matches(parsed);
	check(parsed.sets[0].rtas_mask == 0x0004u,
	      "v6 parse should preserve rtas mask");
}

static void test_parse_slangmosh_resource_layout_v4_success()
{
	const SlangmoshResourceLayoutV4 encoded = make_layout_payload_v4();
	std::vector<uint8_t> payload;
	fill_payload_v4(payload, encoded);

	SlangmoshResourceLayoutV6 parsed = {};
	check(parse_slangmosh_resource_layout_v6(payload.data(), payload.size(), &parsed),
	      "v4 reflection payload should parse via compatibility path");
	assert_v6_layout_common_matches(parsed);
	check(parsed.sets[0].rtas_mask == 0u,
	      "v4 compatibility parse should zero rtas mask");
}

static void test_parse_slangmosh_resource_layout_v6_rejects_invalid_inputs()
{
	SlangmoshResourceLayoutV6 parsed = {};
	check(!parse_slangmosh_resource_layout_v6(nullptr,
	                                          slangmosh_resource_layout_v6_serialized_size(),
	                                          &parsed),
	      "null payload should fail parse");
	check(!parse_slangmosh_resource_layout_v6(reinterpret_cast<const uint8_t *>("abc"),
	                                          3,
	                                          &parsed),
	      "short payload should fail parse");

	std::vector<uint8_t> bad_magic(slangmosh_resource_layout_v6_serialized_size(), 0);
	check(!parse_slangmosh_resource_layout_v6(bad_magic.data(), bad_magic.size(), &parsed),
	      "bad magic should fail parse");
}

static void test_binding_mask_helpers()
{
	check(binding_mask_for_limit(0u) == 0u, "binding mask for 0 should be 0");
	check(binding_mask_for_limit(1u) == 0x1u, "binding mask for 1 should be 0x1");
	check(binding_mask_for_limit(16u) == 0xffffu, "binding mask for 16 should be 0xffff");
	check(binding_mask_for_limit(32u) == 0xffffffffu, "binding mask for >=32 should be 0xffffffff");
}

static void test_set_uses_high_bindings_contract()
{
	SlangmoshDescriptorSetLayoutV6 set = {};
	set.sampled_image_mask = 1u << 15u;
	check(!set_uses_high_bindings(set, 16u), "binding 15 should be valid for 16-binding limit");

	set.sampled_image_mask = 1u << 16u;
	check(set_uses_high_bindings(set, 16u), "binding 16 should exceed 16-binding limit");

	set = {};
	set.uniform_buffer_mask = 1u << 3u;
	set.array_size[3] = 7;
	check(combined_set_binding_mask(set) == (1u << 3u),
	      "combined set mask should include uniform-buffer bindings");
}

static void test_set_uses_unsupported_features_contract()
{
	SlangmoshDescriptorSetLayoutV6 set = {};
	check(!set_uses_unsupported_features(set),
	      "empty descriptor set should not report unsupported features");

	set.rtas_mask = 1u;
	check(set_uses_unsupported_features(set),
	      "rtas usage should report unsupported features");

	set = {};
	set.storage_texel_buffer_mask = 1u;
	check(set_uses_unsupported_features(set),
	      "storage texel buffer usage should report unsupported features");
}
}

int main()
{
	test_parse_slangmosh_resource_layout_v6_success();
	test_parse_slangmosh_resource_layout_v4_success();
	test_parse_slangmosh_resource_layout_v6_rejects_invalid_inputs();
	test_binding_mask_helpers();
	test_set_uses_high_bindings_contract();
	test_set_uses_unsupported_features_contract();

	std::cout << "emu_unit_hires_slangmosh_layout_policy_test: PASS" << std::endl;
	return 0;
}

#include "mupen64plus-video-paraLLEl/rdp_retro_image_mapping.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

using namespace RDP;

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

static void check_static_create_info_fields(const retro_vulkan_image &slot)
{
	check(slot.image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "image layout mismatch");
	check(slot.create_info.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, "create_info sType mismatch");
	check(slot.create_info.viewType == VK_IMAGE_VIEW_TYPE_2D, "viewType mismatch");
	check(slot.create_info.format == VK_FORMAT_R8G8B8A8_UNORM, "format mismatch");
	check(slot.create_info.subresourceRange.baseMipLevel == 0u, "baseMipLevel mismatch");
	check(slot.create_info.subresourceRange.baseArrayLayer == 0u, "baseArrayLayer mismatch");
	check(slot.create_info.subresourceRange.levelCount == 1u, "levelCount mismatch");
	check(slot.create_info.subresourceRange.layerCount == 1u, "layerCount mismatch");
	check(slot.create_info.subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT, "aspectMask mismatch");
	check(slot.create_info.components.r == VK_COMPONENT_SWIZZLE_R, "swizzle r mismatch");
	check(slot.create_info.components.g == VK_COMPONENT_SWIZZLE_G, "swizzle g mismatch");
	check(slot.create_info.components.b == VK_COMPONENT_SWIZZLE_B, "swizzle b mismatch");
	check(slot.create_info.components.a == VK_COMPONENT_SWIZZLE_A, "swizzle a mismatch");
}

static void test_populate_retro_image_slot_sets_expected_metadata()
{
	retro_vulkan_image slot = {};
	const auto image = reinterpret_cast<VkImage>(uintptr_t(0x1234));
	const auto image_view = reinterpret_cast<VkImageView>(uintptr_t(0x5678));

	detail::populate_retro_image_slot(slot, image, image_view);

	check(slot.create_info.image == image, "slot image handle mismatch");
	check(slot.image_view == image_view, "slot image_view handle mismatch");
	check_static_create_info_fields(slot);
}

static void test_slot_rotation_updates_only_target_slot()
{
	std::array<retro_vulkan_image, 2> slots = {};

	detail::populate_retro_image_slot(
			slots[0],
			reinterpret_cast<VkImage>(uintptr_t(0x1111)),
			reinterpret_cast<VkImageView>(uintptr_t(0xaaaa)));
	detail::populate_retro_image_slot(
			slots[1],
			reinterpret_cast<VkImage>(uintptr_t(0x2222)),
			reinterpret_cast<VkImageView>(uintptr_t(0xbbbb)));

	const auto slot1_prev_image = slots[1].create_info.image;
	const auto slot1_prev_view = slots[1].image_view;

	// Simulate sync-index rotation returning to slot 0.
	detail::populate_retro_image_slot(
			slots[0],
			reinterpret_cast<VkImage>(uintptr_t(0x3333)),
			reinterpret_cast<VkImageView>(uintptr_t(0xcccc)));

	check(slots[0].create_info.image == reinterpret_cast<VkImage>(uintptr_t(0x3333)),
	      "slot 0 image should be overwritten on reuse");
	check(slots[0].image_view == reinterpret_cast<VkImageView>(uintptr_t(0xcccc)),
	      "slot 0 image_view should be overwritten on reuse");
	check_static_create_info_fields(slots[0]);

	check(slots[1].create_info.image == slot1_prev_image,
	      "slot 1 image should remain stable when slot 0 is reused");
	check(slots[1].image_view == slot1_prev_view,
	      "slot 1 image_view should remain stable when slot 0 is reused");
	check_static_create_info_fields(slots[1]);
}
}

int main()
{
	test_populate_retro_image_slot_sets_expected_metadata();
	test_slot_rotation_updates_only_target_slot();
	std::cout << "emu_unit_rdp_retro_image_mapping_test: PASS" << std::endl;
	return 0;
}

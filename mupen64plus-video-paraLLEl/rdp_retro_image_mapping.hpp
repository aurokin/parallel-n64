#ifndef PARALLEL_RDP_RETRO_IMAGE_MAPPING_HPP
#define PARALLEL_RDP_RETRO_IMAGE_MAPPING_HPP

#include <libretro_vulkan.h>
#include <vulkan/vulkan.h>

namespace RDP
{
namespace detail
{
inline void populate_retro_image_slot(retro_vulkan_image &slot, VkImage image, VkImageView image_view)
{
	slot.image_view = image_view;
	slot.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	slot.create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	slot.create_info.image = image;
	slot.create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	slot.create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	slot.create_info.subresourceRange.baseMipLevel = 0;
	slot.create_info.subresourceRange.baseArrayLayer = 0;
	slot.create_info.subresourceRange.levelCount = 1;
	slot.create_info.subresourceRange.layerCount = 1;
	slot.create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	slot.create_info.components.r = VK_COMPONENT_SWIZZLE_R;
	slot.create_info.components.g = VK_COMPONENT_SWIZZLE_G;
	slot.create_info.components.b = VK_COMPONENT_SWIZZLE_B;
	slot.create_info.components.a = VK_COMPONENT_SWIZZLE_A;
}
}
}

#endif

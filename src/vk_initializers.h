// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

namespace vkinit {

	/// @brief Create a VkCommandPoolInfo with sensible defaults. 
	/// @param queueFamilyIndex pick queue family index. 
	/// @param flags command pool create flags. 
	/// @return 
	VkCommandPoolCreateInfo command_pool_create_info(
		uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0
	);

	/// @brief Create a VkCommandBufferAllocateInfo with sensible defaults. 
	/// @param pool specify a command pool. 
	/// @param count number of command buffers. 
	/// @param level primary or secondary command buffers. 
	/// @return
	VkCommandBufferAllocateInfo command_buffer_allocate_info(
		VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
	);
}


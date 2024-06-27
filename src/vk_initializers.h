// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_mem_alloc.h>

namespace vkinit
{
	VkResult create_buffer(VmaAllocator allocator, VkDeviceSize size, VmaMemoryUsage memoryUsage, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VmaAllocation &allocation);

	/// @brief Create a VkCommandPoolInfo with sensible defaults.
	/// @param queueFamilyIndex pick queue family index.
	/// @param flags command pool create flags.
	/// @return
	VkCommandPoolCreateInfo command_pool_create_info(
		uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

	/// @brief Create a VkCommandBufferAllocateInfo with sensible defaults.
	/// @param pool specify a command pool.
	/// @param count number of command buffers.
	/// @param level primary or secondary command buffers.
	/// @return
	VkCommandBufferAllocateInfo command_buffer_allocate_info(
		VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// create info's for images
	VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);
	VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

	// graphics pipeline states

	// createinfo used to add shaders to pipeline
	VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule);
	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();
	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);
	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);
	VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();
	VkPipelineColorBlendAttachmentState color_blend_attachment_state();

	// determines any push constants / descriptor sets used for shaders.
	VkPipelineLayoutCreateInfo pipeline_layout_create_info();
	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);
}

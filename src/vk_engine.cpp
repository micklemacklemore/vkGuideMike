﻿
#include "vk_engine.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// bootstrap library
#include "VkBootstrap.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>

#include <iostream>
#include <fstream>
#include <string>

// we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
#define VK_CHECK(x)                                                     \
	do                                                                  \
	{                                                                   \
		VkResult err = x;                                               \
		if (err != VK_SUCCESS)                                          \
		{                                                               \
			std::cout << "Detected Vulkan error: " << err << std::endl; \
			abort();                                                    \
		}                                                               \
	} while (0)

#define ASSETS_PREFIX(x) (std::string("/Users/michaelmason/Desktop/vulkan-guide/assets/") + x).c_str()
#define SHADER_PREFIX(x) (std::string("/Users/michaelmason/Desktop/vulkan-guide/shaders/") + x).c_str()

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it.
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags);

	init_vulkan();	  // create instance and device
	init_swapchain(); // create the swapchain
	init_commands();  // create command pool and buffer
	init_default_renderpass();
	init_framebuffers();
	init_sync_structures();
	init_pipelines();
	init_texture_image();
	init_texture_image_view(); 
	init_texture_sampler(); 
	init_uniform_buffers();
	init_descriptor_pool();
	init_descriptor_set();
	load_meshes();
	init_scene();

	// everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
	{
		vkWaitForFences(_device, _max_frames_in_flight, _renderFences.data(), VK_TRUE, 1000000000);
		_mainDeletionQueue.flush();

		// destroy all objects from init_vulkan
		vmaDestroyAllocator(_allocator);
		vkDestroyDevice(_device, nullptr);
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);
		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::init_scene()
{
	RenderObject monkey;
	monkey.mesh = get_mesh("monkey");
	monkey.material = get_material("defaultmesh");
	monkey.transformMatrix = glm::mat4{1.0f};
	_renderables.push_back(monkey);
}

void VulkanEngine::init_pipelines()
{
	// ==== BUILD GRAPHICS PIPELINE ====

	// build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;

	// setup depth stencil
	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	// input assembly is the configuration for drawing triangle lists, strips, or individual points.
	// we are just going to draw triangle list
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
	VertexInputDescription vertexDescription = Vertex::getVertexDescription();
	pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

	// build viewport and scissor from the swapchain extents
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)_windowExtent.width;
	pipelineBuilder._viewport.height = (float)_windowExtent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;
	pipelineBuilder._scissor.offset = {0, 0};
	pipelineBuilder._scissor.extent = _windowExtent;
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

	// we don't use multisampling, so just run the default one
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

	// a single blend attachment with no blending and writing to RGBA
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

	// we start from just the default empty pipeline layout info
	VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

	// ==== SETUP DESCRIPTOR SET LAYOUTS ====

	// create a uniform buffer layout binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;   // vertex shader input
	uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	// create a layout binding for sampler
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;   // fragment shader input

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
	// create a descriptor set, and attach our UBO to it
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_descriptorSetLayout));

	_mainDeletionQueue.push_function([=]()
									 { vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr); });

	// add our descriptor set to the pipeline
	mesh_pipeline_layout_info.setLayoutCount = 1;
	mesh_pipeline_layout_info.pSetLayouts = &_descriptorSetLayout;

	VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &_meshPipelineLayout));

	pipelineBuilder._pipelineLayout = _meshPipelineLayout;

	// Set up the uniform buffers and provide a mapping so that we can plug in data

	// ======

	// clear the shader stages for the builder
	pipelineBuilder._shaderStages.clear();

	VkShaderModule meshVertexShader;
	if (!load_shader_module(SHADER_PREFIX("tri_mesh.vert.spv"), &meshVertexShader))
	{
		std::cout << "Error when building the triangle vertex shader module" << std::endl;
	}
	else
	{
		std::cout << "vertex shader successfully loaded" << std::endl;
	}

	VkShaderModule meshFragmentShader;
	if (!load_shader_module(SHADER_PREFIX("colored_triangle.frag.spv"), &meshFragmentShader))
	{
		std::cout << "Error when building the triangle vertex shader module" << std::endl;
	}
	else
	{
		std::cout << "fragment shader successfully loaded" << std::endl;
	}

	// add the other shaders
	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertexShader));

	// make sure that triangleFragShader is holding the compiled colored_triangle.frag
	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, meshFragmentShader));

	// build the mesh triangle pipeline
	_meshPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

	create_material(_meshPipeline, _meshPipelineLayout, "defaultmesh");

	// ==== DELETION ====

	// deleting all of the vulkan shaders
	vkDestroyShaderModule(_device, meshVertexShader, nullptr);
	vkDestroyShaderModule(_device, meshFragmentShader, nullptr);

	// adding the pipelines to the deletion queue
	_mainDeletionQueue.push_function([=]()
									 {
		vkDestroyPipeline(_device, _meshPipeline, nullptr);
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr); });
}

void VulkanEngine::draw()
{
	// wait until the GPU has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &_renderFences[_currentFrame], true, 1000000000));
	VK_CHECK(vkResetFences(_device, 1, &_renderFences[_currentFrame]));

	// request image from the swapchain, one second timeout
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, _presentSemaphores[_currentFrame], nullptr, &swapchainImageIndex));
	VK_CHECK(vkResetCommandBuffer(_commandBuffers[_currentFrame], 0));

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;

	cmdBeginInfo.pInheritanceInfo = nullptr;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	// begin recording
	VK_CHECK(vkBeginCommandBuffer(_commandBuffers[_currentFrame], &cmdBeginInfo));

	// create framebuffer clear values for color and depth attachment

	VkClearValue clearValue;
	clearValue.color = {{0.f, 0.f, 0.f, 1.f}};

	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.f;

	VkRenderPassBeginInfo rpInfo = {};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpInfo.pNext = nullptr;
	rpInfo.renderPass = _renderPass;
	rpInfo.renderArea.offset.x = 0;
	rpInfo.renderArea.offset.y = 0;
	rpInfo.renderArea.extent = _windowExtent;
	rpInfo.framebuffer = _framebuffers[swapchainImageIndex];

	// connect clear values
	rpInfo.clearValueCount = 2;
	VkClearValue clearValues[] = {clearValue, depthClear};
	rpInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(_commandBuffers[_currentFrame], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	draw_objects(_commandBuffers[_currentFrame], _renderables.data(), _renderables.size());

	vkCmdEndRenderPass(_commandBuffers[_currentFrame]);
	VK_CHECK(vkEndCommandBuffer(_commandBuffers[_currentFrame]));

	// ==== SUBMIT TO QUEUE ====
	// prepare the submission to the queue.
	// we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	// we will signal the _renderSemaphore, to signal that rendering has finished

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &waitStage;

	// wait on present semaphore before executing the command, we're waiting for the next image in swapchain
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &_presentSemaphores[_currentFrame];
	// signal render semaphore once GPU has finished executing the command
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &_renderSemaphores[_currentFrame];
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &_commandBuffers[_currentFrame];

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block CPU until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _renderFences[_currentFrame]));

	// this will put the image we just rendered into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as it's necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &_renderSemaphores[_currentFrame];
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	// increase the number of frames drawn
	_frameNumber++;
	_currentFrame = (_currentFrame + 1) % _max_frames_in_flight;
}

glm::vec3 VulkanEngine::trackballProject(int pos_x, int pos_y)
{
	// cast everything to a float
	float width = static_cast<float>(_windowExtent.width);
	float height = static_cast<float>(_windowExtent.height);
	float x = static_cast<float>(pos_x);
	float y = static_cast<float>(pos_y);

	// arcball diameter is either width or height
	float s = glm::min(width, height) - 1;
	float s_inverse = (1.f / s);

	// project NDC coordinates to vector from origin of unit sphere
	float sx = s_inverse * (2. * x - width + 1);
	float sy = -s_inverse * (2. * y - height + 1);
	float sx_sx = sx * sx;
	float sy_sy = sy * sy;

	float sz = (sx_sx) + (sy_sy) > 0.5f ? 0.5f / glm::sqrt((sx_sx) + (sy_sy)) : glm::sqrt(1 - (sx_sx) - (sy_sy));

	return glm::normalize(glm::vec3(sx, sy, sz));
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	// main loop
	while (!bQuit)
	{
		// Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			int pos_x, pos_y;

			if (e.type == SDL_MOUSEBUTTONDOWN)
			{
				SDL_GetMouseState(&pos_x, &pos_y);
				_startTrackballV = trackballProject(pos_x, pos_y);
			}

			if (SDL_GetMouseState(&pos_x, &pos_y) & SDL_BUTTON_LMASK)
			{
				glm::vec3 end = trackballProject(pos_x, pos_y);
				_currTrackballQ = glm::rotation(_startTrackballV, end);
			}
			else
			{
				_lastTrackballQ = _currTrackballQ * _lastTrackballQ;
				_currTrackballQ = glm::quat(1.f, 0.f, 0.f, 0.f);
			}
			if (e.type == SDL_QUIT)
				bQuit = true;
		}
		draw();
	}
}

void VulkanEngine::init_texture_image()
{
	int texWidth, texHeight, texChannels;
	stbi_uc *pixels = stbi_load(ASSETS_PREFIX("wahoo.bmp"), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	// 4 bytes per pixel
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels)
	{
		throw std::runtime_error("failed to load texture image!");
	}

	AllocatedBuffer stagingBuffer;
	vkinit::create_buffer(
		_allocator,
		imageSize,
		VMA_MEMORY_USAGE_UNKNOWN,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer._buffer, stagingBuffer._allocation
	);

	// copy image data into staging buffer
	void *data;
	vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vmaUnmapMemory(_allocator, stagingBuffer._allocation);

	stbi_image_free(pixels);

	// create image and allocate the memory
	VK_CHECK(vkinit::create_image(
		_allocator,
		texWidth, texHeight,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_textureImage._image,
		_textureImage._allocation));

	// transition our texture layout to optimal transfer destination, then copy from staging buffer
	vkinit::transition_image_layout(_device, _commandPool, _graphicsQueue,
									_textureImage._image,
									VK_FORMAT_R8G8B8A8_SRGB,
									VK_IMAGE_LAYOUT_UNDEFINED,			 // old layout
									VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL // new layout
	);

	// make the copy from staging buffer to image on device memory
	vkinit::copy_buffer_to_image(_device, _commandPool, _graphicsQueue,
								 stagingBuffer._buffer,
								 _textureImage._image,
								 static_cast<uint32_t>(texWidth),
								 static_cast<uint32_t>(texHeight));

	// finally, transition image so it's optimal for shader access
	// transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkinit::transition_image_layout(
		_device, _commandPool, _graphicsQueue,
		_textureImage._image,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	 // old layout
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // new layout
	);

	// free the staging buffer
	vmaDestroyBuffer(_allocator, stagingBuffer._buffer, stagingBuffer._allocation); 

	_mainDeletionQueue.push_function([=]() {
		vmaDestroyImage(_allocator, _textureImage._image, _textureImage._allocation); 
	}); 
}

void VulkanEngine::init_texture_image_view()
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = _textureImage._image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &_textureImageView)); 

	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _textureImageView, nullptr); 
	}); 
}

void VulkanEngine::init_texture_sampler()
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	// linear interpolate when image is magnified or minified
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;

	// repeat texture when sampling beyond dimensions
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	samplerInfo.anisotropyEnable = VK_TRUE;

	// figure out our anisotropy value
	{
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(_chosenGPU, &properties);	
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy; 
	}

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	// no mip maps yet
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	VK_CHECK(vkCreateSampler(_device, &samplerInfo, nullptr, &_textureSampler)); 

	_mainDeletionQueue.push_function([=]() {
		vkDestroySampler(_device, _textureSampler, nullptr); 
	}); 
}

void VulkanEngine::load_meshes()
{
	Mesh monkeyMesh;
	monkeyMesh.load_from_obj(ASSETS_PREFIX("wahoo.obj"));
	upload_mesh(monkeyMesh);
	_meshes["monkey"] = monkeyMesh;
}

void VulkanEngine::upload_mesh(Mesh &mesh)
{
	// ==== TRANSFER VERTEX BUFFER ====
	VkDeviceSize size = mesh._vertices.size() * sizeof(Vertex);
	AllocatedBuffer stagingBuffer;

	// create staging buffer
	VkResult result = vkinit::create_buffer(
		_allocator,
		size,
		VMA_MEMORY_USAGE_UNKNOWN,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer._buffer,
		stagingBuffer._allocation);

	VK_CHECK(result);

	// copy vertex data to staging buffer
	void *data;
	vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
	memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(_allocator, stagingBuffer._allocation);

	// create device local buffer
	result = vkinit::create_buffer(
		_allocator,
		size,
		VMA_MEMORY_USAGE_UNKNOWN,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mesh._vertexBuffer._buffer,
		mesh._vertexBuffer._allocation);

	VK_CHECK(result);

	vkinit::copy_buffer(_device, _commandPool, _graphicsQueue, stagingBuffer._buffer, mesh._vertexBuffer._buffer, size);

	// no longer need the staging buffer
	vmaDestroyBuffer(_allocator, stagingBuffer._buffer, stagingBuffer._allocation);

	// ==== TRANSFER INDEX BUFFER ====
	size = sizeof(mesh._indices[0]) * mesh._indices.size();

	// create staging buffer
	result = vkinit::create_buffer(
		_allocator,
		size,
		VMA_MEMORY_USAGE_UNKNOWN,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer._buffer,
		stagingBuffer._allocation);
	VK_CHECK(result);

	// copy indice data into host memory
	vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
	memcpy(data, mesh._indices.data(), mesh._indices.size() * sizeof(mesh._indices[0]));
	vmaUnmapMemory(_allocator, stagingBuffer._allocation);

	// create device local buffer
	result = vkinit::create_buffer(
		_allocator,
		size,
		VMA_MEMORY_USAGE_UNKNOWN,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mesh._indexBuffer._buffer,
		mesh._indexBuffer._allocation);

	VK_CHECK(result);

	vkinit::copy_buffer(_device, _commandPool, _graphicsQueue, stagingBuffer._buffer, mesh._indexBuffer._buffer, size);
	vmaDestroyBuffer(_allocator, stagingBuffer._buffer, stagingBuffer._allocation);

	// add the destruction of triangle mesh buffer to the deletion queue
	_mainDeletionQueue.push_function([=]()
									 {  
		vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer, mesh._vertexBuffer._allocation); 
		vmaDestroyBuffer(_allocator, mesh._indexBuffer._buffer, mesh._indexBuffer._allocation); });
}

// private functions

bool VulkanEngine::load_shader_module(const char *filePath, VkShaderModule *outShaderModule)
{
	// open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		return false;
	}

	size_t fileSize = (size_t)file.tellg();

	// spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	// put file cursor at beginning
	file.seekg(0);

	// load the entire file into the buffer
	file.read((char *)buffer.data(), fileSize);

	// now that the file is loaded into the buffer, we can close it
	file.close();

	// create a new shader module, using the buffer we loaded
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	// codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	// check that the creation goes well.
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		return false;
	}
	*outShaderModule = shaderModule;
	return true;
}

void VulkanEngine::init_vulkan()
{
	// ======== INSTANCE =========

	vkb::InstanceBuilder builder;

	// make the Vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Example Vulkan Application")
						.request_validation_layers(true)
						.require_api_version(1, 1, 0)
						.use_default_debug_messenger()
						.build();

	vkb::Instance vkb_inst = inst_ret.value();

	// store the instance
	_instance = vkb_inst.instance;
	// store the debug messenger
	_debug_messenger = vkb_inst.debug_messenger;

	// ======== PHYSICAL DEVICE & DEVICE =========

	// get the surface of the window we opened with SDL
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	VkPhysicalDeviceFeatures features{}; 
	features.samplerAnisotropy = VK_TRUE; 

	// use vkbootstrap to select a GPU.
	// We want a GPU that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{vkb_inst};
	vkb::PhysicalDevice physicalDevice = selector
											 .set_minimum_version(1, 1)
											 .set_surface(_surface)
											 .set_required_features(features)
											 .select()
											 .value();

	// finally create the logical device
	vkb::DeviceBuilder deviceBuilder{physicalDevice};

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

	// get queue handle and queue family index
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	// initialize the memory allocator
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosenGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanEngine::init_swapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
	vkb::Swapchain vkbSwapchain = swapchainBuilder
									  .use_default_format_selection()
									  // use vsync present mode
									  .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
									  .set_desired_extent(_windowExtent.width, _windowExtent.height)
									  .build()
									  .value();

	// store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
	_swapchainImageFormat = vkbSwapchain.image_format;

	_mainDeletionQueue.push_function([=]()
									 { vkDestroySwapchainKHR(_device, _swapchain, nullptr); });

	// ==== allocate depth buffer image to memory ====

	// depth image size will match the window
	VkExtent3D depthImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	// hardcoding the depth format to 32 bit float
	_depthFormat = VK_FORMAT_D32_SFLOAT;

	// the depth image will be an image with the format we selected and Depth Attachment usage flag
	VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

	// for the depth image, we want to allocate it from GPU local memory
	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	dimg_allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	// allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage._image, &_depthImage._allocation, nullptr);

	// build an image-view for the depth image to use for rendering
	VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));

	// add to deletion queues
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _depthImageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation); 
	});
}

void VulkanEngine::init_commands()
{
	// resize to number of frames in flight
	_commandBuffers.resize(_max_frames_in_flight);

	// create command pool for graphics queue
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
		_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool));

	// create command buffer
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(
		_commandPool,
		static_cast<uint32_t>(_commandBuffers.size()));

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, _commandBuffers.data()));

	_mainDeletionQueue.push_function([=]()
									 { vkDestroyCommandPool(_device, _commandPool, nullptr); });
}

void VulkanEngine::init_default_renderpass()
{
	// ==== COLOR ATTACHMENT ====
	// create color attachment
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = _swapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// we Clear when this attachment is loaded
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// we keep the attachment stored when the renderpass ends
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// we don't care about stencil
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// we don't know or care about the starting layout of the attachment
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// after the renderpass ends, the image has to be on a layout ready for display
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// create our subpass
	VkAttachmentReference colorAttachmentReference{};
	// attachment number will index into the pAttachments array in the parent renderpass itself
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// ==== DEPTH ATTACHMENT ====
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.flags = 0;
	depthAttachment.format = _depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference = {};
	depthAttachmentReference.attachment = 1; // refers to the index in pAttachments
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// ==== SETUP DEPENDENCIES ====
	VkSubpassDependency colorDependency = {};
	colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	colorDependency.dstSubpass = 0;
	colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorDependency.srcAccessMask = 0;
	colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency depthDependency = {};
	depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	depthDependency.dstSubpass = 0;
	depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depthDependency.srcAccessMask = 0;
	depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	// ==== SETUP RENDERPASS & SUBPASS ===

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;

	// finally, create the render pass
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};
	VkSubpassDependency dependencies[2] = {colorDependency, depthDependency};

	// connect the color attachment to the info
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	// connect the subpass to the info
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies;

	VK_CHECK(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass));

	_mainDeletionQueue.push_function([=]()
									 { vkDestroyRenderPass(_device, _renderPass, nullptr); });
}

void VulkanEngine::init_framebuffers()
{
	// create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
	VkFramebufferCreateInfo frameBufferInfo = {};
	frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferInfo.pNext = nullptr;

	frameBufferInfo.renderPass = _renderPass;
	frameBufferInfo.attachmentCount = 1;
	frameBufferInfo.width = _windowExtent.width;
	frameBufferInfo.height = _windowExtent.height;
	frameBufferInfo.layers = 1;

	// grab how many images we have in the swapchain
	const uint32_t swapchain_imagecount = _swapchainImages.size();
	_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

	// create framebuffers for each of the swapchain image views
	for (int i = 0; i < swapchain_imagecount; i++)
	{
		VkImageView attachments[2] = {_swapchainImageViews[i], _depthImageView};
		frameBufferInfo.pAttachments = attachments;
		frameBufferInfo.attachmentCount = 2;

		VK_CHECK(vkCreateFramebuffer(_device, &frameBufferInfo, nullptr, &_framebuffers[i]));

		_mainDeletionQueue.push_function([=]()
										 {
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr); 
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr); });
	}
}

void VulkanEngine::init_sync_structures()
{
	_renderFences.resize(_max_frames_in_flight);
	_presentSemaphores.resize(_max_frames_in_flight);
	_renderSemaphores.resize(_max_frames_in_flight);

	// build fence
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	// for the semaphores we don't need any flags
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (size_t i = 0; i < _max_frames_in_flight; i++)
	{
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFences[i]));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphores[i]));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphores[i]));
	}

	_mainDeletionQueue.push_function([=]()
									 {
		for (size_t i = 0; i < _max_frames_in_flight; i++) {
			vkDestroyFence(_device, _renderFences[i], nullptr); 
			vkDestroySemaphore(_device, _presentSemaphores[i], nullptr); 
			vkDestroySemaphore(_device, _renderSemaphores[i], nullptr);
		} });
}

void VulkanEngine::init_uniform_buffers()
{
	{
		VkDeviceSize bufferSize = sizeof(UBO);
		_uniformBuffers.resize(_max_frames_in_flight);
		_uniformBufferMappings.resize(_max_frames_in_flight);

		for (size_t i = 0; i < _max_frames_in_flight; i++)
		{
			VkResult result = vkinit::create_buffer(
				_allocator,
				bufferSize,
				VMA_MEMORY_USAGE_UNKNOWN,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				_uniformBuffers[i]._buffer,
				_uniformBuffers[i]._allocation);
			VK_CHECK(result);
			VK_CHECK(vmaMapMemory(_allocator, _uniformBuffers[i]._allocation, &_uniformBufferMappings[i]));

			_mainDeletionQueue.push_function([=]()
											 {
				vmaUnmapMemory(_allocator, _uniformBuffers[i]._allocation); 
				vmaDestroyBuffer(_allocator, _uniformBuffers[i]._buffer, _uniformBuffers[i]._allocation); });
		}
	}
}

void VulkanEngine::init_descriptor_pool()
{
	// create a descriptor pool. This describes the total number of descriptor sets
	// we would like, per type. We use the descriptor pool to allocate descriptor sets
	VkDescriptorPoolSize poolSizeUniform{};
	poolSizeUniform.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizeUniform.descriptorCount = static_cast<uint32_t>(_max_frames_in_flight);

	VkDescriptorPoolSize poolSizeSampler{}; 
	poolSizeSampler.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; 
	poolSizeSampler.descriptorCount = static_cast<uint32_t>(_max_frames_in_flight); 

	std::array<VkDescriptorPoolSize, 2> poolSizeArray = {poolSizeUniform, poolSizeSampler}; 

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizeArray.size();
	poolInfo.pPoolSizes = poolSizeArray.data();
	poolInfo.maxSets = static_cast<uint32_t>(_max_frames_in_flight);
	VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_descriptorPool));

	_mainDeletionQueue.push_function([=]() { 
		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr); 
	});
}

void VulkanEngine::init_descriptor_set()
{
	// lets allocated our uniform descriptor set, we create an exact copy for each frame in flight
	std::vector<VkDescriptorSetLayout> layouts(_max_frames_in_flight, _descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = _descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(_max_frames_in_flight);
	allocInfo.pSetLayouts = layouts.data();

	_descriptorSets.resize(_max_frames_in_flight);
	VK_CHECK(vkAllocateDescriptorSets(_device, &allocInfo, _descriptorSets.data()));

	// for each of the descriptor sets we created, we need to specify
	// which binding we want our uniform buffer to write into
	// so we actually attach our buffer here
	for (size_t i = 0; i < _max_frames_in_flight; i++)
	{
		// specify the buffer we created
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = _uniformBuffers[i]._buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UBO);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = _textureImageView;
		imageInfo.sampler = _textureSampler;

		// specify the set and binding we want to write into, as well
		// as the buffer we'll use.
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = _descriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		VkWriteDescriptorSet descriptorImageWrite{}; 
		descriptorImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorImageWrite.dstSet = _descriptorSets[i];
		descriptorImageWrite.dstBinding = 1;
		descriptorImageWrite.dstArrayElement = 0;
		descriptorImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorImageWrite.descriptorCount = 1;
		descriptorImageWrite.pImageInfo = &imageInfo;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {descriptorWrite, descriptorImageWrite};

		vkUpdateDescriptorSets(_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
{
	// make viewport state from our stored viewport and scissor.
	// at the moment we won't support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;

	viewportState.viewportCount = 1;
	viewportState.pViewports = &_viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &_scissor;

	// setup dummy color blending. We aren't using transparent objects yet
	// the blending is just "no blend", but we do write to the color attachment
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;

	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &_colorBlendAttachment;

	// build graphics pipeline from stored states

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;

	// our "programmable" stages. (vertex and fragment shaders)
	pipelineInfo.stageCount = _shaderStages.size();
	pipelineInfo.pStages = _shaderStages.data();
	pipelineInfo.pVertexInputState = &_vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &_inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pDepthStencilState = &_depthStencil;
	pipelineInfo.pRasterizationState = &_rasterizer;
	pipelineInfo.pMultisampleState = &_multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = _pipelineLayout; // (?)
	pipelineInfo.renderPass = pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	// it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(
			device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS)
	{
		std::cout << "failed to create pipeline\n";
		return VK_NULL_HANDLE; // failed to create graphics pipeline
	}
	else
	{
		return newPipeline;
	}
}

Material *VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string &name)
{
	Material mat;
	mat.pipeline = pipeline;
	mat.pipelineLayout = layout;
	_materials[name] = mat;
	return &_materials[name];
}

Material *VulkanEngine::get_material(const std::string &name)
{
	// search for the object, and return nullptr if not found
	auto it = _materials.find(name);
	if (it == _materials.end())
	{
		return nullptr;
	}
	else
	{
		return &(*it).second;
	}
}

Mesh *VulkanEngine::get_mesh(const std::string &name)
{
	auto it = _meshes.find(name);
	if (it == _meshes.end())
	{
		return nullptr;
	}
	else
	{
		return &(*it).second;
	}
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject *first, int count)
{
	// make a model view matrix for rendering the object
	// camera view
	glm::vec3 camPos = {0.f, 0.f, -7.f};
	glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 0.1f, 200.0f);
	projection[1][1] *= -1;

	Mesh *lastMesh = nullptr;
	Material *lastMaterial = nullptr;

	for (int i = 0; i < count; i++)
	{
		RenderObject &object = first[i];
		if (object.material != lastMaterial)
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			lastMaterial = object.material;
		}

		glm::mat4 rot = glm::toMat4(_currTrackballQ * _lastTrackballQ);
		glm::mat4 model = rot * object.transformMatrix;
		glm::mat4 mesh_matrix = projection * view * model;

		UBO ubo{
			.time = static_cast<float>(_frameNumber),
			.mvp = mesh_matrix};

		memcpy(_uniformBufferMappings[_currentFrame], &ubo, sizeof(UBO));

		// only bind the mesh if it's a different one from last bind
		if (object.mesh != lastMesh)
		{
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer, &offset);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &_descriptorSets[_currentFrame], 0, nullptr);
			vkCmdBindIndexBuffer(cmd, object.mesh->_indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT32);
			lastMesh = object.mesh;
		}

		vkCmdDrawIndexed(cmd, static_cast<uint32_t>(object.mesh->_indices.size()), 1, 0, 0, 0);
	}

	_frameNumber++;
}

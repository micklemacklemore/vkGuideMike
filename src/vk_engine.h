// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

#include <vector>
#include <functional>
#include <deque> 
#include <vk_mem_alloc.h>
#include <vk_mesh.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <unordered_map>

struct Material {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject {
	Mesh* mesh;
	Material* material;
	glm::mat4 transformMatrix;
};

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call the function
		}

		deletors.clear();
	}
};

struct PipelineBuilder {
	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
	VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkViewport _viewport;
	VkRect2D _scissor;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;

	VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};

class VulkanEngine {
public:
	VmaAllocator _allocator; //vma lib allocator

	const int _max_frames_in_flight = 2;
	uint32_t _currentFrame = 0;

	DeletionQueue _mainDeletionQueue;
	
	VkInstance _instance; // Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
	VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface; // Vulkan window surface

	glm::quat _lastTrackballQ = glm::quat(1.f, 0.f, 0.f, 0.f);
	glm::quat _currTrackballQ = glm::quat(1.f ,0.f, 0.f, 0.f); 
	glm::vec3 _startTrackballV = glm::vec3(0.f); 

	// depth buffer
	VkImageView _depthImageView; 
	AllocatedImage _depthImage; 
	VkFormat _depthFormat; 

	//array of images from the swapchain
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkSwapchainKHR _swapchain; // from other articles
	VkFormat _swapchainImageFormat;

	VkQueue _graphicsQueue; // queue we will submit to
	uint32_t _graphicsQueueFamily; // family of that queue

	VkCommandPool _commandPool; //the command pool for our commands
	std::vector<VkCommandBuffer> _commandBuffers; 

	VkRenderPass _renderPass;
	std::vector<VkFramebuffer> _framebuffers;

	// synchronisation
	std::vector<VkSemaphore> _presentSemaphores, _renderSemaphores; // wait for swap chain to finish rendering current frame before presenting(?)
	std::vector<VkFence> _renderFences; // wait for GPU to finish draw command before continuing loop(?)

	VkPipelineLayout _meshPipelineLayout; 
	VkPipeline _meshPipeline;

	// scene description
	std::vector<RenderObject> _renderables;
	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh> _meshes;

	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
	Material* get_material(const std::string& name);
	Mesh* get_mesh(const std::string& name);
	void draw_objects(VkCommandBuffer cmd,RenderObject* first, int count);

public:
	bool _isInitialized{ false };
	int _frameNumber {0};

	VkExtent2D _windowExtent{ 1000 , 529 };

	struct SDL_Window* _window{ nullptr };

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

    //draw loop
	void draw();

    glm::vec3 trackballProject(int pos_x, int pos_y);

    //run main loop
	void run();

private:
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size); 
	void load_meshes();
	void upload_mesh(Mesh& mesh);

	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);

	void init_vulkan();
	void init_swapchain();
	void init_commands(); 
	void init_default_renderpass();
	void init_framebuffers();
	void init_sync_structures(); 
	void init_pipelines();
	void init_scene();
};

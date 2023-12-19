#include "kgfw_defines.h"

#if defined(KGFW_VULKAN)
//#error Vulkan implementation is unsuitable for use

#include "kgfw_graphics.h"
#include "kgfw_camera.h"
#include "kgfw_log.h"
#include "kgfw_time.h"
#include "kgfw_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linmath.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#define KGFW_GRAPHICS_DEFAULT_VERTICES_COUNT 0
#define KGFW_GRAPHICS_DEFAULT_INDICES_COUNT 0

/* if the vk functions return VkResult, wrap the call in a procedural macro called "VK_CALL" */
#ifdef KGFW_DEBUG
#define VK_SWAPCHAIN_RESIZE() { vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.vk.pdev, state.vk.surface, &state.vk.capabilities); if (state.vk.capabilities.currentExtent.width != 0 && state.vk.capabilities.currentExtent.height != 0) { swapchain_destroy(); swapchain_create(); } }
#define VK_CALL(statement) { VkResult vr = statement; if (vr == VK_ERROR_OUT_OF_DATE_KHR || vr == VK_SUBOPTIMAL_KHR) { VK_SWAPCHAIN_RESIZE(); } else if (vr != VK_SUCCESS) { kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Vulkan Error (%i 0x%x) %s:%u", vr, vr, __FILE__, __LINE__); abort(); } }
#define VK_CHECK_DO(statement, action) { VkResult vr = statement; if (vr == VK_ERROR_OUT_OF_DATE_KHR || vr == VK_SUBOPTIMAL_KHR) { VK_SWAPCHAIN_RESIZE(); } else if (vr != VK_SUCCESS) { action; } }
#else
#define VK_SWAPCHAIN_RESIZE() { vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.vk.pdev, state.vk.surface, &state.vk.capabilities); if (state.vk.capabilities.currentExtent.width != 0 && state.vk.capabilities.currentExtent.height != 0) { swapchain_destroy(); swapchain_create(); } }
#define VK_CALL(statement) { VkResult vr = statement; if (vr == VK_ERROR_OUT_OF_DATE_KHR || vr == VK_SUBOPTIMAL_KHR) { VK_SWAPCHAIN_RESIZE(); } }
#define VK_CHECK_DO(statement, action) { VkResult vr = statement; if (vr == VK_ERROR_OUT_OF_DATE_KHR || vr == VK_SUBOPTIMAL_KHR) { VK_SWAPCHAIN_RESIZE(); } else if (vr != VK_SUCCESS) { action; } }
#endif

#define VK_CHECK_DO_NO_SWAP(statement, action) { VkResult vr = statement; if (vr != VK_SUCCESS) { action; } }

typedef struct mesh_node {
	struct {
		float pos[3];
		float rot[3];
		float scale[3];
		unsigned char absolute;
	} transform;

	struct {
		mat4x4 translation;
		mat4x4 rotation;
		mat4x4 scale;
	} matrices;

	struct mesh_node * parent;
	struct mesh_node * child;
	struct mesh_node * sibling;
	struct mesh_node * prior_sibling;

	struct {
		VkBuffer vbuf;
		VkDeviceMemory vmem;
		VkBuffer ibuf;
		VkDeviceMemory imem;
		VkImage tex;
		VkDeviceMemory tmem;

		unsigned long long int vbo_size;
		unsigned long long int ibo_size;
	} vk;
} mesh_node_t;

typedef struct vk_ubo {
	mat4x4 mvp;
} vk_ubo_t;

struct {
	kgfw_window_t * window;
	kgfw_camera_t * camera;
	mat4x4 vp;

	mesh_node_t * mesh_root;

	unsigned int settings;

	struct {
		vec3 pos;
		vec3 color;
		float ambience;
		float diffusion;
		float speculation;
		float metalic;
	} light;

	struct {
		VkAllocationCallbacks * allocator;
		VkInstance instance;
		VkPhysicalDevice pdev;
		VkSurfaceKHR surface;
		VkSurfaceFormatKHR surface_fmt;
		VkSurfaceCapabilitiesKHR capabilities;
		VkPresentModeKHR present_mode;
		VkExtent2D extent;
		VkSwapchainKHR swapchain;
		VkDevice dev;
		VkQueue gfx_queue;
		VkQueue pres_queue;
		VkBuffer ubo;
		VkDeviceMemory umem;
		void * ubomap;
		VkViewport viewport;
		VkRect2D scissor;
		struct {
			VkImage * images;
			VkImageView * views;
			VkFramebuffer * framebuffers;
			unsigned int count;
		} images;
		struct {
			VkDescriptorSetLayout desc_layout;
			VkDescriptorPool desc_pool;
			VkDescriptorSet desc_set;
			VkPipelineLayout layout;
			VkPipeline pipeline;
			VkRenderPass render_pass;
		} pipeline;
		struct {
			VkShaderModule vshader;
			VkShaderModule fshader;
			VkImageView view;
			VkSampler sampler;
		} shaders;
		struct {
			VkCommandPool pool;
			VkCommandBuffer buffer;
		} cmd;
		struct {
			VkSemaphore image_available;
			VkSemaphore render_finished;
			VkFence in_flight;
		} sync;
		struct {
			unsigned int graphics;
			unsigned int present;
		} queue_families;
	} vk;
} static state = {
	NULL, NULL,
	{ 0 },
	NULL,
	KGFW_GRAPHICS_SETTINGS_DEFAULT,
	{
		{ 0, 100, 0 },
		{ 1, 1, 1 },
		0.0f, 0.5f, 0.25f, 8
	},

	NULL,
	{ 0 },
	VK_NULL_HANDLE,
	{ 0 },
	{ 0 },
	{ 0 },
};

#ifdef KGFW_DEBUG

struct {
	VkDebugUtilsMessengerEXT messenger;
	unsigned char exit;
} static debug_state = {
	{ 0 },
	0
};

#endif

struct {
	mat4x4 model;
	mat4x4 model_r;
	vec3 pos;
	vec3 rot;
	vec3 scale;
	unsigned int img;
} static recurse_state = {
	{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 },
	},
};

static void update_settings(unsigned int change);
static void register_commands(void);

static void meshes_draw_recursive(mesh_node_t * mesh);
static void meshes_draw_recursive_fchild(mesh_node_t * mesh);
static void meshes_free(mesh_node_t * node);
static mesh_node_t * meshes_new(void);
static void mesh_draw(mesh_node_t * mesh, mat4x4 out_m);
static void meshes_free_recursive_fchild(mesh_node_t * mesh);
static void meshes_free_recursive(mesh_node_t * mesh);

static int shaders_load(const char * vpath, const char * fpath, VkShaderModule * out_vertex, VkShaderModule * out_fragment);

static int pipeline_create(void) {
	VkViewport viewport = {
		.x = 0,
		.y = 0,
		.width = state.vk.extent.width,
		.height = state.vk.extent.height,
		.minDepth = 0,
		.maxDepth = 1,
	};

	VkRect2D scissor = {
		.offset = {.x = 0, .y = 0 },
		.extent = state.vk.extent,
	};

	VkPipelineShaderStageCreateInfo psscreate_infos[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = state.vk.shaders.vshader,
			.pName = "main",
			.pSpecializationInfo = NULL,
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = state.vk.shaders.fshader,
			.pName = "main",
			.pSpecializationInfo = NULL,
		},
	};

	VkDynamicState dyn_states[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dyn_state_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.dynamicStateCount = 2,
		.pDynamicStates = dyn_states,
	};

	VkVertexInputBindingDescription binding_desc = {
		.binding = 0,
		.stride = sizeof(kgfw_graphics_vertex_t),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription attr_descs[4] = {
		{
			.binding = 0,
			.location = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(kgfw_graphics_vertex_t, x),
		},
		{
			.binding = 0,
			.location = 1,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(kgfw_graphics_vertex_t, r),
		},
		{
			.binding = 0,
			.location = 2,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(kgfw_graphics_vertex_t, nx),
		},
		{
			.binding = 0,
			.location = 3,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(kgfw_graphics_vertex_t, u),
		}
	};

	VkPipelineVertexInputStateCreateInfo vcreate_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &binding_desc,
		.vertexAttributeDescriptionCount = 4,
		.pVertexAttributeDescriptions = attr_descs,
	};

	VkPipelineInputAssemblyStateCreateInfo icreate_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	VkPipelineViewportStateCreateInfo vpcreate_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor,
	};

	VkPipelineRasterizationStateCreateInfo rcreate_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0,
		.depthBiasClamp = 0,
		.depthBiasSlopeFactor = 0,
		.lineWidth = 1,
	};

	VkPipelineMultisampleStateCreateInfo mscreate_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = NULL,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	VkPipelineColorBlendAttachmentState cba_state = {
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};

	VkPipelineColorBlendStateCreateInfo cbcreate_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &cba_state,
		.blendConstants = { 0, 0, 0, 0 },
	};

	VkPipelineLayoutCreateInfo plcreate_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.setLayoutCount = 1,
		.pSetLayouts = &state.vk.pipeline.desc_layout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = NULL,
	};

	VK_CHECK_DO_NO_SWAP(vkCreatePipelineLayout(state.vk.dev, &plcreate_info, state.vk.allocator, &state.vk.pipeline.layout), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan pipeline layout");
		return 10;
	});

	VkGraphicsPipelineCreateInfo pcreate_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.stageCount = 2,
		.pStages = psscreate_infos,
		.pVertexInputState = &vcreate_info,
		.pInputAssemblyState = &icreate_info,
		.pTessellationState = NULL,
		.pViewportState = &vpcreate_info,
		.pRasterizationState = &rcreate_info,
		.pMultisampleState = &mscreate_info,
		.pDepthStencilState = NULL,
		.pColorBlendState = &cbcreate_info,
		.pDynamicState = &dyn_state_create_info,
		.layout = state.vk.pipeline.layout,
		.renderPass = state.vk.pipeline.render_pass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1,
	};

	VK_CHECK_DO_NO_SWAP(vkCreateGraphicsPipelines(state.vk.dev, VK_NULL_HANDLE, 1, &pcreate_info, state.vk.allocator, &state.vk.pipeline.pipeline), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan graphics pipeline");
		return 10;
	});

	return 0;
}

static void pipeline_destroy(void) {
	vkDestroyPipeline(state.vk.dev, state.vk.pipeline.pipeline, state.vk.allocator);
	vkDestroyPipelineLayout(state.vk.dev, state.vk.pipeline.layout, state.vk.allocator);
}

static int buffer_create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer * out_buffer, VkDeviceMemory * out_memory) {
	VkBufferCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
	};

	VK_CHECK_DO_NO_SWAP(vkCreateBuffer(state.vk.dev, &create_info, state.vk.allocator, out_buffer), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan buffer");
		return 1;
	});

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(state.vk.dev, *out_buffer, &reqs);
	
	VkPhysicalDeviceMemoryProperties props;
	vkGetPhysicalDeviceMemoryProperties(state.vk.pdev, &props);
	unsigned int index = 0;
	for (index = 0; index < props.memoryTypeCount; ++index) {
		if ((reqs.memoryTypeBits & (1 << index)) && (props.memoryTypes[index].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			goto found;
		}
	}

	kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to find appropriate Vulkan memory type");
	return 2;
found:;

	VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = reqs.size,
		.memoryTypeIndex = index,
	};

	VK_CHECK_DO_NO_SWAP(vkAllocateMemory(state.vk.dev, &alloc_info, state.vk.allocator, out_memory), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to allocate Vulkan memory");
		return 3;
	});

	VK_CHECK_DO_NO_SWAP(vkBindBufferMemory(state.vk.dev, *out_buffer, *out_memory, 0), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to allocate Vulkan memory");
		return 3;
	});

	return 0;
}

static void buffer_destroy(VkBuffer * buffer, VkDeviceMemory * memory) {
	vkDestroyBuffer(state.vk.dev, *buffer, state.vk.allocator);
	vkFreeMemory(state.vk.dev, *memory, state.vk.allocator);
}

static int buffer_copy(VkBuffer dst, VkBuffer src, VkDeviceSize size, VkDeviceSize offset) {
	VkCommandBufferAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = NULL,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandPool = state.vk.cmd.pool,
		.commandBufferCount = 1,
	};

	VkCommandBuffer cmd;
	VK_CHECK_DO_NO_SWAP(vkAllocateCommandBuffers(state.vk.dev, &alloc_info, &cmd), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to allocate Vulkan command buffer");
		return 1;
	});

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = NULL,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = NULL,
	};

	VK_CHECK_DO_NO_SWAP(vkBeginCommandBuffer(cmd, &begin_info), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to begin command buffer");
		return 2;
	});

	VkBufferCopy copy = {
		.srcOffset = 0,
		.dstOffset = 0,
		.size = size,
	};
	vkCmdCopyBuffer(cmd, src, dst, 1, &copy);
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};

	vkQueueSubmit(state.vk.gfx_queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(state.vk.gfx_queue);
	vkFreeCommandBuffers(state.vk.dev, state.vk.cmd.pool, 1, &cmd);

	return 0;
}

static int swapchain_create(void) {
	VK_CHECK_DO_NO_SWAP(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.vk.pdev, state.vk.surface, &state.vk.capabilities), return 1);
	state.vk.extent = state.vk.capabilities.currentExtent;

	{
		unsigned int image_count = (state.vk.capabilities.minImageCount + 1 > state.vk.capabilities.maxImageCount) ? state.vk.capabilities.maxImageCount : state.vk.capabilities.minImageCount + 1;
		VkSwapchainCreateInfoKHR create_info = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = NULL,
			.flags = 0,
			.surface = state.vk.surface,
			.minImageCount = image_count,
			.imageFormat = state.vk.surface_fmt.format,
			.imageColorSpace = state.vk.surface_fmt.colorSpace,
			.imageExtent = state.vk.extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = NULL,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = state.vk.present_mode,
			.clipped = VK_FALSE,
			.oldSwapchain = VK_NULL_HANDLE,
		};

		unsigned int indices[2] = { state.vk.queue_families.graphics, state.vk.queue_families.present };
		if (state.vk.queue_families.graphics != state.vk.queue_families.present) {
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			create_info.queueFamilyIndexCount = 2;
			create_info.pQueueFamilyIndices = indices;
		}

		VK_CHECK_DO_NO_SWAP(vkCreateSwapchainKHR(state.vk.dev, &create_info, state.vk.allocator, &state.vk.swapchain), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan swapchain");
			return 7;
		});
	}

	{
		VK_CHECK_DO_NO_SWAP(vkGetSwapchainImagesKHR(state.vk.dev, state.vk.swapchain, &state.vk.images.count, NULL), return 8);

		state.vk.images.images = malloc(state.vk.images.count * sizeof(VkImage));
		if (state.vk.images.images == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
			return 8;
		}
		VK_CHECK_DO_NO_SWAP(vkGetSwapchainImagesKHR(state.vk.dev, state.vk.swapchain, &state.vk.images.count, state.vk.images.images), return 8);

		state.vk.images.views = malloc(state.vk.images.count * sizeof(VkImageView));
		if (state.vk.images.views == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
			return 8;
		}

		for (unsigned int i = 0; i < state.vk.images.count; ++i) {
			VkImageViewCreateInfo create_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.image = state.vk.images.images[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = state.vk.surface_fmt.format,
				.components = {
					VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
				},
				.subresourceRange = {
					VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
				},
			};

			VK_CHECK_DO_NO_SWAP(vkCreateImageView(state.vk.dev, &create_info, state.vk.allocator, &state.vk.images.views[i]), {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan image view");
				return 8;
			});
		}
	}

	{
		state.vk.images.framebuffers = malloc(state.vk.images.count * sizeof(VkFramebuffer));
		if (state.vk.images.framebuffers == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
			return 11;
		}

		for (unsigned int i = 0; i < state.vk.images.count; ++i) {
			VkFramebufferCreateInfo create_info = {
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.renderPass = state.vk.pipeline.render_pass,
				.attachmentCount = 1,
				.pAttachments = &state.vk.images.views[i],
				.width = state.vk.extent.width,
				.height = state.vk.extent.height,
				.layers = 1,
			};

			VK_CHECK_DO_NO_SWAP(vkCreateFramebuffer(state.vk.dev, &create_info, state.vk.allocator, &state.vk.images.framebuffers[i]), {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan framebuffer");
				return 11;
			});
		}
	}

	return 0;
}
static int swapchain_destroy(void) {
	vkDeviceWaitIdle(state.vk.dev);

	for (unsigned int i = 0; i < state.vk.images.count; ++i) {
		vkDestroyFramebuffer(state.vk.dev, state.vk.images.framebuffers[i], state.vk.allocator);
		vkDestroyImageView(state.vk.dev, state.vk.images.views[i], state.vk.allocator);
	}

	free(state.vk.images.framebuffers);
	state.vk.images.framebuffers = NULL;
	free(state.vk.images.images);
	state.vk.images.images = NULL;
	free(state.vk.images.views);
	state.vk.images.views = NULL;
	vkDestroySwapchainKHR(state.vk.dev, state.vk.swapchain, state.vk.allocator);
	state.vk.swapchain = NULL;

	return 0;
}

#ifdef KGFW_DEBUG

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT * cdata, void * udata) {
	kgfw_log_severity_enum sev = KGFW_LOG_SEVERITY_TRACE;
	switch (severity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			sev = KGFW_LOG_SEVERITY_INFO;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			sev = KGFW_LOG_SEVERITY_WARN;
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			sev = KGFW_LOG_SEVERITY_ERROR;
			debug_state.exit = 1;
			break;
	}

	char * t = (type == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) ? "general" : (type == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) ? "violation" : (type == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) ? "performance" : "unknown";

	kgfw_logf(sev, "Vulkan Debug log (type: %s): %s", t, cdata->pMessage);
	if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		abort();
	}
	return VK_FALSE;
}

#endif

#define KGFW_VK_EXTEND(name) unsigned int extension_##name##_index = extensions_count + extra; ++extra;
#define KGFW_VK_SET_EXTENSION(name) extensions[extension_##name##_index] = VK_##name##_EXTENSION_NAME;
#define KGFW_VK_VERSION_MAJOR(version) (((version) >> 22U) &0x3FF)
#define KGFW_VK_VERSION_MINOR(version) (((version) >> 12U) &0x3FF)
#define KGFW_VK_VERSION_PATCH(version) (version &0xFFF)
#define KGFW_VULKAN_INFO

int kgfw_graphics_init(kgfw_window_t * window, kgfw_camera_t * camera) {
	register_commands();

	state.window = window;
	state.camera = camera;

	{
		unsigned int extensions_count = 0;
		const char ** extensions = NULL;
		{
			const char ** ext = glfwGetRequiredInstanceExtensions(&extensions_count);
			unsigned int extra = 0;

			#ifdef KGFW_DEBUG
			KGFW_VK_EXTEND(EXT_DEBUG_UTILS);
			#endif

			#ifdef KGFW_APPLE_MACOS
			KGFW_VK_EXTEND(KHR_PORTABILITY_ENUMERATION);
			#endif

			extensions = malloc((extensions_count + extra) * sizeof(char *));
			if (extensions == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
				return 2;
			}

			memcpy(extensions, ext, extensions_count * sizeof(char *));

			#ifdef KGFW_APPLE_MACOS
			KGFW_VK_SET_EXTENSION(KHR_PORTABILITY_ENUMERATION);
			create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
			#endif

			#ifdef KGFW_DEBUG
			KGFW_VK_SET_EXTENSION(EXT_DEBUG_UTILS);
			#endif

			extensions_count += extra;
			#ifdef KGFW_VULKAN_INFO
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "Selected Vulkan instance extensions:");
			for (unsigned int i = 0; i < extensions_count; ++i) {
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    %s", extensions[i]);
			}
			#endif
		}

		VkApplicationInfo app_info = {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = NULL,
			.pApplicationName = "kgfw",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "kgfw",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_API_VERSION_1_3,
		};

		VkInstanceCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = NULL, 0,
			.pApplicationInfo = &app_info,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = NULL,
			.enabledExtensionCount = 0,
			.ppEnabledExtensionNames = NULL,
		};

		create_info.enabledExtensionCount = extensions_count;
		create_info.ppEnabledExtensionNames = extensions;

		#ifdef KGFW_DEBUG
		{
			const char * val_layers[1] = {
				"VK_LAYER_KHRONOS_validation"
			};

			unsigned int layers_count = 0;
			VK_CALL(vkEnumerateInstanceLayerProperties(&layers_count, NULL));

			VkLayerProperties * layers = malloc(layers_count * sizeof(VkLayerProperties));
			if (layers == NULL) {
				goto no_val_layers;
			}
			VK_CALL(vkEnumerateInstanceLayerProperties(&layers_count, layers));

			#ifdef KGFW_VULKAN_INFO
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "Vulkan instance layers:");
			#endif
			VkBool32 found = VK_FALSE;
			for (unsigned int i = 0; i < layers_count; ++i) {
				#ifdef KGFW_VULKAN_INFO
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    %s", layers[i].layerName);
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "      Specification Version:  %u.%u.%u", KGFW_VK_VERSION_MAJOR(layers[i].specVersion), KGFW_VK_VERSION_MINOR(layers[i].specVersion), KGFW_VK_VERSION_PATCH(layers[i].specVersion));
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "      Implementation Version: %u.%u.%u", KGFW_VK_VERSION_MAJOR(layers[i].implementationVersion), KGFW_VK_VERSION_MINOR(layers[i].implementationVersion), KGFW_VK_VERSION_PATCH(layers[i].implementationVersion));
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "      Description: %s", layers[i].description);
				#endif
				if (strcmp(layers[i].layerName, val_layers[0]) == 0) {
					create_info.enabledLayerCount = 1;
					create_info.ppEnabledLayerNames = val_layers;
					found = VK_TRUE;
				}
			}
			free(layers);

			if (!found) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to find required Vulkan instance layers");
				return 1;
			}
		no_val_layers:;
		}
		#endif

		VK_CHECK_DO_NO_SWAP(vkCreateInstance(&create_info, state.vk.allocator, &state.vk.instance), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan instance");
			return 1;
		});

		free(extensions);
	}

	#ifdef KGFW_DEBUG
	{
		VkDebugUtilsMessengerCreateInfoEXT create_info = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext = NULL,
			.flags = 0,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = vk_debug_callback,
			.pUserData = NULL,
		};

		PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(state.vk.instance, "vkCreateDebugUtilsMessengerEXT");
		if (vkCreateDebugUtilsMessengerEXT == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "Failed to create Vulkan debug utilities messenger");
		} else {
			VK_CHECK_DO_NO_SWAP(vkCreateDebugUtilsMessengerEXT(state.vk.instance, &create_info, state.vk.allocator, &debug_state.messenger), {
				kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "Failed to create Vulkan debug utilities messenger");
			});
		}
	}
	#endif

	{
		if (glfwCreateWindowSurface(state.vk.instance, state.window->internal, state.vk.allocator, &state.vk.surface) != VK_SUCCESS) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan window surface");
			return 6;
		}
	}

	{
		unsigned int pdev_count = 0;
		VK_CALL(vkEnumeratePhysicalDevices(state.vk.instance, &pdev_count, NULL));

		VkPhysicalDevice * pdevs = malloc(pdev_count * sizeof(VkPhysicalDevice));
		if (pdevs == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
			return 3;
		}
		VK_CALL(vkEnumeratePhysicalDevices(state.vk.instance, &pdev_count, pdevs));

		unsigned int best = 0;
		int best_score = 0;
		int score = 0;

		#ifdef KGFW_VULKAN_INFO
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "Vulkan available physical devices:");
		#endif
		for (unsigned int i = 0; i < pdev_count; ++i) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(pdevs[i], &props);
			VkPhysicalDeviceFeatures feats;
			vkGetPhysicalDeviceFeatures(pdevs[i], &feats);

			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				score += 1000;
			}

			score += props.limits.maxImageDimension2D;

			if (score > best_score) {
				best_score = score;
				best = i;
			}

			#ifdef KGFW_VULKAN_INFO
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    %s", props.deviceName);
			const char * type = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_OTHER) ? "unknown" : (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ? "integrated" : (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? "dedicated" : (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) ? "virtual" : (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) ? "CPU" : "unknown";
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "      Device type: %s", type);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "      Api version: %u.%u.%u", KGFW_VK_VERSION_MAJOR(props.apiVersion), KGFW_VK_VERSION_MINOR(props.apiVersion), KGFW_VK_VERSION_PATCH(props.apiVersion));
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "      Score: %i", score);
			#endif
		}

		state.vk.pdev = pdevs[best];
		free(pdevs);
	}

	{
		unsigned int queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(state.vk.pdev, &queue_family_count, NULL);

		VkQueueFamilyProperties * queue_families = malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
		if (queue_families == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
			return 4;
		}
		vkGetPhysicalDeviceQueueFamilyProperties(state.vk.pdev, &queue_family_count, queue_families);

		state.vk.queue_families.graphics = UINT32_MAX;
		state.vk.queue_families.present = UINT32_MAX;
		for (unsigned int i = 0; i < queue_family_count; ++i) {
			VkBool32 present = 0;
			VK_CALL(vkGetPhysicalDeviceSurfaceSupportKHR(state.vk.pdev, i, state.vk.surface, &present));

			#ifdef KGFW_VULKAN_INFO
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "Vulkan queue family %u:", i);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Queue count: %u", queue_families[i].queueCount);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Timestamp valid bits: %u", queue_families[i].timestampValidBits);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Min image transfer granularity: %ux%u", queue_families[i].minImageTransferGranularity.width, queue_families[i].minImageTransferGranularity.height);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Supported operations: %u", queue_families[i].queueFlags);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    [%c] Present", (present) ? 'X' : ' ');
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    [%c] Graphics", (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ? 'X' : ' ');
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    [%c] Transfer", (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) ? 'X' : ' ');
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    [%c] Compute", (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) ? 'X' : ' ');
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    [%c] Protected", (queue_families[i].queueFlags & VK_QUEUE_PROTECTED_BIT) ? 'X' : ' ');
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    [%c] Sparse binding", (queue_families[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) ? 'X' : ' ');
			#endif

			if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				state.vk.queue_families.graphics = i;
			}

			if (present) {
				state.vk.queue_families.present = i;
			}
		}

		free(queue_families);

		if (state.vk.queue_families.graphics == UINT32_MAX || state.vk.queue_families.present == UINT32_MAX) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Vulkan families not found");
			return 4;
		}

		{
			VkSurfaceFormatKHR * formats = NULL;
			unsigned int formats_count = 0;
			VkPresentModeKHR * modes = NULL;
			unsigned int modes_count = 0;

			VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.vk.pdev, state.vk.surface, &state.vk.capabilities));
			VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(state.vk.pdev, state.vk.surface, &formats_count, NULL));
			formats = malloc(formats_count * sizeof(VkSurfaceFormatKHR));
			if (formats == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
				return 5;
			}

			VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(state.vk.pdev, state.vk.surface, &formats_count, formats));

			VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(state.vk.pdev, state.vk.surface, &modes_count, NULL));
			modes = malloc(modes_count * sizeof(VkPresentModeKHR));
			if (modes == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
				return 5;
			}

			VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(state.vk.pdev, state.vk.surface, &modes_count, modes));

			#ifdef KGFW_VULKAN_INFO
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "Vulkan surface state.vk.capabilities:");
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Min image count: %u", state.vk.capabilities.minImageCount);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Max image count: %u", state.vk.capabilities.maxImageCount);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Current extent: %ux%u", state.vk.capabilities.currentExtent.width, state.vk.capabilities.currentExtent.height);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Min extent: %ux%u", state.vk.capabilities.minImageExtent.width, state.vk.capabilities.minImageExtent.height);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Max extent: %ux%u", state.vk.capabilities.maxImageExtent.width, state.vk.capabilities.maxImageExtent.height);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Max image array layers: %u", state.vk.capabilities.maxImageArrayLayers);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Supported transforms: %u", state.vk.capabilities.supportedTransforms);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Current transform: %u", state.vk.capabilities.currentTransform);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Supported composite alpha flags: %u", state.vk.capabilities.supportedCompositeAlpha);
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "    Supported usage flags: %u", state.vk.capabilities.supportedUsageFlags);
			#endif

			for (unsigned int i = 0; i < formats_count; ++i) {
				if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					state.vk.surface_fmt = formats[i];
					break;
				}
			}
			
			state.vk.present_mode = VK_PRESENT_MODE_FIFO_KHR;
			for (unsigned int i = 0; i < modes_count; ++i) {
				if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
					state.vk.present_mode = modes[i];
					break;
				}
			}

			free(formats);
			free(modes);

			if (state.vk.capabilities.currentExtent.width != UINT32_MAX) {
				state.vk.extent = state.vk.capabilities.currentExtent;
			} else {
				state.vk.extent.width = state.window->width;
				state.vk.extent.height = state.window->height;

				state.vk.extent.width = (state.vk.extent.width < state.vk.capabilities.minImageExtent.width) ? state.vk.capabilities.minImageExtent.width : (state.vk.extent.width < state.vk.capabilities.maxImageExtent.width) ? state.vk.capabilities.maxImageExtent.width : state.vk.extent.width;
				state.vk.extent.height = (state.vk.extent.height < state.vk.capabilities.minImageExtent.height) ? state.vk.capabilities.minImageExtent.height : (state.vk.extent.height < state.vk.capabilities.maxImageExtent.height) ? state.vk.capabilities.maxImageExtent.height : state.vk.extent.height;
			}
		}

		float priority = 1.0f;
		VkDeviceQueueCreateInfo queue_create_infos[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.queueFamilyIndex = state.vk.queue_families.graphics,
				.queueCount = 1,
				.pQueuePriorities = &priority,
			},
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.queueFamilyIndex = state.vk.queue_families.present,
				.queueCount = 1,
				.pQueuePriorities = &priority,
			},
		};

		VkPhysicalDeviceFeatures feats = { 0 };

		VkDeviceCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.queueCreateInfoCount = 2,
			.pQueueCreateInfos = queue_create_infos,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = NULL,
			.enabledExtensionCount = 0,
			.ppEnabledExtensionNames = NULL,
			.pEnabledFeatures = &feats,
		};

		#ifdef KGFW_DEBUG
		const char * val_layers[1] = {
			"VK_LAYER_KHRONOS_validation"
		};

		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = &val_layers;
		#endif

		{
			unsigned int extensions_count = 0;
			vkEnumerateDeviceExtensionProperties(state.vk.pdev, NULL, &extensions_count, NULL);
			VkExtensionProperties * extensions = malloc(extensions_count * sizeof(VkExtensionProperties));
			if (extensions == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
				return 4;
			}

			vkEnumerateDeviceExtensionProperties(state.vk.pdev, NULL, &extensions_count, extensions);

			const char * required[] = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			};
			unsigned int unfound = sizeof(required) / sizeof(const char *);

			#ifdef KGFW_VULKAN_INFO
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "Vulkan available physical device extensions:");
			#endif
			for (unsigned int i = 0; i < extensions_count; ++i) {
				#ifdef KGFW_VULKAN_INFO
				char chosen = ' ';
				#endif
				for (unsigned int j = 0; j < sizeof(required) / sizeof(const char *); ++j) {
					if (strcmp(required[j], extensions[i].extensionName) == 0) {
						#ifdef KGFW_VULKAN_INFO
						chosen = 'X';
						#endif
						--unfound;
						break;
					}
				}
				#ifdef KGFW_VULKAN_INFO
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "[%c] %s", chosen, extensions[i].extensionName);
				kgfw_logf(KGFW_LOG_SEVERITY_INFO, "      Specification version: %u.%u.%u", KGFW_VK_VERSION_MAJOR(extensions[i].specVersion), KGFW_VK_VERSION_MINOR(extensions[i].specVersion), KGFW_VK_VERSION_PATCH(extensions[i].specVersion));
				#endif
			}

			free(extensions);

			if (unfound > 0) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Required Vulkan physical device extensions not found");
				return 4;
			}

			create_info.enabledExtensionCount = sizeof(required) / sizeof(const char *);
			create_info.ppEnabledExtensionNames = required;
		}

		VK_CHECK_DO_NO_SWAP(vkCreateDevice(state.vk.pdev, &create_info, state.vk.allocator, &state.vk.dev), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan device");
			return 5;
		});

		vkGetDeviceQueue(state.vk.dev, state.vk.queue_families.graphics, 0, &state.vk.gfx_queue);
		vkGetDeviceQueue(state.vk.dev, state.vk.queue_families.present, 0, &state.vk.pres_queue);
	}

	int ret = shaders_load("assets/shaders/vertex.spv", "assets/shaders/fragment.spv", &state.vk.shaders.vshader, &state.vk.shaders.fshader);
	if (ret != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to load shaders (%i 0x%x)", ret, ret);
		return 9;
	}

	{
		VkAttachmentDescription adesc = {
			.flags = 0,
			.format = state.vk.surface_fmt.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		};

		VkAttachmentReference aref = {
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription sdesc = {
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = NULL,
			.colorAttachmentCount = 1,
			.pColorAttachments = &aref,
			.pResolveAttachments = NULL,
			.pDepthStencilAttachment = NULL,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments = NULL,
		};

		VkSubpassDependency dep = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0,
		};

		VkRenderPassCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &adesc,
			.subpassCount = 1,
			.pSubpasses = &sdesc,
			.dependencyCount = 1,
			.pDependencies = &dep,
		};

		VK_CHECK_DO_NO_SWAP(vkCreateRenderPass(state.vk.dev, &create_info, state.vk.allocator, &state.vk.pipeline.render_pass), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan render pass");
			return 10;
		});
	}

	{
		VkDescriptorSetLayoutBinding layout_binding = {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.pImmutableSamplers = NULL,
		};

		VkDescriptorSetLayoutCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.bindingCount = 1,
			.pBindings = &layout_binding,
		};

		VK_CHECK_DO_NO_SWAP(vkCreateDescriptorSetLayout(state.vk.dev, &create_info, state.vk.allocator, &state.vk.pipeline.desc_layout), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan descriptor set layout");
			return 10;
		});
	}

	{
		int r = pipeline_create();
		if (r != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan pipeline");
			return r;
		}
	}

	{
		int r = swapchain_create();
		if (r != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan swapchain");
			return r;
		}
	}

	{
		VkCommandPoolCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = NULL,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = state.vk.queue_families.graphics,
		};

		VK_CHECK_DO_NO_SWAP(vkCreateCommandPool(state.vk.dev, &create_info, state.vk.allocator, &state.vk.cmd.pool), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan command pool");
			return 12;
		});

		VkCommandBufferAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = NULL,
			.commandPool = state.vk.cmd.pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VK_CHECK_DO_NO_SWAP(vkAllocateCommandBuffers(state.vk.dev, &alloc_info, &state.vk.cmd.buffer), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to allocate Vulkan command buffer");
			return 12;
		});
	}

	{
		VkSemaphoreCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
		};

		VK_CHECK_DO_NO_SWAP(vkCreateSemaphore(state.vk.dev, &create_info, state.vk.allocator, &state.vk.sync.image_available), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan semaphore");
			return 13;
		});

		VK_CHECK_DO_NO_SWAP(vkCreateSemaphore(state.vk.dev, &create_info, state.vk.allocator, &state.vk.sync.render_finished), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan semaphore");
			return 13;
		});
	}

	{
		VkFenceCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = NULL,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		VK_CHECK_DO_NO_SWAP(vkCreateFence(state.vk.dev, &create_info, state.vk.allocator, &state.vk.sync.in_flight), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan fence");
			return 14;
		});
	}

	{
		kgfw_graphics_vertex_t vertices[] = {
			{
				-1, -1, 0,
				1, 1, 0,
				0, 0, 0,
				0, 0,
			},
			{
				-1, 1, 0,
				0, 1, 1,
				0, 0, 0,
				0, 0,
			},
			{
				1, -1, 0,
				1, 0, 1,
				0, 0, 0,
				0, 0,
			},
			{
				1, 1, 0,
				1, 1, 1,
				0, 0, 0,
				0, 0,
			}
		};

		unsigned int indices[] = {
			0, 1, 2,
			1, 3, 2,
		};

		{
			if (buffer_create(sizeof(vk_ubo_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &state.vk.ubo, &state.vk.umem) != 0) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan uniform buffer");
				return 15;
			}

			vkMapMemory(state.vk.dev, state.vk.umem, 0, sizeof(vk_ubo_t), 0, &state.vk.ubomap);
		}

		{
			VkDescriptorPoolSize pool_size = {
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
			};

			VkDescriptorPoolCreateInfo create_info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.maxSets = 1,
				.poolSizeCount = 1,
				.pPoolSizes = &pool_size,
			};

			VK_CHECK_DO_NO_SWAP(vkCreateDescriptorPool(state.vk.dev, &create_info, state.vk.allocator, &state.vk.pipeline.desc_pool), {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan descriptor pool");
				return 15;
				});
		}

		{
			VkDescriptorSetAllocateInfo alloc_info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = NULL,
				.descriptorPool = state.vk.pipeline.desc_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &state.vk.pipeline.desc_layout,
			};

			VK_CHECK_DO_NO_SWAP(vkAllocateDescriptorSets(state.vk.dev, &alloc_info, &state.vk.pipeline.desc_set), {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to allocate Vulkan descriptor set");
				return 16;
				});

			VkDescriptorBufferInfo buffer_info = {
				.buffer = state.vk.ubo,
				.offset = 0,
				.range = sizeof(vk_ubo_t),
			};

			VkWriteDescriptorSet write = {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = NULL,
				.dstSet = state.vk.pipeline.desc_set,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pImageInfo = NULL,
				.pBufferInfo = &buffer_info,
				.pTexelBufferView = NULL,
			};

			vkUpdateDescriptorSets(state.vk.dev, 1, &write, 0, NULL);
		}
	}

	update_settings(state.settings);

	return 0;
}

void kgfw_graphics_deinit(void) {
	vkDeviceWaitIdle(state.vk.dev);

	meshes_free_recursive_fchild(state.mesh_root);

	buffer_destroy(&state.vk.ubo, &state.vk.umem);

	vkDestroyFence(state.vk.dev, state.vk.sync.in_flight, state.vk.allocator);
	vkDestroySemaphore(state.vk.dev, state.vk.sync.render_finished, state.vk.allocator);
	vkDestroySemaphore(state.vk.dev, state.vk.sync.image_available, state.vk.allocator);

	vkDestroyCommandPool(state.vk.dev, state.vk.cmd.pool, state.vk.allocator);

	swapchain_destroy();
	pipeline_destroy();

	vkDestroyDescriptorPool(state.vk.dev, state.vk.pipeline.desc_pool, state.vk.allocator);
	vkDestroyDescriptorSetLayout(state.vk.dev, state.vk.pipeline.desc_layout, state.vk.allocator);

	vkDestroyRenderPass(state.vk.dev, state.vk.pipeline.render_pass, state.vk.allocator);

	vkDestroyShaderModule(state.vk.dev, state.vk.shaders.vshader, state.vk.allocator);
	vkDestroyShaderModule(state.vk.dev, state.vk.shaders.fshader, state.vk.allocator);

	vkDestroyDevice(state.vk.dev, state.vk.allocator);
	vkDestroySurfaceKHR(state.vk.instance, state.vk.surface, state.vk.allocator);

	#ifdef KGFW_DEBUG
	if (debug_state.messenger != NULL) {
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(state.vk.instance, "vkDestroyDebugUtilsMessengerEXT");
		vkDestroyDebugUtilsMessengerEXT(state.vk.instance, debug_state.messenger, state.vk.allocator);
	}
	#endif

	vkDestroyInstance(state.vk.instance, state.vk.allocator);
}

int kgfw_graphics_draw(void) {
	#ifdef KGFW_DEBUG
	if (debug_state.exit) {
		return -1;
	}
	#endif

	mat4x4 mvp;
	mat4x4 m;
	mat4x4 v;
	mat4x4 p;

	mat4x4_identity(mvp);
	mat4x4_identity(state.vp);
	mat4x4_identity(m);
	mat4x4_identity(v);
	mat4x4_identity(p);

	kgfw_camera_view(state.camera, v);
	kgfw_camera_perspective(state.camera, p);

	p[1][1] *= -1;
	mat4x4_mul(state.vp, p, v);

	state.light.pos[0] = sinf(kgfw_time_get() * 2) * 10;
	state.light.pos[1] = cosf(kgfw_time_get() / 6) * 15;
	state.light.pos[2] = sinf(kgfw_time_get() + 3) * 10;

	VK_CHECK_DO(vkWaitForFences(state.vk.dev, 1, &state.vk.sync.in_flight, VK_TRUE, UINT64_MAX), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to wait for Vulkan fence");
		return 1;
	});

	VK_CHECK_DO(vkResetFences(state.vk.dev, 1, &state.vk.sync.in_flight), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to reset Vulkan fence");
		return 1;
	});

	VK_CHECK_DO(vkResetCommandBuffer(state.vk.cmd.buffer, 0), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to reset Vulkan command buffer");
		return 1;
	});

	{
		VkCommandBufferBeginInfo cbbegin_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = NULL,
			.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
			.pInheritanceInfo = NULL,
		};

		VK_CHECK_DO(vkBeginCommandBuffer(state.vk.cmd.buffer, &cbbegin_info), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to begin Vulkan command buffer");
			return 1;
		});
	}

	unsigned int img;
	VK_CHECK_DO(vkAcquireNextImageKHR(state.vk.dev, state.vk.swapchain, UINT64_MAX, state.vk.sync.image_available, VK_NULL_HANDLE, &img), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to acquire next Vulkan swapchain image");
		return 2;
	});
	recurse_state.img = img;

	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = NULL,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = state.vk.images.images[img],
		.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
	};

	vkCmdPipelineBarrier(state.vk.cmd.buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	state.vk.viewport.x = 0;
	state.vk.viewport.y = 0;
	state.vk.viewport.width = state.vk.extent.width;
	state.vk.viewport.height = state.vk.extent.height;
	state.vk.viewport.minDepth = 0;
	state.vk.viewport.maxDepth = 1;

	state.vk.scissor.offset = (VkOffset2D){ 0, 0 };
	state.vk.scissor.extent = state.vk.extent;

	VkClearValue color = {
		.color = {
			.float32 = {
				0.086f, 0.082f, 0.090f, 1.0f
			}
		}
	};

	VkRenderPassBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = NULL,
		.renderPass = state.vk.pipeline.render_pass,
		.framebuffer = state.vk.images.framebuffers[recurse_state.img],
		.renderArea = {
			.offset = { 0, 0 },
			.extent = state.vk.extent,
		},
		.clearValueCount = 1,
		.pClearValues = &color,
	};

	vkCmdBeginRenderPass(state.vk.cmd.buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(state.vk.cmd.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.vk.pipeline.pipeline);
	vkCmdBindDescriptorSets(state.vk.cmd.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.vk.pipeline.layout, 0, 1, &state.vk.pipeline.desc_set, 0, NULL);

	if (state.mesh_root != NULL) {
		mat4x4_identity(recurse_state.model);

		recurse_state.pos[0] = 0;
		recurse_state.pos[1] = 0;
		recurse_state.pos[2] = 0;
		recurse_state.rot[0] = 0;
		recurse_state.rot[1] = 0;
		recurse_state.rot[2] = 0;
		recurse_state.scale[0] = 1;
		recurse_state.scale[1] = 1;
		recurse_state.scale[2] = 1;

		meshes_draw_recursive_fchild(state.mesh_root);
	}

	vkCmdEndRenderPass(state.vk.cmd.buffer);

	VK_CHECK_DO(vkEndCommandBuffer(state.vk.cmd.buffer), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to end Vulkan command buffer");
		return 1;
	});

	{
		VkPipelineStageFlags psflags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submit_info = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = NULL,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &state.vk.sync.image_available,
			.pWaitDstStageMask = &psflags,
			.commandBufferCount = 1,
			.pCommandBuffers = &state.vk.cmd.buffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &state.vk.sync.render_finished,
		};

		VK_CHECK_DO(vkQueueSubmit(state.vk.gfx_queue, 1, &submit_info, state.vk.sync.in_flight), {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to submit Vulkan queue");
			return 2;
		});

		VkResult result = VK_SUCCESS;
		VkPresentInfoKHR present_info = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = NULL,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &state.vk.sync.render_finished,
			.swapchainCount = 1,
			.pSwapchains = &state.vk.swapchain,
			.pImageIndices = &img,
			.pResults = &result,
		};

		VK_CHECK_DO(vkQueuePresentKHR(state.vk.pres_queue, &present_info), {
			return 3;
		});
		VK_CHECK_DO(result, return 3);
	}

	return 0;
}

void kgfw_graphics_mesh_texture(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_t * texture, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;
	unsigned long long int size = texture->width * texture->height * 4;

	VkBuffer staging;
	VkDeviceMemory staging_mem;

	{
		if (buffer_create(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging, &staging_mem) != 0) {
			return;
		}

		void * data;
		vkMapMemory(state.vk.dev, staging_mem, 0, size, 0, &data);
		memcpy(data, texture->bitmap, size);
		vkUnmapMemory(state.vk.dev, staging_mem);
	}

	VkFormat fmt = (texture->fmt == KGFW_GRAPHICS_TEXTURE_FORMAT_RGBA) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_B8G8R8A8_SRGB;

	VkImageCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = fmt,
		.extent = { .width = texture->width, .height = texture->height, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VK_CHECK_DO_NO_SWAP(vkCreateImage(state.vk.dev, &create_info, state.vk.allocator, &m->vk.tex), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan texture image");
		return;
	});

	VkMemoryRequirements reqs;
	vkGetImageMemoryRequirements(state.vk.dev, m->vk.tex, &reqs);

	VkPhysicalDeviceMemoryProperties props;
	vkGetPhysicalDeviceMemoryProperties(state.vk.pdev, &props);
	unsigned int index = 0;
	for (index = 0; index < props.memoryTypeCount; ++index) {
		if ((reqs.memoryTypeBits & (1 << index)) && (props.memoryTypes[index].propertyFlags & (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) == (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
			goto found;
		}
	}
	kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to find Vulkan memory type for texture image");
	return;
found:;

	VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = reqs.size,
		.memoryTypeIndex = index,
	};

	VK_CHECK_DO_NO_SWAP(vkAllocateMemory(state.vk.dev, &alloc_info, state.vk.allocator, &m->vk.tmem), {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to allocate Vulkan texture image memory");
		return;
	});

	vkBindImageMemory(state.vk.dev, m->vk.tex, m->vk.tmem, 0);

	{
		VkCommandBufferAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = NULL,
			.commandPool = state.vk.cmd.pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VkCommandBuffer cmd;
		vkAllocateCommandBuffers(state.vk.dev, &alloc_info, &cmd);

		VkCommandBufferBeginInfo begin_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = NULL,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = NULL,
		};

		vkBeginCommandBuffer(cmd, &begin_info);

		{
			VkImageMemoryBarrier barrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = NULL,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = m->vk.tex,
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
			};

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
			vkCmdCopyBufferToImage(cmd, staging, m->vk.tex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy){
				.bufferOffset = 0,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
				.imageOffset = { 0, 0, 0 },
				.imageExtent = { texture->width, texture->height, 1 },
			});

			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
		}

		vkEndCommandBuffer(cmd);

		VkSubmitInfo submit_info = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = NULL,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = NULL,
			.pWaitDstStageMask = NULL,
			.commandBufferCount = 1,
			.pCommandBuffers = &cmd,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = NULL,
		};

		vkQueueSubmit(state.vk.gfx_queue, 1, &submit_info, VK_NULL_HANDLE);
		vkQueueWaitIdle(state.vk.gfx_queue);

		vkFreeCommandBuffers(state.vk.dev, state.vk.cmd.pool, 1, &cmd);
	}

	buffer_destroy(&staging, &staging_mem);
}

void kgfw_graphics_mesh_texture_detach(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;

}

kgfw_graphics_mesh_node_t * kgfw_graphics_mesh_new(kgfw_graphics_mesh_t * mesh, kgfw_graphics_mesh_node_t * parent) {
	mesh_node_t * node = meshes_new();
	node->parent = (mesh_node_t *) parent;
	memcpy(node->transform.pos, mesh->pos, sizeof(vec3));
	memcpy(node->transform.rot, mesh->rot, sizeof(vec3));
	memcpy(node->transform.scale, mesh->scale, sizeof(vec3));
	
	{
		VkBuffer staging;
		VkDeviceMemory staging_mem;
		node->vk.vbo_size = mesh->vertices_count;
		node->vk.ibo_size = mesh->indices_count;
		VkDeviceSize vsize = sizeof(kgfw_graphics_vertex_t) * node->vk.vbo_size;
		VkDeviceSize isize = sizeof(unsigned int) * node->vk.ibo_size;
		if (buffer_create(vsize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging, &staging_mem) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan staging buffer");
			return 15;
		}

		void * data;
		vkMapMemory(state.vk.dev, staging_mem, 0, vsize, 0, &data);
		memcpy(data, mesh->vertices, vsize);
		vkUnmapMemory(state.vk.dev, staging_mem);

		if (buffer_create(vsize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &node->vk.vbuf, &node->vk.vmem) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan vertex buffer");
			return 15;
		}

		if (buffer_copy(node->vk.vbuf, staging, vsize, 0) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to copy Vulkan staging buffer to mesh buffer");
			return 15;
		}

		buffer_destroy(&staging, &staging_mem);

		if (buffer_create(isize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging, &staging_mem) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan staging buffer");
			return 15;
		}

		vkMapMemory(state.vk.dev, staging_mem, 0, isize, 0, &data);
		memcpy(data, mesh->indices, isize);
		vkUnmapMemory(state.vk.dev, staging_mem);

		if (buffer_create(isize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &node->vk.ibuf, &node->vk.imem) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan vertex buffer");
			return 15;
		}

		if (buffer_copy(node->vk.ibuf, staging, isize, 0) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to copy Vulkan staging buffer to index buffer");
			return 15;
		}

		buffer_destroy(&staging, &staging_mem);
	}

	if (parent == NULL) {
		if (state.mesh_root == NULL) {
			state.mesh_root = node;
		}
		else {
			mesh_node_t * n;
			for (n = state.mesh_root; n->sibling != NULL; n = n->sibling);
			n->sibling = node;
			node->prior_sibling = n;
		}
		return (kgfw_graphics_mesh_node_t *) node;
	}

	if (parent->child == NULL) {
		parent->child = (kgfw_graphics_mesh_node_t *) node;
	}
	else {
		mesh_node_t * n;
		for (n = (mesh_node_t *) parent->child; n->sibling != NULL; n = n->sibling);
		n->sibling = node;
		node->prior_sibling = n;
	}

	return (kgfw_graphics_mesh_node_t *) node;
}

void kgfw_graphics_mesh_destroy(kgfw_graphics_mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	if (mesh->parent != NULL) {
		mesh->parent->child = mesh->child;
	}
	if (mesh->prior_sibling != NULL) {
		mesh->prior_sibling->sibling = mesh->sibling;
	}
	if (mesh->child != NULL) {
		mesh->child->parent = mesh->parent;
	}
	if (mesh->sibling != NULL) {
		mesh->sibling->prior_sibling = mesh->prior_sibling;
	}

	if (state.mesh_root == (mesh_node_t *) mesh) {
		state.mesh_root = NULL;
	}

	meshes_free((mesh_node_t *) mesh);
}

void kgfw_graphics_set_window(kgfw_window_t * window) {
	state.window = window;
	if (window != NULL) {
		if (window->internal != NULL) {
			glfwMakeContextCurrent(window->internal);
		}
	}
}

void kgfw_graphics_viewport(unsigned int width, unsigned int height) {
	int w = 0;
	int h = 0;
	glfwGetFramebufferSize(state.window->internal, &w, &h);
	while (w == 0 || h == 0) {
		glfwGetFramebufferSize(state.window->internal, &w, &h);
		glfwWaitEvents();
	}
	vkDeviceWaitIdle(state.vk.dev);
	VK_SWAPCHAIN_RESIZE();
}

kgfw_window_t * kgfw_graphics_get_window(void) {
	return state.window;
}

static mesh_node_t * meshes_alloc(void) {
	mesh_node_t * m = malloc(sizeof(mesh_node_t));
	if (m == NULL) {
		return NULL;
	}

	memset(m, 0, sizeof(*m));
	m->transform.scale[0] = 1;
	m->transform.scale[1] = 1;
	m->transform.scale[2] = 1;
	return m;
}

static void meshes_free(mesh_node_t * node) {
	if (node == NULL) {
		return;
	}

	buffer_destroy(&node->vk.vbuf, &node->vk.vmem);
	buffer_destroy(&node->vk.ibuf, &node->vk.imem);
	if (node->vk.tex != VK_NULL_HANDLE && node->vk.tmem != VK_NULL_HANDLE) {
		vkDestroyImage(state.vk.dev, node->vk.tex, state.vk.allocator);
		vkFreeMemory(state.vk.dev, node->vk.tmem, state.vk.allocator);
	}

	free(node);
}

static mesh_node_t * meshes_new(void) {
	mesh_node_t * m = meshes_alloc();
	if (m == NULL) {
		return NULL;
	}

	return m;
}

static void mesh_transform(mesh_node_t * mesh, mat4x4 out_m) {
	if (mesh->transform.absolute) {
		mat4x4_identity(out_m);
		mat4x4_translate(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], recurse_state.pos[0] + mesh->transform.pos[2]);
		recurse_state.pos[0] = mesh->transform.pos[0];
		recurse_state.pos[1] = mesh->transform.pos[1];
		recurse_state.pos[2] = mesh->transform.pos[2];
		recurse_state.rot[0] = mesh->transform.rot[0];
		recurse_state.rot[1] = mesh->transform.rot[1];
		recurse_state.rot[2] = mesh->transform.rot[2];
		recurse_state.scale[0] = mesh->transform.scale[0];
		recurse_state.scale[1] = mesh->transform.scale[1];
		recurse_state.scale[2] = mesh->transform.scale[2];
	}
	else {
		mat4x4_translate_in_place(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], recurse_state.pos[2] + mesh->transform.pos[2]);
		recurse_state.pos[0] += mesh->transform.pos[0];
		recurse_state.pos[1] += mesh->transform.pos[1];
		recurse_state.pos[2] += mesh->transform.pos[2];
		recurse_state.rot[0] += mesh->transform.rot[0];
		recurse_state.rot[1] += mesh->transform.rot[1];
		recurse_state.rot[2] += mesh->transform.rot[2];
		recurse_state.scale[0] *= mesh->transform.scale[0];
		recurse_state.scale[1] *= mesh->transform.scale[1];
		recurse_state.scale[2] *= mesh->transform.scale[2];
	}

	mat4x4_identity(recurse_state.model_r);
	mat4x4_rotate_X(out_m, out_m, (recurse_state.rot[0]) * 3.141592f / 180.0f);
	mat4x4_rotate_Y(out_m, out_m, (recurse_state.rot[1]) * 3.141592f / 180.0f);
	mat4x4_rotate_Z(out_m, out_m, (recurse_state.rot[2]) * 3.141592f / 180.0f);
	mat4x4_rotate_X(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[0]) * 3.141592f / 180.0f);
	mat4x4_rotate_Y(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[1]) * 3.141592f / 180.0f);
	mat4x4_rotate_Z(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[2]) * 3.141592f / 180.0f);
	mat4x4_scale_aniso(out_m, out_m, recurse_state.scale[0], recurse_state.scale[1], recurse_state.scale[2]);
}

static void mesh_draw(mesh_node_t * mesh, mat4x4 out_m) {
	if (mesh == NULL || out_m == NULL) {
		return;
	}

	mat4x4_identity(out_m);
	mesh_transform(mesh, out_m);

	vk_ubo_t ubo;
	mat4x4_mul(ubo.mvp, state.vp, out_m);
	memcpy(state.vk.ubomap, &ubo, sizeof(vk_ubo_t));

	{
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(state.vk.cmd.buffer, 0, 1, &mesh->vk.vbuf, &offset);
		vkCmdBindIndexBuffer(state.vk.cmd.buffer, mesh->vk.ibuf, 0, VK_INDEX_TYPE_UINT32);
		vkCmdSetViewport(state.vk.cmd.buffer, 0, 1, &state.vk.viewport);
		vkCmdSetScissor(state.vk.cmd.buffer, 0, 1, &state.vk.scissor);
		vkCmdDrawIndexed(state.vk.cmd.buffer, mesh->vk.ibo_size, 1, 0, 0, 0);
	}
}

static void meshes_draw_recursive(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	mesh_draw(mesh, recurse_state.model);
	if (mesh->child == NULL) {
		return;
	}

	meshes_draw_recursive_fchild(mesh->child);
}

static void meshes_draw_recursive_fchild(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	vec3 pos;
	vec3 rot;
	vec3 scale;
	memcpy(pos, recurse_state.pos, sizeof(pos));
	memcpy(rot, recurse_state.rot, sizeof(rot));
	memcpy(scale, recurse_state.scale, sizeof(scale));

	meshes_draw_recursive(mesh);
	for (mesh_node_t * m = mesh;;) {
		m = m->sibling;
		if (m == NULL) {
			break;
		}

		memcpy(recurse_state.pos, pos, sizeof(pos));
		memcpy(recurse_state.rot, rot, sizeof(rot));
		memcpy(recurse_state.scale, scale, sizeof(scale));
		meshes_draw_recursive(m);
	}
}

static void meshes_free_recursive(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	if (mesh->child != NULL) {
		meshes_free_recursive_fchild(mesh->child);
	}

	meshes_free(mesh);
}

static void meshes_free_recursive_fchild(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	mesh_node_t * sibling = mesh->sibling;
	for (mesh_node_t * m = mesh;;) {
		m = sibling;
		if (m == NULL) {
			break;
		}
		sibling = m->sibling;

		meshes_free_recursive(m);
	}
	meshes_free_recursive(mesh);
}

void kgfw_graphics_settings_set(kgfw_graphics_settings_action_enum action, unsigned int settings) {
	unsigned int change = 0;

	switch (action) {
	case KGFW_GRAPHICS_SETTINGS_ACTION_SET:
		change = state.settings &~settings;
		state.settings = settings;
		break;
	case KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE:
		state.settings |= settings;
		change = settings;
		break;
	case KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE:
		state.settings &= ~settings;
		change = settings;
		break;
	default:
		return;
	}

	update_settings(change);
}

unsigned int kgfw_graphics_settings_get(void) {
	return state.settings;
}

static void update_settings(unsigned int change) {
	if (change & KGFW_GRAPHICS_SETTINGS_VSYNC) {
		if (state.window != NULL) {

		}
	}
}

static int gfx_command(int argc, char ** argv) {
	const char * subcommands = "set    enable    disable    reload";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
		return 0;
	}

	if (strcmp("reload", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("shaders", argv[2]) == 0) {
			vkDestroyShaderModule(state.vk.dev, state.vk.shaders.vshader, state.vk.allocator);
			vkDestroyShaderModule(state.vk.dev, state.vk.shaders.fshader, state.vk.allocator);
			shaders_load("assets/shaders/vertex.spv", "assets/shaders/fragment.spv", &state.vk.shaders.vshader, &state.vk.shaders.fshader);

			vkDeviceWaitIdle(state.vk.dev);
			pipeline_destroy();
			pipeline_create();
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	}
	else if (strcmp("enable", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	}
	else if (strcmp("disable", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	}
	else if (strcmp("options", argv[1]) == 0) {
		const char * options = "vsync    shaders";
		const char * arguments = "[option]    see 'gfx options'";
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "options: %s", options);
	}
	else {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
	}
	return 0;
}

static void register_commands(void) {
	kgfw_console_register_command("gfx", gfx_command);
}

static int shaders_load(const char * vpath, const char * fpath, VkShaderModule * out_vertex, VkShaderModule * out_fragment) {
	void * vshader = NULL;
	unsigned long long int vlen = 0;
	void * fshader = NULL;
	unsigned long long int flen = 0;

	{
		FILE * fp = fopen(vpath, "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to open vertex shader file %s", vpath);
			return 1;
		}

		fseek(fp, 0, SEEK_END);
		vlen = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		vshader = malloc(vlen);
		if (vshader == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
			return 1;
		}

		if (fread(vshader, 1, vlen, fp) != vlen) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed while reading vertex shader file %s", vpath);
			return 1;
		}
		fclose(fp);

		fp = fopen(fpath, "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to open fragment shader file %s", fpath);
			return 1;
		}

		fseek(fp, 0, SEEK_END);
		flen = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		fshader = malloc(flen);
		if (fshader == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Allocation failure");
			return 1;
		}

		if (fread(fshader, 1, flen, fp) != flen) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed while reading vertex shader file %s", fpath);
			return 1;
		}
		fclose(fp);
	}

	VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.codeSize = vlen,
		.pCode = vshader,
	};

	VkShaderModule mod;
	if (vkCreateShaderModule(state.vk.dev, &create_info, state.vk.allocator, &mod) != VK_SUCCESS) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan vertex shader module");
		return 2;
	}

	*out_vertex = mod;

	create_info.pCode = fshader;
	create_info.codeSize = flen;

	if (vkCreateShaderModule(state.vk.dev, &create_info, state.vk.allocator, &mod) != VK_SUCCESS) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to create Vulkan fragment shader module");
		return 2;
	}

	*out_fragment = mod;

	return 0;
}

#elif (KGFW_OPENGL == 33)

#include "kgfw_graphics.h"
#include "kgfw_camera.h"
#include "kgfw_log.h"
#include "kgfw_time.h"
#include "kgfw_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linmath.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef KGFW_APPLE
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define KGFW_GRAPHICS_DEFAULT_VERTICES_COUNT 0
#define KGFW_GRAPHICS_DEFAULT_INDICES_COUNT 0

#ifdef KGFW_DEBUG
#define GL_CHECK_ERROR() { GLenum err = glGetError(); if (err != GL_NO_ERROR) { kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "(%s:%u) OpenGL Error (%u 0x%X) %s", __FILE__, __LINE__, err, err, (err == 0x500) ? "INVALID ENUM" : (err == 0x501) ? "INVALID VALUE" : (err == 0x502) ? "INVALID OPERATION" : (err == 0x503) ? "STACK OVERFLOW" : (err == 0x504) ? "STACK UNDERFLOW" : (err == 0x505) ? "OUT OF MEMORY" : (err == 0x506) ? "INVALID FRAMEBUFFER OPERATION" : "UNKNOWN"); abort(); } }
#define GL_CALL(statement) statement; GL_CHECK_ERROR()
#else
#define GL_CHECK_ERROR()
#define GL_CALL(statement) statement;
#endif

typedef struct mesh_node {
	struct {
		float pos[3];
		float rot[3];
		float scale[3];
		unsigned char absolute;
	} transform;

	struct {
		mat4x4 translation;
		mat4x4 rotation;
		mat4x4 scale;
	} matrices;

	struct mesh_node * parent;
	struct mesh_node * child;
	struct mesh_node * sibling;
	struct mesh_node * prior_sibling;

	struct {
		GLuint vao;
		GLuint vbo;
		GLuint ibo;
		GLuint program;
		GLuint tex;
		GLuint normal;

		unsigned long long int vbo_size;
		unsigned long long int ibo_size;
	} gl;
} mesh_node_t;

struct {
	kgfw_window_t * window;
	kgfw_camera_t * camera;
	GLuint vshader;
	GLuint fshader;
	GLuint program;
	mat4x4 vp;

	mesh_node_t * mesh_root;

	unsigned int settings;

	struct {
		vec3 pos;
		vec3 color;
		float ambience;
		float diffusion;
		float speculation;
		float metalic;
	} light;
} static state = {
	NULL, NULL,
	0, 0, 0,
	{ 0 },
	NULL,
	KGFW_GRAPHICS_SETTINGS_DEFAULT,
	{
		{ 0, 100, 0 },
		{ 1, 1, 1 },
		0.0f, 0.5f, 0.25f, 8
	}
};

struct {
	mat4x4 model;
	mat4x4 model_r;
	vec3 pos;
	vec3 rot;
	vec3 scale;
} static recurse_state = {
	{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 },
	},
};

static void update_settings(unsigned int change);
static void register_commands(void);

static void meshes_draw_recursive(mesh_node_t * mesh);
static void meshes_draw_recursive_fchild(mesh_node_t * mesh);
static void meshes_free(mesh_node_t * node);
static mesh_node_t * meshes_new(void);
static void mesh_draw(mesh_node_t * mesh, mat4x4 out_m);
static void meshes_free_recursive_fchild(mesh_node_t * mesh);
static void meshes_free_recursive(mesh_node_t * mesh);
static void gl_errors(void);

static int shaders_load(const char * vpath, const char * fpath, GLuint * out_program);

void kgfw_graphics_settings_set(kgfw_graphics_settings_action_enum action, unsigned int settings) {
	unsigned int change = 0;

	switch (action) {
	case KGFW_GRAPHICS_SETTINGS_ACTION_SET:
		change = state.settings &~settings;
		state.settings = settings;
		break;
	case KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE:
		state.settings |= settings;
		change = settings;
		break;
	case KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE:
		state.settings &= ~settings;
		change = settings;
		break;
	default:
		return;
	}

	update_settings(change);
}

unsigned int kgfw_graphics_settings_get(void) {
	return state.settings;
}

int kgfw_graphics_init(kgfw_window_t * window, kgfw_camera_t * camera) {
	register_commands();

	state.window = window;
	state.camera = camera;

	if (window != NULL) {
		if (window->internal != NULL) {
			glfwMakeContextCurrent(window->internal);
			glfwSwapInterval(0);
		}
	}

	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		kgfw_log(KGFW_LOG_SEVERITY_ERROR, "Failed to load OpenGL 3.3 context");
		return 1;
	}

	if (window != NULL) {
		if (window->internal != NULL) {
			GL_CALL(glViewport(0, 0, window->width, window->height));
		}
	}

	state.program = GL_CALL(glCreateProgram());
	int r = shaders_load("assets/shaders/shader.vert", "assets/shaders/shader.frag", &state.program);
	if (r != 0) {
		return r;
	}

	GL_CALL(glEnable(GL_DEPTH_TEST));
	GL_CALL(glEnable(GL_FRAMEBUFFER_SRGB));
	//GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_POINT));
	GL_CALL(glEnable(GL_BLEND));
	GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
	//GL_CALL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO));
	GL_CALL(glCullFace(GL_BACK));
	GL_CALL(glFrontFace(GL_CCW));
	GL_CALL(glEnable(GL_CULL_FACE));

	update_settings(state.settings);

	return 0;
}

int kgfw_graphics_draw(void) {
	GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	GL_CALL(glClearColor(0.086f, 0.082f, 0.090f, 1.0f));

	mat4x4 mvp;
	mat4x4 m;
	mat4x4 v;
	mat4x4 p;

	mat4x4_identity(mvp);
	mat4x4_identity(state.vp);
	mat4x4_identity(m);
	mat4x4_identity(v);
	mat4x4_identity(p);

	kgfw_camera_view(state.camera, v);
	kgfw_camera_perspective(state.camera, p);

	mat4x4_mul(state.vp, p, v);

	if (state.mesh_root != NULL) {
		mat4x4_identity(recurse_state.model);

		recurse_state.pos[0] = 0;
		recurse_state.pos[1] = 0;
		recurse_state.pos[2] = 0;
		recurse_state.rot[0] = 0;
		recurse_state.rot[1] = 0;
		recurse_state.rot[2] = 0;
		recurse_state.scale[0] = 1;
		recurse_state.scale[1] = 1;
		recurse_state.scale[2] = 1;

		meshes_draw_recursive_fchild(state.mesh_root);
	}

	return 0;
}

void kgfw_graphics_mesh_texture(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_t * texture, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;
	GLenum fmt = (texture->fmt == KGFW_GRAPHICS_TEXTURE_FORMAT_RGBA) ? GL_RGBA : GL_BGRA;
	GLenum filtering = (texture->filtering == KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST) ? GL_NEAREST : GL_LINEAR;
	GLenum filtering_mipmap = (texture->filtering == KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST) ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
	GLenum u_wrap = (texture->u_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? GL_CLAMP_TO_BORDER : GL_REPEAT;
	GLenum v_wrap = (texture->v_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? GL_CLAMP_TO_BORDER : GL_REPEAT;
	GLuint * t = NULL;
	if (use == KGFW_GRAPHICS_TEXTURE_USE_COLOR) {
		t = &m->gl.tex;
	}
	else if (use == KGFW_GRAPHICS_TEXTURE_USE_NORMAL) {
		t = &m->gl.normal;
	}

	if (*t == 0) {
		GL_CALL(glGenTextures(1, t));
	}

	GL_CALL(glBindTexture(GL_TEXTURE_2D, *t));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, u_wrap));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, v_wrap));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering_mipmap));
	GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->width, texture->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture->bitmap));
	GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
}

void kgfw_graphics_mesh_texture_detach(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;
	GLuint * t = NULL;
	if (use == KGFW_GRAPHICS_TEXTURE_USE_COLOR) {
		t = &m->gl.tex;
	}
	else if (use == KGFW_GRAPHICS_TEXTURE_USE_NORMAL) {
		t = &m->gl.normal;
	}

	if (*t != 0) {
		GL_CALL(glDeleteTextures(1, t));
		*t = 0;
	}
}

kgfw_graphics_mesh_node_t * kgfw_graphics_mesh_new(kgfw_graphics_mesh_t * mesh, kgfw_graphics_mesh_node_t * parent) {
	mesh_node_t * node = meshes_new();
	node->parent = (mesh_node_t *) parent;
	memcpy(node->transform.pos, mesh->pos, sizeof(vec3));
	memcpy(node->transform.rot, mesh->rot, sizeof(vec3));
	memcpy(node->transform.scale, mesh->scale, sizeof(vec3));
	node->gl.vbo_size = mesh->vertices_count;
	node->gl.ibo_size = mesh->indices_count;
	GL_CALL(glBindVertexArray(node->gl.vao));
	GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, node->gl.vbo));
	GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(kgfw_graphics_vertex_t) * mesh->vertices_count, mesh->vertices, GL_STATIC_DRAW));
	GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, node->gl.ibo));
	GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * mesh->indices_count, mesh->indices, GL_STATIC_DRAW));

	GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, x)));
	GL_CALL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, r)));
	GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, nx)));
	GL_CALL(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(kgfw_graphics_vertex_t), (void *) offsetof(kgfw_graphics_vertex_t, u)));
	GL_CALL(glEnableVertexAttribArray(0));
	GL_CALL(glEnableVertexAttribArray(1));
	GL_CALL(glEnableVertexAttribArray(2));
	GL_CALL(glEnableVertexAttribArray(3));

	if (parent == NULL) {
		if (state.mesh_root == NULL) {
			state.mesh_root = node;
		}
		else {
			mesh_node_t * n;
			for (n = state.mesh_root; n->sibling != NULL; n = n->sibling);
			n->sibling = node;
			node->prior_sibling = n;
		}
		return (kgfw_graphics_mesh_node_t *) node;
	}

	if (parent->child == NULL) {
		parent->child = (kgfw_graphics_mesh_node_t *) node;
	}
	else {
		mesh_node_t * n;
		for (n = (mesh_node_t *) parent->child; n->sibling != NULL; n = n->sibling);
		n->sibling = node;
		node->prior_sibling = n;
	}

	return (kgfw_graphics_mesh_node_t *) node;
}

void kgfw_graphics_mesh_destroy(kgfw_graphics_mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	if (mesh->parent != NULL) {
		mesh->parent->child = mesh->child;
	}
	if (mesh->prior_sibling != NULL) {
		mesh->prior_sibling->sibling = mesh->sibling;
	}
	if (mesh->child != NULL) {
		mesh->child->parent = mesh->parent;
	}
	if (mesh->sibling != NULL) {
		mesh->sibling->prior_sibling = mesh->prior_sibling;
	}

	if (state.mesh_root == (mesh_node_t *) mesh) {
		state.mesh_root = NULL;
	}

	meshes_free((mesh_node_t *) mesh);
}

void kgfw_graphics_set_window(kgfw_window_t * window) {
	state.window = window;
	if (window != NULL) {
		if (window->internal != NULL) {
			glfwMakeContextCurrent(window->internal);
		}
		GL_CALL(glViewport(0, 0, window->width, window->height));
	}
}

void kgfw_graphics_viewport(unsigned int width, unsigned int height) {
	GL_CALL(glViewport(0, 0, width, height));
}

kgfw_window_t * kgfw_graphics_get_window(void) {
	return state.window;
}

void kgfw_graphics_deinit(void) {
	meshes_free_recursive_fchild(state.mesh_root);
}

static mesh_node_t * meshes_alloc(void) {
	mesh_node_t * m = malloc(sizeof(mesh_node_t));
	if (m == NULL) {
		return NULL;
	}

	memset(m, 0, sizeof(*m));
	m->transform.scale[0] = 1;
	m->transform.scale[1] = 1;
	m->transform.scale[2] = 1;
	return m;
}

static void meshes_free(mesh_node_t * node) {
	if (node == NULL) {
		return;
	}

	if (node->gl.vbo != 0) {
		GL_CALL(glDeleteBuffers(1, &node->gl.vbo));
	}
	if (node->gl.ibo != 0) {
		GL_CALL(glDeleteBuffers(1, &node->gl.ibo));
	}
	if (node->gl.program != 0) {
		GL_CALL(glDeleteProgram(node->gl.program));
	}
	if (node->gl.vao != 0) {
		GL_CALL(glDeleteVertexArrays(1, &node->gl.vao));
	}
	if (node->gl.tex != 0) {
		GL_CALL(glDeleteTextures(1, &node->gl.tex));
	}
	if (node->gl.normal != 0) {
		GL_CALL(glDeleteTextures(1, &node->gl.normal));
	}

	free(node);
}

static void meshes_gen(mesh_node_t * node) {
	GL_CALL(glGenVertexArrays(1, &node->gl.vao));
	GL_CALL(glGenBuffers(1, &node->gl.vbo));
	GL_CALL(glGenBuffers(1, &node->gl.ibo));
	node->gl.tex = 0;
	node->gl.normal = 0;
}

static mesh_node_t * meshes_new(void) {
	mesh_node_t * m = meshes_alloc();
	if (m == NULL) {
		return NULL;
	}

	meshes_gen(m);
	return m;
}

static void mesh_transform(mesh_node_t * mesh, mat4x4 out_m) {
	if (mesh->transform.absolute) {
		mat4x4_identity(out_m);
		mat4x4_translate(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], recurse_state.pos[0] + mesh->transform.pos[2]);
		recurse_state.pos[0] = mesh->transform.pos[0];
		recurse_state.pos[1] = mesh->transform.pos[1];
		recurse_state.pos[2] = mesh->transform.pos[2];
		recurse_state.rot[0] = mesh->transform.rot[0];
		recurse_state.rot[1] = mesh->transform.rot[1];
		recurse_state.rot[2] = mesh->transform.rot[2];
		recurse_state.scale[0] = mesh->transform.scale[0];
		recurse_state.scale[1] = mesh->transform.scale[1];
		recurse_state.scale[2] = mesh->transform.scale[2];
	} else {
		mat4x4_translate_in_place(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], recurse_state.pos[2] + mesh->transform.pos[2]);
		recurse_state.pos[0] += mesh->transform.pos[0];
		recurse_state.pos[1] += mesh->transform.pos[1];
		recurse_state.pos[2] += mesh->transform.pos[2];
		recurse_state.rot[0] += mesh->transform.rot[0];
		recurse_state.rot[1] += mesh->transform.rot[1];
		recurse_state.rot[2] += mesh->transform.rot[2];
		recurse_state.scale[0] *= mesh->transform.scale[0];
		recurse_state.scale[1] *= mesh->transform.scale[1];
		recurse_state.scale[2] *= mesh->transform.scale[2];
	}
	mat4x4_identity(recurse_state.model_r);
	mat4x4_rotate_X(out_m, out_m, (recurse_state.rot[0]) * 3.141592f / 180.0f);
	mat4x4_rotate_Y(out_m, out_m, (recurse_state.rot[1]) * 3.141592f / 180.0f);
	mat4x4_rotate_Z(out_m, out_m, (recurse_state.rot[2]) * 3.141592f / 180.0f);
	mat4x4_rotate_X(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[0]) * 3.141592f / 180.0f);
	mat4x4_rotate_Y(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[1]) * 3.141592f / 180.0f);
	mat4x4_rotate_Z(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[2]) * 3.141592f / 180.0f);
	mat4x4_scale_aniso(out_m, out_m, recurse_state.scale[0], recurse_state.scale[1], recurse_state.scale[2]);
}

static void mesh_draw(mesh_node_t * mesh, mat4x4 out_m) {
	if (mesh == NULL || out_m == NULL) {
		return;
	}
	if (mesh->gl.vbo_size == 0 || mesh->gl.ibo_size == 0 || mesh->gl.vbo == 0 || mesh->gl.ibo == 0) {
		return;
	}

	GLuint program = (mesh->gl.program == 0) ? state.program : mesh->gl.program;
	GL_CALL(glUseProgram(program));
	GLint uniform_model = GL_CALL(glGetUniformLocation(program, "unif_m"));
	GLint uniform_vp = GL_CALL(glGetUniformLocation(program, "unif_vp"));
	GLint uniform_time = GL_CALL(glGetUniformLocation(program, "unif_time"));
	GLint uniform_view = GL_CALL(glGetUniformLocation(program, "unif_view_pos"));
	GLint uniform_textured_color = GL_CALL(glGetUniformLocation(program, "unif_textured_color"));
	GLint uniform_textured_normal = GL_CALL(glGetUniformLocation(program, "unif_textured_normal"));
	GLint uniform_texture_color = GL_CALL(glGetUniformLocation(program, "unif_texture_color"));
	GLint uniform_texture_normal = GL_CALL(glGetUniformLocation(program, "unif_texture_normal"));

	mat4x4_identity(out_m);
	mesh_transform(mesh, out_m);

	GL_CALL(glUniformMatrix4fv(uniform_model, 1, GL_FALSE, &out_m[0][0]));
	GL_CALL(glUniformMatrix4fv(uniform_vp, 1, GL_FALSE, &state.vp[0][0]));
	GL_CALL(glUniform1f(uniform_time, kgfw_time_get()));
	GL_CALL(glUniform3f(uniform_view, state.camera->pos[0], state.camera->pos[1], state.camera->pos[2]));

	GL_CALL(glUniform1i(uniform_texture_color, 0));
	if (mesh->gl.tex == 0) {
		GL_CALL(glUniform1f(uniform_textured_color, 0));
		GL_CALL(glActiveTexture(GL_TEXTURE0));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
	} else {
		GL_CALL(glUniform1f(uniform_textured_color, 1));
		GL_CALL(glActiveTexture(GL_TEXTURE0));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, mesh->gl.tex));
	}

	GL_CALL(glUniform1i(uniform_texture_normal, 1));
	if (mesh->gl.normal == 0) {
		GL_CALL(glUniform1f(uniform_textured_normal, 0));
		GL_CALL(glActiveTexture(GL_TEXTURE1));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
	} else {
		GL_CALL(glUniform1f(uniform_textured_normal, 1));
		GL_CALL(glActiveTexture(GL_TEXTURE1));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, mesh->gl.normal));
	}

	GL_CALL(glBindVertexArray(mesh->gl.vao));
	GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mesh->gl.vbo));
	GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->gl.ibo));
	//GL_CALL(glDrawArrays(GL_TRIANGLES, 0, mesh->gl.vbo_size));
	GL_CALL(glDrawElements(GL_TRIANGLES, mesh->gl.ibo_size, GL_UNSIGNED_INT, 0));
}

static void meshes_draw_recursive(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	mesh_draw(mesh, recurse_state.model);
	if (mesh->child == NULL) {
		return;
	}

	meshes_draw_recursive_fchild(mesh->child);
}

static void meshes_draw_recursive_fchild(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	vec3 pos;
	vec3 rot;
	vec3 scale;
	memcpy(pos, recurse_state.pos, sizeof(pos));
	memcpy(rot, recurse_state.rot, sizeof(rot));
	memcpy(scale, recurse_state.scale, sizeof(scale));

	meshes_draw_recursive(mesh);
	for (mesh_node_t * m = mesh;;) {
		m = m->sibling;
		if (m == NULL) {
			break;
		}

		memcpy(recurse_state.pos, pos, sizeof(pos));
		memcpy(recurse_state.rot, rot, sizeof(rot));
		memcpy(recurse_state.scale, scale, sizeof(scale));
		meshes_draw_recursive(m);
	}
}

static void meshes_free_recursive(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	if (mesh->child != NULL) {
		meshes_free_recursive_fchild(mesh->child);
	}

	meshes_free(mesh);
}

static void meshes_free_recursive_fchild(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	mesh_node_t * sibling = mesh->sibling;
	for (mesh_node_t * m = mesh;;) {
		m = sibling;
		if (m == NULL) {
			break;
		}
		sibling = m->sibling;

		meshes_free_recursive(m);
	}
	meshes_free_recursive(mesh);
}

static void update_settings(unsigned int change) {
	if (change &KGFW_GRAPHICS_SETTINGS_VSYNC) {
		if (state.window != NULL) {
			glfwSwapInterval((state.settings &KGFW_GRAPHICS_SETTINGS_VSYNC));
		}
	}
}

static int gfx_command(int argc, char ** argv) {
	const char * subcommands = "set    enable    disable    reload";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
		return 0;
	}

	if (strcmp("reload", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("shaders", argv[2]) == 0) {
			GL_CALL(glDeleteProgram(state.program));
			state.program = GL_CALL(glCreateProgram());
			int r = shaders_load("assets/shaders/shader.vert", "assets/shaders/shader.frag", &state.program);
			if (r != 0) {
				return r;
			}
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	}
	else if (strcmp("enable", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	}
	else if (strcmp("disable", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	}
	else if (strcmp("options", argv[1]) == 0) {
		const char * options = "vsync    shaders";
		const char * arguments = "[option]    see 'gfx options'";
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "options: %s", options);
	}
	else {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
	}
	return 0;
}

static void register_commands(void) {
	kgfw_console_register_command("gfx", gfx_command);
}

static void gl_errors(void) {
	GLenum err = -1;
	while (1) {
		err = glGetError();
		if (err == GL_NO_ERROR) {
			break;
		}
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "err %i", err);
	}
}

static int shaders_load(const char * vpath, const char * fpath, GLuint * out_program) {
	const GLchar * fallback_vshader =
		"#version 330 core\n"
		"layout(location = 0) in vec3 in_pos; layout(location = 1) in vec3 in_color; layout(location = 2) in vec3 in_normal; layout(location = 3) in vec2 in_uv; uniform mat4 unif_m; uniform mat4 unif_vp; out vec3 v_pos; out vec3 v_color; out vec3 v_normal; out vec2 v_uv; void main() { gl_Position = unif_vp * unif_m * vec4(in_pos, 1.0); v_pos = vec3(unif_m * vec4(in_pos, 1.0)); v_color = in_color; v_normal = in_normal; v_uv = in_uv; }";
	const GLchar * fallback_fshader =
		"#version 330 core\n"
		"in vec3 v_pos; in vec3 v_color; in vec3 v_normal; in vec2 v_uv; out vec4 out_color; void main() { out_color = vec4(v_color, 1); }";

	GLchar * vshader = (GLchar *) fallback_vshader;
	GLchar * fshader = (GLchar *) fallback_fshader;
	{
		FILE * fp = fopen(vpath, "rb");
		unsigned long long int length = 0;
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"assets/shaders/shader.vert\" falling back to default shader");
			vshader = (GLchar *) fallback_vshader;
			goto load_fshader;
		}
		fseek(fp, 0L, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		vshader = malloc(length + 1);
		if (vshader == NULL) {
			length = 0;
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (GLchar *) fallback_vshader;
			goto load_fshader;
		}
		if (fread(vshader, 1, length, fp) != length) {
			length = 0;
			free(vshader);
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (GLchar *) fallback_vshader;
			goto load_fshader;
		}
		vshader[length] = '\0';
		fclose(fp);
	load_fshader:
		fp = fopen(fpath, "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", fpath);
			fshader = (GLchar *) fallback_fshader;
			goto end_fshader;
		}
		fseek(fp, 0L, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		fshader = malloc(length + 1);
		if (fshader == NULL) {
			length = 0;
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", fpath);
			fshader = (GLchar *) fallback_fshader;
			goto end_fshader;
		}
		if (fread(fshader, 1, length, fp) != length) {
			length = 0;
			free(fshader);
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", fpath);
			fshader = (GLchar *) fallback_fshader;
			goto end_fshader;
		}
		fshader[length] = '\0';
		fclose(fp);
	end_fshader:;
	}
	GLuint vert = GL_CALL(glCreateShader(GL_VERTEX_SHADER));
	GL_CALL(glShaderSource(vert, 1, (const GLchar * const *) &vshader, NULL));
	GL_CALL(glCompileShader(vert));
	GLint success = GL_TRUE;
	GL_CALL(glGetShaderiv(vert, GL_COMPILE_STATUS, &success));
	if (success == GL_FALSE) {
		char msg[512];
		GL_CALL(glGetShaderInfoLog(vert, 512, NULL, msg));
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided vertex shader compilation error: %s", msg);
	vfallback_compilation:
		vshader = (GLchar *) fallback_vshader;
		GL_CALL(glShaderSource(vert, 1, (const GLchar * const *) &vshader, NULL));
		GL_CALL(glCompileShader(vert));
		GL_CALL(glGetShaderiv(vert, GL_COMPILE_STATUS, &success));
		if (success == GL_FALSE) {
			GL_CALL(glGetShaderInfoLog(vert, 512, NULL, msg));
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback vertex shader compilation error: %s", msg);
			return 2;
		}
	}
	GLuint frag = GL_CALL(glCreateShader(GL_FRAGMENT_SHADER));
	GL_CALL(glShaderSource(frag, 1, (const GLchar * const *) &fshader, NULL));
	GL_CALL(glCompileShader(frag));

	GL_CALL(glGetShaderiv(frag, GL_COMPILE_STATUS, &success));
	if (success == GL_FALSE) {
		char msg[512];
		GL_CALL(glGetShaderInfoLog(frag, 512, NULL, msg));
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL user provided fragment shader compilation error: %s", msg);
	ffallback_compilation:
		fshader = (GLchar *) fallback_fshader;
		GL_CALL(glShaderSource(frag, 1, (const GLchar * const *) &fshader, NULL));
		GL_CALL(glCompileShader(frag));
		GL_CALL(glGetShaderiv(frag, GL_COMPILE_STATUS, &success));
		if (success == GL_FALSE) {
			GL_CALL(glGetShaderInfoLog(frag, 512, NULL, msg));
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "OpenGL fallback fragment shader compilation error: %s", msg);
			return 2;
		}
	}

	GL_CALL(glAttachShader(*out_program, vert));
	GL_CALL(glAttachShader(*out_program, frag));
	GL_CALL(glLinkProgram(*out_program));
	GL_CALL(glDeleteShader(vert));
	GL_CALL(glDeleteShader(frag));

	return 0;
}

#elif (KGFW_DIRECTX == 11)

#include "kgfw_graphics.h"
#include "kgfw_camera.h"
#include "kgfw_log.h"
#include "kgfw_time.h"
#include "kgfw_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linmath.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>

#define KGFW_GRAPHICS_DEFAULT_VERTICES_COUNT 0
#define KGFW_GRAPHICS_DEFAULT_INDICES_COUNT 0



#ifdef KGFW_DEBUG
#define D3D11_CALL(statement) { HRESULT hr = statement; if (FAILED(hr)) { kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "D3D11 Error (%i 0x%X) %s:%u", hr, hr, __FILE__, __LINE__); abort(); } }
#define D3D11_CALL_DO(statement, todo) { HRESULT hr = statement; if (FAILED(hr)) { todo; abort(); } }
#else
#define D3D11_CALL(statement) statement;
#define D3D11_CALL_DO(statement, todo)
#endif

typedef struct mesh_node {
	struct {
		float pos[3];
		float rot[3];
		float scale[3];
		unsigned char absolute;
	} transform;

	struct {
		mat4x4 translation;
		mat4x4 rotation;
		mat4x4 scale;
	} matrices;

	struct mesh_node * parent;
	struct mesh_node * child;
	struct mesh_node * sibling;
	struct mesh_node * prior_sibling;

	struct {
		ID3D11Buffer * vbo;
		ID3D11Buffer * ibo;
		ID3D11ShaderResourceView * tex;
		ID3D11SamplerState * sampler;

		unsigned long long int vbo_size;
		unsigned long long int ibo_size;
	} d3d11;
} mesh_node_t;

struct kgfw_graphics_mesh_node_internal_t {
	void * _a;
	void * _b;
	void * _c;
	void * _d;

	unsigned long long int _e;
	unsigned long long int _f;
};

typedef struct mesh_shader_uniform {
	mat4x4 mvp;
} mesh_shader_uniform_t;

struct {
	kgfw_window_t * window;
	kgfw_camera_t * camera;
	mat4x4 vp;

	mesh_node_t * mesh_root;

	unsigned int settings;

	struct {
		vec3 pos;
		vec3 color;
		float ambience;
		float diffusion;
		float speculation;
		float metalic;
	} light;

	ID3D11Device * dev;
	ID3D11DeviceContext * devctx;
	IDXGISwapChain1 * swapchain;
	ID3D11InputLayout * layout;
	ID3D11VertexShader * vshader;
	ID3D11PixelShader * pshader;
	ID3D11Buffer * ubuffer;
	ID3D11BlendState * blend;
	ID3D11RasterizerState * rasterizer;
	ID3D11DepthStencilState * depth;
	ID3D11DepthStencilView * depth_view;
	ID3D11RenderTargetView * target;
} static state = {
	NULL, NULL,
	{ 0 },
	NULL,
	KGFW_GRAPHICS_SETTINGS_DEFAULT,
	{
		{ 0, 100, 0 },
		{ 1, 1, 1 },
		0.0f, 0.5f, 0.25f, 8
	},
	NULL, NULL,
	NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL,
};

struct {
	mat4x4 model;
	mat4x4 model_r;
	vec3 pos;
	vec3 rot;
	vec3 scale;
} static recurse_state = {
	{
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 },
	},
};

static void update_settings(unsigned int change);
static void register_commands(void);

static void meshes_draw_recursive(mesh_node_t * mesh);
static void meshes_draw_recursive_fchild(mesh_node_t * mesh);
static void meshes_free(mesh_node_t * node);
static mesh_node_t * meshes_new(void);
static void mesh_draw(mesh_node_t * mesh, mat4x4 out_m);
static void meshes_free_recursive_fchild(mesh_node_t * mesh);
static void meshes_free_recursive(mesh_node_t * mesh);

static int shaders_load(const char * vpath, const char * ppath, ID3D11VertexShader ** out_vshader, ID3D11PixelShader ** out_pshader);

void kgfw_graphics_settings_set(kgfw_graphics_settings_action_enum action, unsigned int settings) {
	unsigned int change = 0;

	switch (action) {
		case KGFW_GRAPHICS_SETTINGS_ACTION_SET:
			change = state.settings &~settings;
			state.settings = settings;
			break;
		case KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE:
			state.settings |= settings;
			change = settings;
			break;
		case KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE:
			state.settings &= ~settings;
			change = settings;
			break;
		default:
			return;
	}

	update_settings(change);
}

unsigned int kgfw_graphics_settings_get(void) {
	return state.settings;
}

int kgfw_graphics_init(kgfw_window_t * window, kgfw_camera_t * camera) {
	register_commands();

	state.window = window;
	state.camera = camera;
	{
		D3D_FEATURE_LEVEL features_levels[1] = { D3D_FEATURE_LEVEL_11_0 };

		UINT flags = 0;
		#ifdef _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
		#endif

		D3D11_CALL(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, features_levels, 1, D3D11_SDK_VERSION, &state.dev, NULL, &state.devctx));

		{
			if (shaders_load("assets/shaders/vertex.hlsl", "assets/shaders/pixel.hlsl", &state.vshader, &state.pshader) != 0) {
				return 1;
			}

			D3D11_BUFFER_DESC desc = {
				.ByteWidth = sizeof(mesh_shader_uniform_t),
				.Usage = D3D11_USAGE_DYNAMIC,
				.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
				.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
			};

			D3D11_CALL(state.dev->lpVtbl->CreateBuffer(state.dev, &desc, NULL, &state.ubuffer));
		}

		#ifdef _DEBUG
		ID3D11InfoQueue * iq;
		D3D11_CALL(state.dev->lpVtbl->QueryInterface(state.dev, &IID_ID3D11InfoQueue, &iq));
		D3D11_CALL(iq->lpVtbl->SetBreakOnSeverity(iq, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE));
		D3D11_CALL(iq->lpVtbl->SetBreakOnSeverity(iq, D3D11_MESSAGE_SEVERITY_ERROR, TRUE));
		iq->lpVtbl->Release(iq);

		HMODULE dxgidbg = LoadLibraryA("dxgidebug.dll");
		if (dxgidbg != NULL) {
			HRESULT(WINAPI * dxgiGetDebugInterface)(REFIID riid, void ** ppDebug);
			*(FARPROC *)&dxgiGetDebugInterface = GetProcAddress(dxgidbg, "DXGIGetDebugInterface");

			IDXGIInfoQueue * dxgiiq;
			D3D11_CALL(dxgiGetDebugInterface(&IID_IDXGIInfoQueue, &dxgiiq));
			D3D11_CALL(dxgiiq->lpVtbl->SetBreakOnSeverity(dxgiiq, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE));
			D3D11_CALL(dxgiiq->lpVtbl->SetBreakOnSeverity(dxgiiq, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE));
			dxgiiq->lpVtbl->Release(dxgiiq);
		}
		#endif
	}

	{
		IDXGIDevice * dev;;
		D3D11_CALL(state.dev->lpVtbl->QueryInterface(state.dev, &IID_IDXGIDevice, &dev));

		IDXGIAdapter * adapter;
		D3D11_CALL(dev->lpVtbl->GetAdapter(dev, &adapter));

		IDXGIFactory2 * factory;
		D3D11_CALL(adapter->lpVtbl->GetParent(adapter, &IID_IDXGIFactory2, &factory));

		DXGI_SWAP_CHAIN_DESC1 desc;
		SecureZeroMemory(&desc, sizeof(desc));
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 2;
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc.Width = state.window->width;
		desc.Height = state.window->height;

		D3D11_CALL(factory->lpVtbl->CreateSwapChainForHwnd(factory, (IUnknown *) state.dev, state.window->internal, &desc, NULL, NULL, &state.swapchain));
		D3D11_CALL(factory->lpVtbl->MakeWindowAssociation(factory, state.window->internal, DXGI_MWA_NO_ALT_ENTER));
		factory->lpVtbl->Release(factory);
		adapter->lpVtbl->Release(adapter);
		dev->lpVtbl->Release(dev);
	}

	{
		D3D11_BLEND_DESC desc = {
			.RenderTarget[0] = {
				.BlendEnable = TRUE,
				.SrcBlend = D3D11_BLEND_SRC_ALPHA,
				.DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
				.BlendOp = D3D11_BLEND_OP_ADD,
				.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA,
				.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
				.BlendOpAlpha = D3D11_BLEND_OP_ADD,
				.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
			},
		};

		D3D11_CALL(state.dev->lpVtbl->CreateBlendState(state.dev, &desc, &state.blend));
	}

	{ 
		D3D11_RASTERIZER_DESC desc = {
			.FillMode = D3D11_FILL_SOLID,
			.CullMode = D3D11_CULL_NONE,
		};
		D3D11_CALL(state.dev->lpVtbl->CreateRasterizerState(state.dev, &desc, &state.rasterizer));


		ID3D11Texture2D * backbuffer;
		D3D11_CALL(state.swapchain->lpVtbl->GetBuffer(state.swapchain, 0, &IID_ID3D11Texture2D, &backbuffer));
		D3D11_CALL(state.dev->lpVtbl->CreateRenderTargetView(state.dev, (ID3D11Resource *) backbuffer, NULL, &state.target));
		backbuffer->lpVtbl->Release(backbuffer);
	}

	{
		D3D11_DEPTH_STENCIL_DESC desc = {
			TRUE,
			D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS,
			TRUE,
			0xFF, 0xFF,
			{ D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_INCR, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS },
			{ D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_DECR, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS },
		};

		D3D11_CALL(state.dev->lpVtbl->CreateDepthStencilState(state.dev, &desc, &state.depth));
	}

	{
		ID3D11Texture2D * depth;
		{
			D3D11_TEXTURE2D_DESC desc = {
				state.window->width, state.window->height,
				1, 1,
				DXGI_FORMAT_D32_FLOAT,
				{ 1, 0 },
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_DEPTH_STENCIL,
				0, 0
			};
			D3D11_CALL(state.dev->lpVtbl->CreateTexture2D(state.dev, &desc, NULL, &depth));
		}

		{
			D3D11_DEPTH_STENCIL_VIEW_DESC desc = {
				DXGI_FORMAT_D32_FLOAT,
				D3D11_DSV_DIMENSION_TEXTURE2D,
			};
			desc.Texture2D.MipSlice = 0;

			D3D11_CALL(state.dev->lpVtbl->CreateDepthStencilView(state.dev, (ID3D11Resource *) depth, &desc, &state.depth_view));
		}
		depth->lpVtbl->Release(depth);
	}

	update_settings(state.settings);

	return 0;
}

int kgfw_graphics_draw(void) {
	float color[4] = { 0.086f, 0.082f, 0.090f, 1.0f };
	state.devctx->lpVtbl->ClearRenderTargetView(state.devctx, state.target, color);
	state.devctx->lpVtbl->ClearDepthStencilView(state.devctx, state.depth_view, D3D11_CLEAR_DEPTH, 1, 0xFF);

	mat4x4 mvp;
	mat4x4 m;
	mat4x4 v;
	mat4x4 p;

	mat4x4_identity(mvp);
	mat4x4_identity(state.vp);
	mat4x4_identity(m);
	mat4x4_identity(v);
	mat4x4_identity(p);

	kgfw_camera_view(state.camera, v);
	kgfw_camera_perspective(state.camera, p);

	mat4x4_mul(state.vp, p, v);

	state.light.pos[0] = sinf(kgfw_time_get() * 2) * 10;
	state.light.pos[1] = cosf(kgfw_time_get() / 6) * 15;
	state.light.pos[2] = sinf(kgfw_time_get() + 3) * 10;
	if (state.mesh_root != NULL) {
		mat4x4_identity(recurse_state.model);

		recurse_state.pos[0] = 0;
		recurse_state.pos[1] = 0;
		recurse_state.pos[2] = 0;
		recurse_state.rot[0] = 0;
		recurse_state.rot[1] = 0;
		recurse_state.rot[2] = 0;
		recurse_state.scale[0] = 1;
		recurse_state.scale[1] = 1;
		recurse_state.scale[2] = 1;

		meshes_draw_recursive_fchild(state.mesh_root);
	}

	if (FAILED(state.swapchain->lpVtbl->Present(state.swapchain, (state.settings & KGFW_GRAPHICS_SETTINGS_VSYNC) != 0, 0))) {
		return 1;
	}

	return 0;
}

void kgfw_graphics_mesh_texture(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_t * texture, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;

	if (m->d3d11.sampler != NULL) {
		m->d3d11.sampler->lpVtbl->Release(m->d3d11.sampler);
		m->d3d11.sampler = NULL;
	}

	{
		D3D11_FILTER filter = (texture->filtering == KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST) ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		D3D11_TEXTURE_ADDRESS_MODE u = (texture->u_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		D3D11_TEXTURE_ADDRESS_MODE v = (texture->v_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		D3D11_TEXTURE_ADDRESS_MODE w = (texture->v_wrap == KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;

		D3D11_SAMPLER_DESC desc = {
			filter,
			u, v, w
		};

		D3D11_CALL(state.dev->lpVtbl->CreateSamplerState(state.dev, &desc, &m->d3d11.sampler));
	}

	{
		DXGI_FORMAT fmt = (texture->fmt == KGFW_GRAPHICS_TEXTURE_FORMAT_RGBA) ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;

		D3D11_TEXTURE2D_DESC desc = {
			texture->width, texture->height,
			1, 1,
			fmt,
			{ 1, 0 },
			D3D11_USAGE_IMMUTABLE, D3D11_BIND_SHADER_RESOURCE,
		};

		D3D11_SUBRESOURCE_DATA init = {
			texture->bitmap,
			texture->width * sizeof(unsigned int),
			texture->height * sizeof(unsigned int),
		};

		ID3D11Texture2D * tex;
		D3D11_CALL(state.dev->lpVtbl->CreateTexture2D(state.dev, &desc, &init, &tex));
		D3D11_CALL(state.dev->lpVtbl->CreateShaderResourceView(state.dev, (ID3D11Resource *) tex, NULL, &m->d3d11.tex));
		tex->lpVtbl->Release(tex);
	}
}

void kgfw_graphics_mesh_texture_detach(kgfw_graphics_mesh_node_t * mesh, kgfw_graphics_texture_use_enum use) {
	mesh_node_t * m = (mesh_node_t *) mesh;
	if (m->d3d11.sampler != NULL) {
		m->d3d11.sampler->lpVtbl->Release(m->d3d11.sampler);
		m->d3d11.sampler = NULL;
	}
	if (m->d3d11.tex != NULL) {
		m->d3d11.tex->lpVtbl->Release(m->d3d11.tex);
		m->d3d11.tex = NULL;
	}
}

kgfw_graphics_mesh_node_t * kgfw_graphics_mesh_new(kgfw_graphics_mesh_t * mesh, kgfw_graphics_mesh_node_t * parent) {
	mesh_node_t * node = meshes_new();
	node->parent = (mesh_node_t *) parent;
	memcpy(node->transform.pos, mesh->pos, sizeof(vec3));
	memcpy(node->transform.rot, mesh->rot, sizeof(vec3));
	memcpy(node->transform.scale, mesh->scale, sizeof(vec3));
	
	{
		D3D11_BUFFER_DESC desc = {
			0
		};

		D3D11_SUBRESOURCE_DATA init = {
			0
		};

		desc.ByteWidth = mesh->vertices_count * sizeof(kgfw_graphics_vertex_t);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		init.pSysMem = mesh->vertices;

		D3D11_CALL(state.dev->lpVtbl->CreateBuffer(state.dev, &desc, &init, &node->d3d11.vbo));

		desc.ByteWidth = mesh->indices_count * sizeof(unsigned int);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		init.pSysMem = mesh->indices;
		D3D11_CALL(state.dev->lpVtbl->CreateBuffer(state.dev, &desc, &init, &node->d3d11.ibo));

		node->d3d11.vbo_size = mesh->vertices_count;
		node->d3d11.ibo_size = mesh->indices_count;
	}

	if (parent == NULL) {
		if (state.mesh_root == NULL) {
			state.mesh_root = node;
		} else {
			mesh_node_t * n;
			for (n = state.mesh_root; n->sibling != NULL; n = n->sibling);
			n->sibling = node;
			node->prior_sibling = n;
		}
		return (kgfw_graphics_mesh_node_t *) node;
	}

	if (parent->child == NULL) {
		parent->child = (kgfw_graphics_mesh_node_t *) node;
	} else {
		mesh_node_t * n;
		for (n = (mesh_node_t *) parent->child; n->sibling != NULL; n = n->sibling);
		n->sibling = node;
		node->prior_sibling = n;
	}
	
	return (kgfw_graphics_mesh_node_t *) node;
}

void kgfw_graphics_mesh_destroy(kgfw_graphics_mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	if (mesh->parent != NULL) {
		mesh->parent->child = mesh->child;
	}
	if (mesh->prior_sibling != NULL) {
		mesh->prior_sibling->sibling = mesh->sibling;
	}
	if (mesh->child != NULL) {
		mesh->child->parent = mesh->parent;
	}
	if (mesh->sibling != NULL) {
		mesh->sibling->prior_sibling = mesh->prior_sibling;
	}

	if (state.mesh_root == (mesh_node_t *) mesh) {
		state.mesh_root = NULL;
	}

	meshes_free((mesh_node_t *) mesh);
}

void kgfw_graphics_debug_line(vec3 p0, vec3 p1) {
}

void kgfw_graphics_set_window(kgfw_window_t * window) {
	state.window = window;
	if (window != NULL) {
		if (window->internal != NULL) {
		}
	}
}

void kgfw_graphics_viewport(unsigned int width, unsigned int height) {
	state.devctx->lpVtbl->ClearState(state.devctx);
	state.target->lpVtbl->Release(state.target);
	
	D3D11_CALL(state.swapchain->lpVtbl->ResizeBuffers(state.swapchain, 0, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
	ID3D11Texture2D * backbuffer;
	D3D11_CALL(state.swapchain->lpVtbl->GetBuffer(state.swapchain, 0, &IID_ID3D11Texture2D, &backbuffer));
	D3D11_CALL(state.dev->lpVtbl->CreateRenderTargetView(state.dev, (ID3D11Resource *) backbuffer, NULL, &state.target));
	backbuffer->lpVtbl->Release(backbuffer);
}

kgfw_window_t * kgfw_graphics_get_window(void) {
	return state.window;
}

void kgfw_graphics_deinit(void) {
	meshes_free_recursive_fchild(state.mesh_root);
}

static mesh_node_t * meshes_alloc(void) {
	mesh_node_t * m = malloc(sizeof(mesh_node_t));
	if (m == NULL) {
		return NULL;
	}

	memset(m, 0, sizeof(*m));
	m->transform.scale[0] = 1;
	m->transform.scale[1] = 1;
	m->transform.scale[2] = 1;
	return m;
}

static void meshes_free(mesh_node_t * node) {
	if (node == NULL) {
		return;
	}

	if (node->d3d11.vbo != NULL) {
		node->d3d11.vbo->lpVtbl->Release(node->d3d11.vbo);
	}
	if (node->d3d11.ibo != NULL) {
		node->d3d11.ibo->lpVtbl->Release(node->d3d11.ibo);
	}
	if (node->d3d11.tex != NULL) {
		node->d3d11.tex->lpVtbl->Release(node->d3d11.tex);
	}
	if (node->d3d11.sampler != NULL) {
		node->d3d11.sampler->lpVtbl->Release(node->d3d11.sampler);
	}

	free(node);
}

static mesh_node_t * meshes_new(void) {
	mesh_node_t * m = meshes_alloc();	
	if (m == NULL) {
		return NULL;
	}

	return m;
}

static void mesh_transform(mesh_node_t * mesh, mat4x4 out_m) {
	if (mesh->transform.absolute) {
		mat4x4_identity(out_m);
		mat4x4_translate(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], recurse_state.pos[0] + mesh->transform.pos[2]);
		recurse_state.pos[0] = mesh->transform.pos[0];
		recurse_state.pos[1] = mesh->transform.pos[1];
		recurse_state.pos[2] = mesh->transform.pos[2];
		recurse_state.rot[0] = mesh->transform.rot[0];
		recurse_state.rot[1] = mesh->transform.rot[1];
		recurse_state.rot[2] = mesh->transform.rot[2];
		recurse_state.scale[0] = mesh->transform.scale[0];
		recurse_state.scale[1] = mesh->transform.scale[1];
		recurse_state.scale[2] = mesh->transform.scale[2];
	} else {
		mat4x4_translate_in_place(out_m, recurse_state.pos[0] + mesh->transform.pos[0], recurse_state.pos[1] + mesh->transform.pos[1], recurse_state.pos[2] + mesh->transform.pos[2]);
		recurse_state.pos[0] += mesh->transform.pos[0];
		recurse_state.pos[1] += mesh->transform.pos[1];
		recurse_state.pos[2] += mesh->transform.pos[2];
		recurse_state.rot[0] += mesh->transform.rot[0];
		recurse_state.rot[1] += mesh->transform.rot[1];
		recurse_state.rot[2] += mesh->transform.rot[2];
		recurse_state.scale[0] *= mesh->transform.scale[0];
		recurse_state.scale[1] *= mesh->transform.scale[1];
		recurse_state.scale[2] *= mesh->transform.scale[2];
	}
	mat4x4_identity(recurse_state.model_r);
	mat4x4_rotate_X(out_m, out_m, (recurse_state.rot[0]) * 3.141592f / 180.0f);
	mat4x4_rotate_Y(out_m, out_m, (recurse_state.rot[1]) * 3.141592f / 180.0f);
	mat4x4_rotate_Z(out_m, out_m, (recurse_state.rot[2]) * 3.141592f / 180.0f);
	mat4x4_rotate_X(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[0]) * 3.141592f / 180.0f);
	mat4x4_rotate_Y(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[1]) * 3.141592f / 180.0f);
	mat4x4_rotate_Z(recurse_state.model_r, recurse_state.model_r, (recurse_state.rot[2]) * 3.141592f / 180.0f);
	mat4x4_scale_aniso(out_m, out_m, recurse_state.scale[0], recurse_state.scale[1], recurse_state.scale[2]);
}

static void mesh_draw(mesh_node_t * mesh, mat4x4 out_m) {
	if (mesh == NULL || out_m == NULL) {
		return;
	}

	mat4x4_identity(out_m);
	mesh_transform(mesh, out_m);

	mat4x4 mvp;
	mat4x4_mul(mvp, state.vp, out_m);

	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		D3D11_CALL(state.devctx->lpVtbl->Map(state.devctx, (ID3D11Resource *) state.ubuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		CopyMemory(mapped.pData, mvp, sizeof(mvp));
		state.devctx->lpVtbl->Unmap(state.devctx, (ID3D11Resource *) state.ubuffer, 0);
	}

	state.devctx->lpVtbl->IASetInputLayout(state.devctx, state.layout);
	state.devctx->lpVtbl->IASetPrimitiveTopology(state.devctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = sizeof(kgfw_graphics_vertex_t);
	UINT offset = 0;
	state.devctx->lpVtbl->IASetVertexBuffers(state.devctx, 0, 1, &mesh->d3d11.vbo, &stride, &offset);
	state.devctx->lpVtbl->IASetIndexBuffer(state.devctx, mesh->d3d11.ibo, DXGI_FORMAT_R32_UINT, 0);

	state.devctx->lpVtbl->VSSetConstantBuffers(state.devctx, 0, 1, &state.ubuffer);
	state.devctx->lpVtbl->VSSetShader(state.devctx, state.vshader, NULL, 0);

	D3D11_VIEWPORT viewport = {
		.TopLeftX = 0,
		.TopLeftY = 0,
		.Width = (float) state.window->width,
		.Height = (float) state.window->height,
		.MinDepth = 0,
		.MaxDepth = 1,
	};
	state.devctx->lpVtbl->RSSetViewports(state.devctx, 1, &viewport);
	state.devctx->lpVtbl->RSSetState(state.devctx, state.rasterizer);

	state.devctx->lpVtbl->PSSetSamplers(state.devctx, 0, 1, &mesh->d3d11.sampler);
	state.devctx->lpVtbl->PSSetShaderResources(state.devctx, 0, 1, &mesh->d3d11.tex);
	state.devctx->lpVtbl->PSSetShader(state.devctx, state.pshader, NULL, 0);

	state.devctx->lpVtbl->OMSetDepthStencilState(state.devctx, state.depth, 1);
	state.devctx->lpVtbl->OMSetBlendState(state.devctx, state.blend, NULL, 0xFFFFFFFFFFFFFFFF);
	state.devctx->lpVtbl->OMSetRenderTargets(state.devctx, 1, &state.target, state.depth_view);
	
	state.devctx->lpVtbl->DrawIndexed(state.devctx, mesh->d3d11.ibo_size, 0, 0);
}

static void meshes_draw_recursive(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	mesh_draw(mesh, recurse_state.model);
	if (mesh->child == NULL) {
		return;
	}

	meshes_draw_recursive_fchild(mesh->child);
}

static void meshes_draw_recursive_fchild(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	vec3 pos;
	vec3 rot;
	vec3 scale;
	memcpy(pos, recurse_state.pos, sizeof(pos));
	memcpy(rot, recurse_state.rot, sizeof(rot));
	memcpy(scale, recurse_state.scale, sizeof(scale));
	
	meshes_draw_recursive(mesh);
	for (mesh_node_t * m = mesh;;) {
		m = m->sibling;
		if (m == NULL) {
			break;
		}

		memcpy(recurse_state.pos, pos, sizeof(pos));
		memcpy(recurse_state.rot, rot, sizeof(rot));
		memcpy(recurse_state.scale, scale, sizeof(scale));
		meshes_draw_recursive(m);
	}
}

static void meshes_free_recursive(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	if (mesh->child != NULL) {
		meshes_free_recursive_fchild(mesh->child);
	}

	meshes_free(mesh);
}

static void meshes_free_recursive_fchild(mesh_node_t * mesh) {
	if (mesh == NULL) {
		return;
	}

	mesh_node_t * sibling = mesh->sibling;
	for (mesh_node_t * m = mesh;;) {
		m = sibling;
		if (m == NULL) {
			break;
		}
		sibling = m->sibling;

		meshes_free_recursive(m);
	}
	meshes_free_recursive(mesh);
}

static void update_settings(unsigned int change) {
	if (change &KGFW_GRAPHICS_SETTINGS_VSYNC) {
		if (state.window != NULL) {
			//glfwSwapInterval((state.settings &KGFW_GRAPHICS_SETTINGS_VSYNC));
		}
	}
}

static int gfx_command(int argc, char ** argv) {
	const char * subcommands = "set    enable    disable    reload";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
		return 0;
	}
	
	if (strcmp("reload", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("shaders", argv[2]) == 0) {
			if (state.vshader != NULL) {
				state.vshader->lpVtbl->Release(state.vshader);
				state.vshader = NULL;
			}
			if (state.pshader != NULL) {
				state.pshader->lpVtbl->Release(state.pshader);
				state.pshader = NULL;
			}
			if (shaders_load("assets/shaders/vertex.hlsl", "assets/shaders/pixel.hlsl", &state.vshader, &state.pshader) != 0) {
				return 1;
			}
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	} else if (strcmp("enable", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_ENABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	} else if (strcmp("disable", argv[1]) == 0) {
		const char * options = "vsync";
		const char * arguments = "[option]    see 'gfx options'";
		if (argc < 3) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", arguments);
			return 0;
		}

		if (strcmp("vsync", argv[2]) == 0) {
			kgfw_graphics_settings_set(KGFW_GRAPHICS_SETTINGS_ACTION_DISABLE, KGFW_GRAPHICS_SETTINGS_VSYNC);
			return 0;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "no option %s", argv[2]);
	} else if (strcmp("options", argv[1]) == 0) {
		const char * options = "vsync    shaders";
		const char * arguments = "[option]    see 'gfx options'";
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "options: %s", options);
	} else {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
	}
	return 0;
}

static void register_commands(void) {
	kgfw_console_register_command("gfx", gfx_command);
}

static int shaders_load(const char * vpath, const char * ppath, ID3D11VertexShader ** out_vshader, ID3D11PixelShader ** out_pshader) {
	const char * fallback_vshader =
		"#version 330 core\n"
		"layout(location = 0) in vec3 in_pos; layout(location = 1) in vec3 in_color; layout(location = 2) in vec3 in_normal; layout(location = 3) in vec2 in_uv; uniform mat4 unif_m; uniform mat4 unif_vp; out vec3 v_pos; out vec3 v_color; out vec3 v_normal; out vec2 v_uv; void main() { gl_Position = unif_vp * unif_m * vec4(in_pos, 1.0); v_pos = vec3(unif_m * vec4(in_pos, 1.0)); v_color = in_color; v_normal = in_normal; v_uv = in_uv; }";
	const char * fallback_pshader =
		"#version 330 core\n"
		"in vec3 v_pos; in vec3 v_color; in vec3 v_normal; in vec2 v_uv; out vec4 out_color; void main() { out_color = vec4(v_color, 1); }";

	char * vshader = (char *) fallback_vshader;
	char * pshader = (char *) fallback_pshader;
	unsigned long long int vlen = strlen(vshader);
	unsigned long long int plen = strlen(pshader);
	{
		FILE * fp = fopen(vpath, "rb");
		unsigned long long int length = 0;
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (char *) fallback_vshader;
			goto load_pshader;
		}
		fseek(fp, 0L, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		vshader = malloc(length + 1);
		if (vshader == NULL) {
			length = 0;
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (char *) fallback_vshader;
			goto load_pshader;
		}
		if (fread(vshader, 1, length, fp) != length) {
			length = 0;
			free(vshader);
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load vertex shader from \"%s\" falling back to default shader", vpath);
			vshader = (char *) fallback_vshader;
			goto load_pshader;
		}
		vshader[length] = '\0';
		fclose(fp);
		vlen = length;
	load_pshader:
		fp = fopen(ppath, "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", ppath);
			pshader = (char *) fallback_pshader;
			goto end_pshader;
		}
		fseek(fp, 0L, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		pshader = malloc(length + 1);
		if (pshader == NULL) {
			length = 0;
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", ppath);
			pshader = (char *) fallback_pshader;
			goto end_pshader;
		}
		if (fread(pshader, 1, length, fp) != length) {
			length = 0;
			free(pshader);
			fclose(fp);
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "failed to load fragment shader from \"%s\" falling back to default shader", ppath);
			pshader = (char *) fallback_pshader;
			goto end_pshader;
		}
		pshader[length] = '\0';
		fclose(fp);
		plen = length;
	end_pshader:;
	}

	D3D11_INPUT_ELEMENT_DESC desc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(kgfw_graphics_vertex_t, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(kgfw_graphics_vertex_t, nx), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(kgfw_graphics_vertex_t, u), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS;
	#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
	#endif

	ID3DBlob * err;
	ID3DBlob * vblob;
	ID3DBlob * pblob;

	D3D11_CALL_DO(D3DCompile(vshader, vlen, vpath, NULL, NULL, "vs", "vs_5_0", flags, 0, &vblob, &err),
		{
			if (err != NULL) {
				const char * message = err->lpVtbl->GetBufferPointer(err);
				OutputDebugStringA(message);
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "D3DCompile Failed!", "%s", message);
			}
		}
	);

	if (vblob == NULL) {
		return 2;
	}

	D3D11_CALL_DO(D3DCompile(pshader, plen, ppath, NULL, NULL, "ps", "ps_5_0", flags, 0, &pblob, &err),
		{
			if (err != NULL) {
				const char * message = err->lpVtbl->GetBufferPointer(err);
				OutputDebugStringA(message);
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "D3DCompile Failed!", "%s", message);
			}
		}
	);

	if (vblob == NULL) {
		return 3;
	}

	D3D11_CALL(state.dev->lpVtbl->CreateVertexShader(state.dev, vblob->lpVtbl->GetBufferPointer(vblob), vblob->lpVtbl->GetBufferSize(vblob), NULL, &state.vshader));
	D3D11_CALL(state.dev->lpVtbl->CreatePixelShader(state.dev, pblob->lpVtbl->GetBufferPointer(pblob), pblob->lpVtbl->GetBufferSize(pblob), NULL, &state.pshader));
	D3D11_CALL(state.dev->lpVtbl->CreateInputLayout(state.dev, desc, 4, vblob->lpVtbl->GetBufferPointer(vblob), vblob->lpVtbl->GetBufferSize(vblob), &state.layout));

	vblob->lpVtbl->Release(vblob);
	pblob->lpVtbl->Release(pblob);

	return 0;
}

#endif

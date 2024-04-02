#include "vulkan.h"
#include "window.h"

#include <time.h>
#include <ext/stb_ds.h>
#include <vulkan/vulkan.h>
#include <ext/vk_mem_alloc.h>

#define VK_CHECK(result) \
    ASSERT((result) == VK_SUCCESS)

#define VK_RET_ERR(result) \
    do { VkResult _vk_result = result; if (_vk_result != VK_SUCCESS) return _vk_result; } while(0)

#define MAX_PROPERTIES_LEN 32

typedef struct SPIRVBinary {
	u8 *data;
	u64 size;
} SPIRVBinary;

#define VK_FORMAT_FVEC2 VK_FORMAT_R32G32_SFLOAT
#define VK_FORMAT_FVEC3 VK_FORMAT_R32G32B32_SFLOAT

typedef struct fvec2 {
    f32 x;
    f32 y;
} fvec2;

typedef struct fvec3 {
    f32 x;
    f32 y;
    f32 z;
} fvec3;

typedef struct Vertex {
    fvec2 pos;
    fvec3 color;
} Vertex;

static_assert(sizeof(Vertex) == 5 * sizeof(f32), "packed vertex");

typedef struct BufferAllocationCreateInfo {
    VkBufferUsageFlags buffer_usage;
    VmaMemoryUsage memory_usage;
    VkDeviceSize size;
    VmaAllocationCreateFlags allocation_flag;
} BufferAllocationCreateInfo;

typedef struct BufferAllocation {
    VkBuffer buffer;
    VmaAllocation allocation;
} BufferAllocation;

typedef struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR surface_capabilities;
	VkSurfaceFormatKHR surface_format;
	VkPresentModeKHR present_mode;
} SwapchainSupportDetails;

#define MAX_QUEUE_INDICES_COUNT 2
typedef struct SwapchainCreateInfo {
	VkExtent2D image_extent;
	VkSharingMode sharing_mode;
	u32 queue_family_index_count;
	u32 queue_family_indices[MAX_QUEUE_INDICES_COUNT];
} SwapchainCreateInfo;

typedef struct Swapchain {
	VkSwapchainKHR vk_swapchain;
	VkFormat image_format;
	VkExtent2D extent;
	u32 image_count;
	VkImageView image_views[MAX_PROPERTIES_LEN];
	VkFramebuffer framebuffers[MAX_PROPERTIES_LEN];
} Swapchain;

typedef struct VkContext {
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physical_device;
	VkDevice device;
    VmaAllocator allocator;
	VkQueue graphics_queue;
	VkQueue present_queue;
	u32 present_queue_index;
	u32 graphics_queue_index;
} VkContext;

#define MAX_FRAMES_IN_FLIGHT 2
typedef struct PapertrailRenderpass {
	VkPipeline pipeline;
	SwapchainCreateInfo swapchain_create_info; // for rebuilding the swapchain
	Swapchain swapchain;
	VkRenderPass renderpass;
	VkCommandPool command_pool;

	VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore semaphore_image_available[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore semaphore_render_finished[MAX_FRAMES_IN_FLIGHT];
	VkFence fence_in_flight[MAX_FRAMES_IN_FLIGHT];

    u32 current_frame_index;
    u32 current_swapchain_image_index;
} PapertrailRenderpass;

typedef struct PapertrailRenderData {
    BufferAllocation vertex_buffer;
    BufferAllocation index_buffer;
} PapertrailRenderData;

// callback functions and data for window events (e.g. resizing)
typedef struct PapertrailWindowCallbackFn {
	VkContext *p_context;
	PapertrailRenderpass *p_renderpass;
    PapertrailRenderData *p_render_data;
} PapertrailWindowCallbackFn;

const Vertex VERTICES[] = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const u16 INDICES[] = {
        0, 1, 2, 2, 3, 0
};


const VkVertexInputBindingDescription vertex_binding_description = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};

const VkVertexInputAttributeDescription vertex_attribute_descriptions[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_FVEC2,
            .offset = offsetof(Vertex, pos),
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_FVEC3,
            .offset = offsetof(Vertex, color)
        }
};


/// WINDOW ///

VkExtent2D get_vk_window_size(PapertrailWindow *window) {
    u32 width, height;
    ptrail_window_get_size(window, &width, &height);

    width = MAX(1, width);
    height = MAX(1, height);

    return (VkExtent2D) {
            .width = width,
            .height = height
    };
}

void wait_if_minimized(PapertrailWindow *window) {
    u32 width, height;
    ptrail_window_get_size(window, &width, &height);

    while (width == 0 || height == 0) {
        ptrail_window_get_size(window, &width, &height);
        ptrail_window_wait_events();
    }
}


/// SHADER_MODULES ///

void free_spirv_binary(SPIRVBinary *buffer) {
	ASSERT(buffer->data);
	free(buffer->data);
}

SPIRVBinary load_spirv_binary(const char *path) {
	u8 *source = NULL;
	u64 bufsize = 0;

	//TODO: set bin mode?
	FILE *fp;
    fopen_s(&fp, path, "rb");
	ASSERT_MSG(fp, "could not open file: %s", path);

	if (fseek(fp, 0L, SEEK_END) == 0) {
		bufsize = ftell(fp);
		ASSERT_MSG(bufsize != -1, "could not tell buffer size");

		source = malloc(sizeof(char) * (bufsize + 1));
		ASSERT(source);
		memset(source, 0, bufsize + 1);

		ASSERT(fseek(fp, 0L, SEEK_SET) == 0);
		usize newLen = fread(source, sizeof(u8), bufsize, fp);
		ASSERT_MSG(ferror(fp) == 0, "Error reading file");

		source[newLen++] = '\0';
	}
	fclose(fp);

	return (SPIRVBinary) {
		.data = source,
			.size = bufsize,
	};
}

VkShaderModule vk_create_shader_module(SPIRVBinary spv, const VkContext *vc) {
	VkShaderModuleCreateInfo shader_module_create_info = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = spv.size,
			.pCode = (u32 *)spv.data,
	};

	VkShaderModule shader_module;
	VK_CHECK(vkCreateShaderModule(vc->device, &shader_module_create_info, NULL, &shader_module));
	return shader_module;
}


// BUFFERS //

local inline VkResult buffer_allocation_create(VmaAllocator allocator,
                                  const BufferAllocationCreateInfo *create_info,
                                  BufferAllocation *buffer_allocation)
 {
    VkBufferCreateInfo buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = create_info->buffer_usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .size = create_info->size,
    };

    VmaAllocationCreateInfo allocation_create_info = {
        .usage = create_info->memory_usage,
        .flags = create_info->allocation_flag,
    };

    VkBuffer buffer;
    VmaAllocation allocation;
    VK_RET_ERR(vmaCreateBuffer(
            allocator,
            &buffer_create_info,
            &allocation_create_info,
            &buffer,
            &allocation,
            NULL)
    );

    *buffer_allocation = (BufferAllocation) {
        .buffer = buffer,
        .allocation = allocation,
    };

    return VK_SUCCESS;
}

local inline void buffer_allocation_destroy(BufferAllocation *buff, VmaAllocator allocator) {
    vmaDestroyBuffer(allocator, buff->buffer, buff->allocation);
}

local inline void ptrail_render_data_destroy(PapertrailRenderData *data, const VkContext *c) {
    buffer_allocation_destroy(&data->vertex_buffer, c->allocator);
    buffer_allocation_destroy(&data->index_buffer, c->allocator);
}


/// SWAPCHAIN ///

local SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

	u32 surface_format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, NULL);
	ASSERT(surface_format_count != 0 && surface_format_count <= MAX_PROPERTIES_LEN);
	VkSurfaceFormatKHR surface_formats[MAX_PROPERTIES_LEN];
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, surface_formats);

	u32 surface_present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &surface_present_mode_count, NULL);
	ASSERT(surface_present_mode_count != 0 && surface_present_mode_count <= MAX_PROPERTIES_LEN);
	VkPresentModeKHR surface_present_modes[MAX_PROPERTIES_LEN];
	vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &surface_present_mode_count,
            surface_present_modes);

	VkSurfaceFormatKHR surface_format = surface_formats[0];
	for (u32 i = 0; i < surface_format_count; i++) {
		VkSurfaceFormatKHR format = surface_formats[i];
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
			format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			surface_format = format;
			break;
		}
	}
	VkPresentModeKHR swap_surface_present_mode = VK_PRESENT_MODE_FIFO_KHR;
	for (u32 i = 0; i < surface_present_mode_count; i++) {
		if (surface_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			swap_surface_present_mode = surface_present_modes[i];
			break;
		}
	}

	return (SwapchainSupportDetails) {
		.surface_capabilities = surface_capabilities,
			.surface_format = surface_format,
			.present_mode = swap_surface_present_mode,
	};
}

local void swapchain_destroy(VkDevice device, Swapchain *s) {
	for (u32 i = 0; i < s->image_count; i++) {
		vkDestroyFramebuffer(device, s->framebuffers[i], NULL);
	}
	for (u32 i = 0; i < s->image_count; i++) {
		vkDestroyImageView(device, s->image_views[i], NULL);
	}
	vkDestroySwapchainKHR(device, s->vk_swapchain, NULL);
}

local VkResult swapchain_init(
	VkDevice device,
	VkPhysicalDevice physical_device,
	VkSurfaceKHR surface,
	VkRenderPass renderpass,
	const SwapchainCreateInfo *create_info,
	Swapchain *out_swapchain)
{
	VkSwapchainKHR swapchain;
	SwapchainSupportDetails swapchain_support = query_swapchain_support(physical_device, surface);
	VkSurfaceCapabilitiesKHR capabilities = swapchain_support.surface_capabilities;

	u32 min_image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount != 0) {
		min_image_count = MAX(min_image_count, capabilities.maxImageCount);
	}

	VkExtent2D swap_extent;
	if (capabilities.currentExtent.width != U32_MAX
		&& capabilities.currentExtent.height != U32_MAX) {
		swap_extent = capabilities.currentExtent;
	}
	else {
		swap_extent = (VkExtent2D){
				.width = CLAMP(create_info->image_extent.width,
							   capabilities.maxImageExtent.width,
							   capabilities.minImageExtent.width),

				.height = CLAMP(create_info->image_extent.height,
								capabilities.maxImageExtent.height,
								capabilities.minImageExtent.height),
		};
	}

	VkSwapchainCreateInfoKHR swapchain_create_info = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surface,
			.minImageCount = min_image_count,
			.imageFormat = swapchain_support.surface_format.format,
			.imageColorSpace = swapchain_support.surface_format.colorSpace,
			.imageExtent = swap_extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = create_info->sharing_mode,
			.queueFamilyIndexCount = create_info->queue_family_index_count,
			.pQueueFamilyIndices = create_info->queue_family_indices,
			.preTransform = swapchain_support.surface_capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = swapchain_support.present_mode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE,
	};

	VK_RET_ERR(vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain));

	u32 image_count = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);
	ASSERT(image_count <= MAX_PROPERTIES_LEN);
	VkImage swapchain_images[MAX_PROPERTIES_LEN];
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images);

	VkImageView swapchain_image_views[MAX_PROPERTIES_LEN];
	for (u32 i = 0; i < image_count; i++) {
		VkImageViewCreateInfo image_view_create_info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = swapchain_images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = swapchain_support.surface_format.format,
				.components = {
						.r = VK_COMPONENT_SWIZZLE_IDENTITY,
						.g = VK_COMPONENT_SWIZZLE_IDENTITY,
						.b = VK_COMPONENT_SWIZZLE_IDENTITY,
						.a = VK_COMPONENT_SWIZZLE_IDENTITY,
				},
				.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
				}
		};

		VK_RET_ERR(vkCreateImageView(device, &image_view_create_info, NULL, &swapchain_image_views[i]));
	}

	VkFramebuffer framebuffers[MAX_PROPERTIES_LEN];

	for (u32 i = 0; i < image_count; i++) {
		VkFramebufferCreateInfo framebuffer_create_info = {
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass = renderpass,
				.attachmentCount = 1,
				.pAttachments = &swapchain_image_views[i],
				.width = swap_extent.width,
				.height = swap_extent.height,
				.layers = 1,
		};

		VK_RET_ERR(vkCreateFramebuffer(device, &framebuffer_create_info, NULL, &framebuffers[i]));
	}

	*out_swapchain = (Swapchain){
			.vk_swapchain = swapchain,
			.extent = swap_extent,
			.image_format = swapchain_support.surface_format.format,
			.image_count = image_count,
	};
	memcpy(out_swapchain->image_views, swapchain_image_views, MAX_PROPERTIES_LEN * sizeof(VkImageView));
	memcpy(out_swapchain->framebuffers, framebuffers, MAX_PROPERTIES_LEN * sizeof(VkFramebuffer));

	return VK_SUCCESS;
}

local VkResult swapchain_rebuild(
	VkDevice device,
	VkPhysicalDevice physical_device,
	VkSurfaceKHR surface,
	VkRenderPass renderpass,
	const SwapchainCreateInfo *create_info,
	Swapchain *swapchain)
{
	vkDeviceWaitIdle(device);
	swapchain_destroy(device, swapchain);
	VK_RET_ERR(swapchain_init(
		device,
		physical_device,
		surface,
		renderpass,
		create_info,
		swapchain
	));

	return VK_SUCCESS;
}

local void framebuffer_resized_callback(PapertrailWindow *window, i32 width, i32 height) {
	PapertrailWindowCallbackFn *ptrs = ptrail_window_get_client_state(window);
	VkContext *c = ptrs->p_context;
	PapertrailRenderpass *rp = ptrs->p_renderpass;

	rp->swapchain_create_info.image_extent = (VkExtent2D){
            .width = (u32)width,
            .height = (u32)height,
    };
	VK_CHECK(swapchain_rebuild(c->device, c->physical_device, c->surface, rp->renderpass,
		&rp->swapchain_create_info, &rp->swapchain));
}


VkPhysicalDevice vk_pick_physical_device(
        const char **req_extensions,
        u32 extension_count,
        VkInstance instance
        ){
    u32 device_count = 0;
    VkPhysicalDevice devices[MAX_PROPERTIES_LEN];
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    ASSERT(device_count <= MAX_PROPERTIES_LEN);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);

    for (u32 device_indx = 0; device_indx < device_count; device_indx++) {
        VkPhysicalDevice physical_device = devices[device_indx];

        u32 available_extension_count = 0;
        vkEnumerateDeviceExtensionProperties(physical_device, NULL, &available_extension_count, NULL);
        VkExtensionProperties *available_extensions =
                malloc(available_extension_count * sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(physical_device, NULL, &available_extension_count, available_extensions);

        bool all_ext_supported = true;
        for (u32 i = 0; i < extension_count; i++) {
            const char *req_ext = req_extensions[i];

            bool ext_supported = false;
            for (u32 j = 0; j < available_extension_count; j++) {
                VkExtensionProperties avail_ext = available_extensions[j];
                if (strcmp(req_ext, avail_ext.extensionName) == 0) {
                    ext_supported = true;
                }
            }

            if (!ext_supported) {
                all_ext_supported = false;
            }
        }

        free(available_extensions);
        if (!all_ext_supported) return physical_device;
    }
    ASSERT_MSG(false, "could not find suitable device");
    return (VkPhysicalDevice){0};
}

/// VK_CONTEXT ///

VkContext vk_context_init(PapertrailWindow *window) {

	VkApplicationInfo application_info = {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = NULL,
			.pApplicationName = "Papertrail",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "PaperEngine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_API_VERSION_1_2,
	};

	const char *required_extensions[MAX_PROPERTIES_LEN];
    u32 required_extensions_count = 0;
    u32 window_extension_count = 0;
    const char **window_extensions = ptrail_vk_get_required_instance_exts(&window_extension_count);
    ASSERT(window_extension_count <= MAX_PROPERTIES_LEN);
    for (u32 i = 0; i < window_extension_count; i++) {
        required_extensions[i] = window_extensions[i];
    }
    required_extensions_count += window_extension_count;



	/// INSTANCE ///

	const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
	VkInstanceCreateInfo instance_create_info = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &application_info,

			.enabledExtensionCount = required_extensions_count,
			.ppEnabledExtensionNames = required_extensions,

			.enabledLayerCount = 1,
			.ppEnabledLayerNames = validation_layers,
	};
	VkInstance instance;
	VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &instance));

	/// Surface ///
	VkSurfaceKHR surface;
	VK_CHECK(ptrail_vk_window_surface_init(instance, window, &surface));


	/// PHYSICAL DEVICE ///

	VkPhysicalDevice physical_device = vk_pick_physical_device(
            required_extensions,
            required_extensions_count,
            instance);
    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
    println("physical device: %s", physical_device_properties.deviceName);


	/// QUEUE FAMILIES ///

	u32 queue_family_count = 0;
    VkQueueFamilyProperties queue_families[MAX_PROPERTIES_LEN];
	vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device,
            &queue_family_count,
            NULL);
	ASSERT(queue_family_count <= MAX_PROPERTIES_LEN);
	vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device,
            &queue_family_count,
            queue_families);

	/* graphics queue */
	u32 graphics_queue_index = 0;
	bool found_graphics_queue = false;

	for (u32 i = 0; i < queue_family_count; i++) {
		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			found_graphics_queue = true;
			graphics_queue_index = i;
			break;
		}
	}

	/* present queue */
	VkBool32 found_present_support = false;
	u32 present_queue_index = 0;
	for (u32 i = 0; i < queue_family_count; i++) {
		vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &found_present_support);
		if (found_present_support) {
			present_queue_index = i;
			break;
		}
	}

	ASSERT_MSG(found_graphics_queue && found_present_support, "could not find suitable graphics & present queue");

	float queue_priority = 1.0f;
	u32 queue_create_info_count = 2;
	if (graphics_queue_index == present_queue_index) queue_create_info_count = 1;
	VkDeviceQueueCreateInfo queue_create_infos[2] = {
			(VkDeviceQueueCreateInfo) {
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = graphics_queue_index,
					.queueCount = 1,
					.pQueuePriorities = &queue_priority,
			},

			(VkDeviceQueueCreateInfo) {
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = present_queue_index,
					.queueCount = 1,
					.pQueuePriorities = &queue_priority,
			}
	};


	/// LOGICAL DEVICE ///

	const char *required_device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkPhysicalDeviceFeatures deviceFeatures = { 0 };
	VkDeviceCreateInfo device_create_info = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pQueueCreateInfos = queue_create_infos,
			.queueCreateInfoCount = queue_create_info_count,
			.pEnabledFeatures = &deviceFeatures,
			.enabledExtensionCount = 1,
			.ppEnabledExtensionNames = required_device_extensions,
	};

	VkDevice device = { 0 };
	VK_CHECK(vkCreateDevice(physical_device, &device_create_info, NULL, &device));

	VkQueue graphics_queue;
	VkQueue present_queue;
	vkGetDeviceQueue(device, graphics_queue_index, 0, &graphics_queue);
	vkGetDeviceQueue(device, present_queue_index, 0, &present_queue);

    VmaAllocatorCreateInfo allocator_create_info = {
            .physicalDevice = physical_device,
            .device = device,
            .instance = instance,
    };
    VmaAllocator allocator;
    vmaCreateAllocator(&allocator_create_info, &allocator);

	return (VkContext) {
        .instance = instance,
        .surface = surface,
        .physical_device = physical_device,
        .device = device,
        .allocator = allocator,
        .graphics_queue = graphics_queue,
        .graphics_queue_index = graphics_queue_index,
        .present_queue = present_queue,
        .present_queue_index = present_queue_index,
	};
}

void vk_context_destroy(VkContext *c) {
    vmaDestroyAllocator(c->allocator);
	vkDestroySurfaceKHR(c->instance, c->surface, NULL);
	vkDestroyDevice(c->device, NULL);
	vkDestroyInstance(c->instance, NULL);
}


/****************************************************************
 *                         PAPER_TRAIL                          *
 ****************************************************************/

void ptrail_renderpass_destroy(VkDevice device, PapertrailRenderpass *rp) {
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, rp->semaphore_image_available[i], NULL);
		vkDestroySemaphore(device, rp->semaphore_render_finished[i], NULL);
		vkDestroyFence(device, rp->fence_in_flight[i], NULL);
	}

	vkDestroyCommandPool(device, rp->command_pool, NULL);
	vkDestroyPipeline(device, rp->pipeline, NULL);
	vkDestroyRenderPass(device, rp->renderpass, NULL);
	swapchain_destroy(device, &rp->swapchain);
}

PapertrailRenderpass ptrail_renderpass_init(const VkContext *c, PapertrailWindow *window) {
	VkRenderPass renderpass;
	VkPipeline pipeline;
	Swapchain swapchain;

	VkCommandPool command_pool;
	VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];

	VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];


	/// PIPELINE LAYOUT ///

	VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertex_binding_description,
			.vertexAttributeDescriptionCount = COUNT_OF(vertex_attribute_descriptions),
			.pVertexAttributeDescriptions = vertex_attribute_descriptions,
	};

	VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
	};

	VkDynamicState dynamic_states[2] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = sizeof(dynamic_states) / sizeof(VkDynamicState),
			.pDynamicStates = dynamic_states,
	};

	VkPipelineViewportStateCreateInfo viewport_state_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports = NULL,
			.scissorCount = 1,
			.pScissors = NULL,
	};

	VkPipelineRasterizationStateCreateInfo rasterization_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.lineWidth = 1.0f,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
	};

	VkPipelineMultisampleStateCreateInfo multisample_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.sampleShadingEnable = VK_FALSE,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkPipelineColorBlendAttachmentState color_blend_attachment = {
			.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT,
			.blendEnable = VK_FALSE,
	};

	VkPipelineColorBlendStateCreateInfo color_blend_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = &color_blend_attachment,
			.blendConstants[0] = 0.0f,
			.blendConstants[1] = 0.0f,
			.blendConstants[2] = 0.0f,
			.blendConstants[3] = 0.0f,

	};

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 0,
			.pushConstantRangeCount = 0,
	};
    VkPipelineLayout pipeline_layout;
	VK_CHECK(vkCreatePipelineLayout(c->device, &pipeline_layout_create_info, NULL, &pipeline_layout));


	/// PIPELINE ///


	/* shaders */
	SPIRVBinary vertex_bin = load_spirv_binary("../shaders/vert.spv");
	VkShaderModule vertex_module = vk_create_shader_module(vertex_bin, c);
	free_spirv_binary(&vertex_bin);

	SPIRVBinary fragment_bin = load_spirv_binary("../shaders/frag.spv");
	VkShaderModule fragment_module = vk_create_shader_module(fragment_bin, c);
	free_spirv_binary(&fragment_bin);

	VkPipelineShaderStageCreateInfo shader_stages[2] = {
			{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_VERTEX_BIT,
					.module = vertex_module,
					.pName = "main",
			},
			{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
					.module = fragment_module,
					.pName = "main",
			}
	};

	/* renderpass */
	SwapchainSupportDetails swapchain_support = query_swapchain_support(c->physical_device, c->surface);

	VkAttachmentDescription color_attachment_desc = {
			.format = swapchain_support.surface_format.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference color_attachment_ref = {
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpass_desc = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_ref,
	};

	VkSubpassDependency subpass_dependency = {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	};

	VkRenderPassCreateInfo renderpass_create_info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &color_attachment_desc,
			.subpassCount = 1,
			.pSubpasses = &subpass_desc,
			.dependencyCount = 1,
			.pDependencies = &subpass_dependency,
	};
	VK_CHECK(vkCreateRenderPass(c->device, &renderpass_create_info, NULL, &renderpass));

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = shader_stages,
			.pVertexInputState = &pipeline_vertex_input_create_info,
			.pInputAssemblyState = &pipeline_input_assembly_create_info,
			.pViewportState = &viewport_state_create_info,
			.pRasterizationState = &rasterization_create_info,
			.pMultisampleState = &multisample_create_info,
			.pDepthStencilState = NULL,
			.pColorBlendState = &color_blend_create_info,
			.pDynamicState = &dynamic_state_create_info,
			.layout = pipeline_layout,
			.renderPass = renderpass,
			.subpass = 0,
	};
	VK_CHECK(vkCreateGraphicsPipelines(c->device, VK_NULL_HANDLE, 1,
		&graphics_pipeline_create_info, NULL, &pipeline));

    /* cleanup */
    vkDestroyPipelineLayout(c->device, pipeline_layout, NULL);
    vkDestroyShaderModule(c->device, vertex_module, NULL);
    vkDestroyShaderModule(c->device, fragment_module, NULL);

	/// SWAPCHAIN ///

	u32 queue_family_index_count = 2;
	VkSharingMode swapchain_sharing_mode = VK_SHARING_MODE_CONCURRENT;

    // if graphics_queue_index == present_queue_index we only have a single queue family
	if (c->graphics_queue_index == c->present_queue_index) {
		queue_family_index_count = 0;
		swapchain_sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
	}

	ASSERT(queue_family_index_count <= MAX_QUEUE_INDICES_COUNT);
	SwapchainCreateInfo swapchain_create_info = {
			.image_extent = get_vk_window_size(window),
			.sharing_mode = swapchain_sharing_mode,
			.queue_family_index_count = queue_family_index_count,
            .queue_family_indices = {c->graphics_queue_index, c->present_queue_index }
	};
	VK_CHECK(swapchain_init(c->device, c->physical_device, c->surface, renderpass, &swapchain_create_info, &swapchain));


	/// COMMAND BUFFER ///

	VkCommandPoolCreateInfo command_pool_create_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = c->graphics_queue_index,
	};
	VK_CHECK(vkCreateCommandPool(c->device, &command_pool_create_info, NULL, &command_pool));

	VkCommandBufferAllocateInfo command_buffer_allocate_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = command_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
	};
	VK_CHECK(vkAllocateCommandBuffers(c->device, &command_buffer_allocate_info, command_buffers));


	/// SYNCHRONIZATION ///

	VkSemaphoreCreateInfo semaphore_create_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};
	VkFenceCreateInfo fence_create_info = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VK_CHECK(vkCreateSemaphore(c->device, &semaphore_create_info, NULL, &image_available_semaphores[i]));
		VK_CHECK(vkCreateSemaphore(c->device, &semaphore_create_info, NULL, &render_finished_semaphores[i]));
		VK_CHECK(vkCreateFence(c->device, &fence_create_info, NULL, &in_flight_fences[i]));
	}

	PapertrailRenderpass ptrail_renderpass = {
		.pipeline = pipeline,
		.renderpass = renderpass,
		.swapchain = swapchain,
		.swapchain_create_info = swapchain_create_info,
		.command_pool = command_pool,
		.current_frame_index = 0,
	};

	memcpy(ptrail_renderpass.command_buffers, command_buffers,
		MAX_FRAMES_IN_FLIGHT * sizeof(VkCommandBuffer));
	memcpy(ptrail_renderpass.semaphore_image_available, image_available_semaphores,
		MAX_FRAMES_IN_FLIGHT * sizeof(VkSemaphore));
	memcpy(ptrail_renderpass.semaphore_render_finished, render_finished_semaphores,
		MAX_FRAMES_IN_FLIGHT * sizeof(VkSemaphore));
	memcpy(ptrail_renderpass.fence_in_flight, in_flight_fences,
		MAX_FRAMES_IN_FLIGHT * sizeof(VkFence));

	return ptrail_renderpass;
}

void ptrail_renderpass_begin(PapertrailRenderpass *rp, const VkContext *c, PapertrailWindow *window) {
    VkCommandBuffer command_buffer = rp->command_buffers[rp->current_frame_index];
    VkSemaphore image_available_semaphore = rp->semaphore_image_available[rp->current_frame_index];
    VkSemaphore render_finished_semaphore = rp->semaphore_render_finished[rp->current_frame_index];
    VkFence in_flight_fence = rp->fence_in_flight[rp->current_frame_index];


    /// ACQUIRE IMAGE ///

    vkWaitForFences(c->device, 1, &in_flight_fence, VK_TRUE, U64_MAX);

    VkResult result = vkAcquireNextImageKHR(c->device,
                                            rp->swapchain.vk_swapchain,
                                            U64_MAX,
                                            image_available_semaphore,
                                            VK_NULL_HANDLE,
                                            &rp->current_swapchain_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        rp->swapchain_create_info.image_extent = get_vk_window_size(window);
        swapchain_rebuild(c->device, c->physical_device, c->surface, rp->renderpass,
                          &rp->swapchain_create_info, &rp->swapchain);
        return;
    }
    ASSERT_MSG(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "failed to acquire swapchain image");
    vkResetFences(c->device, 1, &in_flight_fence);



    vkResetCommandBuffer(command_buffer, 0);
    VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));


    /// BEGIN RENDERPASS ///

    VkClearValue clear_value = {
            .color = { 0.0f, 0.0f, 0.0f, 1.0f }
    };
    VkRenderPassBeginInfo renderpass_begin_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = rp->renderpass,
            .framebuffer = rp->swapchain.framebuffers[rp->current_swapchain_image_index],
            .renderArea = {
                    .offset = {0},
                    .extent = rp->swapchain.extent,
            },
            .clearValueCount = 1,
            .pClearValues = &clear_value,
    };
    vkCmdBeginRenderPass(command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    /* set viewport */
    VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)rp->swapchain.extent.width,
            .height = (float)rp->swapchain.extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };
    VkRect2D viewport_scissor = {
            .offset = {0, 0},
            .extent = rp->swapchain.extent,
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &viewport_scissor);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rp->pipeline);
}

void ptrail_renderpass_end(PapertrailRenderpass *rp, const VkContext *c, PapertrailWindow *window) {
    VkCommandBuffer command_buffer = rp->command_buffers[rp->current_frame_index];
    VkSemaphore image_available_semaphore = rp->semaphore_image_available[rp->current_frame_index];
    VkSemaphore render_finished_semaphore = rp->semaphore_render_finished[rp->current_frame_index];
    VkFence in_flight_fence = rp->fence_in_flight[rp->current_frame_index];

    vkCmdEndRenderPass(command_buffer);
    VK_CHECK(vkEndCommandBuffer(command_buffer));

    /// END RENDERPASS ///

    VkSemaphore wait_semaphore = image_available_semaphore;
    VkSemaphore signal_semaphore = render_finished_semaphore;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &wait_semaphore,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &signal_semaphore,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
    };
    VK_CHECK(vkQueueSubmit(c->graphics_queue, 1, &submit_info, in_flight_fence));


    VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &signal_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &rp->swapchain.vk_swapchain,
            .pImageIndices = &rp->current_swapchain_image_index,
    };

    VkResult result = vkQueuePresentKHR(c->present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        rp->swapchain_create_info.image_extent = get_vk_window_size(window);
        VK_CHECK(swapchain_rebuild(c->device, c->physical_device, c->surface, rp->renderpass,
                                   &rp->swapchain_create_info, &rp->swapchain));
    }
    else {
        ASSERT_MSG(result == VK_SUCCESS, "failed to present swapchain image. err_code: %i", result);
    }

    rp->current_frame_index = (rp->current_frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

void ptrail_render_frame(
        PapertrailRenderpass *rp,
        PapertrailRenderData *render_data,
        const VkContext *c,
        PapertrailWindow *window)
{
    ptrail_renderpass_begin(rp, c, window);

    VkCommandBuffer command_buffer = rp->command_buffers[rp->current_frame_index];
    VkDeviceSize offset = {0};

    vkCmdBindVertexBuffers(command_buffer, 0, 1, &render_data->vertex_buffer.buffer, &offset);
    vkCmdBindIndexBuffer(command_buffer, render_data->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);

    ptrail_renderpass_end(rp, c, window);
}

void window_refresh_callback(PapertrailWindow *window) {
	PapertrailWindowCallbackFn *ptrs = ptrail_window_get_client_state(window);
	ptrail_render_frame(ptrs->p_renderpass, ptrs->p_render_data, ptrs->p_context, window);
}

void run() {
    PapertrailWindowCreateInfo window_create_info = {
            .title = "Papertrail",
            .width = 1000,
            .height = 800,
            .min_height = 1,
            .min_width = 1,
    };

    PapertrailWindow *window = ptrail_vk_window_init(&window_create_info);

    VkContext c = vk_context_init(window);
	PapertrailRenderpass rp = ptrail_renderpass_init(&c, window);
    PapertrailRenderData render_data;


	PapertrailWindowCallbackFn callback_ptrs = {
			.p_renderpass = &rp,
            .p_render_data = &render_data,
			.p_context = &c,
	};

	ptrail_window_set_resize_callback(window, framebuffer_resized_callback);
	ptrail_window_set_refresh_callback(window, window_refresh_callback);
	ptrail_window_set_client_state(window, &callback_ptrs);

    /// VERTEX & INDEX BUFFER INIT ///

    BufferAllocationCreateInfo vertex_buffer_create_info = {
            .size = sizeof(VERTICES),
            .buffer_usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .memory_usage = VMA_MEMORY_USAGE_AUTO,
            .allocation_flag = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    };

    BufferAllocationCreateInfo index_buffer_create_info = {
            .size = sizeof(VERTICES),
            .buffer_usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .memory_usage = VMA_MEMORY_USAGE_AUTO,
            .allocation_flag = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    };

    VK_CHECK(buffer_allocation_create(c.allocator, &vertex_buffer_create_info, &render_data.vertex_buffer));
    VK_CHECK(buffer_allocation_create(c.allocator, &index_buffer_create_info, &render_data.index_buffer));

    void *buffer_data;
    vmaMapMemory(c.allocator, render_data.vertex_buffer.allocation, &buffer_data);
    memcpy(buffer_data, VERTICES, sizeof(VERTICES));
    vmaUnmapMemory(c.allocator, render_data.vertex_buffer.allocation);

    vmaMapMemory(c.allocator, render_data.index_buffer.allocation, &buffer_data);
    memcpy(buffer_data, INDICES, sizeof(INDICES));
    vmaUnmapMemory(c.allocator, render_data.index_buffer.allocation);

    f64 prev_time = ptrail_get_time();
    u64 frame_count = 0;

	/// RENDER LOOP ///
	while (ptrail_window_is_open(window)) {
		wait_if_minimized(window);

        f64 curr_time = ptrail_get_time();
        frame_count += 1;
        if (curr_time - prev_time >= 1) {
            println("fps: %llu", frame_count);
            frame_count = 0;
            prev_time = curr_time;
        }

		ptrail_render_frame(&rp, &render_data, &c, window);

		ptrail_window_poll_events();
	}


	/// CLEANUP ///
	vkDeviceWaitIdle(c.device);

    buffer_allocation_destroy(&render_data.vertex_buffer, c.allocator);
    buffer_allocation_destroy(&render_data.index_buffer, c.allocator);

	ptrail_renderpass_destroy(c.device, &rp);
	vk_context_destroy(&c);

    ptrail_window_free(window);
}

#include "window.h"

#include "stb_ds.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define VK_CHECK_MSG(result, msg, ...) \
    ASSERT_MSG((result) == VK_SUCCESS, msg, __VA_ARGS__)

#define VK_CHECK(result) \
    ASSERT((result) == VK_SUCCESS)

void glfw_error_callback(i32 error, const char *desc) {
    PANIC("GLFW_ERROR: %s\n", desc);
};

void key_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void print_supported_layers() {
    u32 supported_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&supported_layer_count, NULL);
    VkLayerProperties *supported_validation_layer = malloc(supported_layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&supported_layer_count, supported_validation_layer);

    println("supported layers:");
    for (u32 i = 0; i < supported_layer_count; i++) {
        println("%s", supported_validation_layer[i].layerName);
    }

    free(supported_validation_layer);
}

void print_supported_extensions() {
    u32 supported_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_count, NULL);
    VkExtensionProperties *supported_extensions = malloc(supported_extension_count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &supported_extension_count, supported_extensions);

    println("supported extensions:");
    for (u32 i = 0; i < supported_extension_count; i++) {
        println("%s", supported_extensions[i].extensionName);
    }

    free(supported_extensions);
}

void print_supported_device_extensions() {
}

typedef struct VkContext {
    GLFWwindow *window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;

    VkDevice device;
    u32 graphics_queue_index;
    VkQueue graphics_queue;
    u32 present_queue_index;
    VkQueue present_queue;

    VkSwapchainKHR swapchain;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
    u32 swapchain_image_count;
    VkImageView *swapchain_image_views;
} VkContext;

VkContext vulkan_context_init() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(1000, 800, "Papertrail", NULL, NULL);

    VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "Papertrail",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "PaperEngine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    const char **required_extensions = NULL;
    {
        u32 glfw_extension_count = 0;
        const char **glfw_extensions = 
            glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        for (u32 i = 0; i < glfw_extension_count; i++) {
            arrput(required_extensions, glfw_extensions[i]);
        }
    }


    /// INSTANCE ///

    const char *validation_layers[] = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,

        .enabledExtensionCount = arrlen(required_extensions),
        .ppEnabledExtensionNames = required_extensions,

        .enabledLayerCount = 1,
        .ppEnabledLayerNames = validation_layers,
    };


    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &instance));
    arrfree(required_extensions);

    
    /// Surface ///

    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &surface));


    /// PHYSICAL DEVICE ///

    u32 physical_device_count = 0;
    vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL);
    ASSERT_MSG(physical_device_count > 0, "failed to find device with vulkan support");
    VkPhysicalDevice *physical_devices = malloc(physical_device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices);

    // TODO: pick device
    // vkEnumerateDeviceExtensionProperties require VK_KHR_SWAPCHAIN_EXTENSION_NAME
    VkPhysicalDevice physical_device = physical_devices[0];
    free(physical_devices);



    /// QUEUE FAMILIES ///

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families = malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

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


    float queue_priority = 1.0;

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

    VkPhysicalDeviceFeatures deviceFeatures = {0};
    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_infos,
        .queueCreateInfoCount = queue_create_info_count,
        .pEnabledFeatures = &deviceFeatures,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = required_device_extensions,
    };


    VkDevice device = {0};
    VK_CHECK(vkCreateDevice(physical_device, &device_create_info, NULL, &device));


    VkQueue graphics_queue;
    VkQueue present_queue;
    vkGetDeviceQueue(device, graphics_queue_index, 0, &graphics_queue);
    vkGetDeviceQueue(device, present_queue_index, 0, &present_queue);


    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);


    /// CHOOSE FORMAT & PRESENT MODE


    u32 surface_format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, NULL);
    ASSERT(surface_format_count != 0);
    VkSurfaceFormatKHR *surface_formats = malloc(surface_format_count * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, surface_formats);

    u32 surface_present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &surface_present_mode_count, NULL);
    ASSERT(surface_present_mode_count != 0);
    VkPresentModeKHR *surface_present_modes = malloc(surface_present_mode_count * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &surface_present_mode_count, surface_present_modes);


    VkSurfaceFormatKHR surface_format = surface_formats[0];
    for (u32 i = 0; i < surface_format_count; i++) {
        VkSurfaceFormatKHR format = surface_formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && 
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = format;
            break;
        }
    }
    VkFormat swapchain_image_format = surface_format.format;

    VkPresentModeKHR swap_surface_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < surface_present_mode_count; i++) {
        if (surface_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            swap_surface_present_mode = surface_present_modes[i];
            break;
        }
    }

    free(surface_present_modes);
    free(surface_formats);


    VkExtent2D swap_extent = {0};
    if (surface_capabilities.currentExtent.width != U32_MAX) {
        swap_extent = surface_capabilities.currentExtent;
    } else {
        i32 width, height;
        glfwGetFramebufferSize(window, &width, &height);

        swap_extent = (VkExtent2D) {
            .width = CLAMP(width,
                surface_capabilities.minImageExtent.width,
                surface_capabilities.maxImageExtent.width),

            .height = CLAMP(height,
                surface_capabilities.minImageExtent.height,
                surface_capabilities.maxImageExtent.height),
        };

    }

    u32 swap_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount != 0) {
        swap_image_count = MAX(swap_image_count, surface_capabilities.maxImageCount);
    }

    u32 queue_family_indices[] = { graphics_queue_index, present_queue_index };
    u32 *distinct_queue_family_indices = queue_family_indices;
    u32 queue_family_index_count = 2;
    VkSharingMode swapchain_sharing_mode = VK_SHARING_MODE_CONCURRENT;
    if (graphics_queue_index == present_queue_index) {
        swapchain_sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        queue_family_index_count = 0;
        distinct_queue_family_indices = NULL;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = swap_image_count,

        .imageFormat = swapchain_image_format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = swap_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = swapchain_sharing_mode,

        .queueFamilyIndexCount = queue_family_index_count,
        .pQueueFamilyIndices = distinct_queue_family_indices,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

        .presentMode = swap_surface_present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    VkSwapchainKHR swapchain;
    VK_CHECK(vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain));



    u32 swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, NULL);
    VkImage *swapchain_images = malloc(swapchain_image_count * sizeof(VkImage));
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images);

    VkImageView *swapchain_image_views = malloc(swapchain_image_count * sizeof(VkImageView));

    for (u32 i = 0; i < swapchain_image_count; i++) {
        VkImageViewCreateInfo image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain_image_format,
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

        VK_CHECK(vkCreateImageView(device, &image_view_create_info, NULL, &swapchain_image_views[i]));
    }
    free(swapchain_images);


    /* for (u32 i = 0; i < swapchain_image_count; i++) { */
    /*     vkDestroyImageView(device, swapchain_image_views[i], NULL); */
    /* } */

    /* vkDestroySwapchainKHR(device, swapchain, NULL); */
    /* vkDestroySurfaceKHR(instance, surface, NULL); */
    /* vkDestroyDevice(device, NULL); */
    /* vkDestroyInstance(instance, NULL); */
    /* glfwDestroyWindow(window); */
    /* glfwTerminate(); */

    return (VkContext) {
        .window = window,
        .instance = instance,
        .surface = surface,
        .physical_device = physical_device,
        .device = device,
        .graphics_queue = graphics_queue,
        .graphics_queue_index = graphics_queue_index,
        .present_queue = present_queue,
        .present_queue_index = present_queue_index,
        .swapchain = swapchain,
        .swapchain_image_format = swapchain_image_format,
        .swapchain_extent = swap_extent,
        .swapchain_image_count = swap_image_count,
        .swapchain_image_views = swapchain_image_views,
    };
}

void vulkan_context_destroy(VkContext *vc) {
    for (u32 i = 0; i < vc->swapchain_image_count; i++) {
        vkDestroyImageView(vc->device, vc->swapchain_image_views[i], NULL);
    }
    free(vc->swapchain_image_views);

    vkDestroySwapchainKHR(vc->device, vc->swapchain, NULL);
    vkDestroySurfaceKHR(vc->instance, vc->surface, NULL);
    vkDestroyDevice(vc->device, NULL);
    vkDestroyInstance(vc->instance, NULL);
    glfwDestroyWindow(vc->window);
    glfwTerminate();
}

typedef struct cwSPIRVBinary {
    u8 *data;
    u64 size;
} SPIRVBinary;

void free_spirv_binary(SPIRVBinary *buffer) {
    ASSERT(buffer->data);
    free(buffer->data);
}

SPIRVBinary load_spirv_binary(const char *path) {
    u8 *source = NULL;
    u64 bufsize = 0;
    // SET_BIN_MODE?
    FILE *fp = fopen(path, "rb");
    ASSERT_MSG(fp, "could not open file: %s", path);

    /* Go to the end of the file. */
    if (fseek(fp, 0L, SEEK_END) == 0) {

        bufsize = ftell(fp);
        ASSERT_MSG(bufsize != -1, "could not tell buffer size");

        /* Allocate our buffer to that size. */
        source = malloc(sizeof(char) * (bufsize + 1));
        ASSERT(source);
        memset(source, 0, bufsize + 1);

        /* Go back to the start of the file. */
        ASSERT(fseek(fp, 0L, SEEK_SET) == 0);

        /* read into memory */
        usize newLen = fread(source, sizeof(u8), bufsize, fp);

        ASSERT_MSG(ferror(fp) == 0, "Error reading file" );

        source[newLen++] = '\0';
    }

    fclose(fp);

    return (SPIRVBinary) {
        .data = source,
        .size = bufsize,
    };
}

VkShaderModule vk_create_shader_module(SPIRVBinary spv, VkContext *vc) {
    VkShaderModuleCreateInfo shader_module_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spv.size,
        .pCode = (u32 *)spv.data,
    };

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(vc->device, &shader_module_create_info, NULL, &shader_module));
    return shader_module;
}

void run() {

    VkContext vc = vulkan_context_init();

    SPIRVBinary vertex_bin = load_spirv_binary("../shaders/vert.spv");
    VkShaderModule vertex_module = vk_create_shader_module(vertex_bin, &vc);
    free_spirv_binary(&vertex_bin);

    SPIRVBinary fragment_bin = load_spirv_binary("../shaders/frag.spv");
    VkShaderModule fragment_module = vk_create_shader_module(fragment_bin, &vc);
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

    VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL,
    };

    VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (f32)vc.swapchain_extent.width,
        .height = (f32)vc.swapchain_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D viewport_scissor = {
        .offset = {0},
        .extent = vc.swapchain_extent,
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
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &viewport_scissor,
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
    VK_CHECK(vkCreatePipelineLayout(vc.device, &pipeline_layout_create_info, NULL, &pipeline_layout));



    VkAttachmentDescription color_attachment_desc = {
        .format = vc.swapchain_image_format,
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
    VkRenderPass render_pass;
    VK_CHECK(vkCreateRenderPass(vc.device, &renderpass_create_info, NULL, &render_pass));




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
        .renderPass = render_pass,
        .subpass = 0,
    };

    VkPipeline graphics_pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(vc.device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, NULL, &graphics_pipeline));


    VkFramebuffer *swapchain_framebuffers = malloc(vc.swapchain_image_count * sizeof(VkFramebuffer));

    for (u32 i = 0; i < vc.swapchain_image_count; i++) {

        VkFramebufferCreateInfo framebuffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = &vc.swapchain_image_views[i],
            .width = vc.swapchain_extent.width,
            .height = vc.swapchain_extent.height,
            .layers = 1,
        };

        VK_CHECK(vkCreateFramebuffer(vc.device, &framebuffer_create_info, NULL, &swapchain_framebuffers[i]));
    }



    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vc.graphics_queue_index,
    };
    VkCommandPool command_pool;
    VK_CHECK(vkCreateCommandPool(vc.device, &command_pool_create_info, NULL, &command_pool));



    VkCommandBufferAllocateInfo command_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer command_buffer;
    VK_CHECK(vkAllocateCommandBuffers(vc.device, &command_buffer_allocate_info, &command_buffer));


    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence in_flight_fence;
    VK_CHECK(vkCreateSemaphore(vc.device, &semaphore_create_info, NULL, &image_available_semaphore));
    VK_CHECK(vkCreateSemaphore(vc.device, &semaphore_create_info, NULL, &render_finished_semaphore));
    VK_CHECK(vkCreateFence(vc.device, &fence_create_info, NULL, &in_flight_fence));


    while (!glfwWindowShouldClose(vc.window)) {

        vkWaitForFences(vc.device, 1, &in_flight_fence, VK_TRUE, U64_MAX);
        vkResetFences(vc.device, 1, &in_flight_fence);

        u32 swapchain_image_index;
        vkAcquireNextImageKHR(vc.device, vc.swapchain, U64_MAX, image_available_semaphore, 
                VK_NULL_HANDLE, &swapchain_image_index);

        vkResetCommandBuffer(command_buffer, 0);
        

        VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));


        VkClearValue clear_value = {
            .color = { 0.0f, 0.0f, 0.0f, 1.0f }
        };

        VkRenderPassBeginInfo renderpass_begin_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = swapchain_framebuffers[swapchain_image_index],
            .renderArea = {
                .offset = {0},
                .extent = vc.swapchain_extent,
            },
            .clearValueCount = 1,
            .pClearValues = &clear_value,
        };


        vkCmdBeginRenderPass(command_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

        viewport = (VkViewport){
            .x = 0.0f,
                .y = 0.0f,
                .width = (float)vc.swapchain_extent.width,
                .height = (float)vc.swapchain_extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        viewport_scissor = (VkRect2D) {
            .offset = {0, 0},
                .extent = vc.swapchain_extent,
        };
        vkCmdSetScissor(command_buffer, 0, 1, &viewport_scissor);
        vkCmdDraw(command_buffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);
        VK_CHECK(vkEndCommandBuffer(command_buffer));


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

        VK_CHECK(vkQueueSubmit(vc.graphics_queue, 1, &submit_info, in_flight_fence));


        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &signal_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &vc.swapchain,
            .pImageIndices = &swapchain_image_index,
        };

        VK_CHECK(vkQueuePresentKHR(vc.present_queue, &present_info));

        glfwPollEvents();
    }


    vkDeviceWaitIdle(vc.device);

    vkDestroySemaphore(vc.device, image_available_semaphore, NULL);
    vkDestroySemaphore(vc.device, render_finished_semaphore, NULL);
    vkDestroyFence(vc.device, in_flight_fence, NULL);
    vkDestroyCommandPool(vc.device, command_pool, NULL);

    for (u32 i = 0; i < vc.swapchain_image_count; i++) {
        vkDestroyFramebuffer(vc.device, swapchain_framebuffers[i], NULL);
    }
    free(swapchain_framebuffers);

    vkDestroyPipeline(vc.device, graphics_pipeline, NULL);
    vkDestroyRenderPass(vc.device, render_pass, NULL);
    vkDestroyPipelineLayout(vc.device, pipeline_layout, NULL);
    vkDestroyShaderModule(vc.device, vertex_module, NULL);
    vkDestroyShaderModule(vc.device, fragment_module, NULL);


    vulkan_context_destroy(&vc);
}

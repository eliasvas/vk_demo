#include "stdio.h"
#include "tools.h"
#include "stdlib.h"
#include "string.h"

#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

/*
 -make win32 backend
 -swapchain resizing
 -rendering synchronization (fences)
 -depth buffering
 -vertex/index buffers
 -texture sampling
 -offscreen rendering
 -pipeline recreation
 */
GLFWwindow* window; //our GLFW window!

VkInstance instance; //connection between application and the vulkan library 

//[SPEC]: A single complete implementation of Vulkan available to the host.
VkPhysicalDevice physical_device = VK_NULL_HANDLE; //the Physical Device (the GPU Vulkan will use)

//[SPEC]: An instance of that implementation with its own state and resources independent of other logical devices.
VkDevice device; //the Logical Device

//[SPEC]: Creating a logical device also creates the queues associated with that device.
VkQueue graphics_queue; //handle to graphics queue (for submitting RENDER command buffers)
VkQueue present_queue; //handle to present queue


//[SPEC]: A Native platform surface or window object.
VkSurfaceKHR surface; //an abstract type of surface to present rendered images to

//[SPEC]: Provides the ability to present rendering results to a _surface_.
VkSwapchainKHR swapchain;//an abstraction for an array of presentable images associated with a surface

VkImage *swapchain_images;//handles to the images inside the swapchain!!
u32 swapchain_image_count;
VkFormat swapchain_image_format;
VkExtent2D swapchain_extent;
//[SPEC]: Contiguous range of image subresources + additional metadata for accessing ??
VkImageView *swapchain_image_views;
//[SPEC]: Framebuffers represent a collection of specific memory attachments that a render pass needs.
VkFramebuffer *swapchain_framebuffers; //check https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan


//[SPEC]: Shader modules contain shader code (in SPIR-V format) + entry point
VkShaderModule vert_shader_module;
VkShaderModule frag_shader_module;

//[SPEC]: A collection of attachments, subpasses, and dependencies between the subpasses
VkRenderPass render_pass; //we need to _begin_ a render pass at the start of rendering and _end_ before submitting to Queue

//[SPEC]: Acess to descriptor sets from a pipeline is accompished through a pipeline layout 
VkPipelineLayout pipeline_layout;

//[SPEC]: An object that encompasses the configuration of the entire GPU for the draw
VkPipeline graphics_pipeline;

//[SPEC]: Objects from which _command buffer memory_ is allocated from
VkCommandPool command_pool;

//[SPEC]: Object used to record commands which can then be submitted to a device queue for execution
VkCommandBuffer *command_buffers;

const int MAX_FRAMES_IN_FLIGHT = 2;
//[SPEC]: synchronization primitive that can be used to insert a dependency between queue operations/host
VkSemaphore *image_available_semaphores;
VkSemaphore *render_finished_semaphores;
//[SPEC]: synchronization primitive that can be used to insert a dependency from a queue to the host
VkFence *in_flight_fences;
VkFence *images_in_flight;

//[SPEC]: Linear array of data which are used for various purposes by binding them to a pipeline via descriptor sets 
VkBuffer vertex_buffer;
//[SPEC]: A Vulkan device operates on data in device memory via memory objects
VkDeviceMemory vertex_buffer_memory;
VkBuffer index_buffer;
VkDeviceMemory index_buffer_memory;

global current_frame = 0;
global framebuffer_resized = FALSE;

const char *validation_layers[]= {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
    const u32 enable_validation_layers= TRUE;
#else
    const u32 enable_validation_layers = TRUE;
#endif

const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; 

typedef struct TestVertex
{
    vec2 pos;
    vec3 color;
}TestVertex;

TestVertex vertices[] = {
    {{-0.5f, -0.5f},{0.0f, 0.3f, 1.0f}},
    {{0.5f, -0.5f},{0.7f, 0.2f, 0.0f}},
    {{0.5f, 0.5f},{1.0f, 0.8f, 0.1f}},
    {{-0.5f, 0.5f},{0.0f, 0.1f, 1.0f}},
};
u16 indices[] = {0,1,2,2,3,0};

//return the binding description of TestVertex type vertex input
internal VkVertexInputBindingDescription get_bind_desc_test_vert(void)
{
    VkVertexInputBindingDescription bind_desc = {0};
    bind_desc.binding = 0;
    bind_desc.stride = sizeof(TestVertex);
    bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//per-vertex
    return bind_desc;
}

internal void get_attr_desc_test_vert(VkVertexInputAttributeDescription *attr_desc)
{
    attr_desc[0].binding = 0;
    attr_desc[0].location = 0;
    attr_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attr_desc[0].offset = offsetof(TestVertex, pos);
    attr_desc[1].binding = 0;
    attr_desc[1].location = 1;
    attr_desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_desc[1].offset = offsetof(TestVertex, color);
}

typedef struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    u32 format_count;
    VkPresentModeKHR *present_modes;
    u32 present_mode_count;
}SwapChainSupportDetails;


internal SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device)
{
    SwapChainSupportDetails details = {0};

    //[0]: first we query the swapchain capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    //[1]: then we query the swapchain available formats
    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, NULL);
    details.format_count = format_count;

    if (format_count != 0)
    {
        details.formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats);
    }
    //[2]: finally we query available presentation modes
    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, NULL);
    details.present_mode_count = present_mode_count;

    if (present_mode_count != 0)
    {
        details.present_modes = malloc(sizeof(VkPresentModeKHR) * present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes);
    }


    return details;
}

internal u32 check_validation_layer_support(void){
	u32 layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, NULL);

	VkLayerProperties *available_layers = malloc(sizeof(VkLayerProperties) * layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
	for (u32 i = 0; i < array_count(validation_layers); ++i)
	{
		u32 layer_found = 0;
		for (u32 j = 0; j < layer_count; ++j)
		{
			if (strcmp(validation_layers[i], available_layers[j].layerName) == 0)
			{
				layer_found = TRUE;
				break;
			}
		}
		if (layer_found == 0)
			return FALSE;
	}
	return TRUE;
}

internal void vk_error(char *text)
{
	printf("%s\n",text);
}

internal void create_instance(void) {
	if (enable_validation_layers&&(check_validation_layer_support()==0))
		vk_error("Validation layers requested, but not available!");
	VkApplicationInfo appinfo = {0};
	appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appinfo.pApplicationName = "Vulkan Sample";
	appinfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
	appinfo.pEngineName = "Hive3D";
	appinfo.engineVersion = VK_MAKE_VERSION(1,0,0);
	appinfo.apiVersion = VK_API_VERSION_1_0;


	VkInstanceCreateInfo create_info = {0};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &appinfo;


	u32 glfw_ext_count = 0;
	const char **glfw_extensions; //array of strings with names of VK extensions needed!

	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

	create_info.enabledExtensionCount = glfw_ext_count;
	create_info.ppEnabledExtensionNames = glfw_extensions;
	create_info.enabledLayerCount = 0;

	VkResult res = vkCreateInstance(&create_info, NULL, &instance);

	if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS)
		vk_error("Failed to create Instance!");
	

	//(OPTIONAL): extension support
	u32 ext_count = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);
	VkExtensionProperties *extensions = malloc(sizeof(VkExtensionProperties) * ext_count);
	vkEnumerateInstanceExtensionProperties(NULL, &ext_count, extensions);
    /*
	for (u32 i = 0; i < ext_count; ++i)
		printf("EXT: %s\n", extensions[i]);
    */
}



typedef struct QueueFamilyIndices
{
    u32 graphics_family;
    //because we cant ifer whether the vfalue was initialized correctly or is garbage
    u32 graphics_family_found;

    u32 present_family;
    u32 present_family_found;
}QueueFamilyIndices;

internal QueueFamilyIndices find_queue_families(VkPhysicalDevice device)
{ 
    QueueFamilyIndices indices;

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

    VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);
    VkBool32 present_support = FALSE;
    for (u32 i = 0; i < queue_family_count; ++i)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support){indices.present_family = i; indices.present_family_found = TRUE;}

        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) //the queue family supports graphics ops
        {
            indices.graphics_family = i;
            indices.graphics_family_found = TRUE;
        }
    }

    return indices;
}

internal u32 check_device_extension_support(VkPhysicalDevice device)
{
    u32 ext_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, NULL);
    VkExtensionProperties *available_extensions = malloc(sizeof(VkExtensionProperties) * ext_count);
    
    vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, available_extensions);
    for (u32 i = 0; i < array_count(device_extensions); ++i)
    {
        u32 ext_found = FALSE;
        for (u32 j = 0; j < ext_count; ++j)
        {
            if (strcmp(device_extensions[i], available_extensions[j].extensionName)==0)
            {
                ext_found = TRUE;
                break;
            }
        }
        if (ext_found == FALSE)return FALSE;
    }
    return TRUE;
}

internal u32 is_device_suitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = find_queue_families(device);

    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);

    u32 extensions_supported = check_device_extension_support(device);

    SwapChainSupportDetails swapchain_support = query_swapchain_support(device);

    //here we can add more requirements for physical device selection
    return indices.graphics_family_found && extensions_supported && 
        (swapchain_support.format_count > 0) && (swapchain_support.present_mode_count > 0);
}

internal void pick_physical_device(void) {
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if (device_count == 0)
        vk_error("Failed to find GPUs with Vulkan support!");

    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    for (u32 i = 0; i < device_count; ++i)
        if (is_device_suitable(devices[i]))
        {
            physical_device = devices[i];
            break;
        }

}

internal VkSurfaceFormatKHR choose_swap_surface_format(SwapChainSupportDetails details)
{
    for (u32 i = 0; i < details.format_count; ++i)
    {
        if (details.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB  &&
                details.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return details.formats[0];
        }
    }
    return details.formats[0];
}
internal VkPresentModeKHR choose_swap_present_mode(SwapChainSupportDetails details)
{
    return VK_PRESENT_MODE_FIFO_KHR;
}

internal VkExtent2D choose_swap_extent(SwapChainSupportDetails details)
{
    if (details.capabilities.currentExtent.width != UINT32_MAX)
    {
        return details.capabilities.currentExtent;
    }
    else
    {
        u32 width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actual_extent = {width, height};
       actual_extent.height=maximum(details.capabilities.maxImageExtent.height,
           minimum(details.capabilities.minImageExtent.height,actual_extent.height));
       actual_extent.width=maximum(details.capabilities.maxImageExtent.width,
           minimum(details.capabilities.minImageExtent.width,actual_extent.width));
        return actual_extent;
    }
}

internal void create_swapchain(void)
{
    SwapChainSupportDetails swapchain_support = query_swapchain_support(physical_device);

    VkSurfaceFormatKHR surface_format =choose_swap_surface_format(swapchain_support);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support);
    VkExtent2D extent = choose_swap_extent(swapchain_support);

    u32 image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount)
        image_count = swapchain_support.capabilities.maxImageCount;


    VkSwapchainCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface; //we specify which surface the swapchain is tied to

    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    //VK_IMAGE_USAGE_TRANSFER_DST_BIT + memop for offscreen rendering!
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; 

    QueueFamilyIndices indices = find_queue_families(physical_device);
    u32 queue_family_indices[] = {indices.graphics_family, indices.present_family};

    if (indices.graphics_family != indices.present_family)
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = NULL;
    }
    create_info.preTransform = swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &create_info, NULL, &swapchain) != VK_SUCCESS)
        vk_error("Failed to create swapchain!");

    vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);
    swapchain_images = malloc(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images);
    swapchain_image_format = surface_format.format;
    swapchain_extent = extent;
    swapchain_image_count = image_count;//TODO(ilias): check
}

internal void create_image_views(void)
{
    swapchain_image_views = malloc(sizeof(VkImageView) * swapchain_image_count);
    for (u32 i = 0; i < swapchain_image_count; ++i)
    {
        VkImageViewCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = swapchain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D; // 1D/2D/3D textures
        create_info.format = swapchain_image_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &create_info, NULL, &swapchain_image_views[i]) != VK_SUCCESS)
            vk_error("Failed to create image views!!");
    }
}

//Queue initialization is a little weird,TODO(ilias): fix when possible
internal void create_logical_device(void)
{
    QueueFamilyIndices indices = find_queue_families(physical_device);

    f32 queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info[2] = {0};

    queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info[0].queueFamilyIndex = indices.graphics_family;
    queue_create_info[0].queueCount = 1;
    queue_create_info[0].pQueuePriorities = &queue_priority;
    queue_create_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info[1].queueFamilyIndex = indices.present_family;
    queue_create_info[1].queueCount = 1;
    queue_create_info[1].pQueuePriorities = &queue_priority;
    VkPhysicalDeviceFeatures device_features = {0};

    VkDeviceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    create_info.pQueueCreateInfos = &queue_create_info[0];
    create_info.queueCreateInfoCount = (indices.graphics_family != indices.present_family) ? 2 : 1;
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = array_count(device_extensions);
    create_info.ppEnabledExtensionNames = &device_extensions[0];
    //printf("graphics fam: %i, present fam: %i, createinfocount: %i\n", 
    //       indices.graphics_family, indices.present_family, create_info.queueCreateInfoCount);
    if (enable_validation_layers)
    {
        create_info.enabledLayerCount = array_count(validation_layers);
        create_info.ppEnabledLayerNames = validation_layers;
    }
    else
        create_info.enabledLayerCount = 0;

    if (vkCreateDevice(physical_device, &create_info, NULL, &device) != VK_SUCCESS)
        vk_error("Failed to create logical device!!");


    vkGetDeviceQueue(device, indices.graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, indices.present_family, 0, &present_queue);
}

internal VkShaderModule create_shader_module(char *code, u32 size)
{
	VkShaderModuleCreateInfo create_info = {0};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = size;
	create_info.pCode = (u32*)code;
	VkShaderModule shader_module;
	if (vkCreateShaderModule(device, &create_info, NULL, &shader_module) != VK_SUCCESS)
		vk_error("Error creating some shader!");
	return shader_module;
}

internal void create_graphics_pipeline(void)
{
    //---SHADERS---
	
	//read the spirv files as binary files
	u32 vert_size, frag_size;
	char *vert_shader_code = read_whole_file_binary("vert.spv", &vert_size);
	//char *vert_shader_code = read_whole_file_binary("fullscreen.spv", &vert_size);
	char *frag_shader_code = read_whole_file_binary("frag.spv", &frag_size);
	
	//make shader modules out of those
	vert_shader_module = create_shader_module(vert_shader_code, vert_size);
	frag_shader_module = create_shader_module(frag_shader_code, frag_size);
	
    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {0};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {0};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";
    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

    //---VERTEX INPUT---
    VkVertexInputBindingDescription bind_desc = get_bind_desc_test_vert();
    VkVertexInputAttributeDescription attr_desc[2];
    get_attr_desc_test_vert(attr_desc);

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;//we have 1 vertex description 
    vertex_input_info.pVertexBindingDescriptions = &bind_desc;
    vertex_input_info.vertexAttributeDescriptionCount = 2; //with 2 different attributes
    vertex_input_info.pVertexAttributeDescriptions = attr_desc;

    //---INPUT ASSEMBLY---
    VkPipelineInputAssemblyStateCreateInfo input_asm_info = {0};
    input_asm_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_asm_info.primitiveRestartEnable = VK_FALSE;

    //---VIEWPORT---
    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (f32)swapchain_extent.width;
    viewport.height = (f32)swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {0};
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_state_info = {0};
    viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_info.viewportCount = 1;
    viewport_state_info.pViewports = &viewport;
    viewport_state_info.scissorCount = 1;
    viewport_state_info.pScissors = &scissor;

    //---RASTERIZER---
    VkPipelineRasterizationStateCreateInfo rasterizer_info = {0};
    rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_info.depthClampEnable = VK_FALSE; //if VK_TRUE, beyond near and far planes fragments CLAMPED 
    rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_info.lineWidth = 1.0f;
    rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer_info.depthBiasEnable = VK_FALSE;
    rasterizer_info.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer_info.depthBiasClamp = 0.0f; // Optional
    rasterizer_info.depthBiasSlopeFactor = 0.0f; // Optional

    //---MULTISAMPLING---
    VkPipelineMultisampleStateCreateInfo multisampling_info = {0};
    multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_info.sampleShadingEnable = VK_FALSE;
    multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling_info.minSampleShading = 1.0f;
    multisampling_info.pSampleMask = NULL;
    multisampling_info.alphaToCoverageEnable = VK_FALSE;
    multisampling_info.alphaToOneEnable = VK_FALSE;

    //---COLOR BLENDING--- (NO BLENDING RIGHT NOW)
    VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp= VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending_info = {0};
    color_blending_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending_info.logicOpEnable = VK_FALSE;
    color_blending_info.logicOp = VK_LOGIC_OP_COPY;
    color_blending_info.attachmentCount = 1;
    color_blending_info.pAttachments = &color_blend_attachment;
    color_blending_info.blendConstants[0] = 0.0f;
    color_blending_info.blendConstants[1] = 0.0f;
    color_blending_info.blendConstants[2] = 0.0f;
    color_blending_info.blendConstants[3] = 0.0f;

    //---DYNAMIC STATE---
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH};
    VkPipelineDynamicStateCreateInfo dynamic_state = {0};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    ///---PIPELINE LAYOUT--- (for uniform and push values referenced by shaders)
    VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pSetLayouts = NULL;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = NULL;
    if (vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout) != VK_SUCCESS)
        vk_error("Error creating pipeline!");

    //---GRAPHICS PIPELINE CREATION---
    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    //[0]: define programmable shaders
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    //[1]: define fixed function stages
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_asm_info;
    pipeline_info.pViewportState = &viewport_state_info;
    pipeline_info.pRasterizationState = &rasterizer_info;
    pipeline_info.pMultisampleState = &multisampling_info;
    pipeline_info.pDepthStencilState = NULL;
    pipeline_info.pColorBlendState = &color_blending_info;
    pipeline_info.pDynamicState = NULL;
    //[2]: define pipeline layout
    pipeline_info.layout = pipeline_layout;
    //[3]: define the render pass
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0; //index of subpass where this pipeline will be used (the first and only rn)
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &graphics_pipeline)!=VK_SUCCESS)
        vk_error("Failed to create graphics pipeline!!");
}

internal void create_render_pass(void)
{
    VkAttachmentDescription color_attachment = {0};
    color_attachment.format = swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    //each subpass references one or more attachments through our descriptions above
    VkAttachmentReference color_attachment_ref = {0};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
     if (vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass)!=VK_SUCCESS)
        vk_error("Failed to create render pass!");

}

internal void create_framebuffers(void)
{
   swapchain_framebuffers = malloc(sizeof(VkFramebuffer) * swapchain_image_count); 
   for (u32 i = 0; i < swapchain_image_count; ++i)
   {
       VkImageView attachments[] = {swapchain_image_views[i]};
   
       VkFramebufferCreateInfo framebuffer_info = {0};
       framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
       framebuffer_info.renderPass = render_pass; //VkFramebuffers need a render pass?
       framebuffer_info.attachmentCount = 1;
       framebuffer_info.pAttachments = attachments;
       framebuffer_info.width = swapchain_extent.width;
       framebuffer_info.height = swapchain_extent.height;
       framebuffer_info.layers = 1;

       if (vkCreateFramebuffer(device, &framebuffer_info, NULL, &swapchain_framebuffers[i]) != VK_SUCCESS)
           vk_error("Error creating a framebuffer!");

   }

}
internal void create_command_pool(void)
{
    QueueFamilyIndices queue_family_indices = find_queue_families(physical_device);
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
    pool_info.flags = 0;
    if (vkCreateCommandPool(device, &pool_info, NULL, &command_pool) != VK_SUCCESS)
        vk_error("Failed to create command pool!");
}

internal void create_command_buffers(void)
{
    command_buffers = malloc(sizeof(VkCommandBuffer) * swapchain_image_count);
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool; //where to allocate the buffer from
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = swapchain_image_count;
    if (vkAllocateCommandBuffers(device, &alloc_info, command_buffers)!=VK_SUCCESS)
        vk_error("Failed to allocate command buffers!");

    //record the command buffers
    for(u32 i = 0; i < swapchain_image_count; ++i)
    {
        VkCommandBufferBeginInfo begin_info = {0};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = NULL;
        if (vkBeginCommandBuffer(command_buffers[i], &begin_info)!=VK_SUCCESS)
            vk_error("Gailed to begin recording command buffer!");


        VkRenderPassBeginInfo renderpass_info = {0};
        renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_info.renderPass = render_pass;
        renderpass_info.framebuffer = swapchain_framebuffers[i]; //we bind a _framebuffer_ to a render pass
        renderpass_info.renderArea.offset = (VkOffset2D){0,0};
        renderpass_info.renderArea.extent = swapchain_extent;
        VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderpass_info.clearValueCount = 1;
        renderpass_info.pClearValues = &clear_color;
        vkCmdBeginRenderPass(command_buffers[i], &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

        VkBuffer vertex_buffers[] = {vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(command_buffers[i], index_buffer, 0, VK_INDEX_TYPE_UINT16);

        vkCmdDrawIndexed(command_buffers[i], array_count(indices), 1, 0, 0, 0);
        vkCmdEndRenderPass(command_buffers[i]);
        if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS)
            vk_error("Failed to record command buffer!");
    }
}

internal u32 find_mem_type(u32 type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    for (u32 i = 0; i < mem_properties.memoryTypeCount; ++i)
    {
        if (type_filter & (1 << i) && 
                (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    vk_error("Failed to find suitable memory type!");
}
internal void create_vertex_buffer(void)
{
    VkBufferCreateInfo buffer_info = {0};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(vertices[0]) * array_count(vertices);
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, NULL, &vertex_buffer)!=VK_SUCCESS)
        vk_error("Failed to create vertex buffer!");

    VkMemoryRequirements mem_req = {0};
    vkGetBufferMemoryRequirements(device, vertex_buffer, &mem_req);

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_mem_type(mem_req.memoryTypeBits, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &alloc_info, NULL, &vertex_buffer_memory)!=VK_SUCCESS)
        vk_error("Failed to allocate vertex buffer memory!");
    vkBindBufferMemory(device, vertex_buffer, vertex_buffer_memory, 0);

    void *data;
    vkMapMemory(device, vertex_buffer_memory, 0, buffer_info.size, 0, &data);
    memcpy(data, vertices, buffer_info.size);
    vkUnmapMemory(device, vertex_buffer_memory);
}
internal void create_index_buffer(void)
{
    VkBufferCreateInfo buffer_info = {0};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(indices[0]) * array_count(indices);
    buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, NULL, &index_buffer)!=VK_SUCCESS)
        vk_error("Failed to create index buffer!");

    VkMemoryRequirements mem_req = {0};
    vkGetBufferMemoryRequirements(device, index_buffer, &mem_req);

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_mem_type(mem_req.memoryTypeBits, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &alloc_info, NULL, &index_buffer_memory)!=VK_SUCCESS)
        vk_error("Failed to allocate index buffer memory!");
    vkBindBufferMemory(device, index_buffer, index_buffer_memory, 0);

    void *data;
    vkMapMemory(device, index_buffer_memory, 0, buffer_info.size, 0, &data);
    memcpy(data, indices, buffer_info.size);
    vkUnmapMemory(device, index_buffer_memory);
}


internal void create_sync_objects(void)
{
    image_available_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    in_flight_fences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    images_in_flight = malloc(sizeof(VkFence) * swapchain_image_count);
    for (u32 i = 0; i < swapchain_image_count; ++i)images_in_flight[i] = VK_NULL_HANDLE;

    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (vkCreateSemaphore(device, &semaphore_info, NULL, &image_available_semaphores[i])!=VK_SUCCESS || 
                vkCreateSemaphore(device, &semaphore_info, NULL, &render_finished_semaphores[i])!=VK_SUCCESS ||
                vkCreateFence(device, &fence_info, NULL, &in_flight_fences[i]) != VK_SUCCESS)
            vk_error("Failed to create sync objects for some frame!");
    }
}

internal void create_surface(void)
{
    if (glfwCreateWindowSurface(instance, window, NULL, &surface)!=VK_SUCCESS)
        vk_error("Failed to create window surface!");
}

internal void framebuffer_resize_callback(void){framebuffer_resized = TRUE;}

internal void cleanup_swapchain(void)
{
    for (u32 i = 0; i < swapchain_image_count; ++i)
        vkDestroyFramebuffer(device, swapchain_framebuffers[i], NULL);
    //vkFreeCommandBuffers(device, command_pool, swapchain_image_count, command_buffers);
    vkDestroyPipeline(device, graphics_pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyRenderPass(device, render_pass, NULL);
    for (u32 i = 0; i < swapchain_image_count; ++i)
        vkDestroyImageView(device, swapchain_image_views[i], NULL);
    vkDestroySwapchainKHR(device, swapchain, NULL);
    //these will be recreated at pipeline creation for next swapchain
    vkDestroyShaderModule(device, vert_shader_module, NULL);
    vkDestroyShaderModule(device, frag_shader_module, NULL);
}

internal void recreate_swapchain(void)
{
    //in case of window minimization (w = 0, h = 0) we wait until we get a proper window again
    i32 width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    } 

    vkDeviceWaitIdle(device);
    cleanup_swapchain();
    create_swapchain();
    create_image_views();
    create_render_pass();
    create_graphics_pipeline();
    create_framebuffers();
    create_command_buffers();
}
internal int vulkan_init(void) {
	create_instance();
    create_surface();
	pick_physical_device();
    create_logical_device();
    create_command_pool();
    create_vertex_buffer();
    create_index_buffer();
    recreate_swapchain();
    create_sync_objects();
	return 1;
}

internal void draw_frame(void)
{
    vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
    //[0]: Acquire free image from the swapchain
    u32 image_index;
    VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, 
            image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
    if (res == VK_ERROR_OUT_OF_DATE_KHR){recreate_swapchain();return;}
    else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)vk_error("Failed to acquire swapchain image!");

    // check if the image is already used (in flight) froma previous frame, and if so wait
    if (images_in_flight[image_index] != VK_NULL_HANDLE)vkWaitForFences(device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX);
    //mark image as used by _this frame_
    images_in_flight[image_index] = in_flight_fences[current_frame];

    //[1]: Execute the command buffer with that image as attachment in the framebuffer
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[image_index];
    VkSemaphore signal_semaphores[] = {render_finished_semaphores[current_frame]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    vkResetFences(device, 1, &in_flight_fences[current_frame]);
    if (vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame])!=VK_SUCCESS)
        vk_error("Failed to submit draw command buffer!");
    //[2]: Return the image to the swapchain for presentation
    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains[] = {swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = NULL;

    //we push the data to be presented to the present queue
    res = vkQueuePresentKHR(present_queue, &present_info); 

    //recreate swapchain if necessary
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebuffer_resized)
    {
        framebuffer_resized = FALSE;
        recreate_swapchain();
    }
    else if (res != VK_SUCCESS)
        vk_error("Failed to present swapchain image!");

    current_frame = (current_frame+1) % MAX_FRAMES_IN_FLIGHT;
}

global f32 nb_frames = 0.0f;
global f32 current_time = 0.0f;
global f32 prev_time = 0.0f;
internal void calc_fps(void)
{
    f32 current_time = glfwGetTime();
    f32 delta = current_time - prev_time;
    nb_frames++;
    if ( delta >= 1.0f){
         f32 fps = nb_frames / delta;
         char frames[256];
         sprintf(frames, "FPS: %f", fps);
         glfwSetWindowTitle(window, frames);

         nb_frames = 0;
         prev_time = current_time;
    }
}
internal void main_loop(void) 
{
    
	while (!glfwWindowShouldClose(window)) {
        calc_fps();
		glfwPollEvents();
        draw_frame();
	}

    //vkDeviceWaitIdle(device);
}

internal void cleanup(void) 
{
    vkDeviceWaitIdle(device);  //so we dont close the window while commands are still being executed
    cleanup_swapchain();
    //vkDestroyBuffer(device, vertex_buffer, NULL); //validation layer? @check
    vkFreeMemory(device, vertex_buffer_memory, NULL);
    vkDestroyBuffer(device, vertex_buffer, NULL);

    vkFreeMemory(device, index_buffer_memory, NULL);
    vkDestroyBuffer(device, index_buffer, NULL); //validation layer? @check
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(device, render_finished_semaphores[i], NULL);
        vkDestroySemaphore(device, image_available_semaphores[i], NULL);
        vkDestroyFence(device, in_flight_fences[i], NULL);
    }
    vkDestroyCommandPool(device, command_pool, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
	glfwDestroyWindow(window);
	glfwTerminate();
}

int main(void) {
	//first we initialize window stuff (GLFW)
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(800, 600, "Vulkan Sample", NULL, NULL);
	glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);


	if(vulkan_init())
		printf("Vulkan OK");
	
	main_loop();
	
	cleanup();
	return 0;
}

#include "stdio.h"
#include "tools.h"
#include "stdlib.h"
#include "string.h"

#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

GLFWwindow* window; //our GLFW window!

VkInstance instance; //connection between application and the vulkan library
VkPhysicalDevice physical_device = VK_NULL_HANDLE; //the physical device
VkDevice device; //the logical device
VkQueue graphics_queue; //handle to graphics queue
VkQueue present_queue; //handle to present queue
VkSurfaceKHR surface; //an abstract type of surface to present rendered images to

VkSwapchainKHR swap_chain; //our swap chain ?? :)
VkSurfaceFormatKHR swapchain_format; //image format of the swapchain
VkExtent2D swapchain_extent; //extents of the swapchain
VkImage *swapchain_images; //all image attachments of our swap chain
u32 swapchain_image_count; //how many images there are in the swapchain
VkImageView *swapchain_image_views;

VkShaderModule vert_shader_module;
VkShaderModule frag_shader_module;
VkRenderPass render_pass;
VkDescriptorSetLayout descriptor_set_layout;
VkPipelineLayout pipeline_layout;
VkPipeline graphics_pipeline; //!!!

VkFramebuffer *swapchain_framebuffers;
VkCommandPool command_pool;
VkCommandBuffer *command_buffers;

VkSemaphore *image_available_semaphore;
VkSemaphore *render_finished_semaphore;
VkFence *in_flight_fences;
VkFence *image_in_flight; //whether an image from our swapchain is being rendered at that moment

VkBuffer vertex_buffer;
VkDeviceMemory vertex_buffer_memory;

VkBuffer index_buffer;
VkDeviceMemory index_buffer_memory;

VkBuffer *uniform_buffers;
VkDeviceMemory *uniform_buffers_memory;

VkDescriptorPool descriptor_pool;
VkDescriptorSet *descriptor_sets;


u32 current_frame = 0;
const int MAX_FRAMES_IN_FLIGHT = 2;

u32 framebuffer_resized = 0;

const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};


typedef struct Vert
{
    vec2 pos;
    vec3 color;
}Vert;

internal const Vert vertices[] = {
    {{-0.5f,-0.5f},{0.2f,0.0f,1.0f}},
    {{0.5f, -0.5f},{1.0f,0.2f,0.0f}},
    {{-0.5f,0.5f},{0.2f,0.0f,1.0f}},
    {{0.5f,  0.5f},{0.0f,1.0f,0.2f}},
};
internal const u32 indices[] = {0,1,2,2,1,3};

typedef struct UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
}UniformBufferObject;

internal VkVertexInputBindingDescription get_binding_desc(void)
{
    VkVertexInputBindingDescription binding_desc;

    binding_desc.binding = 0;
    binding_desc.stride = sizeof(Vert);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding_desc;
}

//gets attribute descriptions for 'Vert' type vertices
internal void vert_get_attrib_descs(VkVertexInputAttributeDescription *attribs) //size 2
{
    attribs[0].binding = 0;
    attribs[0].location = 0;
    attribs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribs[0].offset = offsetof(Vert, pos);//nice macro

    attribs[1].binding = 0;
    attribs[1].location = 1;
    attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribs[1].offset = offsetof(Vert, color); 
}

#define VALIDATION_LAYERS_COUNT 1
const char *validation_layers[VALIDATION_LAYERS_COUNT] = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
	const u32 enable_validation_layers = FALSE;
#else
	const u32 enable_validation_layers = TRUE;
#endif

//checks if all requested layers are available
u32 check_validation_layer_support()
{
	
	u32 layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, NULL);
	
	VkLayerProperties *available_layers = malloc(sizeof(VkLayerProperties) * layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
	
	//we check that all layers in validation layers exist in the available layers list
	for (u32 i = 0; i < VALIDATION_LAYERS_COUNT; ++i)
	{
		u32 layer_found = FALSE;
		for (u32 j = 0; j< layer_count; ++j)
		{
			if (strcmp(available_layers[j].layerName, validation_layers[i]) == 0)
			{
				layer_found = TRUE;
				break;
			}
		}
		if (layer_found == FALSE)
			return FALSE;
	}
	if (available_layers!=NULL)free(available_layers);
	return TRUE;
}


void create_instance() {
	VkApplicationInfo app_info = {0};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Hello Vulkan!";
	app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1,0,0);
	app_info.apiVersion = VK_API_VERSION_1_0;
	
	VkInstanceCreateInfo create_info = {0};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	
	u32 glfw_extension_count = 0;
	const char **glfw_extensions;
	
	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	create_info.enabledExtensionCount = glfw_extension_count;
	create_info.ppEnabledExtensionNames = glfw_extensions;
	create_info.enabledLayerCount = 0;
	
	
	/*
	//print all supported extensions
	u32 extension_count = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
	VkExtensionProperties *supported_extensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * extension_count);
	vkEnumerateInstanceExtensionProperties(NULL, &extension_count, supported_extensions);
	printf("Available extensions: \n");
	for (u32 i = 0; i < extension_count; ++i)
		printf("[%i]: %s\n",i, supported_extensions[i].extensionName);
	*/
	
	
	
	//we check to see if the validation layers we request are available (its mainly for debugging stuff)
	if (enable_validation_layers && (check_validation_layer_support() == FALSE)) {
        printf("validation layers requested, but not available!\n");
    }
	
	
	if (enable_validation_layers)
	{
		create_info.enabledLayerCount = VALIDATION_LAYERS_COUNT;
		create_info.ppEnabledLayerNames = validation_layers;
		
	}
	else
	{
		create_info.enabledLayerCount = 0; //in release builds we have no validation layers!
	}
	
	
	
	
	VkResult res = vkCreateInstance(&create_info, NULL, &instance);
	if (res != VK_SUCCESS)
	{
		printf("Uh-Oh, error creating Vulkan Instance!\n");
		return;
	}
}

typedef struct QueueFamilyIndices
{
	u32 graphics_family;
	u32 present_family;
}QueueFamilyIndices;

internal QueueFamilyIndices find_queue_families(VkPhysicalDevice device)
{
	QueueFamilyIndices indices = (QueueFamilyIndices){INT_MAX, INT_MAX};
	
	u32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
	
	VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);
	//we are looking for a queue family that has GRAPHICS support ?? idk
	
	for (u32 i = 0; i < queue_family_count; ++i)
	{
		if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)indices.graphics_family = i;
		VkBool32 present_support = FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
		if (present_support)indices.present_family = i;
		if (indices.graphics_family != INT_MAX && indices.present_family != INT_MAX)break;//@check
	}
	if (queue_families != NULL)free(queue_families);
	return indices;
}

internal b32 check_device_extension_support(VkPhysicalDevice device)
{	
	u32 extension_count;
	vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
	
	VkExtensionProperties *available_extensions = malloc(sizeof(VkExtensionProperties) * extension_count);
	vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);
	
	u32 supported_extensions = 0;
	for (u32 i = 0; i< array_count(device_extensions);++i)
	{
		for (u32 j = 0; j < extension_count; ++j)
		{
			if (strcmp(available_extensions[j].extensionName, device_extensions[i]))
			{
				supported_extensions++;
				break;
			}
		}
	}
	
	if (available_extensions != NULL)free(available_extensions);
	return (supported_extensions == array_count(device_extensions));
}

typedef struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR *formats;
	u32 formats_count;
	VkPresentModeKHR *present_modes;
	u32 present_modes_count;
}SwapChainSupportDetails;

internal VkSurfaceFormatKHR choose_swapchain_surface_format(SwapChainSupportDetails *details)
{
	VkSurfaceFormatKHR *available_formats = details->formats;
	u32 formats_count = details->formats_count;
	
	for (u32 i = 0; i < formats_count; ++i)
	{
		if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return available_formats[i];
	}
	
	return available_formats[0]; //if we don't find the format we are looking for, (RGBA32/SRGB) we just pick the first one :P
}

internal VkPresentModeKHR choose_swapchain_present_mode(SwapChainSupportDetails *details)
{
	VkPresentModeKHR *available_present_modes = details->present_modes;
	u32 present_modes_count = details->present_modes_count;
	
	
	//check to see if we can use mailbox (triple buffering)
	for (u32 i = 0; i < present_modes_count; ++i)
	{
		if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			return available_present_modes[i];
	}
	
	//if not just use plain old FIFO (vsync kinda?)
	return VK_PRESENT_MODE_FIFO_KHR;
}

internal VkExtent2D choose_swap_extent(SwapChainSupportDetails *details)
{
	VkSurfaceCapabilitiesKHR capabilities = details->capabilities;
	
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		i32 width, height;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D actual_extent = {(u32)width, (u32)height};
		
		actual_extent.width = clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actual_extent.height = clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return actual_extent;
	}
}



internal SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device)
{
	SwapChainSupportDetails details = {0};
	
	//get all supported capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
	
	//get all supported formats
	u32 format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, NULL);
	
	if (format_count != 0)
	{
		details.formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats);
		details.formats_count = format_count;
	}
	
	//get all supported presentation modes
	u32 present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, NULL);
	
	if (present_mode_count != 0)
	{
		details.present_modes = malloc(sizeof(VkPresentModeKHR) * present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes);
		details.present_modes_count = present_mode_count;
	}
	
	
	return details;
}

internal u32 device_is_suitable(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(device, &device_properties);
	
	VkPhysicalDeviceFeatures device_features;
	vkGetPhysicalDeviceFeatures(device, &device_features);
	
	QueueFamilyIndices indices = find_queue_families(device);
	
	b32 extensions_supported = check_device_extension_support(device); //we need a VK_KHR_SWAPCHAIN_EXTENSION_NAME
	//if (extensions_supported)printf("All needed extensions supported! Yay!!\n");
	
	b32 swapchain_adequate = FALSE;
	if (extensions_supported)
	{
		SwapChainSupportDetails swapchain_support = query_swap_chain_support(device);
		swapchain_adequate = (swapchain_support.formats_count > 0 && swapchain_support.present_modes_count > 0);
	}
	//if(swapchain_adequate)printf("Swapchain is OK!\n");
	
	//return device_features.geometryShader; //thats how we can pick GPUs with certain features over others!!
	return ((indices.graphics_family != INT_MAX && indices.present_family != INT_MAX) && extensions_supported && swapchain_adequate); //because indexes can't reach that high! :)
}


internal void pick_physical_device()
{
	u32 device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, NULL);
	if (device_count == 0)
	{
		printf("Failed to find Vulkan capable GPUs!\n");
		return;
	}
	VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices);
	for (u32 i = 0; i < device_count; ++i)
	{
		if (device_is_suitable(devices[i]))
		{
			physical_device = devices[i];
			break;
		}
	}
	if (physical_device == VK_NULL_HANDLE)printf("Failed to find a suitable GPU!\n");
	if (devices != NULL)free(devices);
}

internal void create_logical_device()
{
	QueueFamilyIndices indices = find_queue_families(physical_device);
	
	VkDeviceQueueCreateInfo queue_create_infos[2] = {0};
	u32 queues_count = 1;
	if (indices.graphics_family != indices.present_family)queues_count = 2;
	
	
	//-------------@CHECK------------------
	for (u32 i = 0; i < queues_count; ++i)
	{
		VkDeviceQueueCreateInfo *q = &queue_create_infos[i];
		q->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		q->queueFamilyIndex = indices.graphics_family * (i == 0) + indices.present_family * (i == 1);//TODO: FIX, this is garbage
		q->queueCount = 1;
		f32 queue_priority = 1.0f;
		q->pQueuePriorities = &queue_priority;
	}
	queue_create_infos[0].queueFamilyIndex = indices.graphics_family;
	queue_create_infos[1].queueFamilyIndex = indices.present_family; //just to be sure!
	
	
	VkPhysicalDeviceFeatures device_features = {0};
	
	VkDeviceCreateInfo create_info = {0};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = &queue_create_infos;
	create_info.queueCreateInfoCount = queues_count;
	create_info.pEnabledFeatures = &device_features;
	
	create_info.enabledExtensionCount = (u32)array_count(device_extensions);
	create_info.ppEnabledExtensionNames = device_extensions;
	
	if (enable_validation_layers)
	{
		create_info.enabledLayerCount = VALIDATION_LAYERS_COUNT;
		create_info.ppEnabledLayerNames = validation_layers;
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}
	if (vkCreateDevice(physical_device, &create_info, NULL, &device) != VK_SUCCESS)
		printf("Error creating the logical device!\n");
	
	vkGetDeviceQueue(device, indices.graphics_family, 0, &graphics_queue);
	vkGetDeviceQueue(device, indices.present_family, 0, &present_queue);
}


internal u32 create_surface()
{
	if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS) {
        printf("failed to create window surface! :(\n");
		return FALSE;
    }
	return TRUE;
}

internal void create_swapchain()
{
	SwapChainSupportDetails swapchain_support = query_swap_chain_support(physical_device);
	
	VkSurfaceFormatKHR surface_format = choose_swapchain_surface_format(&swapchain_support);
	VkPresentModeKHR present_mode = choose_swapchain_present_mode(&swapchain_support);
	VkExtent2D extent = choose_swap_extent(&swapchain_support);
	
	//number of images in the swapchain
	u32 image_count = swapchain_support.capabilities.minImageCount + 1;
	if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount)
		image_count = swapchain_support.capabilities.maxImageCount;
	
	VkSwapchainCreateInfoKHR create_info = {0};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageExtent = extent;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	
	
	QueueFamilyIndices indices = find_queue_families(physical_device);
	u32 queue_family_indices[] = {indices.graphics_family, indices.present_family};
	
	if (queue_family_indices[0] != queue_family_indices[1])
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
	create_info.clipped = TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(device, &create_info, NULL, &swap_chain) != VK_SUCCESS)
		printf("Uh-Oh! Error creating the Swap Chain!\n");
	
	//retrive the handles with the images of the swap chain
	vkGetSwapchainImagesKHR(device, swap_chain, &swapchain_image_count, NULL);
	swapchain_images = malloc(sizeof(VkImage) * swapchain_image_count);
	vkGetSwapchainImagesKHR(device, swap_chain, &swapchain_image_count, swapchain_images);
	
	//these are globals!
	swapchain_format = surface_format;
	swapchain_extent = extent;
}

internal void create_image_views()
{
	swapchain_image_views = malloc(sizeof(VkImageView) * swapchain_image_count);
	for (u32 i = 0; i < swapchain_image_count; ++i)
	{
		VkImageViewCreateInfo create_info = {0};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = swapchain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = swapchain_format.format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;
		if (vkCreateImageView(device, &create_info, NULL, &swapchain_image_views[i])!= VK_SUCCESS)
			printf("Error: Couldn't Create Image views!\n");
	}
}

VkShaderModule create_shader_module(char *code, u32 size)
{
	VkShaderModuleCreateInfo create_info = {0};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = size;
	create_info.pCode = (u32*)code;
	VkShaderModule shader_module;
	if (vkCreateShaderModule(device, &create_info, NULL, &shader_module) != VK_SUCCESS)
		printf("Error creating some shader!\n");
	return shader_module;
}

internal void create_descriptor_sets()
{ //in our case we make one descriptor set for each swapchain image with the same layout
    VkDescriptorSetLayout *layouts=malloc(sizeof(VkDescriptorSetLayout)*swapchain_image_count);
    for (u32 i = 0; i < swapchain_image_count; ++i)
        layouts[i] = descriptor_set_layout;

    VkDescriptorSetAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = swapchain_image_count;
    alloc_info.pSetLayouts = layouts;

    descriptor_sets = malloc(sizeof(VkDescriptorSet) * swapchain_image_count);
    if (vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets)!=VK_SUCCESS)
        printf("Failed to allocate descriptor sets!\n");

    for(u32 i = 0; i < swapchain_image_count; ++i)
    {
        //we declare the data we want to pass to the descriptor
        VkDescriptorBufferInfo buf_info = {0};
        buf_info.buffer = uniform_buffers[i];
        buf_info.offset = 0;
        buf_info.range = sizeof(UniformBufferObject);
        
        //we update the descriptors contents
        VkWriteDescriptorSet descriptor_write = {0};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_sets[i];
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &buf_info;
        descriptor_write.pImageInfo = NULL;
        descriptor_write.pTexelBufferView = NULL;

        vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, NULL);
    }

}

internal void create_descriptor_pool()
{
    VkDescriptorPoolSize pool_size = {0};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = swapchain_image_count;

    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = swapchain_image_count;
    if (vkCreateDescriptorPool(device, &pool_info, NULL, &descriptor_pool) != VK_SUCCESS)
        printf("Uh-Oh! Couldnt create descriptor pool!\n");
}

internal void create_descriptor_set_layout()
{
    //We make each binding (UBO) for our shaders
    VkDescriptorSetLayoutBinding ubo_layout_binding = {0};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    ubo_layout_binding.pImmutableSamplers = NULL;

    //We combine all descriptor bingins in a descriptor SET layout
    VkDescriptorSetLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &ubo_layout_binding;

    if (vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_set_layout)
           != VK_SUCCESS)
        printf("Failed to create descriptor set layout!\n");
}

internal void create_graphics_pipeline()
{
	
	//---SHADERS---
	
	//read the spirv files as binary files
	u32 vert_size, frag_size;
	char *vert_shader_code = read_whole_file_binary("vert.spv", &vert_size);
	char *frag_shader_code = read_whole_file_binary("frag.spv", &frag_size);
	
	//make shader modules out of those
	vert_shader_module = create_shader_module(vert_shader_code, vert_size);
	frag_shader_module = create_shader_module(frag_shader_code, frag_size);
	
	//make pipeline stages with each shader
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
	
	
    VkVertexInputBindingDescription binding_desc = get_binding_desc();
    VkVertexInputAttributeDescription attribs[2];
    vert_get_attrib_descs(attribs);
	
	//---VERTEX INPUT--- (+vertex buffer above)
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = 1;
	vertex_input_info.pVertexBindingDescriptions = &binding_desc;
	vertex_input_info.vertexAttributeDescriptionCount = array_count(attribs);
	vertex_input_info.pVertexAttributeDescriptions = &attribs;
	
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
	scissor.offset = (VkOffset2D){0,0};
	scissor.extent = swapchain_extent;
	
	VkPipelineViewportStateCreateInfo viewport_state = {0};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;
	
	//---Rasterizer---
	VkPipelineRasterizationStateCreateInfo rasterizer = {0};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;
	
	//---Multisampling---
	VkPipelineMultisampleStateCreateInfo multisampling = {0};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = NULL;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;
	
	//---Color Blending---
	VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT 
	|VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	
	VkPipelineColorBlendStateCreateInfo color_blending = {0};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.logicOp = VK_LOGIC_OP_COPY;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_blend_attachment;
	color_blending.blendConstants[0] = 0.0f;
	color_blending.blendConstants[1] = 0.0f;
	color_blending.blendConstants[2] = 0.0f;
	color_blending.blendConstants[3] = 0.0f;
	
	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};
	
	VkPipelineDynamicStateCreateInfo dynamic_state = {0};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 2;
	dynamic_state.pDynamicStates = dynamic_states;
	
	//---Pipeline Layout--- (for uniforms??)
	
	VkPipelineLayoutCreateInfo pipeline_layout_info ={0};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = NULL;
	
	if (vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout) != VK_SUCCESS)
		printf("Error createing Pipeline Layout!\n");
	
	VkGraphicsPipelineCreateInfo pipeline_info = {0};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = shader_stages;
	pipeline_info.pVertexInputState = &vertex_input_info;
	pipeline_info.pInputAssemblyState = &input_asm_info;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pDepthStencilState = NULL;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.pDynamicState = NULL;
	
	pipeline_info.layout = pipeline_layout;
	pipeline_info.renderPass = render_pass;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info,
	NULL, &graphics_pipeline)!=VK_SUCCESS)
		printf("Error creating the graphics pipeline!\n");
	
	
}

internal void create_render_pass()
{
	VkAttachmentDescription color_attachment = {0};
	color_attachment.format = swapchain_format.format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	
	VkAttachmentReference color_attachment_ref = {0};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	
	VkSubpassDescription subpass = {0};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	
	VkSubpassDependency dep = {0};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.srcAccessMask = 0;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	
	VkRenderPassCreateInfo render_pass_info = {0};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dep;
	
	if (vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass)!=VK_SUCCESS)
		printf("Error creating Render Pass!\n");
	
}


internal void create_framebuffers()
{
	swapchain_framebuffers = malloc(sizeof(VkFramebuffer) * swapchain_image_count);
	
	for (u32 i = 0; i< swapchain_image_count; ++i)
	{
		VkImageView attachments[] = {swapchain_image_views[i]};
		
		VkFramebufferCreateInfo framebuffer_info = {0};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = render_pass;
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = attachments;
		framebuffer_info.width = swapchain_extent.width;
		framebuffer_info.height = swapchain_extent.height;
		framebuffer_info.layers = 1;
		
		if (vkCreateFramebuffer(device, &framebuffer_info,NULL, 
		&swapchain_framebuffers[i])!=VK_SUCCESS)
			printf("Error creating framebuffers!!\m");
		
	}
}

internal u32 find_mem_type(u32 type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    u32 i;
    for(i = 0; i < mem_properties.memoryTypeCount; ++i)
    {
        if ( (type_filter & (1 << i)) && 
               ((mem_properties.memoryTypes[i].propertyFlags & properties)==properties))
            return i;
    }

    if (i == mem_properties.memoryTypeCount) //means we didn't find the mem prop we want
        printf("Failed to find suitable memory type!\n");

}

internal void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer *buf, VkDeviceMemory *buf_memory)
{
    VkBufferCreateInfo buffer_info = {0};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buffer_info, NULL, buf)!=VK_SUCCESS)
        printf("Failed to create buffer.\n");

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, *buf, &mem_req);

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_mem_type(mem_req.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &alloc_info, NULL, buf_memory)!=VK_SUCCESS)
        printf("Failed to allocate buffer memory!\n");

    vkBindBufferMemory(device, *buf, *buf_memory, 0);
}

internal void create_vertex_buffer()
{
    VkDeviceSize buf_size = sizeof(vertices);
    create_buffer(buf_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &vertex_buffer, &vertex_buffer_memory);
  
    void *vert_data;
    //we map vert_data to GPU memory 
    vkMapMemory(device, vertex_buffer_memory, 0, buf_size, 0, &vert_data);
    //we copy the vertex data to that memory 
    memcpy(vert_data, vertices, (u32)buf_size);
    //we unmap it
    vkUnmapMemory(device, vertex_buffer_memory);
}

internal void create_index_buffer()
{
    VkDeviceSize buf_size = sizeof(indices);
    create_buffer(buf_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &index_buffer, &index_buffer_memory);
  
    void *index_data;
    //we map vert_data to GPU memory 
    vkMapMemory(device, index_buffer_memory, 0, buf_size, 0, &index_data);
    //we copy the vertex data to that memory 
    memcpy(index_data, indices, (u32)buf_size);
    //we unmap it
    vkUnmapMemory(device, index_buffer_memory);
}

//We need to make one Uniform buffer per swapchain image (in our case 2-3)
internal void create_uniform_buffer()
{
    uniform_buffers = malloc(sizeof(VkBuffer) * swapchain_image_count);
    uniform_buffers_memory = malloc(sizeof(VkDeviceMemory) * swapchain_image_count);

    VkDeviceSize buf_size = sizeof(UniformBufferObject);

    for (u32 i = 0; i < swapchain_image_count; ++i)
    { 
        create_buffer(buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniform_buffers[i], &uniform_buffers_memory[i]);
    }

}

internal void create_command_pool()
{
	QueueFamilyIndices queue_family_indices = find_queue_families(physical_device);
	
	VkCommandPoolCreateInfo pool_info = {0};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
	pool_info.flags = 0;
	if (vkCreateCommandPool(device, &pool_info, NULL, &command_pool) !=VK_SUCCESS)
		printf("Error creating Command Pool!\n");
}

internal void create_command_buffers()
{
	command_buffers = malloc(sizeof(VkCommandBuffer) * swapchain_image_count);
	VkCommandBufferAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = (u32)swapchain_image_count;//??
	if(vkAllocateCommandBuffers(device, &alloc_info, command_buffers)!=VK_SUCCESS)
		printf("Error allocating Command buffers!\n");
	
	for (u32 i = 0; i < swapchain_image_count; ++i)
	{
		//[1]: We start recording a Command buffer
		VkCommandBufferBeginInfo begin_info = {0};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = 0;
		begin_info.pInheritanceInfo = NULL;
		if (vkBeginCommandBuffer(command_buffers[i], &begin_info)!=VK_SUCCESS)
			printf("Failed to begin recording command buffer!\n");
		
		VkRenderPassBeginInfo render_pass_info = {0};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = render_pass;
		render_pass_info.framebuffer = swapchain_framebuffers[i];
		render_pass_info.renderArea.offset = (VkOffset2D){0,0};
		render_pass_info.renderArea.extent = swapchain_extent;
		
		VkClearValue clear_color = {{{0,0,0,1}}};
		render_pass_info.clearValueCount = 1;
		render_pass_info.pClearValues = &clear_color;
		//[2]: We begin a render pass
		vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);//also clears screen
		//[3]: We bind our pipeline
		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
        //[4]: We bind our vertex/index buffers
        VkBuffer vertex_buffers[] = {vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(command_buffers[i], index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                pipeline_layout, 0, 1, &descriptor_sets[i], 0, NULL);
		//[5]: We draw our vertices
		vkCmdDrawIndexed(command_buffers[i], array_count(indices),1,0,0,0);
		//[6]: We end the render pass, and stop captuing the command buffer, we are done!
		vkCmdEndRenderPass(command_buffers[i]);//stores the image
		
		if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS)
			printf("Failed to record command buffer!\n");
	}
}

internal void create_sync_objects()
{
	//[1]: We create the semaphore arrays
	image_available_semaphore = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
	render_finished_semaphore = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
	
	VkSemaphoreCreateInfo semaphore_info = {0};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		if (vkCreateSemaphore(device, &semaphore_info, NULL, &image_available_semaphore[i])!=VK_SUCCESS ||
		vkCreateSemaphore(device, &semaphore_info,NULL, &render_finished_semaphore[i])!=VK_SUCCESS)
			printf("Error creating semaphores!\n");
	}
	
	//[2]: We create the Fence array
	in_flight_fences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
	image_in_flight = malloc(sizeof(VkFence) * swapchain_image_count);
	VkFenceCreateInfo fence_info = {0};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		if (vkCreateFence(device,&fence_info, NULL, &in_flight_fences[i])!=VK_SUCCESS)
			printf("Error creating a fence!\n");
	}
	for (u32 i = 0; i < swapchain_image_count; ++i)
	{
		image_in_flight[i] = VK_NULL_HANDLE;
	}
}

//this is so we can recreate our swapchain (e.g when resizing a window)
internal void cleanup_swapchain(void)
{
    
	for (u32 i = 0; i < swapchain_image_count; ++i)
		vkDestroyFramebuffer(device, swapchain_framebuffers[i], NULL);
	
	vkFreeCommandBuffers(device, command_pool,2, command_buffers);
	
	vkDestroyPipeline(device, graphics_pipeline, NULL);
	vkDestroyPipelineLayout(device, pipeline_layout, NULL);
	vkDestroyRenderPass(device, render_pass, NULL);

	for (u32 i = 0; i < swapchain_image_count; ++i)
    {
        vkDestroyBuffer(device, uniform_buffers[i], NULL);
        vkFreeMemory(device, uniform_buffers_memory[i], NULL);
    }

	for (u32 i = 0; i< swapchain_image_count; ++i)
		vkDestroyImageView(device, swapchain_image_views[i], NULL);
	vkDestroySwapchainKHR(device, swap_chain, NULL);
	
	vkDestroyShaderModule(device, frag_shader_module, NULL);
	vkDestroyShaderModule(device, vert_shader_module, NULL);

    vkDestroyDescriptorPool(device, descriptor_pool, NULL);
}


internal void recreate_swapchain()
{
	vkDeviceWaitIdle(device);
	
	cleanup_swapchain();
	
	create_swapchain();
	create_image_views();
	create_render_pass();
	create_graphics_pipeline();
	create_framebuffers();
    create_uniform_buffer();
    create_descriptor_pool();
    create_descriptor_sets();
	create_command_buffers();
	
}


internal void draw_frame()
{
	vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
	
	//[1]: We acquire image from swapchain
	u32 image_index;
	VkResult res = vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_available_semaphore[current_frame], VK_NULL_HANDLE, &image_index);
	
	
	if (image_in_flight[image_index]!=VK_NULL_HANDLE)
	{
		vkWaitForFences(device, 1, &image_in_flight[image_index], VK_TRUE, UINT64_MAX);
	}
	
	
	//[2]: We submit the command buffer (we have pre-created a command buffer for every swapchain attachment)
	VkSubmitInfo submit_info = {0};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	
	
	VkSemaphore wait_semaphores[] = {image_available_semaphore[current_frame]};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffers[image_index];
	
	VkSemaphore signal_semaphores[] = {render_finished_semaphore[current_frame]};
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;
	
	
	vkResetFences(device, 1, &in_flight_fences[current_frame]);
	
	if (vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS)
		printf("Failed to submit draw command buffer!\n");
	
	
	
	
	//[3]: We return the image to the swapchain for presentation
	VkPresentInfoKHR present_info = {0};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;
	
	VkSwapchainKHR swapchains[] = {swap_chain};
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;
	present_info.pImageIndices = &image_index;
	
	present_info.pResults = NULL;
	res = vkQueuePresentKHR(present_queue, &present_info);
	
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebuffer_resized)
	{
		framebuffer_resized = FALSE;
		recreate_swapchain();
	}


    { //fill the UBO!
        //[0]: make the new data
        UniformBufferObject ubo = {0};
        ubo.proj = perspective_proj(45.f, swapchain_extent.width / (f32)swapchain_extent.height, 0.1f, 10.0f);
        ubo.view = look_at(v3(0,0,3), v3(0,0,0), v3(0,1,0));
        ubo.model= mat4_rotate(14* sin(4.0f * glfwGetTime()), v3(0,1,0));

        //[1]: update the uniform buffer to the new data
        void *ubo_data;
        vkMapMemory(device, uniform_buffers_memory[image_index], 0, 
                sizeof(ubo), 0, &ubo_data);
        memcpy(ubo_data, &ubo, sizeof(ubo));
        vkUnmapMemory(device, uniform_buffers_memory[image_index]);
    }
	
	current_frame = (current_frame+1) % MAX_FRAMES_IN_FLIGHT;
}

internal void framebuffer_resize_callback()
{
	framebuffer_resized = TRUE;
}
internal int vulkan_init()
{
	//first we initialize the glfw stuff :)
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	
    window = glfwCreateWindow(800, 600, "Vulkan Demo", NULL, NULL);
	glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
	//Initialize Vulkan!
	create_instance();
	//setup_debug_messenger(); //(optional)
	create_surface();
	pick_physical_device();
	create_logical_device();
	create_swapchain();
	create_image_views();
	create_render_pass();
    create_descriptor_set_layout();
    create_graphics_pipeline();
	create_framebuffers();
	create_command_pool();
    create_vertex_buffer();
    create_index_buffer();
    create_uniform_buffer();
    create_descriptor_pool();
    create_descriptor_sets();
	create_command_buffers();
	create_sync_objects();

	return TRUE;
}



internal void main_loop(void)
{
	while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
		draw_frame();
    }
	vkDeviceWaitIdle(device);
}



internal void cleanup(void)
{
	cleanup_swapchain();
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, NULL);
    vkDestroyBuffer(device, vertex_buffer, NULL);
    vkFreeMemory(device, vertex_buffer_memory, NULL);
    vkDestroyBuffer(device, index_buffer, NULL);
    vkFreeMemory(device, index_buffer_memory, NULL);
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(device, render_finished_semaphore[i], NULL);
		vkDestroySemaphore(device, image_available_semaphore[i], NULL);
		vkDestroyFence(device, in_flight_fences[i], NULL);
		vkDestroyFence(device, image_in_flight[i], NULL);
	}
	vkDestroyCommandPool(device, command_pool, NULL);
	
	
	vkDestroyDevice(device, NULL);
	vkDestroySurfaceKHR(instance, surface, NULL);
	vkDestroyInstance(instance, NULL);
	glfwDestroyWindow(window);
	glfwTerminate();
}
int main(void)
{
	if(vulkan_init())
		printf("Hello Vulkan!");
	
	main_loop();
	
	cleanup();
	return 0;
}

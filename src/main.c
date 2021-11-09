#include "stdio.h"
#include "tools.h"
#include "stdlib.h"
#include "string.h"

#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

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

const char *validation_layers[]= {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
    const u32 enable_validation_layers= TRUE;
#else
    const u32 enable_validation_layers = TRUE;
#endif

const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; 


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
	for (u32 i = 0; i < ext_count; ++i)
		printf("EXT: %s\n", extensions[i]);
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

internal void create_surface(void)
{
    if (glfwCreateWindowSurface(instance, window, NULL, &surface)!=VK_SUCCESS)
        vk_error("Failed to create window surface!");
}

internal void framebuffer_resize_callback(){}

internal int vulkan_init(void) {
	create_instance();
    create_surface();
	pick_physical_device();
    create_logical_device();
    create_swapchain();
    create_image_views();
	return 1;
}

internal void main_loop(void) {

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

internal void cleanup(void) {

    for (u32 i = 0; i < swapchain_image_count; ++i)
        vkDestroyImageView(device, swapchain_image_views[i], NULL);
    vkDestroySwapchainKHR(device, swapchain, NULL);
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
		printf("Hello Vulkan!");
	
	main_loop();
	
	cleanup();
	return 0;
}

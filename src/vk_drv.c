#include "stdio.h"
#include "tools.h"
#include "stdlib.h"
#include "string.h"

#define NOGLFW 1
#include "vkwin.h"
Window wnd;


#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

internal s32 window_w = 800;
internal s32 window_h = 600;

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			printf("[%i]Detected Vulkan error: %i \n",__LINE__, err);            \
		}                                                           \
	} while (0);
 
typedef struct VulkanLayer
{
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkSurfaceKHR surface;
	
	VkQueue graphics_queue;
	VkQueue present_queue;
	
	
	//---------------------------------
	VkSwapchainKHR swapchain;
	VkImage *swapchain_images;
	u32 swapchain_image_count;
	VkFormat swapchain_image_format;
	VkExtent2D swapchain_extent;
	VkImageView *swapchain_image_views;
	VkFramebuffer *swapchain_framebuffers; //check https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan
	//----------------------------------
	VkImage depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView depth_image_view;
	
	
	VkCommandPool command_pool;
	VkCommandBuffer *command_buffers;
	
	VkRenderPass render_pass;
	

	
	VkShaderModule fullscreen_vert;
	VkShaderModule fullscreen_frag;
	VkPipeline fullscreen_pipeline;
	VkPipelineLayout fullscreen_pipeline_layout;
	
	VkShaderModule base_vert;
	VkShaderModule base_frag;
	VkPipeline base_pipeline;
	VkPipelineLayout base_pipeline_layout;
}VulkanLayer;
global VulkanLayer vl;

//-----------------------------------------------------------------------------------------
typedef struct PipelineBuilder
{
	VkPipelineShaderStageCreateInfo shader_stages[4];
	u32 shader_stages_count;
	VkPipelineVertexInputStateCreateInfo vertex_input_info;
	VkPipelineInputAssemblyStateCreateInfo input_asm;
	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineColorBlendAttachmentState color_blend_attachment;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineLayout pipeline_layout;
}PipelineBuilder;

internal VkPipelineShaderStageCreateInfo 
	pipe_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shader_module)
{
	VkPipelineShaderStageCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = NULL;
	
	info.stage = stage;
	info.module = shader_module;
	info.pName = "main";
	return info;
}

internal VkPipelineVertexInputStateCreateInfo pipe_vertex_input_state_create_info(void)
{
	VkPipelineVertexInputStateCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	info.pNext = NULL;
	
	//no vertex bindings or attributes
	info.vertexBindingDescriptionCount = 0; //no vertex
	info.vertexAttributeDescriptionCount = 0; //no attribute
	return info;
}

internal VkPipelineInputAssemblyStateCreateInfo pipe_input_assembly_create_info(VkPrimitiveTopology topology)
{
	VkPipelineInputAssemblyStateCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.pNext = NULL;
	
	info.topology = topology;
	info.primitiveRestartEnable = VK_FALSE;
	return info;
}

internal VkPipelineRasterizationStateCreateInfo pipe_rasterization_state_create_info(VkPolygonMode polygon_mode)
{
	VkPipelineRasterizationStateCreateInfo info = {0};
	info.sType= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.pNext = NULL;
	
	info.depthClampEnable = VK_FALSE;
	info.rasterizerDiscardEnable = VK_FALSE;
	info.polygonMode = polygon_mode;
	info.lineWidth = 1.0f;
	info.cullMode = VK_CULL_MODE_NONE;
	info.frontFace = VK_FRONT_FACE_CLOCKWISE;
	//no depth bias (for now)
	info.depthBiasEnable = VK_FALSE;
	info.depthBiasConstantFactor = 0.0f;
	info.depthBiasClamp = 0.0f;
	info.depthBiasSlopeFactor = 0.0f;
	
	return info;
}


internal VkPipelineMultisampleStateCreateInfo pipe_multisampling_state_create_info(void)
{
	VkPipelineMultisampleStateCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.pNext = NULL;
	
	info.sampleShadingEnable = VK_FALSE;
	info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	info.minSampleShading = 1.0f;
	info.pSampleMask = NULL;
	info.alphaToCoverageEnable = VK_FALSE;
	return info;
}

internal VkPipelineColorBlendAttachmentState pipe_color_blend_attachment_state(void)
{
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
	color_blend_attachment.blendEnable = VK_FALSE;
	return color_blend_attachment;
}

//this is retty much empty
internal VkPipelineLayoutCreateInfo pipe_layout_create_info(void)
{
	VkPipelineLayoutCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = NULL;
	
	info.flags = 0;
	info.setLayoutCount = 0;
	info.pSetLayouts = NULL;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = NULL;
	return info;
}

internal VkPipeline build_pipeline(VkDevice device, PipelineBuilder p,VkRenderPass render_pass)
{
	//------these are fixed-------
	VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (f32)vl.swapchain_extent.width;
    viewport.height = (f32)vl.swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {0};
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = vl.swapchain_extent;
	
	VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.minDepthBounds = 0.0f;
	depth_stencil.maxDepthBounds = 1.0f;
	depth_stencil.stencilTestEnable = VK_FALSE;
	depth_stencil.front = (VkStencilOpState){0};
	depth_stencil.back = (VkStencilOpState){0};
	//----------------------------
	
	
	
	VkPipelineViewportStateCreateInfo viewport_state = {0};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;
	
	//dummy color blending
	VkPipelineColorBlendStateCreateInfo color_blending = {0};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.pNext = NULL;
	
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.logicOp = VK_LOGIC_OP_COPY;
	color_blending.attachmentCount = 1;
	VkAttachmentDescription color_attachment = {0};
    color_attachment.format = vl.swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	color_blending.pAttachments = &p.color_blend_attachment;
	
	
	VkGraphicsPipelineCreateInfo pipeline_info = {0};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	
	pipeline_info.stageCount = p.shader_stages_count;
	pipeline_info.pStages = p.shader_stages;
	pipeline_info.pVertexInputState = &p.vertex_input_info;
	pipeline_info.pInputAssemblyState = &p.input_asm;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &p.rasterizer;
	pipeline_info.pMultisampleState = &p.multisampling;
	pipeline_info.pColorBlendState = &color_blending;
	pipeline_info.pDepthStencilState = &depth_stencil;
	pipeline_info.layout = p.pipeline_layout;
	pipeline_info.renderPass = render_pass;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	VkPipeline new_pipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &new_pipeline)!=VK_SUCCESS)
		printf("Error creating some pipeline!\n");
	
	return new_pipeline;
}


VkDescriptorSetLayout descriptor_set_layout;
VkPipelineLayout pipeline_layout;
//-------------------------------------------------------------------------------------------

internal VkShaderModule create_shader_module(char *code, u32 size);
internal VkVertexInputBindingDescription get_bind_desc_test_vert(void);
internal void get_attr_desc_test_vert(VkVertexInputAttributeDescription *attr_desc);
///*
internal void vl_base_pipelines_init(void)
{
	//build pipeline layout that controls the IO of the shader
	VkPipelineLayoutCreateInfo pipeline_layout_info = pipe_layout_create_info();
	
	VK_CHECK(vkCreatePipelineLayout(vl.device, &pipeline_layout_info, NULL, &vl.fullscreen_pipeline_layout));
	
	
	//read the spirv files as binary files
	u32 vert_size, frag_size;
	char *vert_shader_code = read_whole_file_binary("fullscreen_vert.spv", &vert_size);
	char *frag_shader_code = read_whole_file_binary("fullscreen_frag.spv", &frag_size);
	
	//make shader modules out of those
	vl.fullscreen_vert= create_shader_module(vert_shader_code, vert_size);
	vl.fullscreen_frag = create_shader_module(frag_shader_code, frag_size);
	
	PipelineBuilder pb = {0};
	pb.shader_stages_count = 2;
	pb.shader_stages[0] = pipe_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vl.fullscreen_vert);
	pb.shader_stages[1] = pipe_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, vl.fullscreen_frag);
	
	pb.vertex_input_info = pipe_vertex_input_state_create_info();
	pb.input_asm = pipe_input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pb.rasterizer = pipe_rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pb.multisampling = pipe_multisampling_state_create_info();
	pb.color_blend_attachment = pipe_color_blend_attachment_state();
	pb.pipeline_layout = vl.fullscreen_pipeline_layout; //hmmmmmmm TODO check
	vl.fullscreen_pipeline = build_pipeline(vl.device, pb, vl.render_pass);
	
	
	
	
	pipeline_layout_info = pipe_layout_create_info();
	
	VK_CHECK(vkCreatePipelineLayout(vl.device, &pipeline_layout_info, NULL, &vl.base_pipeline_layout));
	
	
	//read the spirv files as binary files
	vert_size, frag_size;
	vert_shader_code = read_whole_file_binary("vert.spv", &vert_size);
	frag_shader_code = read_whole_file_binary("frag.spv", &frag_size);
	
	//make shader modules out of those
	vl.base_vert = create_shader_module(vert_shader_code, vert_size);
	vl.base_frag  = create_shader_module(frag_shader_code, frag_size);
	
	pb.shader_stages_count = 2;
	pb.shader_stages[0] = pipe_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vl.base_vert);
	pb.shader_stages[1] = pipe_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, vl.base_frag);
	
	//---VERTEX INPUT---
    VkVertexInputBindingDescription bind_desc = get_bind_desc_test_vert();
    VkVertexInputAttributeDescription attr_desc[3];
    get_attr_desc_test_vert(attr_desc);
    
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;//we have 1 vertex description 
    vertex_input_info.pVertexBindingDescriptions = &bind_desc;
    vertex_input_info.vertexAttributeDescriptionCount = array_count(attr_desc); //with 2 different attributes
    vertex_input_info.pVertexAttributeDescriptions = attr_desc;
	
	pb.vertex_input_info = vertex_input_info;
	pb.input_asm = pipe_input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pb.rasterizer = pipe_rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pb.multisampling = pipe_multisampling_state_create_info();
	pb.color_blend_attachment = pipe_color_blend_attachment_state();
	
	///---PIPELINE LAYOUT--- (for uniform and push values referenced by shaders)
    pipeline_layout_info = (VkPipelineLayoutCreateInfo){0};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = NULL;
    VK_CHECK(vkCreatePipelineLayout(vl.device, &pipeline_layout_info, NULL, &vl.base_pipeline_layout));
	
	
	pb.pipeline_layout = vl.base_pipeline_layout;
	vl.base_pipeline = build_pipeline(vl.device, pb, vl.render_pass);
}
//*/







VkPipeline graphics_pipeline;



VkDescriptorPool descriptor_pool;
VkDescriptorSet *descriptor_sets;


const int MAX_FRAMES_IN_FLIGHT = 2;
VkSemaphore *image_available_semaphores;
VkSemaphore *render_finished_semaphores;


VkFence *in_flight_fences;
VkFence *images_in_flight;

//[SPEC]: Linear array of data which are used for various purposes by binding them to a pipeline via descriptor sets 
VkBuffer vertex_buffer;
//[SPEC]: A Vulkan device operates on data in device memory via memory objects
VkDeviceMemory vertex_buffer_memory;
VkBuffer index_buffer;
VkDeviceMemory index_buffer_memory;

VkDeviceMemory *uniform_buffer_memories;
VkBuffer *uniform_buffers;

typedef struct Texture {
		VkSampler sampler;
		VkImage image;
		VkImageLayout image_layout;
		VkDeviceMemory device_mem;
		VkImageView view;
		u32 width, height;
		u32 mip_levels;
} Texture;

global Texture sample_texture;

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


typedef struct UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
}UniformBufferObject;

typedef struct Vertex
{
    vec3 pos;
    vec3 normal;
	vec2 tex_coord;
}Vertex;
		
internal vec3 cube_positions[]  = {
	{0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f},  {-0.5f,-0.5f, 0.5f}, {0.5f,-0.5f, 0.5f},   // v0,v1,v2,v3 (front)
    {0.5f, 0.5f, 0.5f},  {0.5f,-0.5f, 0.5f},   {0.5f,-0.5f,-0.5f},  {0.5f, 0.5f,-0.5f},   // v0,v3,v4,v5 (right)
    {0.5f, 0.5f, 0.5f},  {0.5f, 0.5f,-0.5f},   {-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f},   // v0,v5,v6,v1 (top)
    {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f},  {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f},   // v1,v6,v7,v2 (left)
    {-0.5f,-0.5f,-0.5f}, {.05f,-0.5f,-0.5f},   {0.5f,-0.5f, 0.5f},  {-0.5f,-0.5f, 0.5f},   // v7,v4,v3,v2 (bottom)
    {0.5f,-0.5f,-0.5f},  {-0.5f,-0.5f,-0.5f},  {-0.5f, 0.5f,-0.5f}, {0.5f, 0.5f,-0.5f},    // v4,v7,v6,v5 (back)
};

// normal array
internal vec3 cube_normals[] = {
	{0, 0, 1},   {0, 0, 1},   {0, 0, 1},   {0, 0, 1},  // v0,v1,v2,v3 (front)
	{1, 0, 0},   {1, 0, 0},   {1, 0, 0},   {1, 0, 0},  // v0,v3,v4,v5 (right)
	{0, 1, 0},   {0, 1, 0},   {0, 1, 0},   {0, 1, 0},  // v0,v5,v6,v1 (top)
	{-1, 0, 0},  {-1, 0, 0},  {-1, 0, 0},  {-1, 0, 0},  // v1,v6,v7,v2 (left)
	{0,-1, 0},   {0,-1, 0},   {0,-1, 0},   {0,-1, 0},  // v7,v4,v3,v2 (bottom)
	{0, 0,-1},   {0, 0,-1},   {0, 0,-1},   {0, 0,-1},   // v4,v7,v6,v5 (back)
};

// texCoord array
internal vec2 cube_tex_coords[] = {
    {1, 0},   {0, 0},   {0, 1},   {1, 1},               // v0,v1,v2,v3 (front)
	{0, 0},   {0, 1},   {1, 1},   {1, 0},               // v0,v3,v4,v5 (right)
	{1, 1},   {1, 0},   {0, 0},   {0, 1},               // v0,v5,v6,v1 (top)
	{1, 0},   {0, 0},   {0, 1},   {1, 1},               // v1,v6,v7,v2 (left)
	{0, 1},   {1, 1},   {1, 0},   {0, 0},               // v7,v4,v3,v2 (bottom)
	{0, 1},   {1, 1},   {1, 0},   {0, 0}                // v4,v7,v6,v5 (back)
};

internal u32 cube_indices[] = {
     0, 1, 2,   2, 3, 0,    // v0-v1-v2, v2-v3-v0 (front)
     4, 5, 6,   6, 7, 4,    // v0-v3-v4, v4-v5-v0 (right)
     8, 9,10,  10,11, 8,    // v0-v5-v6, v6-v1-v0 (top)
    12,13,14,  14,15,12,    // v1-v6-v7, v7-v2-v1 (left)
    16,17,18,  18,19,16,    // v7-v4-v3, v3-v2-v7 (bottom)
    20,21,22,  22,23,20     // v4-v7-v6, v6-v5-v4 (back)
};
internal Vertex *cube_build_verts(void)
{
	Vertex *verts = malloc(sizeof(Vertex) * 24);
	for (u32 i =0; i < 24; ++i)
	{
		verts[i].pos = cube_positions[i];
		verts[i].normal = cube_normals[i];
		verts[i].tex_coord = cube_tex_coords[i];
	}
	return verts;
}


//return the binding description of Vertex type vertex input
internal VkVertexInputBindingDescription get_bind_desc_test_vert(void)
{
    VkVertexInputBindingDescription bind_desc = {0};
    bind_desc.binding = 0;
    bind_desc.stride = sizeof(Vertex);
    bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//per-vertex
    return bind_desc;
}

internal void get_attr_desc_test_vert(VkVertexInputAttributeDescription *attr_desc)
{
    attr_desc[0].binding = 0;
    attr_desc[0].location = 0;
    attr_desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_desc[0].offset = offsetof(Vertex, pos);
    attr_desc[1].binding = 0;
    attr_desc[1].location = 1;
    attr_desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attr_desc[1].offset = offsetof(Vertex, normal);
	attr_desc[2].binding = 0;
    attr_desc[2].location = 2;
    attr_desc[2].format = VK_FORMAT_R32G32_SFLOAT;
    attr_desc[2].offset = offsetof(Vertex, tex_coord);
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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, vl.surface, &details.capabilities);
    
    //[1]: then we query the swapchain available formats
    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, vl.surface, &format_count, NULL);
    details.format_count = format_count;
    
    if (format_count != 0)
    {
        details.formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, vl.surface, &format_count, details.formats);
    }
    //[2]: finally we query available presentation modes
    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, vl.surface, &present_mode_count, NULL);
    details.present_mode_count = present_mode_count;
    
    if (present_mode_count != 0)
    {
        details.present_modes = malloc(sizeof(VkPresentModeKHR) * present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, vl.surface, &present_mode_count, details.present_modes);
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

internal void vl_create_instance(void) {
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
    
	u32 instance_ext_count = 0;
	char **instance_extensions; //array of strings with names of VK extensions needed!
    char *base_extensions[] = {
		"VK_KHR_surface",
		"VK_KHR_win32_surface",
	};
	//vl.instance_extensions = window_get_required_vl.instance_extensions(&wnd, &vl.instance_ext_count);
    
	create_info.enabledExtensionCount = array_count(base_extensions);
	create_info.ppEnabledExtensionNames = base_extensions;
	create_info.enabledLayerCount = 0;

    
	VK_CHECK(vkCreateInstance(&create_info, NULL, &vl.instance));
	
    
	//(OPTIONAL): extension support
	u32 ext_count = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);
	VkExtensionProperties *extensions = malloc(sizeof(VkExtensionProperties) * ext_count);
	vkEnumerateInstanceExtensionProperties(NULL, &ext_count, extensions);
    /* //prints supported vl.instance extensions
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
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vl.surface, &present_support);
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

internal void vl_pick_physical_device(void) {
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(vl.instance, &device_count, NULL);
    if (device_count == 0)
        vk_error("Failed to find GPUs with Vulkan support!");
    
    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(vl.instance, &device_count, devices);
    for (u32 i = 0; i < device_count; ++i)
        if (is_device_suitable(devices[i]))
    {
        vl.physical_device = devices[i];
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
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
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

		window_get_framebuffer_size(&wnd, &width, &height);
        VkExtent2D actual_extent = {width, height};
        actual_extent.height=maximum(details.capabilities.maxImageExtent.height,
                                     minimum(details.capabilities.minImageExtent.height,actual_extent.height));
        actual_extent.width=maximum(details.capabilities.maxImageExtent.width,
                                    minimum(details.capabilities.minImageExtent.width,actual_extent.width));
        return actual_extent;
    }
}

internal void vl_create_swapchain(void)
{
    SwapChainSupportDetails swapchain_support = query_swapchain_support(vl.physical_device);
    
    VkSurfaceFormatKHR surface_format =choose_swap_surface_format(swapchain_support);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support);
    VkExtent2D extent = choose_swap_extent(swapchain_support);
    
    u32 image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount)
        image_count = swapchain_support.capabilities.maxImageCount;
    
    
    VkSwapchainCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = vl.surface; //we specify which surface the swapchain is tied to
    
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    //VK_IMAGE_USAGE_TRANSFER_DST_BIT + memop for offscreen rendering!
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; 
    
    QueueFamilyIndices indices = find_queue_families(vl.physical_device);
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
    
    VK_CHECK(vkCreateSwapchainKHR(vl.device, &create_info, NULL, &vl.swapchain));
    
    vkGetSwapchainImagesKHR(vl.device, vl.swapchain, &image_count, NULL);
    vl.swapchain_images = malloc(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(vl.device, vl.swapchain, &image_count, vl.swapchain_images);
    vl.swapchain_image_format = surface_format.format;
    vl.swapchain_extent = extent;
    vl.swapchain_image_count = image_count;//TODO(ilias): check
    //printf("new swapchain size: %i\n", image_count);
}

internal VkImageView create_image_view(VkImage image, VkFormat format,  VkImageAspectFlags aspect_flags)
{
	VkImageView image_view;
	
	VkImageViewCreateInfo view_info = {0};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = format;
	view_info.subresourceRange.aspectMask = aspect_flags;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(vl.device, &view_info, NULL, &image_view));
	return image_view;
}

internal void vl_create_swapchain_image_views(void)
{
    vl.swapchain_image_views = malloc(sizeof(VkImageView) * vl.swapchain_image_count);
    for (u32 i = 0; i < vl.swapchain_image_count; ++i)
		vl.swapchain_image_views[i] = create_image_view(vl.swapchain_images[i], vl.swapchain_image_format,VK_IMAGE_ASPECT_COLOR_BIT);
}

//Queue initialization is a little weird,TODO(ilias): fix when possible
internal void vl_create_logical_device(void)
{
    QueueFamilyIndices indices = find_queue_families(vl.physical_device);
    
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
    
    VK_CHECK(vkCreateDevice(vl.physical_device, &create_info, NULL, &vl.device));
    
    
    vkGetDeviceQueue(vl.device, indices.graphics_family, 0, &vl.graphics_queue);
    vkGetDeviceQueue(vl.device, indices.present_family, 0, &vl.present_queue);
}

internal VkShaderModule create_shader_module(char *code, u32 size)
{
	VkShaderModuleCreateInfo create_info = {0};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = size;
	create_info.pCode = (u32*)code;
	VkShaderModule shader_module;
	VK_CHECK(vkCreateShaderModule(vl.device, &create_info, NULL, &shader_module));
	return shader_module;
}

internal void create_descriptor_sets(void)
{
    VkDescriptorSetLayout *layouts = malloc(sizeof(VkDescriptorSetLayout)*vl.swapchain_image_count);
    for (u32 i = 0; i < vl.swapchain_image_count; ++i)
        layouts[i] = descriptor_set_layout;
    VkDescriptorSetAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = vl.swapchain_image_count;
    alloc_info.pSetLayouts = layouts;
    
    descriptor_sets = malloc(sizeof(VkDescriptorSet) * vl.swapchain_image_count);
    VK_CHECK(vkAllocateDescriptorSets(vl.device, &alloc_info, descriptor_sets));
    
    for (size_t i = 0; i < vl.swapchain_image_count; i++) {
        VkDescriptorBufferInfo buffer_info = {0};
        buffer_info.buffer = uniform_buffers[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);
		
		VkDescriptorImageInfo image_info = {0};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = sample_texture.view;
		image_info.sampler = sample_texture.sampler;
        
        VkWriteDescriptorSet descriptor_writes[2] = {0};
        
		descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = descriptor_sets[i];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;
		
		descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = descriptor_sets[i];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].pImageInfo = &image_info;
        
        vkUpdateDescriptorSets(vl.device, array_count(descriptor_writes), descriptor_writes, 0, NULL);
    }
}
internal void create_descriptor_pool(void)
{
    VkDescriptorPoolSize pool_size[2];
    pool_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size[0].descriptorCount = vl.swapchain_image_count;
	pool_size[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size[1].descriptorCount = vl.swapchain_image_count;
    
    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = array_count(pool_size);
    pool_info.pPoolSizes = pool_size;
    pool_info.maxSets = vl.swapchain_image_count;
    
    VK_CHECK(vkCreateDescriptorPool(vl.device, &pool_info, NULL, &descriptor_pool));
}

internal void create_descriptor_set_layout(u32 buf_count, u32 binding, VkDescriptorSetLayout *layout)
{
    VkDescriptorSetLayoutBinding ubo_layout_binding = {0};
    ubo_layout_binding.binding = binding;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = buf_count; //N if we want an array of descriptors (dset?)
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    ubo_layout_binding.pImmutableSamplers = NULL;
	
	VkDescriptorSetLayoutBinding sampler_layout_binding = {0};
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.pImmutableSamplers = NULL;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	
	
	VkDescriptorSetLayoutBinding bindings[2] =  {ubo_layout_binding, sampler_layout_binding};
    
    VkDescriptorSetLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = array_count(bindings);
    layout_info.pBindings = bindings;
    
    VK_CHECK(vkCreateDescriptorSetLayout(vl.device, &layout_info, NULL, layout));
    
}

internal VkFormat find_depth_format(void);

internal void vl_create_render_pass(void)
{
    VkAttachmentDescription color_attachment = {0};
    color_attachment.format = vl.swapchain_image_format;
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
	
	
	VkAttachmentDescription depth_attachment = {0};
	depth_attachment.format = find_depth_format();
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
	VkAttachmentReference depth_attachment_ref = {0};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
    
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;
    
	VkAttachmentDescription attachments[2] = {color_attachment, depth_attachment};
	
    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = array_count(attachments);
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
    VK_CHECK(vkCreateRenderPass(vl.device, &render_pass_info, NULL, &vl.render_pass));
    
}

internal void vl_create_framebuffers(void)
{
    vl.swapchain_framebuffers = malloc(sizeof(VkFramebuffer) * vl.swapchain_image_count); 
    for (u32 i = 0; i < vl.swapchain_image_count; ++i)
    {
        VkImageView attachments[] = {vl.swapchain_image_views[i], vl.depth_image_view};
        
        VkFramebufferCreateInfo framebuffer_info = {0};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vl.render_pass; //VkFramebuffers need a render pass?
        framebuffer_info.attachmentCount = array_count(attachments);
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = vl.swapchain_extent.width;
        framebuffer_info.height = vl.swapchain_extent.height;
        framebuffer_info.layers = 1;
        
        VK_CHECK(vkCreateFramebuffer(vl.device, &framebuffer_info, NULL, &vl.swapchain_framebuffers[i]));
        
    }
    
}

internal VkFormat find_supported_format(VkFormat *candidates, VkImageTiling tiling, VkFormatFeatureFlags features, u32 candidates_count)
{
	for (u32 i = 0; i < candidates_count; ++i)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(vl.physical_device, candidates[i], &props);
		
		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return candidates[i];
		}else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return candidates[i];
		}
	}
	vk_error("Couldn't find desired depth buffer format!");
}


internal void vl_create_command_pool(void)
{
    QueueFamilyIndices queue_family_indices = find_queue_families(vl.physical_device);
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(vl.device, &pool_info, NULL, &vl.command_pool));
}

internal void render_cube(VkCommandBuffer command_buf, u32 image_index)
{
	/*
	VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(command_buf, &begin_info));
        
     
    VkRenderPassBeginInfo renderpass_info = {0};
    renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpass_info.renderPass = vl.render_pass;
    renderpass_info.framebuffer = vl.swapchain_framebuffers[image_index]; //we bind a _framebuffer_ to a render pass
    renderpass_info.renderArea.offset = (VkOffset2D){0,0};
    renderpass_info.renderArea.extent = vl.swapchain_extent;
	VkClearValue clear_values[2] = {0};
	clear_values[0].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 1.0f}};
	clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    renderpass_info.clearValueCount = array_count(clear_values);
    renderpass_info.pClearValues = &clear_values;
    vkCmdBeginRenderPass(command_buf, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
	*/	
        
	vkCmdBindPipeline(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vl.base_pipeline);
		
    VkBuffer vertex_buffers[] = {vertex_buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buf, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(command_buf, index_buffer, 0, VK_INDEX_TYPE_UINT32);
        
    vkCmdBindDescriptorSets(vl.command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                vl.base_pipeline_layout, 0, 1, &descriptor_sets[image_index], 0, NULL);
    vkCmdDrawIndexed(command_buf, array_count(cube_indices), 1, 0, 0, 0);
	
    
    //VK_CHECK(vkEndCommandBuffer(command_buf));
}


internal void render_fullscreen(VkCommandBuffer command_buf, u32 image_index)
{    
	vkCmdBindPipeline(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vl.fullscreen_pipeline);
	
    vkCmdDraw(command_buf, 4, 1, 0, 0);
}

internal void vl_create_command_buffers(void)
{
    vl.command_buffers = malloc(sizeof(VkCommandBuffer) * vl.swapchain_image_count);
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = vl.command_pool; //where to allocate the buffer from
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = vl.swapchain_image_count;
    VK_CHECK(vkAllocateCommandBuffers(vl.device, &alloc_info, vl.command_buffers));
}

#define TYPE_OK(TYPE_FILTER, FILTER_INDEX) (TYPE_FILTER & (1 << FILTER_INDEX)) 
internal u32 find_mem_type(u32 type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(vl.physical_device, &mem_properties);
    for (u32 i = 0; i < mem_properties.memoryTypeCount; ++i)
    {
        if (TYPE_OK(type_filter, i) && 
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    vk_error("Failed to find suitable memory type!");
}

internal void create_buffer(u32 buffer_size,VkBufferUsageFlagBits usage, VkMemoryPropertyFlagBits mem_flags, VkBuffer *buf, VkDeviceMemory *mem)
{
    VkBufferCreateInfo buffer_info = {0};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(vl.device, &buffer_info, NULL, buf));
    
    VkMemoryRequirements mem_req = {0};
    vkGetBufferMemoryRequirements(vl.device, *buf, &mem_req);
    
    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_mem_type(mem_req.memoryTypeBits, mem_flags);
    VK_CHECK(vkAllocateMemory(vl.device, &alloc_info, NULL, mem));
    vkBindBufferMemory(vl.device, *buf, *mem, 0);
}


internal void create_image(u32 width, u32 height, VkFormat format, VkImageTiling tiling, 
VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *image_memory)
{
	VkImageCreateInfo image_info = {0};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.flags = 0;
	VK_CHECK(vkCreateImage(vl.device, &image_info, NULL, image));
	
	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(vl.device, *image, &mem_req);
	
	VkMemoryAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_mem_type(mem_req.memoryTypeBits, properties);
    VK_CHECK(vkAllocateMemory(vl.device, &alloc_info, NULL, image_memory));
	vkBindImageMemory(vl.device, *image, *image_memory, 0);
}



internal VkCommandBuffer begin_single_time_commands(void)
{
	VkCommandBufferAllocateInfo alloc_info = {0};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = vl.command_pool;
	alloc_info.commandBufferCount = 1;
	
	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(vl.device, &alloc_info, &command_buffer);
	
	VkCommandBufferBeginInfo begin_info = {0};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);
	
	return command_buffer;
}
internal void end_single_time_commands(VkCommandBuffer command_buffer)
{
	vkEndCommandBuffer(command_buffer);
	
	VkSubmitInfo submit_info = {0};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	
	vkQueueSubmit(vl.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(vl.graphics_queue);
	vkFreeCommandBuffers(vl.device, vl.command_pool, 1, &command_buffer);
}

//compies from one VkBuffer to another
internal void copy_buffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size)
{
	VkCommandBuffer command_buf = begin_single_time_commands();
	
	VkBufferCopy copy_region = {0};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buf, src_buf, dst_buf, 1, &copy_region);
	
	end_single_time_commands(command_buf);
}

internal void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
{
	VkCommandBuffer command_buf = begin_single_time_commands();
	
	VkImageMemoryBarrier barrier = {0};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;
	
	VkPipelineStageFlags src_stage;
	VkPipelineStageFlags dst_stage;
	
	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		
		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		
		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	
	
	
	vkCmdPipelineBarrier(
		command_buf,
		src_stage, dst_stage,
		0,
		0,NULL,
		0, NULL,
		1, &barrier);
		
	
	end_single_time_commands(command_buf);
}

internal void copy_buffer_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height) {
    VkCommandBuffer command_buf = begin_single_time_commands();
	
	VkBufferImageCopy region = {0};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = (VkOffset3D){0, 0, 0};
	region.imageExtent = (VkExtent3D){
		width,
		height,
		1
	};
		
	vkCmdCopyBufferToImage(
		command_buf,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);
	
    end_single_time_commands(command_buf);
}

//checks whether our depth format has a stencil component
internal u32 has_stencil_component(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}
internal VkFormat find_depth_format(void)
{	VkFormat c[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
	return find_supported_format(
                                 c,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,3);
    
}

internal void vl_create_depth_resources(void)
{
	VkFormat depth_format = find_depth_format();
	create_image(vl.swapchain_extent.width, vl.swapchain_extent.height, 
		depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vl.depth_image, &vl.depth_image_memory);
	vl.depth_image_view = create_image_view(vl.depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
}

internal void create_vertex_buffer(void)
{
	Vertex *cube_vertices = cube_build_verts();
    u32 buf_size = sizeof(Vertex) * 24;
    create_buffer(buf_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&vertex_buffer, &vertex_buffer_memory);
    
    void *data;
    vkMapMemory(vl.device, vertex_buffer_memory, 0, buf_size, 0, &data);
    memcpy(data, cube_vertices, buf_size);
    vkUnmapMemory(vl.device, vertex_buffer_memory);
}

internal void create_uniform_buffers(void)
{
    VkDeviceSize buf_size = sizeof(UniformBufferObject);
    uniform_buffers = (VkBuffer*)malloc(sizeof(VkBuffer) * vl.swapchain_image_count);
    uniform_buffer_memories = (VkDeviceMemory*)malloc(sizeof(VkDeviceMemory) * vl.swapchain_image_count);
    
    for (u32 i = 0; i < vl.swapchain_image_count; ++i)
    {
        create_buffer(buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&uniform_buffers[i], &uniform_buffer_memories[i]);
    }
}
internal void create_index_buffer(void)
{
	u32 buf_size = sizeof(cube_indices[0]) * array_count(cube_indices);
    create_buffer(buf_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&index_buffer, &index_buffer_memory);
    
    void *data;
    vkMapMemory(vl.device, index_buffer_memory, 0, buf_size, 0, &data);
    memcpy(data, cube_indices, buf_size);
    vkUnmapMemory(vl.device, index_buffer_memory);
}


internal void create_sync_objects(void)
{
    image_available_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    in_flight_fences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    images_in_flight = malloc(sizeof(VkFence) * vl.swapchain_image_count);
    for (u32 i = 0; i < vl.swapchain_image_count; ++i)images_in_flight[i] = VK_NULL_HANDLE;
    
    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(vkCreateSemaphore(vl.device, &semaphore_info, NULL, &image_available_semaphores[i])|| 
            vkCreateSemaphore(vl.device, &semaphore_info, NULL, &render_finished_semaphores[i])||
            vkCreateFence(vl.device, &fence_info, NULL, &in_flight_fences[i]));
    }
}

internal void create_surface(void)
{
	window_create_window_surface(vl.instance, &wnd, &vl.surface);
}

internal void framebuffer_resize_callback(void){framebuffer_resized = TRUE;}

internal void vl_cleanup_swapchain(void)
{
	vkDestroyImageView(vl.device, vl.depth_image_view, NULL);
    vkDestroyImage(vl.device, vl.depth_image, NULL);
    vkFreeMemory(vl.device, vl.depth_image_memory, NULL);
    for (u32 i = 0; i < vl.swapchain_image_count; ++i)
        vkDestroyFramebuffer(vl.device, vl.swapchain_framebuffers[i], NULL);
    //vkFreeCommandBuffers(vl.device, command_pool, vl.swapchain_image_count, command_buffers);
    vkDestroyPipeline(vl.device, graphics_pipeline, NULL);
	vkDestroyPipeline(vl.device, vl.fullscreen_pipeline, NULL);
    vkDestroyPipelineLayout(vl.device, pipeline_layout, NULL);
    vkDestroyRenderPass(vl.device, vl.render_pass, NULL);
    for (u32 i = 0; i < vl.swapchain_image_count; ++i)
        vkDestroyImageView(vl.device, vl.swapchain_image_views[i], NULL);
    vkDestroySwapchainKHR(vl.device, vl.swapchain, NULL);
    //these will be recreated at pipeline creation for next swapchain
    vkDestroyShaderModule(vl.device, vl.base_vert, NULL);
    vkDestroyShaderModule(vl.device, vl.base_frag, NULL);
    for (u32 i = 0; i < vl.swapchain_image_count; ++i)
    {
        vkFreeMemory(vl.device, uniform_buffer_memories[i], NULL);
        vkDestroyBuffer(vl.device, uniform_buffers[i], NULL);
    }
    vkDestroyDescriptorPool(vl.device, descriptor_pool, NULL);
}

internal void vl_recreate_swapchain(void)
{
    //in case of window minimization (w = 0, h = 0) we wait until we get a proper window again
    s32 width = 0, height = 0;
	window_get_framebuffer_size(&wnd, &width, &height);
    
    
    vkDeviceWaitIdle(vl.device);
    
    vl_cleanup_swapchain();
    vl_create_swapchain();
    vl_create_swapchain_image_views();
    vl_create_render_pass();
    create_uniform_buffers();
    create_descriptor_pool();
    create_descriptor_sets();
    
    
	vl_base_pipelines_init();
	vl_create_depth_resources();
    vl_create_framebuffers();
    vl_create_command_buffers();
}

internal void create_texture_sampler(VkSampler *sampler)
{
	VkSamplerCreateInfo sampler_info = {0};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	
	VkPhysicalDeviceProperties prop = {0};
	vkGetPhysicalDeviceProperties(vl.physical_device, &prop);
	
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.maxAnisotropy = prop.limits.maxSamplerAnisotropy;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;
	VK_CHECK(vkCreateSampler(vl.device, &sampler_info, NULL, sampler));
}

//TODO(ilias): add mip-mapping + option for staging buffer + native linear encoding
internal Texture create_texture_image(char *filename, VkFormat format)
{
	Texture tex;
	//[0]: we read an image and store all the pixels in a pointer
	s32 tex_w, tex_h, tex_c;
	stbi_uc *pixels = stbi_load(filename, &tex_w, &tex_h, &tex_c, STBI_rgb_alpha);
	VkDeviceSize image_size = tex_w * tex_h * 4;
	
	VkBuffer image_data_buffer;
	VkDeviceMemory image_data_buffer_memory;
	
	if (!pixels)
		vk_error("Error loading image!");
	//[1]: we craete a "transfer src" buffer to put the pixels in gpu memory
	create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &image_data_buffer, &image_data_buffer_memory);
	//[2]: we fill the buffer with image data
	void *data;
	vkMapMemory(vl.device, image_data_buffer_memory, 0, image_size, 0, &data);
	memcpy(data, pixels, image_size);
	vkUnmapMemory(vl.device, image_data_buffer_memory);
	//[3]: we free the cpu side image, we don't need it
	stbi_image_free(pixels);
	//[4]: we create the VkImage that is undefined right now
	create_image(tex_w, tex_h, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT 
		| VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &tex.image, &tex.device_mem);
	
	
	//[5]: we transition the images layout from undefined to dst_optimal
	transition_image_layout(tex.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	//[6]: we copy the buffer we created in [1] to our image of the correct format (R8G8B8A8_SRGB)
	copy_buffer_to_image(image_data_buffer, tex.image, tex_w, tex_h);
	
	//[7]: we transition the image layout so that it can be read by a shader
	transition_image_layout(tex.image, format, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		
	//[8]:cleanup the buffers (all data is now in the image)
	vkDestroyBuffer(vl.device, image_data_buffer, NULL);
	vkFreeMemory(vl.device, image_data_buffer_memory, NULL);
	
	create_texture_sampler(&tex.sampler);
	
	tex.view = create_image_view(tex.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
	tex.mip_levels = 0;
	tex.width = tex_w;
	tex.height = tex_h;
	
	return tex;
}

internal void vulkan_layer_init(void)
{
	vl = (VulkanLayer){0};
	vl_create_instance();
    create_surface();
    vl_pick_physical_device();
    vl_create_logical_device();
    vl_create_swapchain();
    vl_create_swapchain_image_views();
	vl_create_render_pass();
	vl_create_depth_resources();
	vl_create_framebuffers();
    vl_create_command_pool();
	
}
internal int vulkan_init(void) {
	vulkan_layer_init();
    create_descriptor_set_layout(1, 0, &descriptor_set_layout);
	vl_base_pipelines_init();
	sample_texture = create_texture_image("../assets/test.png",VK_FORMAT_R8G8B8A8_SRGB);
	create_vertex_buffer();
    create_index_buffer();
    create_uniform_buffers();
    create_descriptor_pool();
    create_descriptor_sets();
    
    vl_create_command_buffers();
    create_sync_objects();
	return 1;
}
internal void update_uniform_buffer(u32 image_index)
{
    UniformBufferObject ubo = {0};
    ubo.model = mat4_mul(mat4_translate(v3(0,0,-4)),mat4_rotate( 360.0f * sin(get_time()) ,v3(0,1,0)));
    ubo.view = look_at(v3(0,0,0), v3(0,0,-1), v3(0,1,0));
    ubo.proj = perspective_proj_vk(45.0f,window_w/(f32)window_h, 0.1, 10);
    
    void* data;
    vkMapMemory(vl.device, uniform_buffer_memories[image_index], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(vl.device, uniform_buffer_memories[image_index]);
}

internal void draw_frame(void)
{
	
    vkWaitForFences(vl.device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
    //[0]: Acquire free image from the swapchain
    u32 image_index;
    VkResult res = vkAcquireNextImageKHR(vl.device, vl.swapchain, UINT64_MAX, 
                                         image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
    if (res == VK_ERROR_OUT_OF_DATE_KHR){vl_recreate_swapchain();return;}
    else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)vk_error("Failed to acquire swapchain image!");
    
    // check if the image is already used (in flight) froma previous frame, and if so wait
    if (images_in_flight[image_index]!=VK_NULL_HANDLE)vkWaitForFences(vl.device, 1, &images_in_flight[image_index], VK_TRUE, UINT64_MAX);
    update_uniform_buffer(image_index);
    
	//we reset the command buffer (so we can put new info in to submit)
	VK_CHECK(vkResetCommandBuffer(vl.command_buffers[image_index], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
	
	//----BEGIN RENDER PASS----
	VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(vl.command_buffers[image_index], &begin_info));
	
	VkRenderPassBeginInfo renderpass_info = {0};
    renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpass_info.renderPass = vl.render_pass;
    renderpass_info.framebuffer = vl.swapchain_framebuffers[image_index]; //we bind a _framebuffer_ to a render pass
    renderpass_info.renderArea.offset = (VkOffset2D){0,0};
    renderpass_info.renderArea.extent = vl.swapchain_extent;
	VkClearValue clear_values[2] = {0};
	clear_values[0].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 1.0f}};
	clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    renderpass_info.clearValueCount = array_count(clear_values);
    renderpass_info.pClearValues = clear_values;
    vkCmdBeginRenderPass(vl.command_buffers[image_index], &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
	
	//we render the scene onto the command buffer
	
	
	render_cube(vl.command_buffers[image_index], image_index);
	render_fullscreen(vl.command_buffers[image_index], image_index);
	
	
	vkCmdEndRenderPass(vl.command_buffers[image_index]);
    VK_CHECK(vkEndCommandBuffer(vl.command_buffers[image_index]));
	//----END RENDER PASS----

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
    submit_info.pCommandBuffers = &vl.command_buffers[image_index];
    VkSemaphore signal_semaphores[] = {render_finished_semaphores[current_frame]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    vkResetFences(vl.device, 1, &in_flight_fences[current_frame]);
    VK_CHECK(vkQueueSubmit(vl.graphics_queue, 1, &submit_info, in_flight_fences[current_frame]));
    //[2]: Return the image to the swapchain for presentation
    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    
    VkSwapchainKHR swapchains[] = {vl.swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = NULL;
    
    //we push the data to be presented to the present queue
    res = vkQueuePresentKHR(vl.present_queue, &present_info); 
    
    //recreate swapchain if necessary
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebuffer_resized)
    {
        framebuffer_resized = FALSE;
        vl_recreate_swapchain();
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
    f32 current_time = get_time();
    f32 delta = current_time - prev_time;
    nb_frames++;
    if ( delta >= 1.0f){
        f32 fps = nb_frames / delta;
        char frames[256];
        sprintf(frames, "FPS: %f", fps);
		window_set_window_title(&wnd, frames);
        nb_frames = 0;
        prev_time = current_time;
    }
}
internal void main_loop(void) 
{
    
	while (!window_should_close(&wnd)) {
        calc_fps();
		window_poll_events(&wnd);
        draw_frame();
	}
    
    //vkDeviceWaitIdle(device);
}

internal void cleanup(void) 
{
    vkDeviceWaitIdle(vl.device);  //so we dont close the window while commands are still being executed
    vl_cleanup_swapchain();
    vkDestroyDescriptorSetLayout(vl.device, descriptor_set_layout, NULL);
    //vkDestroyBuffer(vl.device, vertex_buffer, NULL); //validation layer? @check
    vkFreeMemory(vl.device, vertex_buffer_memory, NULL);
    vkDestroyBuffer(vl.device, vertex_buffer, NULL);
    
    vkFreeMemory(vl.device, index_buffer_memory, NULL);
    vkDestroyBuffer(vl.device, index_buffer, NULL); //validation layer? @check
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(vl.device, render_finished_semaphores[i], NULL);
        vkDestroySemaphore(vl.device, image_available_semaphores[i], NULL);
        vkDestroyFence(vl.device, in_flight_fences[i], NULL);
    }
    vkDestroyCommandPool(vl.device, vl.command_pool, NULL);
    vkDestroyDevice(vl.device, NULL);
    vkDestroySurfaceKHR(vl.instance, vl.surface, NULL);
    vkDestroyInstance(vl.instance, NULL);
	window_destroy(&wnd);
}



int main(void) {
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	printf("win init\n");
#else
	printf("glfw init\n");
#endif
	if (!window_init_vulkan(&wnd, "Vulkan Sample", 800, 600))
		printf("Failed to create a window :(\n");
	
    window_set_resize_callback(&wnd, framebuffer_resize_callback);
    
	if(vulkan_init())printf("Vulkan OK");
	
	main_loop();
	
	cleanup();
	return 0;
}

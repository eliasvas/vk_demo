#include "stdio.h"
#include "tools.h"
#include "stdlib.h"
#include "string.h"

#define NOGLFW 1
#include "vkwin.h"
Window wnd;


#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "SPIRV/spirv_reflect.h"

/*
                        TODO:
    1) vertex input reflection (array types remaining)
    2) UBO/descriptor set reflection
    2.5) pipeline specific UBO/desc set allocation and usage
    3) multi framebuffer rendering abstraction (render pass?)
    3.5) pipeline layout dynamic infer
    4) command buffer management (@optional)

*/

internal s32 window_w = 800;
internal s32 window_h = 600;

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			printf("[LINE: %i] Detected Vulkan error: %i \n",__LINE__, err);            \
		}                                                           \
	} while (0);

#define SPV_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		SpvReflectResult err = x;                                           \
		if (err != SPV_REFLECT_RESULT_SUCCESS)                                                    \
		{                                                           \
			printf("[LINE: %i] Detected Spv error: %i \n",__LINE__, err);            \
		}                                                           \
	} while (0);
	
typedef struct Swapchain
{ 
	VkSwapchainKHR swapchain;
	VkImage *images;
	VkFormat image_format;
	VkExtent2D extent;
	VkImageView *image_views;
	VkFramebuffer *framebuffers;
	u32 image_count;
}Swapchain;

typedef enum ShaderVarFormat{
  SHADER_VAR_FORMAT_UNDEFINED           =   0, // = VK_FORMAT_UNDEFINED
  SHADER_VAR_FORMAT_R32_UINT            =  98, // = VK_FORMAT_R32_UINT
  SHADER_VAR_FORMAT_R32_SINT            =  99, // = VK_FORMAT_R32_SINT
  SHADER_VAR_FORMAT_R32_SFLOAT          = 100, // = VK_FORMAT_R32_SFLOAT
  SHADER_VAR_FORMAT_R32G32_UINT         = 101, // = VK_FORMAT_R32G32_UINT
  SHADER_VAR_FORMAT_R32G32_SINT         = 102, // = VK_FORMAT_R32G32_SINT
  SHADER_VAR_FORMAT_R32G32_SFLOAT       = 103, // = VK_FORMAT_R32G32_SFLOAT
  SHADER_VAR_FORMAT_R32G32B32_UINT      = 104, // = VK_FORMAT_R32G32B32_UINT
  SHADER_VAR_FORMAT_R32G32B32_SINT      = 105, // = VK_FORMAT_R32G32B32_SINT
  SHADER_VAR_FORMAT_R32G32B32_SFLOAT    = 106, // = VK_FORMAT_R32G32B32_SFLOAT
  SHADER_VAR_FORMAT_R32G32B32A32_UINT   = 107, // = VK_FORMAT_R32G32B32A32_UINT
  SHADER_VAR_FORMAT_R32G32B32A32_SINT   = 108, // = VK_FORMAT_R32G32B32A32_SINT
  SHADER_VAR_FORMAT_R32G32B32A32_SFLOAT = 109, // = VK_FORMAT_R32G32B32A32_SFLOAT
  SHADER_VAR_FORMAT_R64_UINT            = 110, // = VK_FORMAT_R64_UINT
  SHADER_VAR_FORMAT_R64_SINT            = 111, // = VK_FORMAT_R64_SINT
  SHADER_VAR_FORMAT_R64_SFLOAT          = 112, // = VK_FORMAT_R64_SFLOAT
  SHADER_VAR_FORMAT_R64G64_UINT         = 113, // = VK_FORMAT_R64G64_UINT
  SHADER_VAR_FORMAT_R64G64_SINT         = 114, // = VK_FORMAT_R64G64_SINT
  SHADER_VAR_FORMAT_R64G64_SFLOAT       = 115, // = VK_FORMAT_R64G64_SFLOAT
  SHADER_VAR_FORMAT_R64G64B64_UINT      = 116, // = VK_FORMAT_R64G64B64_UINT
  SHADER_VAR_FORMAT_R64G64B64_SINT      = 117, // = VK_FORMAT_R64G64B64_SINT
  SHADER_VAR_FORMAT_R64G64B64_SFLOAT    = 118, // = VK_FORMAT_R64G64B64_SFLOAT
  SHADER_VAR_FORMAT_R64G64B64A64_UINT   = 119, // = VK_FORMAT_R64G64B64A64_UINT
  SHADER_VAR_FORMAT_R64G64B64A64_SINT   = 120, // = VK_FORMAT_R64G64B64A64_SINT
  SHADER_VAR_FORMAT_R64G64B64A64_SFLOAT = 121, // = VK_FORMAT_R64G64B64A64_SFLOAT
} ShaderVarFormat;

typedef enum ShaderDescType{
  SHADER_DESC_TYPE_SAMPLER                    =  0,        // = VK_DESCRIPTOR_TYPE_SAMPLER
  SHADER_DESC_TYPE_COMBINED_IMAGE_SAMPLER     =  1,        // = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
  SHADER_DESC_TYPE_SAMPLED_IMAGE              =  2,        // = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
  SHADER_DESC_TYPE_STORAGE_IMAGE              =  3,        // = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
  SHADER_DESC_TYPE_UNIFORM_TEXEL_BUFFER       =  4,        // = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
  SHADER_DESC_TYPE_STORAGE_TEXEL_BUFFER       =  5,        // = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
  SHADER_DESC_TYPE_UNIFORM_BUFFER             =  6,        // = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
  SHADER_DESC_TYPE_STORAGE_BUFFER             =  7,        // = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
  SHADER_DESC_TYPE_UNIFORM_BUFFER_DYNAMIC     =  8,        // = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
  SHADER_DESC_TYPE_STORAGE_BUFFER_DYNAMIC     =  9,        // = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
  SHADER_DESC_TYPE_INPUT_ATTACHMENT           = 10,        // = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
  SHADER_DESC_TYPE_ACCELERATION_STRUCTURE_KHR = 1000150000 // = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
} ShaderDescType;

typedef enum RPrimitiveTopology{
    RPRIMITIVE_TOPOLOGY_POINT_LIST = 0, // =VK_PRIMITIVE_TOPOLOGY_POINT_LIST 
    RPRIMITIVE_TOPOLOGY_LINE_LIST = 1, // =VK_PRIMITIVE_TOPOLOGY_LINE_LIST 
    RPRIMITIVE_TOPOLOGY_LINE_STRIP = 2, // =VK_PRIMITIVE_TOPOLOGY_LINE_STRIP 
    RPRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3, // =VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    RPRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 4, // =VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP 
    RPRIMITIVE_TOPOLOGY_TRIANGLE_FAN = 5, // =VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN 
} RPrimitiveTopology;

typedef struct ShaderDescriptorBinding
{
    char name[64];
    u32 binding; // bind point of the descriptor (e.g layout(binding = 2) )
    u32 set; // set this descriptor belongs to (e.g layout(set = 0))
    ShaderDescType desc_type;
}ShaderDescriptorBinding;

//TODO: finish this or find another way to infer size of a shader variable
internal u32 get_format_size(ShaderVarFormat format)
{
    switch(format)
    {
        case SHADER_VAR_FORMAT_R32G32B32_SFLOAT:
            return 3 * sizeof(f32);
        case SHADER_VAR_FORMAT_R32G32_SFLOAT:
            return 2 * sizeof(f32);
        case SHADER_VAR_FORMAT_R32_SFLOAT:
            return sizeof(f32);
        default:
            return 0;
    }
}

typedef struct VertexInputAttribute
{
    u32 location;
    u8 name[64];
    b32 builtin;
    u32 array_count;
    u32 size; //size of attribute in bytes
    ShaderVarFormat format;
}VertexInputAttribute;


#define MAX_RESOURCES_PER_SHADER 32
typedef struct ShaderMetaInfo
{

    VertexInputAttribute vertex_input_attributes[MAX_RESOURCES_PER_SHADER];
    u32 vertex_input_attribute_count;

    ShaderDescriptorBinding descriptor_bindings[MAX_RESOURCES_PER_SHADER];
    u32 descriptor_count;



	u32 input_variable_count;
}ShaderMetaInfo;

internal u32 calc_vertex_input_total_size(ShaderMetaInfo *info)
{
    u32 s = 0;
    for (u32 i = 0; i < info->vertex_input_attribute_count; ++i)
        s += info->vertex_input_attributes[i].size;
    return s;
}

internal VkVertexInputBindingDescription get_bind_desc(ShaderMetaInfo *info)
{
    VkVertexInputBindingDescription bind_desc = {0};
    bind_desc.binding = 0;
    bind_desc.stride = calc_vertex_input_total_size(info);
    bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//per-vertex
    return bind_desc;
}

internal u32 get_attr_desc(VkVertexInputAttributeDescription *attr_desc, ShaderMetaInfo *info)
{
    u32 global_offset = 0;
    u32 valid_attribs = 0;
    for (u32 i = 0; i < info->vertex_input_attribute_count; ++i)
    {
        for (u32 j = 0; j < info->vertex_input_attribute_count; ++j)
        {
            u32 location = info->vertex_input_attributes[i].location;
            if (location == i)
            {
                attr_desc[location].binding = 0;
                attr_desc[location].location = location; 
                attr_desc[location].format = info->vertex_input_attributes[i].format;
                attr_desc[location].offset = global_offset;
                global_offset+= info->vertex_input_attributes[i].size;
                valid_attribs++;
                break;
            }
   
        }

    }
    //attr_desc[2].offset = offsetof(Vertex, tex_coord);
    return valid_attribs;
}


typedef struct ShaderObject
{
	VkShaderModule module;
	VkShaderStageFlagBits stage;
	
	ShaderMetaInfo info;
	
	b32 uses_push_constants;
}ShaderObject;


typedef struct PipelineObject
{
    ShaderObject vert_shader;
    ShaderObject frag_shader;
    ShaderObject compute_shader;

    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
}PipelineObject;

typedef struct VulkanLayer
{
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkSurfaceKHR surface;
	
	VkQueue graphics_queue;
	VkQueue present_queue;


	//---------------------------------
	Swapchain swap;
	//----------------------------------
	VkImage depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView depth_image_view;
	
	
	VkCommandPool command_pool;
	VkCommandBuffer *command_buffers;
	
	VkRenderPass render_pass;
	

    PipelineObject fullscreen_pipe;

    PipelineObject base_pipe;
}VulkanLayer;


global VulkanLayer vl;


//------SPIRV_REFLECT------

int SpirvReflectExample(const void* spirv_code, size_t spirv_nbytes)
{
  // Generate reflection data for a shader
  SpvReflectShaderModule module;
  SPV_CHECK(spvReflectCreateShaderModule(spirv_nbytes, spirv_code, &module));

  // Enumerate and extract shader's input variables
  uint32_t var_count = 0;
  SPV_CHECK(spvReflectEnumerateInputVariables(&module, &var_count, NULL));
  
  SpvReflectInterfaceVariable** input_vars =
    (SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
  
  SPV_CHECK(spvReflectEnumerateInputVariables(&module, &var_count, input_vars));

  // Output variables, descriptor bindings, descriptor sets, and push constants
  // can be enumerated and extracted using a similar mechanism.

  // Destroy the reflection data when no longer required.
  spvReflectDestroyShaderModule(&module);
}

//-------------------------

//-----------------BUFFER-------------------
typedef struct DataBuffer
{
	VkDevice device;
	VkBuffer buffer;
	VkDeviceMemory mem;
	VkDescriptorBufferInfo desc;
	VkDeviceSize size;
	VkDeviceSize alignment;
	VkBufferUsageFlags usage_flags;
	VkMemoryPropertyFlags memory_property_flags;
	void *mapped;
}DataBuffer;


//attaches ALLOCATED memory block to buffer!
internal void buf_bind(DataBuffer *buf, VkDevice offset)
{
	vkBindBufferMemory(buf->device, buf->buffer, buf->mem, offset);
}

internal buf_copy_to(DataBuffer *src,void *data, VkDeviceSize size)
{
	assert(src->mapped);
	memcpy(src->mapped, data, size);
}

internal void buf_setup_descriptor(DataBuffer *buf, VkDeviceSize size, VkDeviceSize offset)
{
	buf->desc.offset = offset;
	buf->desc.buffer = buf->buffer;
	buf->desc.range = size;
}

internal VkResult buf_map(DataBuffer *buf, VkDeviceSize size, VkDeviceSize offset)
{
	return vkMapMemory(buf->device, buf->mem, offset, size, 0, &buf->mapped);//@check @check @check @check
}

internal void buf_unmap(DataBuffer *buf)
{
	if (buf->mapped)
	{
		vkUnmapMemory(buf->device, buf->mem);
		buf->mapped = NULL;
	}
}

internal void buf_destroy(DataBuffer *buf)
{
	if (buf->buffer)
	{
		vkDestroyBuffer(buf->device, buf->buffer, NULL);
	}
	if (buf->mem)
	{
		vkFreeMemory(buf->device, buf->mem, NULL);
	}
}

//-----------------BUFFER-------------------




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

internal VkPipelineVertexInputStateCreateInfo 
pipe_vertex_input_state_create_info(ShaderObject *shader, VkVertexInputBindingDescription *bind_desc, VkVertexInputAttributeDescription *attr_desc)
{


    *bind_desc = get_bind_desc(&shader->info);
    u32 attribute_count = get_attr_desc(attr_desc, &shader->info);
 
	VkPipelineVertexInputStateCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	info.pNext = NULL;
	info.vertexBindingDescriptionCount = 1;
    info.pVertexBindingDescriptions = bind_desc;
	info.vertexAttributeDescriptionCount = attribute_count;
    info.pVertexAttributeDescriptions = attr_desc;

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
internal VkPipelineLayoutCreateInfo pipe_layout_create_info(VkDescriptorSetLayout *layouts, u32 layouts_count)
{
	VkPipelineLayoutCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = NULL;
	
	info.flags = 0;
	info.setLayoutCount = layouts_count;
	info.pSetLayouts = layouts;
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
    viewport.width = (f32)vl.swap.extent.width;
    viewport.height = (f32)vl.swap.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {0};
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = vl.swap.extent;
	
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
    color_attachment.format = vl.swap.image_format;
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


internal VkDescriptorSetLayout shader_create_descriptor_set_layout(ShaderObject *vert, ShaderObject *frag, u32 set_count)
{
    VkDescriptorSetLayout layout;

    VkDescriptorSetLayoutBinding bindings[32];
    u32 binding_count = 0;
    for (u32 i = 0; i < vert->info.descriptor_count; ++i)
    {
        VkDescriptorSetLayoutBinding binding = {0};
        binding.binding = vert->info.descriptor_bindings[i].binding;
        binding.descriptorType = vert->info.descriptor_bindings[i].desc_type;
        binding.descriptorCount = set_count; //N if we want an array of descriptors (dset?)
        binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        binding.pImmutableSamplers = NULL;

        //add the binding to the array
        bindings[binding_count++] = binding;
    }
    for (u32 i = 0; i < frag->info.descriptor_count; ++i)
    {
        VkDescriptorSetLayoutBinding binding = {0};
        binding.binding = frag->info.descriptor_bindings[i].binding;
        binding.descriptorType = frag->info.descriptor_bindings[i].desc_type;
        binding.descriptorCount = set_count; //N if we want an array of descriptors (dset?)
        binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        binding.pImmutableSamplers = NULL;

        //add the binding to the array
        bindings[binding_count++] = binding;
    }


    VkDescriptorSetLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = binding_count;
    layout_info.pBindings = bindings;
    
    VK_CHECK(vkCreateDescriptorSetLayout(vl.device, &layout_info, NULL, &layout));
    return layout;
}


internal void shader_reflect(u32 *shader_code, u32 code_size, ShaderMetaInfo *info)
{
	// Generate reflection data for a shader
	SpvReflectShaderModule module;
	SPV_CHECK(spvReflectCreateShaderModule(code_size, shader_code, &module));
	
    //input attributes 
	u32 var_count = 0;
	SPV_CHECK(spvReflectEnumerateInputVariables(&module, &var_count, NULL));
	SpvReflectInterfaceVariable** input_vars =
	  (SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
	SPV_CHECK(spvReflectEnumerateInputVariables(&module, &var_count, input_vars));
    info->vertex_input_attribute_count = var_count;
    for (u32 i = 0; i < var_count; ++i)
    {
        VertexInputAttribute *attr = &info->vertex_input_attributes[i];
        SpvReflectInterfaceVariable *curvar = input_vars[i];

        attr->location = curvar->location;
        attr->format = curvar->format;
        attr->builtin = curvar->built_in;

        if (curvar->array.dims_count > 0)attr->array_count = curvar->array.dims[0];
        else attr->array_count = 1;

        attr->size = get_format_size(attr->format) * attr->array_count; //@THIS IS TEMPORARY (DELETE THIS)

        sprintf(attr->name, input_vars[i]->name);
        //printf("vertex input variable %s is at location: %i[F%i], size=%i\n", attr->name, attr->location, attr->format, attr->size);
    }
    //descriptor set (UBO)

    u32 desc_set_count = 0;
	SPV_CHECK(spvReflectEnumerateDescriptorSets(&module, &desc_set_count, NULL));
    if (desc_set_count)
    {
        SpvReflectDescriptorSet** descriptor_sets=
          (SpvReflectDescriptorSet**)malloc(desc_set_count * sizeof(SpvReflectDescriptorSet*));
        SPV_CHECK(spvReflectEnumerateDescriptorSets(&module, &desc_set_count, descriptor_sets));
        SpvReflectDescriptorBinding **bindings = descriptor_sets[0]->bindings;
        info->descriptor_count = descriptor_sets[0]->binding_count;
        for (u32 i = 0; i < info->descriptor_count; ++i)
        {
            SpvReflectDescriptorBinding *desc_info = bindings[i];
            ShaderDescriptorBinding *desc_binding = &info->descriptor_bindings[i];


            desc_binding->binding = desc_info->binding;
            desc_binding->set = desc_info->set;
            desc_binding->desc_type = desc_info->descriptor_type;
            sprintf(desc_binding->name, desc_info->name);


            printf("DESC: %s(set =%i, binding = %i)[%i]\n", desc_binding->name, desc_binding->set,desc_binding->binding, desc_binding->desc_type);
        }
    }

	
	spvReflectDestroyShaderModule(&module);
}

//@BEWARE(inv): runtime shader compilation should generally be avoided as it is too slow to call the command line!
//one other option would be to use glslang BUT its a big dependency and I don't really need it.
#define SHADER_DST_DIR "shaders/"
#define SHADER_SRC_DIR "../assets/shaders/"
internal void shader_create_dynamic(VkDevice device, ShaderObject *shader, const char *filename, VkShaderStageFlagBits stage)
{
    char command[256];
    char new_file[256];
    sprintf(new_file, "%s%s.spv", SHADER_DST_DIR, filename);
    sprintf(command,"glslangvalidator -V %s%s -o %s", SHADER_SRC_DIR, filename, new_file);
    system(command);
    
	u32 code_size;
	u32 *shader_code = NULL;
	read_file(new_file, &shader_code, &code_size);
	shader->module = create_shader_module(shader_code, code_size);
	shader->uses_push_constants = FALSE;
    shader_reflect(shader_code, code_size, &shader->info);
	printf("Shader: %s has %i input variable(s)!\n", filename, shader->info.input_variable_count);
	shader->stage = stage;
	free(shader_code);
    //remove(new_file);
}  


internal void shader_create(VkDevice device, ShaderObject *shader, const char *filename, VkShaderStageFlagBits stage)
{
    char path[256];
    sprintf(path, "%s%s.spv", SHADER_DST_DIR, filename);
	u32 code_size;
	u32 *shader_code = NULL;
	if (read_file(path, &shader_code, &code_size) == -1){shader_create_dynamic(device, shader, filename, stage);return;};
	shader->module = create_shader_module(shader_code, code_size);
	shader->uses_push_constants = FALSE;
    shader_reflect(shader_code, code_size, &shader->info);
	printf("Shader: %s has %i input variable(s)!\n", filename, shader->info.input_variable_count);
	shader->stage = stage;
	free(shader_code);
}  

internal void pipeline_build_basic(PipelineObject *p,const char *vert, const char *frag, RPrimitiveTopology topology)
{

    VkVertexInputBindingDescription bind_desc;
    VkVertexInputAttributeDescription attr_desc[32];
	

    PipelineBuilder pb = {0};
	shader_create(vl.device, &p->vert_shader, vert, VK_SHADER_STAGE_VERTEX_BIT); 
	shader_create(vl.device, &p->frag_shader, frag, VK_SHADER_STAGE_FRAGMENT_BIT);
	pb.shader_stages_count = 2;
	pb.shader_stages[0] = pipe_shader_stage_create_info(p->vert_shader.stage, p->vert_shader.module);
	pb.shader_stages[1] = pipe_shader_stage_create_info(p->frag_shader.stage, p->frag_shader.module);
    pb.vertex_input_info = pipe_vertex_input_state_create_info(&p->vert_shader, &bind_desc, attr_desc);
	pb.input_asm = pipe_input_assembly_create_info(topology);
	pb.rasterizer = pipe_rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pb.multisampling = pipe_multisampling_state_create_info();
	pb.color_blend_attachment = pipe_color_blend_attachment_state();

    VkDescriptorSetLayout layout = shader_create_descriptor_set_layout(&p->vert_shader, &p->frag_shader, 1);
    VkPipelineLayoutCreateInfo info = pipe_layout_create_info(&layout, 1);
	VK_CHECK(vkCreatePipelineLayout(vl.device, &info, NULL, &pb.pipeline_layout));
	VK_CHECK(vkCreatePipelineLayout(vl.device, &info, NULL, &p->pipeline_layout));
	p->pipeline = build_pipeline(vl.device, pb, vl.render_pass);
}

internal void vl_base_pipelines_init(void)
{

    pipeline_build_basic(&vl.fullscreen_pipe, "fullscreen.vert", "fullscreen.frag", RPRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_build_basic(&vl.base_pipe, "shader.vert", "shader.frag", RPRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
}




VkDescriptorPool descriptor_pool;
VkDescriptorSet *descriptor_sets;


const int MAX_FRAMES_IN_FLIGHT = 2;
VkSemaphore *image_available_semaphores;
VkSemaphore *render_finished_semaphores;


VkFence *in_flight_fences;
VkFence *images_in_flight;

//VkBuffer vertex_buffer;
//VkDeviceMemory vertex_buffer_memory;

DataBuffer vertex_buffer_real;
DataBuffer index_buffer_real;
DataBuffer *uniform_buffers;


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
    
    VK_CHECK(vkCreateSwapchainKHR(vl.device, &create_info, NULL, &vl.swap.swapchain));
    
    vkGetSwapchainImagesKHR(vl.device, vl.swap.swapchain, &image_count, NULL);
    vl.swap.images = malloc(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(vl.device, vl.swap.swapchain, &image_count, vl.swap.images);
    vl.swap.image_format = surface_format.format;
    vl.swap.extent = extent;
    vl.swap.image_count = image_count;//TODO(ilias): check
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
    vl.swap.image_views = malloc(sizeof(VkImageView) * vl.swap.image_count);
    for (u32 i = 0; i < vl.swap.image_count; ++i)
		vl.swap.image_views[i] = create_image_view(vl.swap.images[i], vl.swap.image_format,VK_IMAGE_ASPECT_COLOR_BIT);
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



internal void create_descriptor_sets(VkDescriptorSetLayout layout)
{
    VkDescriptorSetLayout *layouts = malloc(sizeof(VkDescriptorSetLayout)*vl.swap.image_count);
    for (u32 i = 0; i < vl.swap.image_count; ++i)
        layouts[i] = layout;
    VkDescriptorSetAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = vl.swap.image_count;
    alloc_info.pSetLayouts = layouts;
    
    descriptor_sets = malloc(sizeof(VkDescriptorSet) * vl.swap.image_count);
    VK_CHECK(vkAllocateDescriptorSets(vl.device, &alloc_info, descriptor_sets));
    
    for (size_t i = 0; i < vl.swap.image_count; i++) {
        VkDescriptorBufferInfo buffer_info = {0};
        buffer_info.buffer = uniform_buffers[i].buffer;
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
    pool_size[0].descriptorCount = vl.swap.image_count;
	pool_size[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size[1].descriptorCount = vl.swap.image_count;
    
    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = array_count(pool_size);
    pool_info.pPoolSizes = pool_size;
    pool_info.maxSets = vl.swap.image_count;
    
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
	
	
	VkDescriptorSetLayoutBinding bindings[2] = {ubo_layout_binding, sampler_layout_binding};
    
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
    color_attachment.format = vl.swap.image_format;
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
    vl.swap.framebuffers = malloc(sizeof(VkFramebuffer) * vl.swap.image_count); 
    for (u32 i = 0; i < vl.swap.image_count; ++i)
    {
        VkImageView attachments[] = {vl.swap.image_views[i], vl.depth_image_view};
        
        VkFramebufferCreateInfo framebuffer_info = {0};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vl.render_pass; //VkFramebuffers need a render pass?
        framebuffer_info.attachmentCount = array_count(attachments);
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = vl.swap.extent.width;
        framebuffer_info.height = vl.swap.extent.height;
        framebuffer_info.layers = 1;
        
        VK_CHECK(vkCreateFramebuffer(vl.device, &framebuffer_info, NULL, &vl.swap.framebuffers[i]));
        
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
    renderpass_info.framebuffer = vl.swap.framebuffers[image_index]; //we bind a _framebuffer_ to a render pass
    renderpass_info.renderArea.offset = (VkOffset2D){0,0};
    renderpass_info.renderArea.extent = vl.swap.extent;
	VkClearValue clear_values[2] = {0};
	clear_values[0].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 1.0f}};
	clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    renderpass_info.clearValueCount = array_count(clear_values);
    renderpass_info.pClearValues = &clear_values;
    vkCmdBeginRenderPass(command_buf, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
	*/	
        
	vkCmdBindPipeline(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vl.base_pipe.pipeline);
		
    VkBuffer vertex_buffers[] = {vertex_buffer_real.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buf, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(command_buf, index_buffer_real.buffer, 0, VK_INDEX_TYPE_UINT32);
        
    vkCmdBindDescriptorSets(vl.command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                vl.base_pipe.pipeline_layout, 0, 1, &descriptor_sets[image_index], 0, NULL);
    vkCmdDrawIndexed(command_buf, array_count(cube_indices), 1, 0, 0, 0);
	
    
    //VK_CHECK(vkEndCommandBuffer(command_buf));
}


internal void render_fullscreen(VkCommandBuffer command_buf, u32 image_index)
{    
	vkCmdBindPipeline(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vl.fullscreen_pipe.pipeline);
	
    vkCmdDraw(command_buf, 4, 1, 0, 0);
}

internal void vl_create_command_buffers(void)
{
    vl.command_buffers = malloc(sizeof(VkCommandBuffer) * vl.swap.image_count);
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = vl.command_pool; //where to allocate the buffer from
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = vl.swap.image_count;
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

internal void create_buffer_simple(u32 buffer_size,VkBufferUsageFlagBits usage, VkMemoryPropertyFlagBits mem_flags, VkBuffer *buf, VkDeviceMemory *mem)
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


internal void create_buffer(VkBufferUsageFlagBits usage, VkMemoryPropertyFlagBits mem_flags, DataBuffer *buf, VkDeviceSize size, void *data)
{
	buf->device = vl.device;
	
	//[0]: create buffer handle
    VkBufferCreateInfo buffer_info = {0};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(vl.device, &buffer_info, NULL, &(buf->buffer) ));
    
	//[1]: create the memory nacking up the buffer handle
    VkMemoryRequirements mem_req = {0};
    vkGetBufferMemoryRequirements(buf->device, (buf->buffer), &mem_req);
    
    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_mem_type(mem_req.memoryTypeBits, mem_flags);
    VK_CHECK(vkAllocateMemory(vl.device, &alloc_info, NULL, &buf->mem));
	
	//[2]: set some important data fields
	buf->alignment = mem_req.alignment;
	buf->size = size;
	buf->usage_flags = usage;
	//buf->memory_property_flags = properties; //HOST_COHERENT HOST_VISIBLE etc.
	
	//[3]:if data pointer has data, we map the buffer and copy those data over to OUR buffer
	if (data != NULL)
	{
		VK_CHECK(buf_map(buf, size, 0));
		memcpy(buf->mapped, data, size);
		buf_unmap(buf);
	}
	
	//[4]: we initialize a default descriptor that covers the whole buffer size
	buf_setup_descriptor(buf, size, 0);
	
	//[5]: we attach the memory to the buffer object
    vkBindBufferMemory(vl.device, (buf->buffer), (buf->mem), 0);
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
	create_image(vl.swap.extent.width, vl.swap.extent.height, 
		depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vl.depth_image, &vl.depth_image_memory);
	vl.depth_image_view = create_image_view(vl.depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
}



internal void create_uniform_buffers(void)
{
    VkDeviceSize buf_size = sizeof(UniformBufferObject);
    uniform_buffers = (DataBuffer*)malloc(sizeof(DataBuffer) * vl.swap.image_count);
    
    for (u32 i = 0; i < vl.swap.image_count; ++i)
    {
        create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&uniform_buffers[i], buf_size, NULL);
    }
}



internal void create_sync_objects(void)
{
    image_available_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    in_flight_fences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    images_in_flight = malloc(sizeof(VkFence) * vl.swap.image_count);
    for (u32 i = 0; i < vl.swap.image_count; ++i)images_in_flight[i] = VK_NULL_HANDLE;
    
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
    for (u32 i = 0; i < vl.swap.image_count; ++i)
        vkDestroyFramebuffer(vl.device, vl.swap.framebuffers[i], NULL);
	vkDestroyPipeline(vl.device, vl.fullscreen_pipe.pipeline, NULL);
    vkDestroyRenderPass(vl.device, vl.render_pass, NULL);
    for (u32 i = 0; i < vl.swap.image_count; ++i)
        vkDestroyImageView(vl.device, vl.swap.image_views[i], NULL);
    vkDestroySwapchainKHR(vl.device, vl.swap.swapchain, NULL);
    //these will be recreated at pipeline creation for next swapchain
    //vkDestroyShaderModule(vl.device, vl.base_vert.module, NULL);
    //vkDestroyShaderModule(vl.device, vl.base_frag.module, NULL);
    for (u32 i = 0; i < vl.swap.image_count; ++i)
    {
        buf_destroy(&uniform_buffers[i]);
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
    
    
	vl_base_pipelines_init();

    VkDescriptorSetLayout layout = shader_create_descriptor_set_layout(&vl.base_pipe.vert_shader, &vl.base_pipe.frag_shader, 1);
    create_descriptor_sets(layout);

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
	
	
	//[2]: we create a buffer to hold the pixel information (we also fill it)
	DataBuffer idb;
	if (!pixels)
		vk_error("Error loading image!");
	create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &idb, image_size, pixels);
	//[3]: we free the cpu side image, we don't need it
	stbi_image_free(pixels);
	//[4]: we create the VkImage that is undefined right now
	create_image(tex_w, tex_h, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT 
		| VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &tex.image, &tex.device_mem);
	
	//[5]: we transition the images layout from undefined to dst_optimal
	transition_image_layout(tex.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	//[6]: we copy the buffer we created in [1] to our image of the correct format (R8G8B8A8_SRGB)
	copy_buffer_to_image(idb.buffer, tex.image, tex_w, tex_h);
	
	//[7]: we transition the image layout so that it can be read by a shader
	transition_image_layout(tex.image, format, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		
	//[8]:cleanup the buffers (all data is now in the image)
	buf_destroy(&idb);
	
	
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
	vl_base_pipelines_init();
	
	sample_texture = create_texture_image("../assets/test.png",VK_FORMAT_R8G8B8A8_SRGB);

	
	Vertex *cube_vertices = cube_build_verts();
	//create vertex buffer @check first param
	create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
	&vertex_buffer_real, sizeof(Vertex) * 24, cube_vertices);
	
	
	
	//create index buffer
	create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
	&index_buffer_real, sizeof(cube_indices[0]) * array_count(cube_indices), cube_indices);
	
	
    create_uniform_buffers();
    create_descriptor_pool();
    VkDescriptorSetLayout layout = shader_create_descriptor_set_layout(&vl.base_pipe.vert_shader, &vl.base_pipe.frag_shader, 1);
    create_descriptor_sets(layout);
    
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

	void *data = &ubo;
	
	VK_CHECK(buf_map(&uniform_buffers[image_index], uniform_buffers[image_index].size, 0));
	memcpy(uniform_buffers[image_index].mapped, &ubo, sizeof(ubo));
	buf_unmap(&uniform_buffers[image_index]);
}

internal void draw_frame(void)
{
	
    vkWaitForFences(vl.device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
    //[0]: Acquire free image from the swapchain
    u32 image_index;
    VkResult res = vkAcquireNextImageKHR(vl.device, vl.swap.swapchain, UINT64_MAX, 
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
    renderpass_info.framebuffer = vl.swap.framebuffers[image_index]; //we bind a _framebuffer_ to a render pass
    renderpass_info.renderArea.offset = (VkOffset2D){0,0};
    renderpass_info.renderArea.extent = vl.swap.extent;
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
    
    VkSwapchainKHR swapchains[] = {vl.swap.swapchain};
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
        sprintf(frames, "|vulkan demo|  %f fps", fps);
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
    
	buf_destroy(&index_buffer_real);
	buf_destroy(&vertex_buffer_real);
	
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


int main(void) 
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	printf("win init\n");
#else
	printf("glfw init\n");
#endif
	if (!window_init_vulkan(&wnd, "Vulkan Sample", 800, 600))
		printf("Failed to create a window :(\n");
	
    window_set_resize_callback(&wnd, framebuffer_resize_callback);
    
	if(vulkan_init())printf("Vulkan OK\n");
	
	
	main_loop();
	
	cleanup();
	//spvc_context_destroy(context);
	return 0;
}

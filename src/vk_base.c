//#include "stdio.h"

#include "tools.h"
#include "stdlib.h"
#include "string.h"

#define VK_STANDALONE 1
#define USE_VOLK 1

#ifdef VK_STANDALONE
    #define NOGLFW 1
    #include "vkwin.h"
    Window wnd;
#else
    #include "bconfig.h"
    extern int vk_getWidth();
    extern int vk_getHeight();
    extern HWND vk_getWindowSystemHandle();
    extern HINSTANCE vk_getWindowSystemInstance();
    void vk_error(char* text)
    {
        printf("%s\n", text);
    }
    #include "vk_base.h"
#ifdef USE_VOLK    
    #define VK_NO_PROTOTYPES 1
    #define VOLK_IMPLEMENTATION
    #include "vulkan/volk.h"
#else
    #include "vulkan/vulkan.h"
#endif

#endif


#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 

#include "spirv_reflect.h"

s32 window_w = 800;
s32 window_h = 600;


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
	
#ifdef VK_STANDALONE

#define MAX_SWAP_IMAGE_COUNT 4



typedef struct TextureObject {
    VkSampler sampler;
    VkImage image;
    VkImageLayout image_layout;
    VkDeviceMemory mem;
    VkImageView view;
    VkFormat format;
    u32 width, height;
    u32 mip_levels;
} TextureObject;


typedef struct Swapchain
{ 
	VkSwapchainKHR swapchain;
	VkImage *images;
	VkFormat image_format;
	VkExtent2D extent;
	VkImageView *image_views;
	VkFramebuffer *framebuffers;
	TextureObject depth_attachment;
	u32 image_count;
	
	VkRenderPass rp_begin;
}Swapchain;


#define MAX_ATTACHMENTS_COUNT 4
typedef struct FrameBufferObject
{
	u32 width, height; //should framebuffers be RESIZED when the swapchain resizes??????
	VkFramebuffer framebuffer;
	TextureObject depth_attachment;
	b32 has_depth;
	TextureObject attachments[MAX_ATTACHMENTS_COUNT]; //pos,color,normal?
	u32 attachment_count;
	VkRenderPass render_renderpass;
	VkRenderPass clear_renderpass;
}FrameBufferObject;




typedef enum ShaderReflectTypeFlagBits{
  SHADER_REFLECT_TYPE_FLAG_UNDEFINED                       = 0x00000000,
  SHADER_REFLECT_TYPE_FLAG_VOID                            = 0x00000001,
  SHADER_REFLECT_TYPE_FLAG_BOOL                            = 0x00000002,
  SHADER_REFLECT_TYPE_FLAG_INT                             = 0x00000004,
  SHADER_REFLECT_TYPE_FLAG_FLOAT                           = 0x00000008,
  SHADER_REFLECT_TYPE_FLAG_VECTOR                          = 0x00000100,
  SHADER_REFLECT_TYPE_FLAG_MATRIX                          = 0x00000200,
  SHADER_REFLECT_TYPE_FLAG_EXTERNAL_IMAGE                  = 0x00010000,
  SHADER_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLER                = 0x00020000,
  SHADER_REFLECT_TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE          = 0x00040000,
  SHADER_REFLECT_TYPE_FLAG_EXTERNAL_BLOCK                  = 0x00080000,
  SHADER_REFLECT_TYPE_FLAG_EXTERNAL_ACCELERATION_STRUCTURE = 0x00100000,
  SHADER_REFLECT_TYPE_FLAG_EXTERNAL_MASK                   = 0x00FF0000,
  SHADER_REFLECT_TYPE_FLAG_STRUCT                          = 0x10000000,
  SHADER_REFLECT_TYPE_FLAG_ARRAY                           = 0x20000000,
} ShaderReflectTypeFlagBits;

typedef struct ShaderReflectNumTraits{
  struct scalar {
    u32 width;
    u32 signedness;
  } scalar;

  struct vector {
    u32 component_count;
  } vector;

  struct matrix {
    u32 column_count;
    u32 row_count;
    u32 stride; // Measured in bytes
  } matrix;
} ShaderReflectNumTraits;

typedef struct UniformVariable
{
    char name[64];
    u32 size;
    u32 array_count;
    u32 padded_size;
    u32 offset;
    u32 type_flags;
}UniformVariable;

typedef struct ShaderDescriptorBinding
{
    char name[64];
    u32 binding; // bind point of the descriptor (e.g layout(binding = 2) )
    u32 set; // set this descriptor belongs to (e.g layout(set = 0))

    UniformVariable members[32];
    u32 member_count;

    void *mem;
    u32 mem_size;

    VkDescriptorType desc_type; //ubo/image sampler etc..
}ShaderDescriptorBinding;

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
    void* mapped;
    b32 active;
}DataBuffer;

typedef struct VertexInputAttribute
{
    u32 location;
    u8 name[64];
    b32 builtin; //in case builtin, we don't really use it
    u32 array_count;
    u32 size; //size of attribute in bytes
    VkFormat format;
}VertexInputAttribute;


typedef struct OutVar 
{
    u32 location;
    u8 name[64];
    b32 builtin; //in case builtin, we don't really use it
    u32 array_count;
    u32 size; //size of attribute in bytes
    VkFormat format;
}OutVar;


#define MAX_RESOURCES_PER_SHADER 32

typedef struct ShaderMetaInfo
{

    VertexInputAttribute vertex_input_attributes[MAX_RESOURCES_PER_SHADER];
    u32 vertex_input_attribute_count;

    OutVar out_vars[MAX_RESOURCES_PER_SHADER];
    u32 out_var_count;

    ShaderDescriptorBinding descriptor_bindings[MAX_RESOURCES_PER_SHADER];
    u32 descriptor_count;



    u32 input_variable_count;
}ShaderMetaInfo;
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

    // do we really need the descriptor pool? (maybe have them in a cache??)
    VkDescriptorPool descriptor_pools[16]; //pools need to be reallocated with every swapchain recreation! @TODO
    VkDescriptorSet* descriptor_sets;
    DataBuffer* uniform_buffers;
}PipelineObject;
typedef struct VulkanLayer
{
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkSurfaceKHR surface;
    VkDebugUtilsMessengerEXT debug_msg;

    VkQueue graphics_queue;
    VkQueue present_queue;


    //---------------------------------
    Swapchain swap;
    //----------------------------------
	VkRenderPass swap_rp_begin;
	VkRenderPass render_pass_basic;

    VkCommandPool command_pool;
    VkCommandBuffer* command_buffers;

    


    PipelineObject fullscreen_pipe;
    PipelineObject def_pipe;
    //PipelineObject def_pipe_line;
    PipelineObject base_pipe;

    u32 image_index; //current image index to draw

}VulkanLayer;

typedef struct MultiAllocBuffer
{
    DataBuffer gpu_side_buffer;
    void* global_buffer;
    u32 current_offset;
}MultiAllocBuffer;

VulkanLayer vl;
#else 
extern VulkanLayer vl;
#endif
FrameBufferObject fbo1;
//TODO: finish this or find another way to infer size of a shader variable
u32 get_format_size(VkFormat format)
{
    switch(format)
    {
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 4 * sizeof(f32);
        case VK_FORMAT_R32G32B32_SFLOAT:
            return 3 * sizeof(f32);
        case VK_FORMAT_R32G32_SFLOAT:
            return 2 * sizeof(f32);
        case VK_FORMAT_R32_SFLOAT:
            return sizeof(f32);
        default:
            return 0;
    }
}







u32 calc_vertex_input_total_size(ShaderMetaInfo* info)
{
    u32 s = 0;
    for (u32 i = 0; i < info->vertex_input_attribute_count; ++i)
        s += info->vertex_input_attributes[i].size;
    return s;
}
VkVertexInputBindingDescription get_bind_desc(ShaderMetaInfo *info)
{
    VkVertexInputBindingDescription bind_desc = {0};
    bind_desc.binding = 0;
    bind_desc.stride = calc_vertex_input_total_size(info);
    bind_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//per-vertex
    return bind_desc;
}

u32 get_attr_desc(VkVertexInputAttributeDescription *attr_desc, ShaderMetaInfo *info)
{
    u32 global_offset = 0;
    u32 valid_attribs = 0;
    for (u32 i = 0; i < info->vertex_input_attribute_count; ++i)
    {
        u32 location = info->vertex_input_attributes[i].location;
        for (u32 j = 0; j < info->vertex_input_attribute_count; ++j)
        {
            if (location == j)
            {
                attr_desc[location].binding = 0;
                attr_desc[location].location = location;
                attr_desc[location].format = (VkFormat)info->vertex_input_attributes[j].format;
                attr_desc[location].offset = global_offset;
                global_offset+= info->vertex_input_attributes[j].size;
                valid_attribs++;
                break;
            }
        }
    }
    //attr_desc[2].offset = offsetof(Vertex, tex_coord);
    return valid_attribs;
}


void texture_cleanup(TextureObject *t)
{
    vkDeviceWaitIdle(vl.device);
    vkQueueWaitIdle(vl.graphics_queue);
    vkDestroySampler(vl.device, t->sampler, NULL);
    vkDestroyImageView(vl.device, t->view, NULL);
    vkDestroyImage(vl.device, t->image, NULL);
    vkFreeMemory(vl.device, t->mem, NULL);
}

void fbo_cleanup(FrameBufferObject *fbo)
{

	u32 attachment_count = minimum(4, fbo->attachment_count);
	for (u32 i = 0; i < attachment_count; ++i)
		texture_cleanup(&fbo->attachments[i]);
	if (fbo->has_depth)texture_cleanup(&fbo->depth_attachment);
    vkDestroyFramebuffer(vl.device, fbo->framebuffer, NULL);
	vkDestroyRenderPass(vl.device, fbo->clear_renderpass, NULL);
	vkDestroyRenderPass(vl.device, fbo->render_renderpass, NULL);
}


//attaches ALLOCATED memory block to buffer!
void buf_bind(DataBuffer *buf, VkDeviceSize offset)
{
	vkBindBufferMemory(vl.device, buf->buffer, buf->mem, offset);
}

void buf_copy_to(DataBuffer *src,void *data, VkDeviceSize size)
{
	assert(src->mapped);
	memcpy(src->mapped, data, size);
}

void buf_setup_descriptor(DataBuffer *buf, VkDeviceSize size, VkDeviceSize offset)
{
	buf->desc.offset = offset;
	buf->desc.buffer = buf->buffer;
	buf->desc.range = size;
}
VkResult buf_map(DataBuffer *buf, VkDeviceSize size, VkDeviceSize offset)
{
    //printf("buf->mem = %i\n\n", buf->mem);
    //fflush(stdout);
	return vkMapMemory(vl.device, buf->mem, offset, size, 0, &buf->mapped);//@check @check @check @check
}

void buf_unmap(DataBuffer *buf)
{
	if (buf->mapped)
	{
		vkUnmapMemory(vl.device, buf->mem);
		buf->mapped = NULL;
	}
}

void buf_destroy(DataBuffer *buf)
{
    if (!buf->active)return;
	if (buf->buffer)
	{
		vkDestroyBuffer(vl.device, buf->buffer, NULL);
	}
	if (buf->mem)
	{
		vkFreeMemory(vl.device, buf->mem, NULL);
	}
    buf->active = FALSE;
}





MultiAllocBuffer global_vertex_buffer;
MultiAllocBuffer global_index_buffer;
void create_buffer(VkBufferUsageFlagBits usage, VkMemoryPropertyFlagBits mem_flags, DataBuffer *buf, VkDeviceSize size, void *data);

b32 multi_alloc_buffer_init(MultiAllocBuffer* buf, u32 buffer_total_size, VkBufferUsageFlagBits usage)
{
    //buf->global_buffer = malloc(buffer_total_size);
    //assert(buf->global_buffer);
    //if (!buf->global_buffer)return FALSE;
    buf->current_offset = 0;
    create_buffer(usage,
        (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        &buf->gpu_side_buffer, buffer_total_size, NULL);

    return TRUE;
}

void multi_alloc_buffer_clear(MultiAllocBuffer* buf)
{
    buf->current_offset = 0;
    //printf("BUFFER CLEAR\n\n\n\n\n\n\n\n\n\n");
}
//returns the offset where the data start
u32 multi_alloc_buffer_insert(MultiAllocBuffer* buf, void* src_data, u32 data_size)
{
    u32 mem_offset = buf->current_offset;
    ///*
    VK_CHECK(buf_map(&buf->gpu_side_buffer, data_size, buf->current_offset));
    memcpy(buf->gpu_side_buffer.mapped, src_data, data_size);
    buf_unmap(&buf->gpu_side_buffer);
    //*/
    //memcpy(buf->global_buffer, src_data, data_size);
    buf->current_offset += data_size;
    return mem_offset;
}



typedef struct PipelineBuilder
{
	VkPipelineShaderStageCreateInfo shader_stages[4];
	u32 shader_stages_count;
	VkPipelineVertexInputStateCreateInfo vertex_input_info;
	VkPipelineInputAssemblyStateCreateInfo input_asm;
	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineColorBlendAttachmentState color_blend_attachments[4];
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineLayout pipeline_layout;
}PipelineBuilder;

VkPipelineShaderStageCreateInfo 
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

VkPipelineVertexInputStateCreateInfo 
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

VkPipelineInputAssemblyStateCreateInfo pipe_input_assembly_create_info(VkPrimitiveTopology topology)
{
	VkPipelineInputAssemblyStateCreateInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.pNext = NULL;
	
	info.topology = topology;
	info.primitiveRestartEnable = VK_FALSE;
	return info;
}

VkPipelineRasterizationStateCreateInfo pipe_rasterization_state_create_info(VkPolygonMode polygon_mode)
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


VkPipelineMultisampleStateCreateInfo pipe_multisampling_state_create_info(void)
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

VkPipelineColorBlendAttachmentState pipe_color_blend_attachment_state(void)
{
	VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
#ifdef VK_STANDALONE
	color_blend_attachment.blendEnable = VK_FALSE;	
#else
	color_blend_attachment.blendEnable = VK_TRUE;	
#endif		
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp= VK_BLEND_OP_ADD;
	return color_blend_attachment;
}

//this is retty much empty
VkPipelineLayoutCreateInfo pipe_layout_create_info(VkDescriptorSetLayout *layouts, u32 layouts_count)
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

VkViewport viewport_basic(void)
{
    VkViewport viewport = { 0 };
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (f32)vl.swap.extent.width;
    viewport.height = (f32)vl.swap.extent.height;
    //viewport.height *= fabs(sin(get_time()));
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    return viewport;
}

VkViewport viewport(f32 x, f32 y, f32 width, f32 height)
{
    VkViewport viewport = { 0 };
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    //viewport.height *= fabs(sin(get_time()));
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    return viewport;
}

VkRect2D scissor_basic(void)
{
    VkRect2D scissor = {0};
    scissor.offset.x = 0;
	scissor.offset.y = 0;
    scissor.extent = vl.swap.extent;
    //scissor.extent.height *= fabs(sin(get_time()));
	
    return scissor;

}

VkRenderingInfoKHR rendering_info_basic(void)
{
    VkRenderingInfoKHR rendering_info = {0};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;

    VkRenderingAttachmentInfoKHR color_attachment_info = {0};
    color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR; 
    color_attachment_info.pNext = NULL; 
    color_attachment_info.imageView = vl.swap.image_views[vl.image_index];


    VkRenderingAttachmentInfoKHR depth_attachment_info = {0};
    depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR; 
    depth_attachment_info.pNext = NULL; 
    depth_attachment_info.imageView = vl.swap.depth_attachment.view;
    
    u32 width, height;
#ifdef VK_STANDALONE
		window_get_framebuffer_size(&wnd, &width, &height);
#else
        width = vk_getHeight();
        width = vk_getWidth();
#endif
#ifdef VK_STANDALONE
    rendering_info.renderArea  = (VkRect2D){0,0,width, height};
#else
    rendering_info.renderArea = { 0,0,width, height };
#endif
    rendering_info.layerCount = 1;
    rendering_info.pColorAttachments = &color_attachment_info;
    rendering_info.pDepthAttachment = &depth_attachment_info;
    rendering_info.pStencilAttachment = &depth_attachment_info;
    return rendering_info; //this is not safe, pointer to stack allocated variables!
}

VkPipeline build_pipeline(VkDevice device, PipelineBuilder p,VkRenderPass render_pass, u32 out_attachment_count, b32 depth_enable)
{
	//------these are not used because they are dynamic rn!-------
    VkViewport viewport = viewport_basic();
    
    VkRect2D scissor = scissor_basic();
	
	VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    if (depth_enable)
        depth_stencil.depthTestEnable = VK_TRUE;
    else
        depth_stencil.depthTestEnable = VK_FALSE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.minDepthBounds = 0.0f;
	depth_stencil.maxDepthBounds = 1.0f;
	depth_stencil.stencilTestEnable = VK_FALSE;
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
	color_blending.attachmentCount = out_attachment_count;
	VkAttachmentDescription color_attachment = {0};
    color_attachment.format = vl.swap.image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	color_blending.pAttachments = p.color_blend_attachments;
	

    VkDynamicState dynamic_state_enables[] =
    { VK_DYNAMIC_STATE_VIEWPORT, 
        VK_DYNAMIC_STATE_SCISSOR 
#ifdef USE_VOLK
        ,VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT 
#endif
    };
    VkPipelineDynamicStateCreateInfo dynamic_state = { 0 };
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.pNext = NULL;
    dynamic_state.flags = NULL;
    dynamic_state.dynamicStateCount = array_count(dynamic_state_enables);
    dynamic_state.pDynamicStates = dynamic_state_enables;



   	
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
    pipeline_info.pDynamicState = &dynamic_state;
	pipeline_info.layout = p.pipeline_layout;
	pipeline_info.renderPass = render_pass;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    /*
    //Dynamic Rendering stuff
    VkPipelineRenderingCreateInfoKHR pipe_rendering_create_info = {0};
    pipe_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipe_rendering_create_info.colorAttachmentCount = 1;
    pipe_rendering_create_info.pColorAttachmentFormats= &vl.swap.image_format;
    pipe_rendering_create_info.depthAttachmentFormat = vl.swap.depth_attachment.format;
    pipe_rendering_create_info.stencilAttachmentFormat = vl.swap.depth_attachment.format;

    pipeline_info.pNext = &pipe_rendering_create_info;
    */





	VkPipeline new_pipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &new_pipeline)!=VK_SUCCESS)
		printf("Error creating some pipeline!\n");
	
	return new_pipeline;
}


VkShaderModule create_shader_module(char *code, u32 size)
{
	VkShaderModuleCreateInfo create_info = {0};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = size;
	create_info.pCode = (u32*)code;
	VkShaderModule shader_module;
	VK_CHECK(vkCreateShaderModule(vl.device, &create_info, NULL, &shader_module));
	return shader_module;
}


VkDescriptorSetLayout shader_create_descriptor_set_layout(ShaderObject *vert, ShaderObject *frag, u32 set_count)
{
    VkDescriptorSetLayout layout = {0};

    VkDescriptorSetLayoutBinding *bindings = NULL;
    for (u32 i = 0; i < vert->info.descriptor_count; ++i)
    {
        VkDescriptorSetLayoutBinding binding = {0};
        binding.binding = vert->info.descriptor_bindings[i].binding;
        binding.descriptorType = (VkDescriptorType)vert->info.descriptor_bindings[i].desc_type;
        binding.descriptorCount = set_count; //N if we want an array of descriptors (dset?)
        binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;//VK_SHADER_STAGE_VERTEX_BIT;
        binding.pImmutableSamplers = NULL;

        //add the binding to the array
        buf_push(bindings, binding);
    }
    for (u32 i = 0; i < frag->info.descriptor_count; ++i)
    {
        VkDescriptorSetLayoutBinding binding = {0};
        binding.binding = frag->info.descriptor_bindings[i].binding;
        if (frag->info.descriptor_bindings[i].binding == 0)continue; //@NOTE: this is to ignore default UBO description in fragment shader parsing
        binding.descriptorType = (VkDescriptorType)frag->info.descriptor_bindings[i].desc_type;
        binding.descriptorCount = set_count; //N if we want an array of descriptors (dset?)
        binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;//VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = NULL;

        //add the binding to the array
        buf_push(bindings, binding);
    }


    VkDescriptorSetLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = buf_len(bindings);
    layout_info.pBindings = bindings;
    
    VK_CHECK(vkCreateDescriptorSetLayout(vl.device, &layout_info, NULL, &layout));
	buf_free(bindings);
    return layout;
}

void shader_set_immediate(ShaderMetaInfo *info, const char *name, void *src, void *ubo_mem)
{
    u32 member_count = info->descriptor_bindings[0].member_count;
    for(u32 i = 0; i < member_count;++i)
    {
        if (strcmp(name, info->descriptor_bindings[0].members[i].name) == 0)
        {
            u32 offset = info->descriptor_bindings[0].members[i].offset;
            u32 size = info->descriptor_bindings[0].members[i].size;
            //memcpy(ubo_mem + offset, src, size);
            memcpy((char*)ubo_mem + offset, src, size);
            break;
        }
    }
} 


void shader_copy_to_immediate(ShaderMetaInfo *info, DataBuffer *uniform_buffers, u32 image_index)
{
	void *data = info->descriptor_bindings[0].mem;
	
	VK_CHECK(buf_map(&uniform_buffers[image_index], uniform_buffers[image_index].size, 0));
	memcpy(uniform_buffers[image_index].mapped, data,info->descriptor_bindings[0].mem_size);
	buf_unmap(&uniform_buffers[image_index]);
}



void shader_reflect(u32 *shader_code, u32 code_size, ShaderMetaInfo *info)
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
        SpvReflectInterfaceVariable* curvar = input_vars[i];
		if(curvar->location > 100 || curvar->location < 0)continue;
        VertexInputAttribute *attr = &info->vertex_input_attributes[curvar->location];
        

        attr->location = curvar->location;
        attr->format = (VkFormat)curvar->format;
        attr->builtin = curvar->built_in;

        if (curvar->array.dims_count > 0)
            attr->array_count = curvar->array.dims[0];
        else 
            attr->array_count = 1;

        attr->size = get_format_size(attr->format) * attr->array_count; //@THIS IS TEMPORARY (DELETE THIS)

        sprintf((char*const)attr->name, input_vars[i]->name);
        //printf("vertex input variable %s is at location: %i[F%i], size=%i\n", attr->name, attr->location, attr->format, attr->size);
    }


    //output variables 
	u32 out_var_count = 0;
	SPV_CHECK(spvReflectEnumerateOutputVariables(&module, &out_var_count, NULL));
	SpvReflectInterfaceVariable** output_vars =
	  (SpvReflectInterfaceVariable**)malloc(out_var_count * sizeof(SpvReflectInterfaceVariable*));
	SPV_CHECK(spvReflectEnumerateOutputVariables(&module, &out_var_count, output_vars));
    info->out_var_count = out_var_count;
    for (u32 i = 0; i < out_var_count; ++i)
    {
        SpvReflectInterfaceVariable* curvar = output_vars[i];
		if(curvar->location > 100 || curvar->location < 0)continue;
        OutVar *attr = &info->out_vars[curvar->location];
        

        attr->location = curvar->location;
        attr->format = (VkFormat)curvar->format;
        attr->builtin = curvar->built_in;

        if (curvar->array.dims_count > 0)attr->array_count = curvar->array.dims[0];
        else attr->array_count = 1;

        attr->size = get_format_size(attr->format) * attr->array_count; //@THIS IS TEMPORARY (DELETE THIS)

        sprintf((char*const)attr->name, output_vars[i]->name);
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
            desc_binding->desc_type = (VkDescriptorType)desc_info->descriptor_type;
            sprintf(desc_binding->name, desc_info->name);

            SpvReflectTypeDescription *uniform_struct_desc = desc_info->type_description;
            desc_binding->member_count = uniform_struct_desc->member_count;
            for(u32 i = 0; i < desc_binding->member_count;++i)
            {
                //add a type descriptor for each uniform in the uniform buffer
                SpvOp type = uniform_struct_desc->members[i].op;
                const char *name = uniform_struct_desc->members[i].struct_member_name;
                sprintf(desc_binding->members[i].name, name);

                SpvReflectTypeFlags type_flags = uniform_struct_desc->members[i].type_flags;
                desc_binding->members[i].type_flags = type_flags;
                SpvReflectNumericTraits numeric = uniform_struct_desc->members[i].traits.numeric;
                u32 component_count = numeric.vector.component_count;
                desc_binding->members[i].array_count = uniform_struct_desc->members[i].traits.array.dims[0];
                //printf("VARIABLE %s.%s of type %i with %i components \n", desc_binding->name, name, type_flags, component_count);
            }

            SpvReflectBlockVariable uniform_struct = desc_info->block;
            desc_binding->member_count = uniform_struct.member_count;
            u32 size = uniform_struct.size; //size of data
            u32 padded_size = uniform_struct.padded_size;
            //assert(size == padded_size);
            desc_binding->mem_size = size;
            desc_binding->mem = malloc(size);
            //printf("size of %s: %i, size of padded %s: %i\n", desc_binding->name, size, desc_binding->name, padded_size);
            for(u32 i = 0; i < desc_binding->member_count;++i)
            {
                SpvReflectBlockVariable cur_var = uniform_struct.members[i];
                const char *name = cur_var.name;
                desc_binding->members[i].size = cur_var.size;
                
                desc_binding->members[i].padded_size = cur_var.padded_size;
                desc_binding->members[i].offset = cur_var.offset;
                u32 absolute_offset = cur_var.absolute_offset;
            }


            //printf("DESC: %s(set =%i, binding = %i)[%i]\n", desc_binding->name, desc_binding->set,desc_binding->binding, desc_binding->desc_type);
        }
        free(input_vars);
    }

	
	spvReflectDestroyShaderModule(&module);
}

//@BEWARE(inv): runtime shader compilation should generally be avoided as it is too slow to call the command line!
//one other option would be to use glslang BUT its a big dependency and I don't really need it.

#ifdef VK_STANDALONE
#define SHADER_SRC_DIR "../assets/shaders/"
#define SHADER_DST_DIR "shaders/"
#else
#define SHADER_SRC_DIR "../resources/generic/shaders/default/"
#define SHADER_DST_DIR "../resources/generic/shaders/default/"
#endif
void shader_create_dynamic(VkDevice device, ShaderObject *shader, const char *filename, VkShaderStageFlagBits stage)
{
    char command[256];
    char new_file[256];
    sprintf(new_file, "%s%s.spv", SHADER_DST_DIR, filename);
    sprintf(command,"glslangvalidator -V %s%s -o %s", SHADER_SRC_DIR, filename, new_file);
    system(command);
    
	u32 code_size;
	u32 *shader_code = NULL;
	read_file(new_file, &shader_code, &code_size);
	shader->module = create_shader_module((char*)shader_code, code_size);
	shader->uses_push_constants = FALSE;
    shader_reflect(shader_code, code_size, &shader->info);
	//printf("Shader: %s has %i input variable(s)!\n", filename, shader->info.input_variable_count);
	shader->stage = stage;
	free(shader_code);
    //remove(new_file);
}  

void shader_create_from_data(VkDevice device, ShaderObject* shader,u32 *shader_code,u32 code_size, VkShaderStageFlagBits stage)
{
    shader->module = create_shader_module((char*)shader_code, code_size);
    shader->uses_push_constants = FALSE;
    shader_reflect(shader_code, code_size, &shader->info);
    //printf("Shader: %s has %i input variable(s)!\n", filename, shader->info.input_variable_count);
    shader->stage = stage;
    //remove(new_file);
}

void shader_create(VkDevice device, ShaderObject *shader, const char *filename, VkShaderStageFlagBits stage)
{
    char path[256];
    sprintf(path, "%s%s.spv", SHADER_DST_DIR, filename);
	u32 code_size;
	u32 *shader_code = NULL;
	if (read_file(path, &shader_code, &code_size) == -1){shader_create_dynamic(device, shader, filename, stage);return;};
	shader->module = create_shader_module((char*)shader_code, code_size);
	shader->uses_push_constants = FALSE;
    shader_reflect(shader_code, code_size, &shader->info);
	//printf("Shader: %s has %i input variable(s)!\n", filename, shader->info.input_variable_count);
	shader->stage = stage;
	free(shader_code);
}  






const int MAX_FRAMES_IN_FLIGHT = 2;
VkSemaphore *image_available_semaphores;
VkSemaphore *render_finished_semaphores;


VkFence *in_flight_fences;
VkFence *images_in_flight;

//VkBuffer vertex_buffer;
//VkDeviceMemory vertex_buffer_memory;

DataBuffer vertex_buffer_real;
DataBuffer index_buffer_real;




TextureObject sample_texture;
TextureObject sample_texture2;

u32 current_frame = 0;
u32 framebuffer_resized = FALSE;

const char *validation_layers[]= {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const u32 enable_validation_layers= TRUE;
#else
const u32 enable_validation_layers = TRUE;
#endif

const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
#ifdef USE_VOLK
"VK_EXT_extended_dynamic_state"
#endif
};//, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };


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
		
vec3 cube_positions[]  = {
	{0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f},  {-0.5f,-0.5f, 0.5f}, {0.5f,-0.5f, 0.5f},   // v0,v1,v2,v3 (front)
    {0.5f, 0.5f, 0.5f},  {0.5f,-0.5f, 0.5f},   {0.5f,-0.5f,-0.5f},  {0.5f, 0.5f,-0.5f},   // v0,v3,v4,v5 (right)
    {0.5f, 0.5f, 0.5f},  {0.5f, 0.5f,-0.5f},   {-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f},   // v0,v5,v6,v1 (top)
    {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f},  {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f},   // v1,v6,v7,v2 (left)
    {-0.5f,-0.5f,-0.5f}, {0.5f,-0.5f,-0.5f},   {0.5f,-0.5f, 0.5f},  {-0.5f,-0.5f, 0.5f},   // v7,v4,v3,v2 (bottom)
    {0.5f,-0.5f,-0.5f},  {-0.5f,-0.5f,-0.5f},  {-0.5f, 0.5f,-0.5f}, {0.5f, 0.5f,-0.5f},    // v4,v7,v6,v5 (back)
};

// normal array
vec3 cube_normals[] = {
	{0, 0, 1},   {0, 0, 1},   {0, 0, 1},   {0, 0, 1},  // v0,v1,v2,v3 (front)
	{1, 0, 0},   {1, 0, 0},   {1, 0, 0},   {1, 0, 0},  // v0,v3,v4,v5 (right)
	{0, 1, 0},   {0, 1, 0},   {0, 1, 0},   {0, 1, 0},  // v0,v5,v6,v1 (top)
	{-1, 0, 0},  {-1, 0, 0},  {-1, 0, 0},  {-1, 0, 0},  // v1,v6,v7,v2 (left)
	{0,-1, 0},   {0,-1, 0},   {0,-1, 0},   {0,-1, 0},  // v7,v4,v3,v2 (bottom)
	{0, 0,-1},   {0, 0,-1},   {0, 0,-1},   {0, 0,-1},   // v4,v7,v6,v5 (back)
};

// texCoord array
vec2 cube_tex_coords[] = {
    {1, 0},   {0, 0},   {0, 1},   {1, 1},               // v0,v1,v2,v3 (front)
	{0, 0},   {0, 1},   {1, 1},   {1, 0},               // v0,v3,v4,v5 (right)
	{1, 1},   {1, 0},   {0, 0},   {0, 1},               // v0,v5,v6,v1 (top)
	{1, 0},   {0, 0},   {0, 1},   {1, 1},               // v1,v6,v7,v2 (left)
	{0, 1},   {1, 1},   {1, 0},   {0, 0},               // v7,v4,v3,v2 (bottom)
	{0, 1},   {1, 1},   {1, 0},   {0, 0}                // v4,v7,v6,v5 (back)
};

u32 cube_indices[] = {
     0, 1, 2,   2, 3, 0,    // v0-v1-v2, v2-v3-v0 (front)
     4, 5, 6,   6, 7, 4,    // v0-v3-v4, v4-v5-v0 (right)
     8, 9,10,  10,11, 8,    // v0-v5-v6, v6-v1-v0 (top)
    12,13,14,  14,15,12,    // v1-v6-v7, v7-v2-v1 (left)
    16,17,18,  18,19,16,    // v7-v4-v3, v3-v2-v7 (bottom)
    20,21,22,  22,23,20     // v4-v7-v6, v6-v5-v4 (back)
};
Vertex *cube_build_verts(void)
{
	Vertex *verts = (Vertex*)malloc(sizeof(Vertex) * 24);
	for (u32 i =0; i < 24; ++i)
	{
		verts[i].pos = cube_positions[i];
		verts[i].normal = cube_normals[i];
		verts[i].tex_coord = cube_tex_coords[i];
	}
	return verts;
}

typedef struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    u32 format_count;
    VkPresentModeKHR *present_modes;
    u32 present_mode_count;
}SwapChainSupportDetails;


SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device)
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
        details.formats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, vl.surface, &format_count, details.formats);
    }
    //[2]: finally we query available presentation modes
    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, vl.surface, &present_mode_count, NULL);
    details.present_mode_count = present_mode_count;
    
    if (present_mode_count != 0)
    {
        details.present_modes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, vl.surface, &present_mode_count, details.present_modes);
    }
    
    
    return details;
}

u32 check_validation_layer_support(void){
	u32 layer_count = 0;
	vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    
	VkLayerProperties *available_layers = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
    /*
    for (u32 i = 0; i < layer_count; ++i)
        printf("layer %i: %s\n", i, available_layers[i]);
        */
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

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCB(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        printf("validation layer: %s\n", pCallbackData->pMessage);

    return VK_FALSE;
}

void vl_create_instance(void) {
	if (enable_validation_layers&&(check_validation_layer_support()==0))
		vk_error("Validation layers requested, but not available!");
	VkApplicationInfo appinfo = {0};
	appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appinfo.pApplicationName = "Vulkan Sample";
	appinfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
	appinfo.pEngineName = "Hive3D";
	appinfo.engineVersion = VK_MAKE_VERSION(1,0,0);
	appinfo.apiVersion = VK_API_VERSION_1_1;
    
    
	VkInstanceCreateInfo create_info = {0};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &appinfo;
    
	u32 instance_ext_count = 0;
	char **instance_extensions; //array of strings with names of VK extensions needed!
    char *base_extensions[] = {
		"VK_KHR_surface",
		"VK_KHR_win32_surface",
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
	};
	//vl.instance_extensions = window_get_required_vl.instance_extensions(&wnd, &vl.instance_ext_count);
    
	create_info.enabledExtensionCount = array_count(base_extensions);
	create_info.ppEnabledExtensionNames = base_extensions;
	create_info.enabledLayerCount = 0;

    
	VK_CHECK(vkCreateInstance(&create_info, NULL, &vl.instance));
#ifdef USE_VOLK
    volkLoadInstance(vl.instance);
#endif

	//(OPTIONAL): extension support
	u32 ext_count = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);
	VkExtensionProperties *extensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * ext_count);
	vkEnumerateInstanceExtensionProperties(NULL, &ext_count, extensions);
    /* //prints supported vl.instance extensions
	for (u32 i = 0; i < ext_count; ++i)
		printf("EXT: %s\n", extensions[i]);
    */
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}

void vl_setup_debug_messenger(void)
{
    VkDebugUtilsMessengerCreateInfoEXT create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debugCB;
    create_info.pUserData = NULL; // Optional

    if (CreateDebugUtilsMessengerEXT(vl.instance, &create_info, NULL, &vl.debug_msg) != VK_SUCCESS) {
        printf("failed to set up debug messenger!\n");
    }
}


typedef struct QueueFamilyIndices
{
    u32 graphics_family;
    //because we cant ifer whether the vfalue was initialized correctly or is garbage
    u32 graphics_family_found;
    
    u32 present_family;
    u32 present_family_found;
}QueueFamilyIndices;

QueueFamilyIndices find_queue_families(VkPhysicalDevice device)
{ 
    QueueFamilyIndices indices;
    
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    
    VkQueueFamilyProperties *queue_families = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
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

u32 check_device_extension_support(VkPhysicalDevice device)
{
    u32 ext_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, NULL);
    VkExtensionProperties *available_extensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * ext_count);
    
    vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, available_extensions);
    /*
    for (u32 i = 0; i < ext_count; ++i)
        printf("EXT %i: %s\n", i, available_extensions[i]);
        */
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

u32 is_device_suitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = find_queue_families(device);
    
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);
    
    u32 extensions_supported = check_device_extension_support(device);
    if (extensions_supported == 0) printf("some device extension not supported!!\n");
    
    SwapChainSupportDetails swapchain_support = query_swapchain_support(device);
    
    //here we can add more requirements for physical device selection
    return indices.graphics_family_found && extensions_supported && 
    (swapchain_support.format_count > 0) && (swapchain_support.present_mode_count > 0);
}

void vl_pick_physical_device(void) {
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(vl.instance, &device_count, NULL);
    if (device_count == 0)
        vk_error("Failed to find GPUs with Vulkan support!");
    
    VkPhysicalDevice *devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(vl.instance, &device_count, devices);
    for (u32 i = 0; i < device_count; ++i)
        if (is_device_suitable(devices[i]))
    {
        vl.physical_device = devices[i];

        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(vl.physical_device, &p);
        //printf("physical device picked: %s\n", p.deviceName);
        break;
    }
    free(devices);
}

VkSurfaceFormatKHR choose_swap_surface_format(SwapChainSupportDetails details)
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
VkPresentModeKHR choose_swap_present_mode(SwapChainSupportDetails details)
{
    return VK_PRESENT_MODE_FIFO_KHR;
    //return VK_PRESENT_MODE_IMMEDIATE_KHR;
}

VkExtent2D choose_swap_extent(SwapChainSupportDetails details)
{
    if (details.capabilities.currentExtent.width != UINT32_MAX)
    {
        return details.capabilities.currentExtent;
    }
    else
    {
        u32 width, height;
#ifdef VK_STANDALONE
		window_get_framebuffer_size(&wnd, &width, &height);
#else
        width = vk_getHeight();
        width = vk_getWidth();
#endif
        VkExtent2D actual_extent = {width, height};
        actual_extent.height=maximum(details.capabilities.maxImageExtent.height,
                                     minimum(details.capabilities.minImageExtent.height,actual_extent.height));
        actual_extent.width=maximum(details.capabilities.maxImageExtent.width,
                                    minimum(details.capabilities.minImageExtent.width,actual_extent.width));
        return actual_extent;
    }
}
TextureObject create_depth_attachment(u32 width, u32 height);
VkRenderPass create_render_pass(VkAttachmentLoadOp load_op,VkImageLayout initial, 
          VkImageLayout final, u32 color_attachment_count, b32 depth_attachment_active);
void vl_create_swapchain(void)
{
	
    SwapChainSupportDetails swapchain_support = query_swapchain_support(vl.physical_device);
    
    VkSurfaceFormatKHR surface_format =choose_swap_surface_format(swapchain_support);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support);
    VkExtent2D extent = choose_swap_extent(swapchain_support);
    
    u32 image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount)
        image_count = swapchain_support.capabilities.maxImageCount;
    image_count = minimum(image_count, MAX_SWAP_IMAGE_COUNT);
    
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
    vl.swap.images = (VkImage*)malloc(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(vl.device, vl.swap.swapchain, &image_count, vl.swap.images);
    vl.swap.image_format = surface_format.format;
    vl.swap.extent = extent;
    vl.swap.image_count = image_count;//TODO(ilias): check
    printf("new swapchain image_count: %i\n", image_count);
    printf("new swapchain image_dims: %i %i\n", vl.swap.extent.width, vl.swap.extent.height);
	vl.swap.depth_attachment = create_depth_attachment(vl.swap.extent.width, vl.swap.extent.height);
	
	vl.swap.rp_begin = create_render_pass(VK_ATTACHMENT_LOAD_OP_CLEAR,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL, 1, TRUE);
}

VkImageView create_image_view(VkImage image, VkFormat format,  VkImageAspectFlags aspect_flags)
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

void vl_create_swapchain_image_views(void)
{
    vl.swap.image_views = (VkImageView*)malloc(sizeof(VkImageView) * vl.swap.image_count);
    for (u32 i = 0; i < vl.swap.image_count; ++i)
		vl.swap.image_views[i] = create_image_view(vl.swap.images[i], vl.swap.image_format,VK_IMAGE_ASPECT_COLOR_BIT);
}

//Queue initialization is a little weird,TODO(ilias): fix when possible
void vl_create_logical_device(void)
{
    QueueFamilyIndices indices = find_queue_families(vl.physical_device);
    
    f32 queue_priority = 1.0f;
#ifdef __cplusplus
    VkDeviceQueueCreateInfo queue_create_info[2] = {};
#else
	VkDeviceQueueCreateInfo queue_create_info[2] = {0};
#endif     
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
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state = { 0 };
    extended_dynamic_state.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
    extended_dynamic_state.pNext = NULL;
    extended_dynamic_state.extendedDynamicState = VK_TRUE;

    create_info.pNext = &extended_dynamic_state;
    VK_CHECK(vkCreateDevice(vl.physical_device, &create_info, NULL, &vl.device));
    
    
    vkGetDeviceQueue(vl.device, indices.graphics_family, 0, &vl.graphics_queue);
    vkGetDeviceQueue(vl.device, indices.present_family, 0, &vl.present_queue);
}



VkDescriptorSet *create_descriptor_sets(VkDescriptorSetLayout layout, ShaderObject *vert,ShaderObject *frag, VkDescriptorPool pool, DataBuffer *uni_buffers,TextureObject *textures, u32 texture_count,  u32 set_count)
{
    VkDescriptorSetLayout *layouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout)*set_count);
    for (u32 i = 0; i < set_count; ++i)
        layouts[i] = layout;
    VkDescriptorSetAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = set_count;
    alloc_info.pSetLayouts = layouts;
    
    VkDescriptorSet *desc_sets = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * set_count);
    VK_CHECK(vkAllocateDescriptorSets(vl.device, &alloc_info, desc_sets));
    
    for (u32 j = 0; j < set_count; ++j) {
        VkDescriptorBufferInfo buffer_info = {0};
        buffer_info.buffer = uni_buffers[j].buffer;
        buffer_info.offset = 0;
        buffer_info.range = vert->info.descriptor_bindings[0].mem_size; //@check
		
		VkDescriptorImageInfo image_infos[4] = {0};
		for (u32 t = 0; t < texture_count && textures; ++t)
		{
			image_infos[t].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			image_infos[t].sampler = textures[t].sampler;
			image_infos[t].imageView = textures[t].view;
        }
		u32 global_image_index = 0; //gets incremented for every new image description we need ot write 
		
        VkWriteDescriptorSet *descriptor_writes = NULL; //dynamic array, look tools.h
        u32 dw_count = 0;

        for (u32 i = 0; i < vert->info.descriptor_count; ++i)
        {
			VkWriteDescriptorSet desc_write = {0};
			
            desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            desc_write.dstSet = desc_sets[j];
            desc_write.dstBinding = vert->info.descriptor_bindings[i].binding;
            desc_write.dstArrayElement = 0; //u sure?????????????? @BUG
            desc_write.descriptorType = (VkDescriptorType)vert->info.descriptor_bindings[i].desc_type;
            desc_write.descriptorCount = 1;

            if (vert->info.descriptor_bindings[i].desc_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                desc_write.pBufferInfo = &buffer_info;
            else
                desc_write.pImageInfo = &image_infos[global_image_index++];
			
			buf_push(descriptor_writes, desc_write);
        }
        for (u32 i = 0; i < frag->info.descriptor_count; ++i)
        {
			VkWriteDescriptorSet desc_write = {0};
			if (frag->info.descriptor_bindings[i].binding == 0)continue; //@NOTE: this is to ignore default UBO description in fragment shader parsing
            desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            desc_write.dstSet = desc_sets[j];
            desc_write.dstBinding = frag->info.descriptor_bindings[i].binding;
            desc_write.dstArrayElement = 0; //u sure?????????????? @BUG
            desc_write.descriptorType =(VkDescriptorType)frag->info.descriptor_bindings[i].desc_type;
            desc_write.descriptorCount = 1;

            if (frag->info.descriptor_bindings[i].desc_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                desc_write.pBufferInfo = &buffer_info;
            else
                desc_write.pImageInfo = &image_infos[global_image_index++];
			
			buf_push(descriptor_writes, desc_write);
        }
        
        //printf("update %i with dw_count %i!\n", j, dw_count);
        vkUpdateDescriptorSets(vl.device, buf_len(descriptor_writes), descriptor_writes, 0, NULL);
		buf_free(descriptor_writes);
    }
    return desc_sets;
}



void create_descriptor_pool(VkDescriptorPool *descriptor_pool,ShaderObject *vert, ShaderObject *frag)
{
    VkDescriptorPoolSize *pool_size = NULL;
	VkDescriptorPoolSize ps = {0};//dummy
	
    for (u32 i = 0; i < vert->info.descriptor_count; ++i)
    {
		
        ps.type = (VkDescriptorType)vert->info.descriptor_bindings[i].desc_type;
        ps.descriptorCount = vl.swap.image_count * 1000; //do we need to specify big descriptor count?
		buf_push(pool_size, ps);
    }

    for (u32 i = 0; i < frag->info.descriptor_count; ++i)
    {  
        if (frag->info.descriptor_bindings[i].binding == 0)continue; //@NOTE: this is to ignore default UBO description in fragment shader parsing
       
        ps.type = (VkDescriptorType)frag->info.descriptor_bindings[i].desc_type;
        ps.descriptorCount = vl.swap.image_count * 1000; //do we need to specify big descriptor count?
        buf_push(pool_size, ps);
    }
    
    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = buf_len(pool_size);
    pool_info.pPoolSizes = pool_size;
    pool_info.maxSets = vl.swap.image_count * 1000;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    
    VK_CHECK(vkCreateDescriptorPool(vl.device, &pool_info, NULL, descriptor_pool));
	buf_free(pool_size);
}

VkClearValue clear_values[5];
VkRenderPassBeginInfo rp_info(VkRenderPass rp, VkFramebuffer fbo, u32 attachment_count, VkExtent2D extent)
{
    VkRenderPassBeginInfo rp_info = { 0 };


    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = rp;
    rp_info.framebuffer = fbo; //we bind a _framebuffer_ to a render pass

    rp_info.renderArea.offset.x = 0;
    rp_info.renderArea.offset.y = 0;
    rp_info.renderArea.extent = extent;
#ifdef __cplusplus
    for (u32 i = 0; i < attachment_count; ++i)
        clear_values[i].color = {{0.0f, 1.0f, 0.0f, 1.0f}};
	clear_values[attachment_count].depthStencil = {1.0f, 0};
#else
	for (u32 i = 0; i < attachment_count; ++i) clear_values[i].color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 1.0f}};
	clear_values[attachment_count].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
#endif
    rp_info.clearValueCount = array_count(clear_values);
    rp_info.pClearValues = clear_values;

    return rp_info;
}



VkFormat find_depth_format(void);
VkRenderPass create_render_pass(VkAttachmentLoadOp load_op,VkImageLayout initial, VkImageLayout final, u32 color_attachment_count, b32 depth_attachment_active)
{
#ifdef __cplusplus
	VkRenderPass render_pass = {};
#else
	VkRenderPass render_pass = {0};
#endif

    VkAttachmentDescription color_attachment = {0};
    color_attachment.format = vl.swap.image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = load_op;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = initial;
    color_attachment.finalLayout = final;
    //color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	
	VkAttachmentReference *attachment_refs = NULL;
	
	for (u32 i = 0; i < color_attachment_count; ++i)
	{
		VkAttachmentReference color_attachment_ref = {0};
		color_attachment_ref.attachment = i;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_GENERAL;
		buf_push(attachment_refs, color_attachment_ref);
	}
	
	VkAttachmentDescription depth_attachment = {0};
	depth_attachment.format = find_depth_format();
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = load_op;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = initial;
	depth_attachment.finalLayout = final;
    
	VkAttachmentReference depth_attachment_ref = {0};
    depth_attachment_ref.attachment = color_attachment_count; //for 1 attachment its in 1, for 2 in 2([0,1,2]) and so on..
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_GENERAL;
    
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = color_attachment_count;
    subpass.pColorAttachments = attachment_refs;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;
    
	
	VkAttachmentDescription *attachment_descs = NULL;
	for (u32 i = 0; i < color_attachment_count; ++i)
	{
		buf_push(attachment_descs, color_attachment);
	}
	if (depth_attachment_active)
		buf_push(attachment_descs, depth_attachment);
	
    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = color_attachment_count + depth_attachment_active;
    render_pass_info.pAttachments = attachment_descs;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    
    ///*
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
    //*/
    VK_CHECK(vkCreateRenderPass(vl.device, &render_pass_info, NULL, &render_pass));
	
	buf_free(attachment_refs);
	buf_free(attachment_descs);
    return render_pass;
}



void vl_swap_create_framebuffers(void)
{
    vl.swap.framebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * vl.swap.image_count); 
    for (u32 i = 0; i < vl.swap.image_count; ++i)
    {
        VkImageView attachments[] = {vl.swap.image_views[i], vl.swap.depth_attachment.view};
        
        VkFramebufferCreateInfo framebuffer_info = {0};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vl.render_pass_basic; //VkFramebuffers need a render pass?
        framebuffer_info.attachmentCount = array_count(attachments);
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = vl.swap.extent.width;
        framebuffer_info.height = vl.swap.extent.height;
        framebuffer_info.layers = 1;
        
        VK_CHECK(vkCreateFramebuffer(vl.device, &framebuffer_info, NULL, &vl.swap.framebuffers[i]));
    }
    
}

VkFormat find_supported_format(VkFormat *candidates, VkImageTiling tiling, VkFormatFeatureFlags features, u32 candidates_count)
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


void vl_create_command_pool(void)
{
    QueueFamilyIndices queue_family_indices = find_queue_families(vl.physical_device);
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(vl.device, &pool_info, NULL, &vl.command_pool));
}

void render_fullscreen(VkCommandBuffer command_buf, u32 image_index)
{    
    //VkRenderPassBeginInfo renderpass_info = rp_info(vl.render_pass_basic, vl.swap.framebuffers[vl.image_index], 1);
    VkRenderPassBeginInfo renderpass_info = rp_info(fbo1.render_renderpass, fbo1.framebuffer, fbo1.attachment_count, vl.swap.extent);
    vkCmdBeginRenderPass(command_buf, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);


	vkCmdBindPipeline(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vl.fullscreen_pipe.pipeline);
    //vkCmdSetPrimitiveTopologyEXT(command_buf, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    vkCmdDraw(command_buf, 4, 1, 0, 0);
	vkCmdEndRenderPass(command_buf);
}

void vl_create_command_buffers(void)
{
    vl.command_buffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * vl.swap.image_count);
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = vl.command_pool; //where to allocate the buffer from
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = vl.swap.image_count;
    VK_CHECK(vkAllocateCommandBuffers(vl.device, &alloc_info, vl.command_buffers));
}

#define TYPE_OK(TYPE_FILTER, FILTER_INDEX) (TYPE_FILTER & (1 << FILTER_INDEX)) 
u32 find_mem_type(u32 type_filter, VkMemoryPropertyFlags properties)
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

void create_buffer_simple(u32 buffer_size,VkBufferUsageFlagBits usage, VkMemoryPropertyFlagBits mem_flags, VkBuffer *buf, VkDeviceMemory *mem)
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


void create_buffer(VkBufferUsageFlagBits usage, VkMemoryPropertyFlagBits mem_flags, DataBuffer *buf, VkDeviceSize size, void *data)
{
	buf->device = vl.device;
    buf->active = TRUE;
	
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
	buf->usage_flags = usage; //dfsfs
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

void create_image(u32 width, u32 height, VkFormat format, VkImageTiling tiling, 
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
	image_info.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;
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



VkCommandBuffer begin_single_time_commands(void)
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
void end_single_time_commands(VkCommandBuffer command_buffer)
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
void copy_buffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size)
{
	VkCommandBuffer command_buf = begin_single_time_commands();
	
	VkBufferCopy copy_region = {0};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buf, src_buf, dst_buf, 1, &copy_region);
	
	end_single_time_commands(command_buf);
}

void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
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
	
	
	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL){
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		
		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		
		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR){
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		
		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_GENERAL)
    {
    	barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		
		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;
		
		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
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

//for depth images
void transition_dimage_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkCommandBuffer command_buf = begin_single_time_commands();

    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;

    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;


    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_GENERAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    vkCmdPipelineBarrier(
        command_buf,
        src_stage, dst_stage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);


    end_single_time_commands(command_buf);
}

void insert_image_memory_barrier(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresourceRange)
		{
			VkImageMemoryBarrier imageMemoryBarrier = {0};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.srcAccessMask = srcAccessMask;
			imageMemoryBarrier.dstAccessMask = dstAccessMask;
			imageMemoryBarrier.oldLayout = oldImageLayout;
			imageMemoryBarrier.newLayout = newImageLayout;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = subresourceRange;

			vkCmdPipelineBarrier(
				cmdbuffer,
				srcStageMask,
				dstStageMask,
				0,
				0, NULL,
				0, NULL,
				1, &imageMemoryBarrier);
		}

void copy_buffer_to_image(VkBuffer buffer, VkImage image, u32 width, u32 height) {
    VkCommandBuffer command_buf = begin_single_time_commands();
	
	VkBufferImageCopy region = {0};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

#ifdef __cplusplus
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {
		width,
		height,
		1
	};
#else
	region.imageOffset = (VkOffset3D){0, 0, 0};
	region.imageExtent = (VkExtent3D){
		width,
		height,
		1
	};
#endif

	
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
u32 has_stencil_component(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}
VkFormat find_depth_format(void)
{	VkFormat c[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
	return find_supported_format(
                                 c,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,3);
    
}



void create_texture_sampler(VkSampler* sampler);

TextureObject create_depth_attachment(u32 width, u32 height)
{
#ifdef __cplusplus
	TextureObject depth_attachment = {};
#else
	TextureObject depth_attachment = {0};
#endif
	depth_attachment.format = find_depth_format();
	
	create_image(width, height, 
		depth_attachment.format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depth_attachment.image, &depth_attachment.mem);
    transition_dimage_layout(depth_attachment.image, depth_attachment.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    transition_dimage_layout(depth_attachment.image, depth_attachment.format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

	depth_attachment.view = create_image_view(depth_attachment.image, depth_attachment.format, VK_IMAGE_ASPECT_DEPTH_BIT);
    depth_attachment.width = width;
    depth_attachment.height = height;
    create_texture_sampler(&depth_attachment.sampler);
    
    //create_texture_sampler(&depth_attachment.sampler);
	return depth_attachment;
}



TextureObject create_color_attachment(u32 width, u32 height, VkFormat format)
{
#ifdef __cplusplus
	TextureObject color_attachment = {};
#else
	TextureObject color_attachment = {0};
#endif
	color_attachment.format = format;
	
    create_image(width, height, 
    color_attachment.format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &color_attachment.image, &color_attachment.mem);
    transition_image_layout(color_attachment.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    transition_image_layout(color_attachment.image, format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    
    
    color_attachment.view = create_image_view(color_attachment.image, color_attachment.format, VK_IMAGE_ASPECT_COLOR_BIT);
    color_attachment.width = width;
    color_attachment.height = height;
    create_texture_sampler(&color_attachment.sampler);
	return color_attachment;
}


void fbo_init_initialized_attachments(FrameBufferObject *fbo,TextureObject *image_attachments, u32 attachment_count, TextureObject *da, u32 width, u32 height)
{
	fbo->width = width;
	fbo->height = height;
    if (da)
        fbo->depth_attachment = *da; 
    else 
        fbo->depth_attachment = create_depth_attachment(fbo->width, fbo->height);
	fbo->has_depth = TRUE;
    for (u32 i = 0 ; i < attachment_count; ++i)
        fbo->attachments[i] = image_attachments[i];
	fbo->attachment_count = attachment_count;
    fbo->clear_renderpass =create_render_pass(VK_ATTACHMENT_LOAD_OP_CLEAR,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL, attachment_count, TRUE);
    fbo->render_renderpass =create_render_pass(VK_ATTACHMENT_LOAD_OP_LOAD,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL, attachment_count, TRUE);
	//now create the  framebuffers
    
    VkImageView *attachments = NULL;
    
    for (u32 j = 0; j < fbo->attachment_count; ++j)
        buf_push(attachments, fbo->attachments[j].view);
    buf_push(attachments, fbo->depth_attachment.view);
    
    
    VkFramebufferCreateInfo fbo_info = {0};
    fbo_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbo_info.renderPass = fbo->clear_renderpass;
    fbo_info.attachmentCount = attachment_count + 1; //base case!
    fbo_info.pAttachments = attachments;
    fbo_info.width = fbo->width;
    fbo_info.height = fbo->height;
    fbo_info.layers = 1;
    
    VK_CHECK(vkCreateFramebuffer(vl.device, &fbo_info, NULL, &fbo->framebuffer));
    

    //auto renderpass_info = rp_info(fbo1.clear_renderpass, fbo1.framebuffer, fbo1.attachment_count, vl.swap.extent);
    //vkCmdBeginRenderPass(vl.command_buffers[vl.image_index], &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
    //vkCmdEndRenderPass(vl.command_buffers[vl.image_index]);

    buf_free(attachments);
}
void fbo_init(FrameBufferObject *fbo, u32 attachment_count)
{
	fbo->width = vl.swap.extent.width;
	fbo->height = vl.swap.extent.height;
	fbo->depth_attachment = create_depth_attachment(fbo->width, fbo->height);
	fbo->has_depth = TRUE;
    for (u32 i = 0 ; i < attachment_count; ++i)
        fbo->attachments[i] = create_color_attachment(fbo->width, fbo->height, vl.swap.image_format);
	fbo->attachment_count = attachment_count;
    fbo->clear_renderpass =create_render_pass(VK_ATTACHMENT_LOAD_OP_CLEAR,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, attachment_count, TRUE);
    fbo->render_renderpass =create_render_pass(VK_ATTACHMENT_LOAD_OP_LOAD,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, attachment_count, TRUE);
	//now create the  framebuffers
    
    VkImageView *attachments = NULL;
    
    for (u32 j = 0; j < fbo->attachment_count; ++j)
        buf_push(attachments, fbo->attachments[j].view);
    buf_push(attachments, fbo->depth_attachment.view);
    
    
    VkFramebufferCreateInfo fbo_info = {0};
    fbo_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbo_info.renderPass = fbo->clear_renderpass;
    fbo_info.attachmentCount = attachment_count + 1; //base case!
    fbo_info.pAttachments = attachments;
    fbo_info.width = vl.swap.extent.width;
    fbo_info.height = vl.swap.extent.height;
    fbo_info.layers = 1;
    
    VK_CHECK(vkCreateFramebuffer(vl.device, &fbo_info, NULL, &fbo->framebuffer));
    
    buf_free(attachments);
}

DataBuffer *create_uniform_buffers(ShaderMetaInfo *info, u32 buffer_count) //=vl.swap.image_count
{
    VkDeviceSize buf_size = info->descriptor_bindings[0].mem_size;
    DataBuffer *uni_buffers = (DataBuffer*)malloc(sizeof(DataBuffer) * vl.swap.image_count);
    
    for (u32 i = 0; i < buffer_count; ++i)
    {
        create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),&uni_buffers[i], buf_size, NULL);
    }
    return uni_buffers;
}

void pipeline_build_basic_no_reflect(PipelineObject *p,const char *vert, const char *frag, VkPrimitiveTopology topology)
{

#ifdef __cplusplus
    PipelineBuilder pb = {};
#else
	PipelineBuilder pb = {0};
#endif
    //read shaders and register them in the pipeline builder
	shader_create(vl.device, &p->vert_shader, vert, VK_SHADER_STAGE_VERTEX_BIT); 
	shader_create(vl.device, &p->frag_shader, frag, VK_SHADER_STAGE_FRAGMENT_BIT);
	pb.shader_stages_count = 2;
	pb.shader_stages[0] = pipe_shader_stage_create_info(p->vert_shader.stage, p->vert_shader.module);
	pb.shader_stages[1] = pipe_shader_stage_create_info(p->frag_shader.stage, p->frag_shader.module);

 
	VkPipelineVertexInputStateCreateInfo vinfo = {0};
	vinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vinfo.pNext = NULL;
	vinfo.vertexBindingDescriptionCount = 0;
    vinfo.pVertexBindingDescriptions = NULL;
	vinfo.vertexAttributeDescriptionCount = 0;
    vinfo.pVertexAttributeDescriptions = NULL;


    pb.vertex_input_info = vinfo;
	pb.input_asm = pipe_input_assembly_create_info((VkPrimitiveTopology)topology);
	pb.rasterizer = pipe_rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pb.multisampling = pipe_multisampling_state_create_info();
	pb.color_blend_attachments[0] = pipe_color_blend_attachment_state();
	pb.color_blend_attachments[1] = pipe_color_blend_attachment_state();
	pb.color_blend_attachments[2] = pipe_color_blend_attachment_state();
	pb.color_blend_attachments[3] = pipe_color_blend_attachment_state();

    VkDescriptorSetLayout layout = shader_create_descriptor_set_layout(&p->vert_shader, &p->frag_shader, 1);
    VkPipelineLayoutCreateInfo info = pipe_layout_create_info(&layout, 1);
	VK_CHECK(vkCreatePipelineLayout(vl.device, &info, NULL, &pb.pipeline_layout));
	VK_CHECK(vkCreatePipelineLayout(vl.device, &info, NULL, &p->pipeline_layout));
	p->pipeline = build_pipeline(vl.device, pb, fbo1.clear_renderpass, p->frag_shader.info.out_var_count, FALSE);
}


void pipeline_build_basic(PipelineObject *p,const char *vert, const char *frag, VkPrimitiveTopology topology)
{

    VkVertexInputBindingDescription bind_desc;
    VkVertexInputAttributeDescription attr_desc[32];
	
#ifdef __cplusplus
    PipelineBuilder pb = {};
#else
	PipelineBuilder pb = {0};
#endif
    //read shaders and register them in the pipeline builder
	shader_create(vl.device, &p->vert_shader, vert, VK_SHADER_STAGE_VERTEX_BIT); 
	shader_create(vl.device, &p->frag_shader, frag, VK_SHADER_STAGE_FRAGMENT_BIT);
	pb.shader_stages_count = 2;
	pb.shader_stages[0] = pipe_shader_stage_create_info(p->vert_shader.stage, p->vert_shader.module);
	pb.shader_stages[1] = pipe_shader_stage_create_info(p->frag_shader.stage, p->frag_shader.module);

    //make shader input attributes to a valid vertex input info for the pipeline (+misc pipeline stuff)
    pb.vertex_input_info = pipe_vertex_input_state_create_info(&p->vert_shader, &bind_desc, attr_desc);
	pb.input_asm = pipe_input_assembly_create_info((VkPrimitiveTopology)topology);
	pb.rasterizer = pipe_rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pb.multisampling = pipe_multisampling_state_create_info();
	pb.color_blend_attachments[0] = pipe_color_blend_attachment_state();
	pb.color_blend_attachments[1] = pipe_color_blend_attachment_state();
	pb.color_blend_attachments[2] = pipe_color_blend_attachment_state();
	pb.color_blend_attachments[3] = pipe_color_blend_attachment_state();

    //make shader uniform variables to a descriptor set layout and configure the pipeline layout
    //(since we don't use push constants, a descriptor set layout is all that's needed)
    VkDescriptorSetLayout layout = shader_create_descriptor_set_layout(&p->vert_shader, &p->frag_shader, 1);
    VkPipelineLayoutCreateInfo info = pipe_layout_create_info(&layout, 1);
	VK_CHECK(vkCreatePipelineLayout(vl.device, &info, NULL, &pb.pipeline_layout));
	VK_CHECK(vkCreatePipelineLayout(vl.device, &info, NULL, &p->pipeline_layout));
    VkRenderPass rp = 
        create_render_pass(VK_ATTACHMENT_LOAD_OP_LOAD,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,p->frag_shader.info.out_var_count, TRUE);
	p->pipeline = build_pipeline(vl.device, pb, rp, p->frag_shader.info.out_var_count, FALSE);


    for (u32 i = 0;i < vl.swap.image_count; ++i)
        create_descriptor_pool(&p->descriptor_pools[i], &p->vert_shader, &p->frag_shader);
	vkDestroyDescriptorSetLayout(vl.device, layout, NULL);
}

void pipeline_build_compiled_shaders(PipelineObject* p, ShaderObject vert, ShaderObject frag, VkPrimitiveTopology topology, b32 depth_enabled)
{

    VkVertexInputBindingDescription bind_desc;
    VkVertexInputAttributeDescription attr_desc[32];

#ifdef __cplusplus
    PipelineBuilder pb = {};
#else
    PipelineBuilder pb = { 0 };
#endif
    p->vert_shader = vert;
    p->frag_shader = frag;

    pb.shader_stages_count = 2;
    pb.shader_stages[0] = pipe_shader_stage_create_info(p->vert_shader.stage, p->vert_shader.module);
    pb.shader_stages[1] = pipe_shader_stage_create_info(p->frag_shader.stage, p->frag_shader.module);

    //make shader input attributes to a valid vertex input info for the pipeline (+misc pipeline stuff)
    pb.vertex_input_info = pipe_vertex_input_state_create_info(&p->vert_shader, &bind_desc, attr_desc);
    pb.input_asm = pipe_input_assembly_create_info((VkPrimitiveTopology)topology);
    pb.rasterizer = pipe_rasterization_state_create_info(VK_POLYGON_MODE_FILL);
    pb.multisampling = pipe_multisampling_state_create_info();
    pb.color_blend_attachments[0] = pipe_color_blend_attachment_state();
    pb.color_blend_attachments[1] = pipe_color_blend_attachment_state();
    pb.color_blend_attachments[2] = pipe_color_blend_attachment_state();
    pb.color_blend_attachments[3] = pipe_color_blend_attachment_state();

    //make shader uniform variables to a descriptor set layout and configure the pipeline layout
    //(since we don't use push constants, a descriptor set layout is all that's needed)
    VkDescriptorSetLayout layout = shader_create_descriptor_set_layout(&p->vert_shader, &p->frag_shader, 1);
    VkPipelineLayoutCreateInfo info = pipe_layout_create_info(&layout, 1);
    VK_CHECK(vkCreatePipelineLayout(vl.device, &info, NULL, &pb.pipeline_layout));
    VK_CHECK(vkCreatePipelineLayout(vl.device, &info, NULL, &p->pipeline_layout));
    VkRenderPass rp =
        create_render_pass(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, p->frag_shader.info.out_var_count, TRUE);
    p->pipeline = build_pipeline(vl.device, pb, rp, p->frag_shader.info.out_var_count, depth_enabled);


    for (u32 i = 0; i < vl.swap.image_count; ++i)
        create_descriptor_pool(&p->descriptor_pools[i], &p->vert_shader, &p->frag_shader);
    vkDestroyDescriptorSetLayout(vl.device, layout, NULL);
}
//releases ALL pipeline resources
void pipeline_cleanup(PipelineObject *pipe)
{
	vkDestroyPipeline(vl.device, pipe->pipeline, NULL);
    vkDestroyPipelineLayout(vl.device, pipe->pipeline_layout, NULL);
	for (u32 i = 0; i < vl.swap.image_count; ++i)
		buf_destroy(&pipe->uniform_buffers[i]);
    //vkDestroyRenderPass(device, renderPass, NULL);
    //descriptor sets are destroyed along with the descriptor_pool
    for (u32 i = 0; i < vl.swap.image_count; ++i)
        vkDestroyDescriptorPool(vl.device, pipe->descriptor_pools[i], NULL);
	//vkDestroyDescriptorSetLayout(device, pipe->descriptor_set_layout, NULL);
}

void vl_base_pipelines_init(void)
{
#ifdef VK_STANDALONE
    pipeline_build_basic_no_reflect(&vl.fullscreen_pipe, "fullscreen.vert", "fullscreen.frag", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_build_basic(&vl.base_pipe, "base.vert", "base.frag", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
#else
    pipeline_build_basic(&vl.def_pipe, "def.vert", "def.frag", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    //why the base pipe though... windows!!!!!!!!!!!!!
    pipeline_build_basic(&vl.base_pipe, "def_vcolor.vert", "def.frag", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
#endif
}

typedef struct UniformBufferManager
{
    DataBuffer **uniform_buffers;
    u32 *indices;
    //needed because we can free only uniform buffers NOT currently in use, meaning of the same swapchain image
    u32 swap_image_count;
    u32 buffers_per_swap_image;
}UniformBufferManager;
UniformBufferManager ubo_manager;

void ubo_manager_init(u32 buffer_count)
{
    ubo_manager.buffers_per_swap_image = buffer_count;
    ubo_manager.swap_image_count = vl.swap.image_count;
    ubo_manager.indices = (u32*)malloc(sizeof(u32) * ubo_manager.swap_image_count);
    ubo_manager.uniform_buffers = (DataBuffer**)malloc(sizeof(DataBuffer*) * ubo_manager.swap_image_count);
    for(u32 i = 0; i < ubo_manager.swap_image_count; ++i)
    {
        ubo_manager.indices[i] = 0;
        ubo_manager.uniform_buffers[i] = (DataBuffer*)malloc(sizeof(DataBuffer) * ubo_manager.buffers_per_swap_image);
    }
}
DataBuffer *ubo_manager_get_next_buf(u32 image_index)
{
    if(ubo_manager.indices[image_index] >= ubo_manager.buffers_per_swap_image)return NULL;
    u32 buffer_index = ubo_manager.indices[image_index]++;
    return &ubo_manager.uniform_buffers[image_index][buffer_index];
}
void ubo_manager_reset(u32 image_index)
{
    u32 last_active_index = ubo_manager.indices[image_index] - 1;
    //printf("last active index: %i\n", last_active_index);
    for (s32 i = last_active_index; i >= 0; --i)
    {
        buf_destroy(&ubo_manager.uniform_buffers[image_index][i]);
    }
    ubo_manager.indices[image_index] = 0;
}

void render_cube_immediate(VkCommandBuffer command_buf, u32 image_index, PipelineObject *p, mat4 model)
{
    VkRenderPassBeginInfo rp_inf = rp_info(vl.render_pass_basic, vl.swap.framebuffers[vl.image_index], 1, vl.swap.extent);
    vkCmdBeginRenderPass(command_buf, &rp_inf, VK_SUBPASS_CONTENTS_INLINE);


    VkDescriptorSetLayout layout = shader_create_descriptor_set_layout(&p->vert_shader, &p->frag_shader, 1);
    DataBuffer *uniform_buffer = create_uniform_buffers(&p->vert_shader.info, 1);
	TextureObject textures[] = {sample_texture, sample_texture2};
    VkDescriptorSet *desc_set = create_descriptor_sets(layout, &p->vert_shader,&p->frag_shader, p->descriptor_pools[image_index], uniform_buffer,textures, array_count(textures),  1);

    DataBuffer *buf = ubo_manager_get_next_buf(image_index);
    if (buf == NULL)return;
    *buf = *uniform_buffer;
    uniform_buffer = buf;

	vkDestroyDescriptorSetLayout(vl.device, layout, NULL);


	vkCmdBindPipeline(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);
#ifdef USE_VOLK
    vkCmdSetPrimitiveTopologyEXT(command_buf,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
#endif

    VkBuffer vertex_buffers[] = {vertex_buffer_real.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buf, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(command_buf, index_buffer_real.buffer, 0, VK_INDEX_TYPE_UINT32);
    
    VkViewport viewport = viewport_basic();
    vkCmdSetViewport(command_buf, 0, 1, &viewport);

    VkRect2D scissor = scissor_basic();
    vkCmdSetScissor(command_buf, 0, 1, &scissor);
 

    void *data = malloc(p->vert_shader.info.descriptor_bindings[0].mem_size);

    shader_set_immediate(&p->vert_shader.info, "model", model.elements, data);
    mat4 view = look_at(v3(0,0,0), v3(0,0,-1), v3(0,1,0));
    shader_set_immediate(&p->vert_shader.info, "view", view.elements, data);
    mat4 proj = perspective_proj_vk(45.0f,window_w/(f32)window_h, 0.1, 10);
    shader_set_immediate(&p->vert_shader.info, "proj", proj.elements, data);
    f32 color_modifier = 1.0f;
    shader_set_immediate(&p->vert_shader.info, "modifier", &color_modifier, data);

	
	
    //copy uniform data to UBO
	VK_CHECK(buf_map(uniform_buffer, uniform_buffer->size, 0));
	memcpy(uniform_buffer->mapped, data,p->vert_shader.info.descriptor_bindings[0].mem_size);
	buf_unmap(uniform_buffer);


    vkCmdBindDescriptorSets(vl.command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                p->pipeline_layout, 0, 1, desc_set, 0, NULL);

    vkCmdDrawIndexed(command_buf, index_buffer_real.size / sizeof(u32), 1, 0, 0, 0);
	
    vkCmdEndRenderPass(vl.command_buffers[vl.image_index]);
}

VkRenderPass* current_renderpass = NULL;
void render_def_vbo(float *mvp, vec4 color, DataBuffer *vbo, DataBuffer *ibo,u32 vertex_offset, u32 index_offset, TextureObject *tex,u32 vbo_updated, b32 color_enabled, VkPrimitiveTopology topology, int* v, u32 index_count)
{
    VkCommandBuffer command_buf = vl.command_buffers[vl.image_index];
    VkRenderPassBeginInfo rp_inf = rp_info(vl.render_pass_basic, vl.swap.framebuffers[vl.image_index], 1, vl.swap.extent);
    vkCmdBeginRenderPass(vl.command_buffers[vl.image_index], &rp_inf, VK_SUBPASS_CONTENTS_INLINE);
    

    PipelineObject* p = &vl.def_pipe;
    if (color_enabled)p = &vl.base_pipe;
    VkDescriptorSetLayout layout = shader_create_descriptor_set_layout(&p->vert_shader, &p->frag_shader, 1);
    DataBuffer* uniform_buffer = create_uniform_buffers(&p->vert_shader.info, 1);
    TextureObject textures[] = { sample_texture, sample_texture2 };
    VkDescriptorSet* desc_set;
    if (tex)
        desc_set = create_descriptor_sets(layout, &p->vert_shader, &p->frag_shader, p->descriptor_pools[vl.image_index], uniform_buffer, tex, 1, 1);
    else
        desc_set = create_descriptor_sets(layout, &p->vert_shader, &p->frag_shader, p->descriptor_pools[vl.image_index], uniform_buffer, textures, 1, 1);

    //transfers ownership of UBO to manager :P its rly ugly tho FIX
    DataBuffer* buf = ubo_manager_get_next_buf(vl.image_index);
    if (buf == NULL)return;
    *buf = *uniform_buffer;
    uniform_buffer = buf;

    vkDestroyDescriptorSetLayout(vl.device, layout, NULL);


    vkCmdBindPipeline(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);
#ifdef USE_VOLK
    vkCmdSetPrimitiveTopologyEXT(command_buf, topology);
#endif

    if (vbo->active && ibo->active) //this means that we have static geometry
    {
        /*
        VkBuffer vertex_buffers[] = { global_vertex_buffer.gpu_side_buffer.buffer };
        VkDeviceSize offsets[] = { vertex_offset };
        vkCmdBindVertexBuffers(command_buf, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(command_buf, global_index_buffer.gpu_side_buffer.buffer, index_offset, VK_INDEX_TYPE_UINT32);
        */

        VkBuffer vertex_buffers[] = { vbo->buffer };
        VkDeviceSize offsets[] = { vertex_offset };
        vkCmdBindVertexBuffers(command_buf, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(command_buf, ibo->buffer, index_offset, VK_INDEX_TYPE_UINT32);
        index_count = ibo->size / sizeof(u32);
    }
    else //dynamic!
    {
        VkBuffer vertex_buffers[] = { global_vertex_buffer.gpu_side_buffer.buffer };
        VkDeviceSize offsets[] = { vertex_offset };
        vkCmdBindVertexBuffers(command_buf, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(command_buf, global_index_buffer.gpu_side_buffer.buffer, index_offset, VK_INDEX_TYPE_UINT32);
    }



    

    VkViewport viewp = viewport(v[0], v[1], v[2], v[3]);
    vkCmdSetViewport(command_buf,0,1,&viewp);

    VkRect2D scissor = scissor_basic();
    vkCmdSetScissor(command_buf, 0, 1, &scissor);

    void* data = malloc(p->vert_shader.info.descriptor_bindings[0].mem_size);

    shader_set_immediate(&p->vert_shader.info, "WorldViewProj", mvp, data);
    shader_set_immediate(&p->vert_shader.info, "constColor", &color, data);
    //whether the primitive is textured or not
    f32 textured;
    if (tex == NULL)
        textured = 0.0f;
    else
        textured = 1.0f;
    shader_set_immediate(&p->vert_shader.info, "textured", &textured, data);

    //copy uniform data to UBO
    VK_CHECK(buf_map(uniform_buffer, uniform_buffer->size, 0));
    memcpy(uniform_buffer->mapped, data, p->vert_shader.info.descriptor_bindings[0].mem_size);
    buf_unmap(uniform_buffer);


    vkCmdBindDescriptorSets(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
        p->pipeline_layout, 0, 1, desc_set, 0, NULL);

    vkCmdDrawIndexed(command_buf, index_count, 1, 0,0, 0);

    vkCmdEndRenderPass(command_buf);

    if (vbo_updated && 0)
    {
        ///*
        //---------------STALL-------------------
        //VK_STANDALONEute command buffer
        vkEndCommandBuffer(vl.command_buffers[vl.image_index]);
        VkSubmitInfo submit_info = { 0 };
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &vl.command_buffers[vl.image_index];
        //wait for EVERYTHING to finish
        vkQueueSubmit(vl.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(vl.graphics_queue);
        //reset the command buffer for the next drawcall!
        vkResetCommandBuffer(vl.command_buffers[vl.image_index], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        VkCommandBufferBeginInfo begin_info = { 0 };
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = NULL;
        vkBeginCommandBuffer(vl.command_buffers[vl.image_index], &begin_info);
        //*/
    }
}

void render_effect_vbo(PipelineObject *pipe, DataBuffer *ubo, DataBuffer* vbo, DataBuffer* ibo, TextureObject* tex, VkViewport v, VkRect2D s)
{
    PipelineObject* p = pipe;
    VkDescriptorSetLayout layout = shader_create_descriptor_set_layout(&p->vert_shader, &p->frag_shader, 1);
    TextureObject textures[] = { sample_texture, sample_texture2 };
    VkDescriptorSet* desc_set;//                                                                VV MAKE THIS THE PIPELINE POOL!!!
    desc_set = create_descriptor_sets(layout, &p->vert_shader, &p->frag_shader, vl.def_pipe.descriptor_pools[vl.image_index], ubo, textures, 1, 1);
    VkCommandBuffer command_buf = vl.command_buffers[vl.image_index];

    vkDestroyDescriptorSetLayout(vl.device, layout, NULL);


    vkCmdBindPipeline(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);

#ifdef USE_VOLK
    vkCmdSetPrimitiveTopologyEXT(command_buf, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
#endif

    VkBuffer vertex_buffers[] = { vbo->buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(command_buf, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(command_buf, ibo->buffer, 0, VK_INDEX_TYPE_UINT32);

    //VkViewport viewport = viewport_basic();
    vkCmdSetViewport(command_buf, 0, 1, &v);

    //VkRect2D scissor = scissor_basic();
    vkCmdSetScissor(command_buf, 0, 1, &s);

    void* data = malloc(p->vert_shader.info.descriptor_bindings[0].mem_size);

    
    f32 textured = 0.0f;
    
    vkCmdBindDescriptorSets(command_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
        p->pipeline_layout, 0, 1, desc_set, 0, NULL);

    vkCmdDrawIndexed(command_buf, ibo->size / sizeof(u32), 1, 0, 0, 0);
    
    
}

void create_sync_objects(void)
{
    image_available_semaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    in_flight_fences = (VkFence*)malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    images_in_flight = (VkFence*)malloc(sizeof(VkFence) * vl.swap.image_count);
    for (u32 i = 0; i < vl.swap.image_count; ++i)images_in_flight[i] = VK_NULL_HANDLE;
    
    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkCreateSemaphore(vl.device, &semaphore_info, NULL, &image_available_semaphores[i])|| 
            vkCreateSemaphore(vl.device, &semaphore_info, NULL, &render_finished_semaphores[i])||
            vkCreateFence(vl.device, &fence_info, NULL, &in_flight_fences[i]);
    }
}

void create_surface(void)
{
#ifdef VK_STANDALONE
	window_create_window_surface(vl.instance, &wnd, &vl.surface);
#else
    VkWin32SurfaceCreateInfoKHR surface_create_info =
    { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,NULL,0,vk_getWindowSystemInstance(),vk_getWindowSystemHandle() };

    if (vkCreateWin32SurfaceKHR(vl.instance, &surface_create_info, NULL, &vl.surface) != VK_SUCCESS)
        vk_error("Failed to create win32 window surface!");
#endif
}

void framebuffer_resize_callback(void){framebuffer_resized = TRUE;}

void vl_cleanup_swapchain(void)
{
    for (u32 i = 0; i < vl.swap.image_count; ++i)
        vkDestroyFramebuffer(vl.device, vl.swap.framebuffers[i], NULL);
	vkDestroyPipeline(vl.device, vl.fullscreen_pipe.pipeline, NULL);
    for (u32 i = 0; i < vl.swap.image_count; ++i)
        vkDestroyImageView(vl.device, vl.swap.image_views[i], NULL);
    vkDestroySwapchainKHR(vl.device, vl.swap.swapchain, NULL);
	texture_cleanup(&vl.swap.depth_attachment);
	vkDestroyRenderPass(vl.device, vl.swap.rp_begin, NULL);
}

void vl_recreate_swapchain(void)
{
    vkDeviceWaitIdle(vl.device);
    //in case of window minimization (w = 0, h = 0) we wait until we get a proper window again
    u32 width = 0, height = 0;
#ifdef VK_STANDALONE
	window_get_framebuffer_size(&wnd, &width, &height);
#else
    
    width = vk_getWidth();
    height = vk_getHeight();
#endif
    
    
    
    vl_cleanup_swapchain();
    vl_create_swapchain();
    vl_create_swapchain_image_views();
	vl.render_pass_basic = create_render_pass(VK_ATTACHMENT_LOAD_OP_LOAD,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL, 1, TRUE);
	//vl.swap.rp_begin = create_render_pass(VK_ATTACHMENT_LOAD_OP_CLEAR,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL, 1, TRUE);
    
    
	vl_base_pipelines_init();
	
    vl_swap_create_framebuffers();
    vl_create_command_buffers();
}

void create_texture_sampler(VkSampler *sampler)
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
TextureObject create_texture_image(char *filename, VkFormat format)
{
	TextureObject tex;
	//[0]: we read an image and store all the pixels in a pointer
	s32 tex_w, tex_h, tex_c;
	stbi_uc *pixels = stbi_load(filename, &tex_w, &tex_h, &tex_c, STBI_rgb_alpha);
	VkDeviceSize image_size = tex_w * tex_h * 4;
	
	
	//[2]: we create a buffer to hold the pixel information (we also fill it)
	DataBuffer idb;
	if (!pixels)
		vk_error("Error loading image %s!");
	create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
	(VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), &idb, image_size, pixels);
	//[3]: we free the cpu side image, we don't need it
	stbi_image_free(pixels);
	//[4]: we create the VkImage that is undefined right now
	create_image(tex_w, tex_h, format, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT 
		| VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &tex.image, &tex.mem);
	
	//[5]: we transition the images layout from undefined to dst_optimal
	transition_image_layout(tex.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	//[6]: we copy the buffer we created in [1] to our image of the correct format (R8G8B8A8_SRGB)
	copy_buffer_to_image(idb.buffer, tex.image, tex_w, tex_h);
	
	//[7]: we transition the image layout so that it can be read by a shader
	transition_image_layout(tex.image, format, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		
	//[8]:cleanup the buffers (all data is now in the image)
	buf_destroy(&idb);
	
	
	create_texture_sampler(&tex.sampler);
	
	tex.view = create_image_view(tex.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
	tex.mip_levels = 0;
	tex.width = tex_w;
	tex.height = tex_h;
	
	return tex;
}


TextureObject create_texture_image_wdata(u8* pixels,u32 tex_w, u32 tex_h,u32 component_count, VkFormat format)
{
    vkDeviceWaitIdle(vl.device);
    TextureObject tex;
   
    VkDeviceSize image_size = tex_w * tex_h * component_count;

    //[2]: we create a buffer to hold the pixel information (we also fill it)
    DataBuffer idb;
    if (!pixels)
        vk_error("Error loading image %s!");
    create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), &idb, image_size, pixels);
    //[3]: we free the cpu side image, we don't need it
    //[4]: we create the VkImage that is undefined right now
    create_image(tex_w, tex_h, format, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &tex.image, &tex.mem);

    //[5]: we transition the images layout from undefined to dst_optimal
    transition_image_layout(tex.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    //[6]: we copy the buffer we created in [1] to our image of the correct format (R8G8B8A8_SRGB)
    copy_buffer_to_image(idb.buffer, tex.image, tex_w, tex_h);

    //[7]: we transition the image layout so that it can be read by a shader
    transition_image_layout(tex.image, format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    //[8]:cleanup the buffers (all data is now in the image)
    buf_destroy(&idb);


    create_texture_sampler(&tex.sampler);

    tex.view = create_image_view(tex.image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    tex.mip_levels = 0;
    tex.width = tex_w;
    tex.height = tex_h;

    return tex;
}
void vulkan_layer_init(void)
{
#ifdef __cplusplus
    vl = {}; //current image index to draw
#else
	vl = (VulkanLayer){0};
#endif
	vl_create_instance();
    //vl_setup_debug_messenger();
    create_surface();
    vl_pick_physical_device();
    vl_create_logical_device();
    vl_create_command_pool();
    vl_create_swapchain();
    vl_create_swapchain_image_views();
	vl.render_pass_basic = create_render_pass(VK_ATTACHMENT_LOAD_OP_LOAD,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL, 1, TRUE);
	vl_swap_create_framebuffers();
    
	
}

int vulkan_init(void) {
#ifdef USE_VOLK
    volkInitialize();
#endif
	vulkan_layer_init();
    //vkCmdBeginRenderingKHR = (vkGetInstanceProcAddr(vl.instance, "vkCmdBeginRenderingKHR"));
    //vkCmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)vkGetInstanceProcAddr(vl.instance, "vkCmdEndRenderingKHR");
    
    //fbo_init(&fbo1, 2);
	//fbo_cleanup(&fbo1);
#ifdef VK_STANDALONE
    sample_texture = create_texture_image("../assets/test.png",VK_FORMAT_R8G8B8A8_SRGB);
	sample_texture2 = create_texture_image("../assets/test.png",VK_FORMAT_R8G8B8A8_UNORM);
#else
    sample_texture = create_texture_image("../resources/generic/textures/checkerboard.png", VK_FORMAT_R8G8B8A8_SRGB);
    sample_texture2 = create_texture_image("../resources/generic/textures/checkerboard.png", VK_FORMAT_R8G8B8A8_UNORM);
#endif

    ubo_manager_init(700);
	

	
	Vertex *cube_vertices = cube_build_verts();
	//create vertex buffer @check first param
	create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
	(VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), 
	&vertex_buffer_real, sizeof(Vertex) * 24, cube_vertices);
	
	
	
	//create index buffer
	create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
	(VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), 
	&index_buffer_real, sizeof(cube_indices[0]) * array_count(cube_indices), cube_indices);

	

    
    vl_create_command_buffers();
    create_sync_objects();


    TextureObject textures[2];

    textures[0] = create_color_attachment(vl.swap.extent.width, vl.swap.extent.height, vl.swap.image_format);
    textures[1] = create_color_attachment(vl.swap.extent.width, vl.swap.extent.height, vl.swap.image_format);
    TextureObject depth_attachment = create_depth_attachment(vl.swap.extent.width, vl.swap.extent.height);
    fbo_init(&fbo1, 2);
	vl_base_pipelines_init();

	return 1;
}


void frame_start(void)
{
    vkWaitForFences(vl.device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
    //[0]: Acquire free image from the swapchain
 
    VkResult res = vkAcquireNextImageKHR(vl.device, vl.swap.swapchain, UINT64_MAX,
        image_available_semaphores[current_frame], VK_NULL_HANDLE, &vl.image_index);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) { vl_recreate_swapchain(); return; }
    else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)vk_error("Failed to acquire swapchain image!");

    // check if the image is already used (in flight) froma previous frame, and if so wait
    if (images_in_flight[vl.image_index] != VK_NULL_HANDLE)vkWaitForFences(vl.device, 1, &images_in_flight[vl.image_index], VK_TRUE, UINT64_MAX);


    ubo_manager_reset(vl.image_index);
#ifdef VK_STANDALONE
    vkResetDescriptorPool(vl.device, vl.base_pipe.descriptor_pools[vl.image_index], NULL);
#else
    vkResetDescriptorPool(vl.device, vl.def_pipe.descriptor_pools[vl.image_index], NULL);
    vkResetDescriptorPool(vl.device, vl.base_pipe.descriptor_pools[vl.image_index], NULL);
#endif
    
    VK_CHECK(vkResetCommandBuffer(vl.command_buffers[vl.image_index], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
        
		
    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(vl.command_buffers[vl.image_index], &begin_info));


    /*
    insert_image_memory_barrier(
				vl.command_buffers[vl.image_index],
				vl.swap.images[vl.image_index],
				0,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
				(VkImageSubresourceRange){ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    */
    ///*
    VkRenderPassBeginInfo renderpass_info = rp_info(vl.swap.rp_begin, vl.swap.framebuffers[vl.image_index], 1, vl.swap.extent);
    vkCmdBeginRenderPass(vl.command_buffers[vl.image_index], &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(vl.command_buffers[vl.image_index]);
    //*/

    /*
    renderpass_info = rp_info(fbo1.clear_renderpass, fbo1.framebuffer, fbo1.attachment_count, vl.swap.extent);
    vkCmdBeginRenderPass(vl.command_buffers[vl.image_index], &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(vl.command_buffers[vl.image_index]);
 
 */
}
void frame_end(void)
{

insert_image_memory_barrier(
				vl.command_buffers[vl.image_index],
				vl.swap.images[vl.image_index],
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				0,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
#ifdef VK_STANDALONE
				(VkImageSubresourceRange){ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
#else

				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
#endif
    VK_CHECK(vkEndCommandBuffer(vl.command_buffers[vl.image_index]));

    //mark image as used by _this frame_
    images_in_flight[vl.image_index] = in_flight_fences[current_frame];

    //[1]: VK_STANDALONEute the command buffer with that image as attachment in the framebuffer
    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore wait_semaphores[] = { image_available_semaphores[current_frame] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vl.command_buffers[vl.image_index];
    VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    vkResetFences(vl.device, 1, &in_flight_fences[current_frame]);
    VK_CHECK(vkQueueSubmit(vl.graphics_queue, 1, &submit_info, in_flight_fences[current_frame]));
    //[2]: Return the image to the swapchain for presentation
    VkPresentInfoKHR present_info = { 0 };
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains[] = { vl.swap.swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &vl.image_index;
    present_info.pResults = NULL;

    //we push the data to be presented to the present queue
    VkResult res = vkQueuePresentKHR(vl.present_queue, &present_info);

    //recreate swapchain if necessary
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebuffer_resized)
    {
        framebuffer_resized = FALSE;
        vl_recreate_swapchain();
    }
    else if (res != VK_SUCCESS)
        vk_error("Failed to present swapchain image!");

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}


void draw_frame(void)
{
#ifdef VK_STANDALONE
    frame_start();
#endif
	
	
    mat4 m = mat4_mul(mat4_translate(v3(-1.2,0,-6)), m4d(1.f));
    render_cube_immediate(vl.command_buffers[vl.image_index], vl.image_index, &vl.base_pipe, m);
	

#ifdef VK_STANDALONE
    m = mat4_mul(mat4_translate(v3(-2.f * sin(gettimems()/400.f),0,-4)),m4d(1.f));
#else
	m = mat4_mul(mat4_translate(v3(1.2,0,-4)),m4d(1.f));
#endif
    render_cube_immediate(vl.command_buffers[vl.image_index], vl.image_index, &vl.base_pipe, m);
	render_fullscreen(vl.command_buffers[vl.image_index], vl.image_index);
#ifdef VK_STANDALONE
    frame_end();
#endif
}

#ifdef VK_STANDALONE

f32 nb_frames = 0.0f;
f32 current_time = 0.0f;
f32 prev_time = 0.0f;
void calc_fps(void)
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
void main_loop(void)
{
    
	while (!window_should_close(&wnd)) {
        calc_fps();
		window_poll_events(&wnd);
        draw_frame();
	}
    
    //vkDeviceWaitIdle(device);
}
#endif


void cleanup(void)
{
    vkDeviceWaitIdle(vl.device);  //so we dont close the window while commands are still being VK_STANDALONEuted
    vl_cleanup_swapchain();
    
	buf_destroy(&index_buffer_real);
	buf_destroy(&vertex_buffer_real);
	pipeline_cleanup(&vl.base_pipe);
	pipeline_cleanup(&vl.fullscreen_pipe);
	
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkDestroySemaphore(vl.device, render_finished_semaphores[i], NULL);
        vkDestroySemaphore(vl.device, image_available_semaphores[i], NULL);
        vkDestroyFence(vl.device, in_flight_fences[i], NULL);
    }
    vkDestroyCommandPool(vl.device, vl.command_pool, NULL);
    vkDestroyDevice(vl.device, NULL);
    vkDestroySurfaceKHR(vl.instance, vl.surface, NULL);
    DestroyDebugUtilsMessengerEXT(vl.instance, vl.debug_msg, NULL);
    vkDestroyInstance(vl.instance, NULL);

    vkDestroyRenderPass(vl.device, vl.swap.rp_begin, NULL);
	//window_destroy(&wnd);
}


#if defined(VK_STANDALONE)
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
#endif

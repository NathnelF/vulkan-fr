#include "headers.h"

#include "pipeline.h"

struct VertexFormatInfo
{
    VkVertexInputBindingDescription bindings[4];
    VkVertexInputAttributeDescription attributes[8];
    u32 binding_count;
    u32 attribute_count;
};

static VertexFormatInfo GetVertexFormatInfo(VertexFormat format)
{
    VertexFormatInfo info = {};

    switch (format)
    {
        case VERTEX_FORMAT_NONE:
        {
            return info;
        }
        case VERTEX_FORMAT_BASIC:
        {
            info.binding_count = 1;
            info.bindings[0] = {
                .binding = 0,
                .stride = 12,
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            };
            info.attribute_count = 1;
            info.attributes[0] = {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            };
            return info;
        }
        case VERTEX_FORMAT_STATIC:
        {
            info.binding_count = 1;
            info.bindings[0] = {
                .binding = 0,
                .stride = 44,
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            };
            info.attribute_count = 4;
            info.attributes[0] = {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            }; // pos
            info.attributes[1] = {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 12,
            }; // normal
            info.attributes[2] = {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 24,
            }; // uv

            info.attributes[3] = {
                .location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 32,
            }; // tangent
            return info;
        }
        case VERTEX_FORMAT_SKINNED:
        {
            // TODO(Nate): implement skinned layout
            return info;
        }
        case VERTEX_FORMAT_UI:
        {
            // TODO(Nate): implement ui layout
            return info;
        }
    }
}

static VkPipelineColorBlendAttachmentState GetBlendState(BlendMode mode)
{
    VkColorComponentFlags all_channels =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    switch (mode)
    {
        case BLEND_NONE:
        {
            return {
                .blendEnable = VK_FALSE,
                .colorWriteMask = all_channels,
            };
        }
        case BLEND_ALPHA:
        {
            return {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = all_channels,
            };
        }
        case BLEND_ADDITIVE:
        {
            return {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = all_channels,
            };
        }
    }
}

static VkPipelineDepthStencilStateCreateInfo GetDepthState(DepthMode mode)
{
    VkPipelineDepthStencilStateCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    switch (mode)
    {
        case DEPTH_READ_WRITE:
            info.depthTestEnable = VK_TRUE;
            info.depthWriteEnable = VK_TRUE;
            info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            break;
        case DEPTH_READ:
            info.depthTestEnable = VK_TRUE;
            info.depthWriteEnable = VK_FALSE;
            info.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
            break;
        case DEPTH_WRITE:
            info.depthTestEnable = VK_TRUE;
            info.depthWriteEnable = VK_TRUE;
            info.depthCompareOp = VK_COMPARE_OP_ALWAYS;
            break;
        case DEPTH_NONE:
            info.depthTestEnable = VK_FALSE;
            info.depthWriteEnable = VK_FALSE;
            break;
    }

    return info;
}

PipelineDesc DefaultPipelineDesc(State *state)
{
    PipelineDesc desc = {
        .vert = VK_NULL_HANDLE,
        .frag = VK_NULL_HANDLE,
        .blend = BLEND_NONE,
        .depth = DEPTH_READ_WRITE,
        .vertex_format = VERTEX_FORMAT_BASIC,
        // TODO(Nate): when we graduate to static vertex format we need to
        // switch to cull mode back bit!
        .cull_mode = VK_CULL_MODE_NONE,
        .color_format = state->swapchain.image_format,
        .depth_format = state->context.depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
    };
    return desc;
}

Pipeline BuildPipeline(State *state, PipelineDesc *desc)
{
    VkPipelineShaderStageCreateInfo stages[2] = {};
    u32 stage_count = 0;

    stages[stage_count++] = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = desc->vert,
        .pName = "main",
    };

    if (desc->frag != VK_NULL_HANDLE)
    {
        stages[stage_count++] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = desc->frag,
            .pName = "main",
        };
    }

    VertexFormatInfo vertex_format = GetVertexFormatInfo(desc->vertex_format);

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = vertex_format.binding_count,
        .pVertexBindingDescriptions = vertex_format.bindings,
        .vertexAttributeDescriptionCount = vertex_format.attribute_count,
        .pVertexAttributeDescriptions = vertex_format.attributes,
    };

    // totally fixed states

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = desc->cull_mode,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = desc->samples,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // blend + depth
    VkPipelineColorBlendAttachmentState blend_attachment =
      GetBlendState(desc->blend);

    VkPipelineColorBlendStateCreateInfo blend_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    VkPipelineDepthStencilStateCreateInfo depth_state =
      GetDepthState(desc->depth);

    VkPipelineRenderingCreateInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = (desc->frag != VK_NULL_HANDLE) ? 1u : 0u,
        .pColorAttachmentFormats = &desc->color_format,
        .depthAttachmentFormat = desc->depth_format,
        .stencilAttachmentFormat = desc->depth_format,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .stageCount = stage_count,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_state,
        .pColorBlendState = &blend_state,
        .pDynamicState = &dynamic_state,
        .layout = desc->layout,
    };

    Pipeline pipeline = {};
    pipeline.layout = desc->layout;

    validate(vkCreateGraphicsPipelines(state->context.device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &pipeline_info,
                                       NULL,
                                       &pipeline.handle),
             "could not create graphics pipeline");

    debug("created pipeline");
    return pipeline;
}

VkShaderModule LoadShaderModule(State *state, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        err("could not open shader: %s", path);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    u32 *code = (u32 *)malloc(size);
    fread(code, 1, size, f);
    fclose(f);

    VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = (size_t)size,
        .pCode = code,
    };

    VkShaderModule module;
    validate(vkCreateShaderModule(state->context.device, &info, NULL, &module),
             "could not create shader module: %s",
             path);

    free(code);
    debug("loaded shader: %s", path);
    return module;
}

#pragma once
#include "headers.h"

enum BlendMode
{
    BLEND_NONE,
    BLEND_ALPHA,
    BLEND_ADDITIVE,
};

enum DepthMode
{
    DEPTH_NONE,
    DEPTH_READ_WRITE,
    DEPTH_READ,
    DEPTH_WRITE,
};

enum VertexFormat
{
    VERTEX_FORMAT_NONE,
    VERTEX_FORMAT_BASIC,
    VERTEX_FORMAT_STATIC,
    VERTEX_FORMAT_SKINNED,
    VERTEX_FORMAT_UI,
};

struct PipelineDesc
{
    VkShaderModule vert;
    VkShaderModule frag;

    BlendMode blend;
    DepthMode depth;
    VertexFormat vertex_format;
    VkCullModeFlags cull_mode;

    VkFormat color_format;
    VkFormat depth_format;

    VkSampleCountFlagBits samples;

    VkPipelineLayout layout;
};

PipelineDesc DefaultPipelineDesc(State *state);

Pipeline BuildPipeline(State *state, PipelineDesc *desc);

VkShaderModule LoadShaderModule(State *state, const char *path);
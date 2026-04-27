#pragma once
#include "types.h"

#include "debug.h"

#include "includes.h"

#define FRAMES_IN_FLIGHT 3

#define Megabytes(n) ((u64)(n) * 1024 * 1024)

struct FrameContext
{
    VkSemaphore begin_rendering_semaphore;
    VkFence fence;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
};

struct Context
{
    VkInstance instance;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue queue;
    u32 graphics_queue_index;
    VmaAllocator allocator;
    SDL_Window *window;
    VkSurfaceKHR surface;
    VkFormat depth_format;
    FrameContext frame_context[FRAMES_IN_FLIGHT];

    // frame context?
};

struct Swapchain
{
    VkSwapchainKHR handle;
    VkImage images[8];
    VkImageView views[8];
    VkSemaphore begin_presenting_semaphore[8]; // tied to image count
    VkImage depth_image;
    VkImageView depth_view;
    VmaAllocation depth_alloc;
    VkFormat image_format;
    u32 image_count;
    u32 width;
    u32 height;
};

struct Pipeline
{
    VkPipeline handle;
    VkPipelineLayout layout;
};

enum PiplineFormat
{
    PIPELINE_BASIC,
    PIPELINE_DEPTH_PREPASS_STATIC,
    PIPELINE_DEPTH_PREPASS_SKINNED,
    PIPELINE_SHADOW_MAP_STATIC,
    PIPELINE_SHADOW_MAP_SKINNED,
    PIPELINE_OPAQUE_STATIC,
    PIPELINE_OPAQUE_SKINNED,
    PIPELINE_DOUBLE_SIDED_STATIC,
    PIPELINE_DOUBLE_SIDED_SKINNED,
    PIPELINE_ALPHA_TESTED_STATIC,
    PIPELINE_ALPHA_TESTED_SKINNED,
    PIPELINE_TRANSPARENT_STATIC,
    PIPELINE_TRANSPARENT_SKINNED,
    PIPELINE_UI,
    PIPELINE_ADDITIVE_PARTICLE,
};

struct MeshRegion
{
    u32 vertex_offset;
    u32 vertex_count;
    u32 index_offset;
    u32 index_count;
};

#define MAX_MESHES 128
#define MEGA_BUFFER_SIZE Megabytes(128)

struct MeshData
{
    VkBuffer buffer;
    VmaAllocation allocation;
    MeshRegion meshes[MAX_MESHES];
    u32 mesh_count;
};

struct State
{
    Context context;
    Swapchain swapchain;
    Pipeline pipelines[20];
    MeshData mesh_data;
};
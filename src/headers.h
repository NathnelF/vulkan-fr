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

struct CameraConstants
{
    HMM_Mat4 mvp;
};

struct PushConstants
{
    VkDeviceAddress camera_address;
};

struct Camera
{
    HMM_Vec3 target;
    float yaw;
    float pitch;
    float distance;

    float pan_speed;
    float zoom_speed;
    float rotate_speed;

    VkBuffer buffers[FRAMES_IN_FLIGHT];
    VmaAllocation allocs[FRAMES_IN_FLIGHT];
    void *ptrs[FRAMES_IN_FLIGHT];
    VkDeviceAddress camera_buffer_address[FRAMES_IN_FLIGHT];
};

struct Instance
{
    HMM_Mat4 transform;
    u32 mesh_index;
    u32 texture_index;
    float padding[2];
};

#define MAX_INSTANCES 1024
#define MAX_DRAWS 1024

struct Scene
{
    Instance instances[MAX_INSTANCES];
    u32 instance_count;

    VkBuffer instance_buffers[FRAMES_IN_FLIGHT];
    VmaAllocation instance_allocations[FRAMES_IN_FLIGHT];
    VkDeviceAddress instance_addresses[FRAMES_IN_FLIGHT];
    Instance *instance_ptrs[FRAMES_IN_FLIGHT];

    VkDrawIndirectCommand draws[MAX_DRAWS];
    u32 draw_count;

    VkBuffer draw_buffers[FRAMES_IN_FLIGHT];
    VmaAllocation draw_allocations[FRAMES_IN_FLIGHT];
    VkDeviceAddress draw_addresses[FRAMES_IN_FLIGHT];
    VkDrawIndirectCommand *draw_ptrs[FRAMES_IN_FLIGHT];
};

struct State
{
    Context context;
    Swapchain swapchain;
    Pipeline pipelines[20];
    MeshData mesh_data;
    Camera camera;
    Scene scene;
};
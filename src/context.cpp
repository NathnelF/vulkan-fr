#include "headers.h"
//
#include "VkBootstrap.h"
//

struct vkb_handles
{
    vkb::Instance instance;
    vkb::PhysicalDevice gpu;
    vkb::Device device;
};

void CreateWindow(State *state, vkb_handles *handles)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        err("could not initialize sdl");
    }
    SDL_Window *window = SDL_CreateWindow(
      "Amoeba", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        err("failed to create Window");
    }
    state->context.window = window;
    debug("created window");
}

void CreateInstance(State *state, vkb_handles *handles)
{
    vkb::InstanceBuilder instance_builder;
    u32 count;
    const char *const *sdl_extensions =
      SDL_Vulkan_GetInstanceExtensions(&count);
    instance_builder.set_app_name("Vulkan-Fr").require_api_version(1, 3, 0);

    for (u32 i = 0; i < count; i++)
    {
        instance_builder.enable_extension(sdl_extensions[i]);
    }

#ifdef DEBUG_BUILD
    instance_builder.request_validation_layers(true);
    instance_builder.use_default_debug_messenger();
#endif

    auto result = instance_builder.build();

    if (!result)
    {
        err("failed to create vulkan instance");
    }

    vkb::Instance vkb_instance = result.value();
    handles->instance = vkb_instance;
    state->context.instance = vkb_instance.instance;
    volkLoadInstance(state->context.instance);
    debug("created instance");
}

void CreateSurface(State *state)
{
    if (!SDL_Vulkan_CreateSurface(state->context.window,
                                  state->context.instance,
                                  NULL,
                                  &state->context.surface))
    {
        err("failed to create vulkan surface");
    }
    debug("created vulkan surface");
}

void SelectGPU(State *state, vkb_handles *handles)
{
    vkb::PhysicalDeviceSelector gpu_picker(handles->instance);
    VkPhysicalDeviceFeatures core_features = {
        .multiDrawIndirect = true,
        .samplerAnisotropy = true,
    };
    auto result = gpu_picker.set_surface(state->context.surface)
                    .set_minimum_version(1, 3)
                    .set_required_features(core_features)
                    .select();

    if (!result)
    {
        err("couldn't pick gpu");
    }

    vkb::PhysicalDevice vkb_gpu = result.value();
    handles->gpu = vkb_gpu;
    state->context.gpu = vkb_gpu.physical_device;
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(state->context.gpu, &properties);
    debug("selected %s as gpu!", properties.deviceName);
}

void CreateDevice(State *state, vkb_handles *handles)
{
    vkb::DeviceBuilder device_builder(handles->gpu);

    VkPhysicalDeviceVulkan12Features vk_12_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .bufferDeviceAddress = true,
    };

    VkPhysicalDeviceVulkan13Features vk_13_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vk_12_features,
        .synchronization2 = true,
        .dynamicRendering = true,
    };
    auto result = device_builder.set_allocation_callbacks(nullptr)
                    .add_pNext(&vk_13_features)
                    .add_pNext(&vk_12_features)
                    .build();

    if (!result)
    {
        err("failed to create logical deivce");
    }

    vkb::Device vkb_device = result.value();
    handles->device = vkb_device;
    state->context.device = vkb_device.device;
    volkLoadDevice(state->context.device);
    debug("created logical device");
}

void GetGraphicsQueue(State *state, vkb_handles *handles)
{
    auto device = handles->device;
    auto queue_result = device.get_queue(vkb::QueueType::graphics);
    if (!queue_result)
    {
        err("couldn't find graphics queue");
    }

    auto index_result = device.get_queue_index(vkb::QueueType::graphics);

    if (!index_result)
    {
        err("couldn't find graphics queue index");
    }

    state->context.queue = queue_result.value();
    state->context.graphics_queue_index = index_result.value();
    debug("found graphics queue");
}

void InitVma(State *state)
{
    VmaVulkanFunctions vulkan_functions = {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };

    VmaAllocatorCreateInfo allocator_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = state->context.gpu,
        .device = state->context.device,
        .pVulkanFunctions = &vulkan_functions,
        .instance = state->context.instance,
    };

    validate(vmaCreateAllocator(&allocator_info, &state->context.allocator),
             "could not create vma allocator");

    debug("Created vma allocator");
}

void GetDepthFormat(State *state)
{
    VkFormat valid_formats[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    VkFormat current_format = VK_FORMAT_UNDEFINED;

    for (u32 i = 0; i < 1; i++)
    {
        VkFormatProperties2 properties = {
            .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
        };

        vkGetPhysicalDeviceFormatProperties2(
          state->context.gpu, valid_formats[i], &properties);

        if (properties.formatProperties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            current_format = valid_formats[i];
            state->context.depth_format = valid_formats[i];
            break;
        }
    }

    if (current_format == VK_FORMAT_UNDEFINED)
    {
        err("failed to find appropriate depth format");
    }

    debug("Retrieved valid depth format");
}

void InitFrameContext(State *state)
{
    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = state->context.graphics_queue_index,
    };

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        validate(vkCreateSemaphore(
                   state->context.device,
                   &sem_info,
                   NULL,
                   &state->context.frame_context[i].begin_rendering_semaphore),
                 "could not create presentation semapohre");

        validate(vkCreateFence(state->context.device,
                               &fence_info,
                               NULL,
                               &state->context.frame_context[i].fence),
                 "could not create frame fence");

        validate(
          vkCreateCommandPool(state->context.device,
                              &pool_info,
                              NULL,
                              &state->context.frame_context[i].command_pool),
          "could not create command pool");

        VkCommandBufferAllocateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = state->context.frame_context[i].command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        validate(vkAllocateCommandBuffers(
                   state->context.device,
                   &buffer_info,
                   &state->context.frame_context[i].command_buffer),
                 "could not allocate command buffer");
    }

    debug("created frame context");
}

void InitContext(State *state)
{
    validate(volkInitialize(), "could not initialize volk");
    vkb_handles handles = {};
    CreateWindow(state, &handles);
    CreateInstance(state, &handles);
    CreateSurface(state);
    SelectGPU(state, &handles);
    CreateDevice(state, &handles);
    GetGraphicsQueue(state, &handles);
    InitVma(state);
    GetDepthFormat(state);
    InitFrameContext(state);
    debug("Initialized context");
}

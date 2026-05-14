#include "headers.h"

void CreateLightBuffer(State *state)
{
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(LightData),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VmaAllocationInfo result = {};
        validate(vmaCreateBuffer(state->context.allocator,
                                 &buffer_info,
                                 &alloc_info,
                                 &state->light_data.buffers[i],
                                 &state->light_data.allocations[i],
                                 &result),
                 "could not create light buffers");
        state->light_data.ptrs[i] = (LightData *)result.pMappedData;

        VkBufferDeviceAddressInfo address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = state->light_data.buffers[i],
        };

        state->light_data.addresses[i] =
          vkGetBufferDeviceAddress(state->context.device, &address_info);

        // set light defaults
        LightData *data = state->light_data.ptrs[i];
        data->direction = HMM_NormV3({ 1.0f, -1.0f, -1.0f });
        data->color = { 1.0f, 1.0f, 1.0f };
        data->ambient_strength = 0.1f;
        data->specular_strength = 0.5;
        data->shininess = 32.0f;
    };

    debug("created light buffer");
}

void OrbitLight(State *state, int frame_index, float time)
{
    LightData *light = state->light_data.ptrs[frame_index];

    float speed = 0.25f;

    light->direction = HMM_Vec3({
      HMM_CosF(time * speed),
      -1.0f,
      HMM_SinF(time * speed),
    });
}
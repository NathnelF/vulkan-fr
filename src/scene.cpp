#include "headers.h"

void CreateSceneBuffers(State *state)
{
    // Create Instance buffers on GPU
    VkBufferCreateInfo instance_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Instance) * MAX_INSTANCES,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo instance_alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VmaAllocationInfo alloc_result = {};
        validate(vmaCreateBuffer(state->context.allocator,
                                 &instance_buffer_info,
                                 &instance_alloc_info,
                                 &state->scene.instance_buffers[i],
                                 &state->scene.instance_allocations[i],
                                 &alloc_result),
                 "could not create instance buffers");

        state->scene.instance_ptrs[i] = (Instance *)alloc_result.pMappedData;

        VkBufferDeviceAddressInfo address = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = state->scene.instance_buffers[i],
        };

        state->scene.instance_addresses[i] =
          vkGetBufferDeviceAddress(state->context.device, &address);
    }

    // Create draw indirect buffers on GPU
    VkBufferCreateInfo draw_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Instance) * MAX_INSTANCES,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo draw_alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VmaAllocationInfo alloc_result = {};
        validate(vmaCreateBuffer(state->context.allocator,
                                 &draw_buffer_info,
                                 &draw_alloc_info,
                                 &state->scene.draw_buffers[i],
                                 &state->scene.draw_allocations[i],
                                 &alloc_result),
                 "could not create instance buffers");

        state->scene.draw_ptrs[i] =
          (VkDrawIndirectCommand *)alloc_result.pMappedData;

        VkBufferDeviceAddressInfo address = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = state->scene.draw_buffers[i],
        };

        state->scene.draw_addresses[i] =
          vkGetBufferDeviceAddress(state->context.device, &address);
    }

    debug("Created scene buffers");
}
#include "headers.h"

void CreateSceneBuffers(State *state)
{
    // Create Instance buffers on GPU
    VkBufferCreateInfo instance_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(GpuData) * MAX_ENTITIES,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo instance_alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VmaAllocationInfo alloc_result = {};
        validate(vmaCreateBuffer(state->context.allocator,
                                 &instance_buffer_info,
                                 &instance_alloc_info,
                                 &state->scene.data_buffers[i],
                                 &state->scene.data_allocations[i],
                                 &alloc_result),
                 "could not create instance buffers");

        state->scene.data_ptrs[i] = (GpuData *)alloc_result.pMappedData;

        VkBufferDeviceAddressInfo address = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = state->scene.data_buffers[i],
        };

        state->scene.data_addresses[i] =
          vkGetBufferDeviceAddress(state->context.device, &address);
    }

    // Create draw indirect buffers on GPU
    VkBufferCreateInfo draw_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(VkDrawIndexedIndirectCommand) * MAX_DRAWS,
        .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,

    };

    VmaAllocationCreateInfo draw_alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
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
          (VkDrawIndexedIndirectCommand *)alloc_result.pMappedData;

        // VkBufferDeviceAddressInfo address = {
        //     .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        //     .buffer = state->scene.draw_buffers[i],
        // };

        // state->scene.draw_addresses[i] =
        //   vkGetBufferDeviceAddress(state->context.device, &address);
    }

    debug("Created scene buffers");
}

void BuildTransform(SceneData *data, u32 index)
{
    HMM_Mat4 translation = HMM_Translate(data->positions[index]);
    HMM_Mat4 rotation =
      HMM_Rotate_RH(HMM_AngleDeg(data->rotations[index]), { 0.0f, 1.0f, 0.0f });
    HMM_Mat4 scale = HMM_Scale(data->scales[index]);
    data->transforms[index] =
      HMM_MulM4(translation, HMM_MulM4(rotation, scale));
}

void AddMeshToScene(Scene *scene,
                    HMM_Vec3 position,
                    u32 mesh_index,
                    u32 texture_index)
{
    if (scene->entity_count + 1 > MAX_ENTITIES)
    {
        err("exceeded max entity count");
    }
    u32 count = scene->entity_count;
    scene->scene_data.positions[count] = position;
    scene->scene_data.scales[count] = { 1.0f, 1.0f, 1.0f };
    scene->scene_data.rotations[count] = 0.0f;
    scene->scene_data.mesh_indices[count] = mesh_index;
    scene->scene_data.texture_indices[count] = texture_index;
    BuildTransform(&scene->scene_data, count);
    scene->entity_count++;
}

void UpdateScene(State *state, int frame_index)
{
    // write gpu data to data buffer
    Scene *scene = &state->scene;
    GpuData *data = scene->data_ptrs[frame_index];
    for (u32 i = 0; i < scene->entity_count; i++)
    {
        data[i] = {
            .transform = scene->scene_data.transforms[i],
            .mesh_index = scene->scene_data.mesh_indices[i],
            .texture_index = scene->scene_data.texture_indices[i],
        };
    }

    scene->draw_count = 0;
    VkDrawIndexedIndirectCommand *draws = scene->draw_ptrs[frame_index];
    for (u32 i = 0; i < scene->entity_count; i++)
    {
        u32 mesh_index = scene->scene_data.mesh_indices[i];
        MeshRegion *mesh = &state->mesh_data.meshes[mesh_index];

        draws[scene->draw_count] = {
            .indexCount = mesh->index_count,
            .instanceCount = 1,
            .firstIndex = mesh->index_offset,
            .vertexOffset = (int)mesh->vertex_offset,
            .firstInstance = i,
        };

        scene->draw_count++;
    }

    // debug("draw_count: %u", state->scene.draw_count);
    // debug(
    //   "draw[0]: indexCount=%u firstIndex=%u vertexOffset=%d
    //   firstInstance=%u", state->scene.draw_ptrs[frame_index][0].indexCount,
    //   state->scene.draw_ptrs[frame_index][0].firstIndex,
    //   state->scene.draw_ptrs[frame_index][0].vertexOffset,
    //   state->scene.draw_ptrs[frame_index][0].firstInstance);

    // write draw commands to draw buffer
}

//
void CreateStaticScene(State *state)
{
    // a scene is a list of mesh indices and transforms that are converted into
    // draw calls

    // initially let's draw three cubes
    AddMeshToScene(&state->scene, { 0.0f, 0.0f, 0.0f }, 0, 0);
    AddMeshToScene(&state->scene, { -6.0f, 0.0f, 0.0f }, 0, 0);
    AddMeshToScene(&state->scene, { 6.0f, 0.0f, 0.0f }, 0, 0);
}

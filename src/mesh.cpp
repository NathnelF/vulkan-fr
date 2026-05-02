#include "headers.h"

void CreateMegaBuffer(State *state)
{
    // Create the buffer on the gpu
    VkBufferCreateInfo mega_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = MEGA_BUFFER_SIZE,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo mega_alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    validate(vmaCreateBuffer(state->context.allocator,
                             &mega_buffer_info,
                             &mega_alloc_info,
                             &state->mesh_data.buffer,
                             &state->mesh_data.allocation,
                             NULL),
             "could not create mega buffer");

    state->mesh_data.mesh_count = 0;
}

// TODO(Nate): This only works for our basic format with 3 floats as position.
// When we graduate to static and skinned vertex formats we will need to
// duplicate code.
struct RawMesh
{
    StaticVertex *vertices;
    u32 vertex_bytes;
    u32 *indices;
    u32 index_bytes;
};

u32 LoadMesh(State *state, RawMesh *output, const char *path)
{
    // load vertex / index data from cgltf
    cgltf_options options = {};
    cgltf_data *data = NULL;

    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success)
    {
        err("could not parse glb file %s", path);
    }

    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success)
    {
        cgltf_free(data);
        err("could not load glb buffers %s", path);
    }

    if (data->meshes_count == 0 || data->meshes[0].primitives_count == 0)
    {
        cgltf_free(data);
        err("no mesh data found %s", path);
    }

    cgltf_primitive *primitive = &data->meshes[0].primitives[0];

    cgltf_accessor *position_accessor = NULL;
    cgltf_accessor *normal_accessor = NULL;
    cgltf_accessor *uv_accessor = NULL;

    for (u32 i = 0; i < primitive->attributes_count; i++)
    {
        switch (primitive->attributes[i].type)
        {
            case cgltf_attribute_type_position:
            {
                position_accessor = primitive->attributes[i].data;
                break;
            }
            case cgltf_attribute_type_normal:
            {
                normal_accessor = primitive->attributes[i].data;
                break;
            }
            case cgltf_attribute_type_texcoord:
            {
                uv_accessor = primitive->attributes[i].data;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    if (position_accessor == NULL)
    {
        cgltf_free(data);
        err("could not find position accessor %s", path);
    }
    if (!normal_accessor)
    {
        cgltf_free(data);
        err("no normal data found in %s", path);
    }
    if (!uv_accessor)
    {
        cgltf_free(data);
        err("no uv data found in %s", path);
    }

    // get data in RawMesh struct
    u32 vertex_count = (u32)position_accessor->count;
    u32 vertex_size = vertex_count * sizeof(StaticVertex);

    if (output->vertex_bytes + vertex_size > MEGA_BUFFER_SIZE / 2)
    {
        cgltf_free(data);
        err("Veretx staging area full");
    }

    StaticVertex *vertex_dest =
      (StaticVertex *)((u8 *)output->vertices + output->vertex_bytes);

    // unpack positions
    for (u32 i = 0; i < vertex_count; i++)
    {
        cgltf_accessor_read_float(
          position_accessor, i, &vertex_dest[i].position.X, 3);
    }
    // unpack normals
    for (u32 i = 0; i < vertex_count; i++)
    {
        cgltf_accessor_read_float(
          normal_accessor, i, &vertex_dest[i].normal.X, 3);
    }

    // unpack uvs
    for (u32 i = 0; i < vertex_count; i++)
    {
        cgltf_accessor_read_float(uv_accessor, i, &vertex_dest[i].uv.X, 2);
    }

    // indices
    cgltf_accessor *index_accessor = primitive->indices;
    if (index_accessor == NULL)
    {
        cgltf_free(data);
        err("no positon data found in %s", path);
    }

    u32 index_count = (u32)index_accessor->count;
    u32 index_size = index_count * sizeof(u32);

    if (output->index_bytes + index_size > MEGA_BUFFER_SIZE / 2)
    {
        cgltf_free(data);
        err("Index staging area full");
    }

    u32 *index_dest = (u32 *)((u8 *)output->indices + output->index_bytes);

    cgltf_accessor_unpack_indices(
      index_accessor, index_dest, sizeof(u32), index_count);

    u32 mesh_index = state->mesh_data.mesh_count;
    state->mesh_data.meshes[mesh_index] = {
        .vertex_offset = output->vertex_bytes / (u32)(sizeof(StaticVertex)),
        .vertex_count = vertex_count,
        .index_offset = output->index_bytes / (u32)sizeof(u32),
        .index_count = index_count,
    };

    state->mesh_data.mesh_count++;

    output->vertex_bytes += vertex_size;
    output->index_bytes += index_size;
    cgltf_free(data);

    debug("loaded in mesh %s, %u vertices and %u indices with mesh slot %u",
          path,
          vertex_count,
          index_count,
          mesh_index);

    return mesh_index;
}

void UploadMeshToGPU(State *state, RawMesh *input)
{
    // Create staging buffer on GPU
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = input->index_bytes + input->vertex_bytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };
    VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_ONLY,
    };
    VkBuffer staging_buffer;
    VmaAllocation staging_alloc;
    VmaAllocationInfo staging_result;
    validate(vmaCreateBuffer(state->context.allocator,
                             &buffer_info,
                             &alloc_info,
                             &staging_buffer,
                             &staging_alloc,
                             &staging_result),
             "could not create staging buffer");

    // memcpy from cpu to staging buffer
    u8 *mapped = (u8 *)staging_result.pMappedData;
    memcpy(mapped, input->vertices, input->vertex_bytes);
    memcpy(mapped + input->vertex_bytes, input->indices, input->index_bytes);

    FrameContext *frame = &state->context.frame_context[0];
    // Transfer from staging buffer to GPU only memory using command buffer
    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = frame->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd;
    validate(vkAllocateCommandBuffers(state->context.device, &cmd_alloc, &cmd),
             "could not allocate command buffer");

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(cmd, &begin_info);

    VkBufferCopy vertex_copy = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = input->vertex_bytes,
    };

    vkCmdCopyBuffer(
      cmd, staging_buffer, state->mesh_data.buffer, 1, &vertex_copy);

    VkBufferCopy index_copy = {
        .srcOffset = input->vertex_bytes,
        .dstOffset = MEGA_BUFFER_SIZE / 2,
        .size = input->index_bytes,
    };

    vkCmdCopyBuffer(
      cmd, staging_buffer, state->mesh_data.buffer, 1, &index_copy);

    vkEndCommandBuffer(cmd);

    // submit

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    VkFence fence;
    validate(vkCreateFence(state->context.device, &fence_info, NULL, &fence),
             "could not create fence");

    validate(vkQueueSubmit(state->context.queue, 1, &submit_info, fence),
             "could not submit to queue");

    vkWaitForFences(state->context.device, 1, &fence, VK_TRUE, UINT64_MAX);

    // destroy staging buffer / temp fence
    vkDestroyFence(state->context.device, fence, NULL);
    vkFreeCommandBuffers(state->context.device, frame->command_pool, 1, &cmd);
    vmaDestroyBuffer(state->context.allocator, staging_buffer, staging_alloc);

    debug("uploaded mesh data to gpu!");
}

void LoadMeshes(State *state)
{
    CreateMegaBuffer(state);

    RawMesh cpu_staging_buffer = {};
    cpu_staging_buffer.vertices = (StaticVertex *)malloc(MEGA_BUFFER_SIZE / 2);
    cpu_staging_buffer.indices = (u32 *)malloc(MEGA_BUFFER_SIZE / 2);

    LoadMesh(state, &cpu_staging_buffer, "assets/Cube.glb");

    UploadMeshToGPU(state, &cpu_staging_buffer);
}
#include "headers.h"

#include "stb_image.h"

void CreateTexturePool(State *state)
{
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
    };

    validate(vkCreateSampler(state->context.device,
                             &sampler_info,
                             NULL,
                             &state->texture_data.sampler),
             "could not create sampler");

    // descriptor set layout
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_TEXTURES,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorBindingFlags binding_flags =
      VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {
        .sType =
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &binding_flags
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flags_info,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    validate(
      vkCreateDescriptorSetLayout(
        state->context.device, &layout_info, NULL, &state->texture_data.layout),
      "could not create descriptor layout");

    // descriptor pool
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = MAX_TEXTURES,
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    validate(
      vkCreateDescriptorPool(
        state->context.device, &pool_info, NULL, &state->texture_data.pool),
      "could not create descriptor pool");

    // descriptor set
    u32 max_textures = MAX_TEXTURES;

    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count = {
        .sType =
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &max_textures,
    };

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &variable_count,
        .descriptorPool = state->texture_data.pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &state->texture_data.layout,
    };

    validate(vkAllocateDescriptorSets(
               state->context.device, &alloc_info, &state->texture_data.set),
             "could not allocate descriptor set");

    debug("created texture pool");
}

u32 LoadTexture(State *state, const char *path)
{
    // load from disk
    int width, height, channels;
    stbi_uc *pixels =
      stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        err("failed to load texture found at %s", path);
    }

    u32 image_size = width * height * 4;

    // staging buffer
    VkBufferCreateInfo staging_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = image_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo staging_alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkBuffer staging_buffer;
    VmaAllocation staging_alloc;
    VmaAllocationInfo staging_result = {};

    validate(vmaCreateBuffer(state->context.allocator,
                             &staging_info,
                             &staging_alloc_info,
                             &staging_buffer,
                             &staging_alloc,
                             &staging_result),
             "could not create staging buffer");

    // copy pixels to staging buffer
    memcpy(staging_result.pMappedData, pixels, image_size);
    // free pixels from cpu ram
    stbi_image_free(pixels);

    // time to create the image on the gpu
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {
            .width = (u32)width,
            .height = (u32)height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo image_alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    Texture texture = {};

    validate(vmaCreateImage(state->context.allocator,
                            &image_info,
                            &image_alloc_info,
                            &texture.image,
                            &texture.allocation,
                            NULL),
             "could not create image for texture %s",
             path);

    // transfer time
    VkCommandBufferAllocateInfo buffer_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = state->context.frame_context[0].command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer buffer;
    validate(
      vkAllocateCommandBuffers(state->context.device, &buffer_alloc, &buffer),
      "could not allocate transfer buffer");

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(buffer, &begin_info);

    // barrier!
    VkImageMemoryBarrier2 to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = texture.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &to_transfer,
    };

    vkCmdPipelineBarrier2(buffer, &dep_info);

    // copy buffer to image
    VkBufferImageCopy2 copy_region = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .imageExtent = {
            .width =  (u32)width,
            .height = (u32)height,
            .depth = 1
        },
    };

    VkCopyBufferToImageInfo2 copy_info = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
        .srcBuffer = staging_buffer,
        .dstImage = texture.image,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &copy_region,
    };

    vkCmdCopyBufferToImage2(buffer, &copy_info);

    // barrier again
    VkImageMemoryBarrier2 to_shader_read = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .image = texture.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    dep_info.pImageMemoryBarriers = &to_shader_read;
    vkCmdPipelineBarrier2(buffer, &dep_info);

    vkEndCommandBuffer(buffer);

    // submit

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VkFence fence;
    validate(vkCreateFence(state->context.device, &fence_info, NULL, &fence),
             "could not create upload fence");

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &buffer,
    };

    validate(vkQueueSubmit(state->context.queue, 1, &submit_info, fence),
             "could not submit texture upload");

    // cleanup after fence

    vkWaitForFences(state->context.device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(state->context.device, fence, NULL);
    vkFreeCommandBuffers(state->context.device,
                         state->context.frame_context[0].command_pool,
                         1,
                         &buffer);
    vmaDestroyBuffer(state->context.allocator, staging_buffer, staging_alloc);

    // create image view

    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    validate(vkCreateImageView(
               state->context.device, &image_view_info, NULL, &texture.view),
             "could not create texture image view for %s",
             path);

    // register into descriptor array
    texture.index = state->texture_data.count++;

    VkDescriptorImageInfo image_descriptor = {
        .sampler = state->texture_data.sampler,
        .imageView = texture.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = state->texture_data.set,
        .dstBinding = 0,
        .dstArrayElement = texture.index,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_descriptor,
    };

    vkUpdateDescriptorSets(state->context.device, 1, &write, 0, NULL);
    debug("loaded texture %s into descriptor slot %u", path, texture.index);
    return texture.index;
}

void LoadTextures(State *state) {}
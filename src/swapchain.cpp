#include "headers.h"
//
#include "VkBootstrap.h"
//
#include "swapchain.h"

// Add this function to swapchain.cpp
static void TransitionDepthImage(State *state)
{
    // grab any frame's command pool for a one-time submit
    VkCommandPool pool = state->context.frame_context[0].command_pool;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd;
    validate(vkAllocateCommandBuffers(state->context.device, &alloc_info, &cmd),
             "could not allocate transition command buffer");

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    validate(vkBeginCommandBuffer(cmd, &begin_info),
             "could not begin transition command buffer");

    VkImageMemoryBarrier2 barrier = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask     = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask    = 0,
        .dstStageMask     = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask    = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .image            = state->swapchain.depth_image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			.levelCount = 1,
            .layerCount = 1,
        },
    };

    VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dep_info);

    validate(vkEndCommandBuffer(cmd),
             "could not end transition command buffer");

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    validate(
      vkQueueSubmit(state->context.queue, 1, &submit_info, VK_NULL_HANDLE),
      "could not submit transition command buffer");

    // wait for it to finish before returning
    vkQueueWaitIdle(state->context.queue);

    vkFreeCommandBuffers(state->context.device, pool, 1, &cmd);
    debug("transitioned depth image layout");
}

void CreateSwapchain(State *state, VkSwapchainKHR old_handle)
{
    int width, height;
    SDL_GetWindowSize(state->context.window, &width, &height);
    state->swapchain.width = (u32)width;
    state->swapchain.height = (u32)height;

    vkb::SwapchainBuilder builder(
      state->context.gpu, state->context.device, state->context.surface);

    auto result =
      builder
        .set_desired_extent(state->swapchain.width, state->swapchain.height)
        .set_old_swapchain(old_handle)
        .build();

    if (!result)
    {
        err("couldn't build swapchain");
    }

    vkb::Swapchain vkb_swapchain = result.value();

    state->swapchain.handle = vkb_swapchain.swapchain;
    state->swapchain.image_count = vkb_swapchain.image_count;
    state->swapchain.image_format = vkb_swapchain.image_format;

    auto images = vkb_swapchain.get_images().value();
    for (u32 i = 0; i < state->swapchain.image_count; i++)
    {
        state->swapchain.images[i] = images[i];
    }

    auto views = vkb_swapchain.get_image_views().value();
    for (u32 i = 0; i < state->swapchain.image_count; i++)
    {
        state->swapchain.views[i] = views[i];
    }

    for (u32 i = 0; i < state->swapchain.image_count; i++)
    {
        VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        validate(
          vkCreateSemaphore(state->context.device,
                            &semaphore_info,
                            NULL,
                            &state->swapchain.begin_presenting_semaphore[i]),
          "could not create semaphore");
    }

    debug("created swapchain");

    // depth stuff
    VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkImageCreateInfo image_info = {
    	.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    	.imageType = VK_IMAGE_TYPE_2D,
    	.format = state->context.depth_format,
    	.extent = {
    		.width = state->swapchain.width,
    		.height = state->swapchain.height,
    		.depth = 1,
    	},
    	.mipLevels = 1,
    	.arrayLayers = 1,
    	.samples = VK_SAMPLE_COUNT_1_BIT,
    	.tiling = VK_IMAGE_TILING_OPTIMAL,
    	.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    	.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    validate(vmaCreateImage(state->context.allocator,
                            &image_info,
                            &alloc_info,
                            &state->swapchain.depth_image,
                            &state->swapchain.depth_alloc,
                            NULL),
             "could not create depth image");

    VkImageViewCreateInfo view_info = {
    	.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    	.image = state->swapchain.depth_image,
    	.viewType = VK_IMAGE_VIEW_TYPE_2D,
    	.format = state->context.depth_format,
    	.subresourceRange = {
    		.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    		.levelCount = 1,
    		.layerCount = 1,
    	},
    };

    validate(
      vkCreateImageView(
        state->context.device, &view_info, NULL, &state->swapchain.depth_view),
      "could not create image view");

    TransitionDepthImage(state);
    debug("created depth image and view");
}

void RecreateSwapchain(State *state)
{
    // wait for all frames to finish
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        vkWaitForFences(state->context.device,
                        1,
                        &state->context.frame_context[i].fence,
                        VK_TRUE,
                        UINT64_MAX);
    }

    Swapchain old = state->swapchain;

    CreateSwapchain(state, old.handle);

    vkDestroyImageView(state->context.device, old.depth_view, NULL);
    vmaDestroyImage(state->context.allocator, old.depth_image, old.depth_alloc);

    for (u32 i = 0; i < old.image_count; i++)
    {
        vkDestroySemaphore(
          state->context.device, old.begin_presenting_semaphore[i], NULL);
        vkDestroyImageView(state->context.device, old.views[i], NULL);
    }

    vkDestroySwapchainKHR(state->context.device, old.handle, NULL);

    debug("recreated swapchain");
}

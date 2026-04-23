// render.cpp
#include "headers.h"
#include "swapchain.h"

// Triangle in NDC — matches your shader which passes pos straight to
// gl_Position Bottom left, bottom right, top center
static float triangle_vertices[] = {
    -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f,
};

struct RenderContext
{
    VkBuffer vertex_buffer;
    VmaAllocation vertex_alloc;
};

void InitRender(State *state, RenderContext *rc)
{
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(triangle_vertices),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };

    VmaAllocationCreateInfo alloc_info = {
        // HOST_VISIBLE + DEVICE_LOCAL where available (ReBAR),
        // falls back to host visible on systems without it.
        // Fine for a static triangle — upgrade to staging buffer later
        // for real mesh data.
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocationInfo alloc_result = {};
    validate(vmaCreateBuffer(state->context.allocator,
                             &buffer_info,
                             &alloc_info,
                             &rc->vertex_buffer,
                             &rc->vertex_alloc,
                             &alloc_result),
             "could not create vertex buffer");

    // Copy triangle data — pMappedData is valid because of MAPPED_BIT above
    memcpy(
      alloc_result.pMappedData, triangle_vertices, sizeof(triangle_vertices));

    // Flush so GPU sees the write (only needed if memory is not HOST_COHERENT,
    // but safe to always call)
    vmaFlushAllocation(
      state->context.allocator, rc->vertex_alloc, 0, VK_WHOLE_SIZE);

    debug("created triangle vertex buffer");
}

void Render(State *state, RenderContext *rc, int frame_index)
{
    FrameContext *frame = &state->context.frame_context[frame_index];

    // ── Wait for this frame slot to be free ──────────────────────
    validate(vkWaitForFences(
               state->context.device, 1, &frame->fence, VK_TRUE, UINT64_MAX),
             "wait for fence failed");

    // ── Acquire swapchain image ───────────────────────────────────
    u32 image_index = 0;
    VkResult acquire = vkAcquireNextImageKHR(
      state->context.device,
      state->swapchain.handle,
      UINT64_MAX,
      frame->begin_rendering_semaphore, // signaled when image is ready
      VK_NULL_HANDLE,
      &image_index);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain(state);
        return;
    }

    validate(vkResetFences(state->context.device, 1, &frame->fence),
             "reset fence failed");

    // ── Record ───────────────────────────────────────────────────
    validate(vkResetCommandPool(state->context.device, frame->command_pool, 0),
             "reset command pool failed");

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    validate(vkBeginCommandBuffer(frame->command_buffer, &begin_info),
             "begin command buffer failed");

    // ── Transition swapchain image: UNDEFINED → COLOR_ATTACHMENT ─
    VkImageMemoryBarrier2 to_attachment = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask     = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask    = 0,
        .dstStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image            = state->swapchain.images[image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &to_attachment,
    };
    vkCmdPipelineBarrier2(frame->command_buffer, &dep_info);

    // ── Begin dynamic rendering ───────────────────────────────────
    VkRenderingAttachmentInfo color_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = state->swapchain.views[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = { .float32 = { 0.1f, 0.1f, 0.1f, 1.0f } } },
    };

    VkRenderingAttachmentInfo depth_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = state->swapchain.depth_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = { .depthStencil = { .depth =
                                            1.0f, .stencil = 0, } }, // 0.0 for reverse-Z
    };

    VkRenderingInfo rendering_info = {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {
            .offset = { 0, 0 },
            .extent = { state->swapchain.width, state->swapchain.height },
        },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment,
        .pDepthAttachment     = &depth_attachment,
        .pStencilAttachment = &depth_attachment
    };

    vkCmdBeginRendering(frame->command_buffer, &rendering_info);

    // ── Draw ─────────────────────────────────────────────────────
    Pipeline *pipeline = &state->pipelines[PIPELINE_BASIC];
    vkCmdBindPipeline(
      frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);

    // Viewport and scissor are dynamic — must set every frame
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)state->swapchain.width,
        .height = (float)state->swapchain.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(frame->command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = { state->swapchain.width, state->swapchain.height },
    };
    vkCmdSetScissor(frame->command_buffer, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(
      frame->command_buffer, 0, 1, &rc->vertex_buffer, &offset);

    vkCmdDraw(frame->command_buffer, 3, 1, 0, 0); // 3 verts, 1 instance

    vkCmdEndRendering(frame->command_buffer);

    // ── Transition swapchain image: COLOR_ATTACHMENT → PRESENT ───
    VkImageMemoryBarrier2 to_present = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask     = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask    = 0,
        .oldLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image            = state->swapchain.images[image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    dep_info.pImageMemoryBarriers = &to_present;
    vkCmdPipelineBarrier2(frame->command_buffer, &dep_info);

    validate(vkEndCommandBuffer(frame->command_buffer),
             "end command buffer failed");

    // ── Submit ───────────────────────────────────────────────────
    VkSemaphore wait_semaphore = frame->begin_rendering_semaphore;
    VkSemaphore signal_semaphore =
      state->swapchain.begin_presenting_semaphore[image_index];

    VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &wait_semaphore,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame->command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &signal_semaphore,
    };

    validate(vkQueueSubmit(state->context.queue, 1, &submit_info, frame->fence),
             "queue submit failed");

    // ── Present ──────────────────────────────────────────────────
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &signal_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &state->swapchain.handle,
        .pImageIndices = &image_index,
    };

    VkResult present = vkQueuePresentKHR(state->context.queue, &present_info);
    if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR)
    {
        RecreateSwapchain(state);
    }
}
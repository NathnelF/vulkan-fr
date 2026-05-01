// render.cpp
#include "headers.h"
#include "swapchain.h"

void Render(State *state, int frame_index)
{
    FrameContext *frame = &state->context.frame_context[frame_index];

    // validate(vkWaitForFences(
    //            state->context.device, 1, &frame->fence, VK_TRUE, UINT64_MAX),
    //          "wait for fence failed");

    // validate(vkResetFences(state->context.device, 1, &frame->fence),
    //          "reset fence failed");

    u32 image_index = 0;
    VkResult acquire = vkAcquireNextImageKHR(state->context.device,
                                             state->swapchain.handle,
                                             UINT64_MAX,
                                             frame->begin_rendering_semaphore,
                                             VK_NULL_HANDLE,
                                             &image_index);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain(state);
        return;
    }

    validate(vkResetCommandPool(state->context.device, frame->command_pool, 0),
             "reset command pool failed");

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    validate(vkBeginCommandBuffer(frame->command_buffer, &begin_info),
             "begin command buffer failed");

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

    VkRenderingAttachmentInfo color_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = state->swapchain.views[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { 
            .color = { 
                .float32 = { 0.1f, 0.1f, 0.1f, 1.0f, } 
            } 
        },
    };

    VkRenderingAttachmentInfo depth_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = state->swapchain.depth_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = { 
            .depthStencil = { 
                .depth = 1.0f, 
                .stencil = 0, 
                } 
            },
    };

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = { 
                state->swapchain.width, 
                state->swapchain.height, 
            },
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
        .pDepthAttachment = &depth_attachment,
        .pStencilAttachment = &depth_attachment
    };

    vkCmdBeginRendering(frame->command_buffer, &rendering_info);

    Pipeline *pipeline = &state->pipelines[PIPELINE_BASIC];

    vkCmdBindPipeline(
      frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);

    PushConstants push = {
        .camera_address = state->camera.camera_buffer_address[frame_index],
        .scene_address = state->scene.data_addresses[frame_index],
    };

    vkCmdPushConstants(frame->command_buffer,
                       pipeline->layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(PushConstants),
                       &push);

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
        .extent = { 
            state->swapchain.width, 
            state->swapchain.height,
        },
    };
    vkCmdSetScissor(frame->command_buffer, 0, 1, &scissor);

    VkDeviceSize vertex_offset = 0;
    vkCmdBindVertexBuffers(
      frame->command_buffer, 0, 1, &state->mesh_data.buffer, &vertex_offset);

    VkDeviceSize index_offset = MEGA_BUFFER_SIZE / 2;
    vkCmdBindIndexBuffer(frame->command_buffer,
                         state->mesh_data.buffer,
                         index_offset,
                         VK_INDEX_TYPE_UINT32);

    // MeshRegion *mesh = &state->mesh_data.meshes[0];
    // vkCmdDrawIndexed(frame->command_buffer,
    //                  mesh->index_count,
    //                  1,
    //                  mesh->index_offset,
    //                  mesh->vertex_offset,
    //                  0);

    vkCmdDrawIndexedIndirect(frame->command_buffer,
                             state->scene.draw_buffers[frame_index],
                             0,
                             state->scene.draw_count,
                             sizeof(VkDrawIndexedIndirectCommand));

    vkCmdEndRendering(frame->command_buffer);

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
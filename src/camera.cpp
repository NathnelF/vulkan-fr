#include "headers.h"

void CreateCameraBuffer(State *state)
{
    // create mapped
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(CameraConstants),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VmaAllocationInfo result = {};
        validate(vmaCreateBuffer(state->context.allocator,
                                 &buffer_info,
                                 &alloc_info,
                                 &state->camera.buffers[i],
                                 &state->camera.allocs[i],
                                 &result),
                 "could not create camera buffer");

        state->camera.ptrs[i] = result.pMappedData;
    }

    state->camera.target = { 0.0f, 0.0f, 0.0f };
    state->camera.yaw = 45.0f;
    state->camera.pitch = 45.0f;
    state->camera.distance = 60.0f;
    state->camera.pan_speed = 50.0f;
    state->camera.zoom_speed = 20.0f;
    state->camera.rotate_speed = 180.0f;

    debug("created camera uniform buffers");
}

void CreateCameraDescriptors(State *state)
{
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    validate(vkCreateDescriptorSetLayout(state->context.device,
                                         &layout_info,
                                         NULL,
                                         &state->camera.descriptor_layout),
             "could not create camera descriptor set layout");

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = FRAMES_IN_FLIGHT,
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    validate(vkCreateDescriptorPool(state->context.device,
                                    &pool_info,
                                    NULL,
                                    &state->camera.descriptor_pool),
             "could not create camera descriptor pool");

    VkDescriptorSetLayout layouts[FRAMES_IN_FLIGHT];
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        layouts[i] = state->camera.descriptor_layout;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = state->camera.descriptor_pool,
        .descriptorSetCount = FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts,
    };

    validate(vkAllocateDescriptorSets(state->context.device,
                                      &alloc_info,
                                      state->camera.descriptor_sets),
             "could not allocate camera descriptor sets");

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = state->camera.buffers[i],
            .offset = 0,
            .range = sizeof(CameraConstants),
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = state->camera.descriptor_sets[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets(state->context.device, 1, &write, 0, NULL);
    }

    debug("created camera descriptors");
}

static HMM_Vec3 CameraGetForward(Camera *cam)
{
    float yaw_rad = HMM_AngleDeg(cam->yaw);
    HMM_Vec3 forward =
      HMM_NormV3({ HMM_SinF(yaw_rad), 0.0f, HMM_CosF(yaw_rad) });
    return forward;
}

static HMM_Vec3 CameraGetRight(Camera *cam, HMM_Vec3 forward)
{
    HMM_Vec3 up = { 0.0f, 1.0f, 0.0f };
    HMM_Vec3 right = HMM_NormV3(HMM_Cross(forward, up));
    return right;
}

void UpdateCamera(State *state, float dt, int frame_index)
{
    // TODO(Nate): implement
    Camera *camera = &state->camera;
    const bool *keys = SDL_GetKeyboardState(NULL);

    HMM_Vec3 forward = CameraGetForward(camera);
    HMM_Vec3 right = CameraGetRight(camera, forward);

    float pan = camera->pan_speed * dt;

    // move with keys
    if (keys[SDL_SCANCODE_W])
    {
        debug("forward");
        camera->target = HMM_AddV3(camera->target, HMM_MulV3F(forward, pan));
    }
    if (keys[SDL_SCANCODE_S])
    {
        debug("back");
        camera->target = HMM_AddV3(camera->target, HMM_MulV3F(forward, -pan));
    }

    if (keys[SDL_SCANCODE_D])
    {
        debug("right");
        camera->target = HMM_AddV3(camera->target, HMM_MulV3F(right, pan));
    }
    if (keys[SDL_SCANCODE_A])
    {
        debug("left");
        camera->target = HMM_AddV3(camera->target, HMM_MulV3F(right, -pan));
    }

    // Zoom

    float zoom = dt * camera->zoom_speed;

    if (keys[SDL_SCANCODE_R])
    {
        camera->distance += pan;
    }
    if (keys[SDL_SCANCODE_F])
    {
        camera->distance -= pan;
    }
    camera->distance = HMM_Clamp(10.0f, camera->distance, 110.0f);

    // Rotate
    // --- Orbit (arrow keys or RF) ---
    float rot = camera->rotate_speed * dt;

    if (keys[SDL_SCANCODE_Q])
        camera->yaw += rot;
    if (keys[SDL_SCANCODE_E])
        camera->yaw -= rot;

    // Clamp pitch so camera doesn't flip — your raylib version had fixed 45deg,
    // expose it here so you can add controls later
    camera->pitch = HMM_Clamp(5.0f, camera->pitch, 89.0f);

    // --- Rebuild position from spherical coords, same math as your raylib ---
    float yaw_rad = HMM_AngleDeg(camera->yaw);
    float pitch_rad = HMM_AngleDeg(camera->pitch);

    // Start with offset along -Z, then rotate by pitch then yaw (your exact
    // logic)
    HMM_Vec3 offset = { 0.0f, 0.0f, -camera->distance };

    // Rotate by pitch around X axis
    float cp = HMM_CosF(pitch_rad), sp = HMM_SinF(pitch_rad);
    offset = {
        offset.X,
        offset.Y * cp - offset.Z * sp,
        offset.Y * sp + offset.Z * cp,
    };

    // Rotate by yaw around Y axis
    float cy = HMM_CosF(yaw_rad), sy = HMM_SinF(yaw_rad);
    offset = {
        offset.X * cy + offset.Z * sy,
        offset.Y,
        -offset.X * sy + offset.Z * cy,
    };

    HMM_Vec3 position = HMM_AddV3(camera->target, offset);

    // --- Build MVP and upload ---
    HMM_Vec3 up = { 0.0f, 1.0f, 0.0f };
    HMM_Mat4 view = HMM_LookAt_RH(position, camera->target, up);
    HMM_Mat4 proj = HMM_Perspective_RH_ZO(
      HMM_AngleDeg(35.0f), // matching your raylib fovy
      (float)state->swapchain.width / (float)state->swapchain.height,
      0.1f,
      500.0f);
    proj.Elements[1][1] *= -1.0f;

    HMM_Mat4 model = HMM_M4D(1.0f);
    CameraConstants c = { HMM_MulM4(proj, HMM_MulM4(view, model)) };
    memcpy(state->camera.ptrs[frame_index], &c, sizeof(CameraConstants));
}
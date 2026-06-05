#include "headers.h"

#include "camera.cpp"
#include "context.cpp"
#include "light.cpp"
#include "mesh.cpp"
#include "pipeline.cpp"
#include "render.cpp"
#include "scene.cpp"
#include "shadow.cpp"
#include "swapchain.cpp"
#include "texture.cpp"

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0); // disable buffering entirely
    setvbuf(stderr, NULL, _IONBF,
            0); // stderr too since validate() uses it
    debug("Hello, World");
    State state = {};
    InitContext(&state);
    CreateSwapchain(&state, VK_NULL_HANDLE);

    VmaTotalStatistics stats;
    LoadMeshes(&state);
    vmaCalculateStatistics(state.context.allocator, &stats);
    debug("Mesh VRAM used: %llu MB",
          stats.total.statistics.allocationBytes / (1024 * 1024));
    CreateTexturePool(&state);
    // TODO(Nate): move texture loads to scene creation
    LoadTexture(&state, "assets/bricks_albedo.png"); // 0
    LoadTexture(&state, "assets/bricks_ao.png");     // 1
    LoadTexture(&state, "assets/bricks_normal.png"); // 2
    // default flat textures for surfaces without materials
    LoadSolidTexture(&state, 255, 0, 0, 255);     // 3: white albedo
    LoadSolidTexture(&state, 255, 255, 255, 255); // 4: white AO
    LoadSolidTexture(
      &state, 128, 128, 255, 255, VK_FORMAT_R8G8B8A8_UNORM); // 5: flat normal

    CreateCameraBuffer(&state);
    CreateLightBuffer(&state);
    CreateShadowMap(&state);
    CreateSceneBuffers(&state);
    CreateStaticScene(&state);

    LoadAllPipelines(&state);

    int frame_index = 0;
    int running = 1;
    float total_time = 0.0f;

    u64 freq = SDL_GetPerformanceFrequency();
    u64 last = SDL_GetPerformanceCounter();

    SDL_Event event;
    while (running)
    {
        u64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - last) / (float)freq;
        last = now;
        total_time += dt;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = 0;
                debug("Quitting");
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                RecreateSwapchain(&state);
            }
        }

        const bool *keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_LCTRL] && keys[SDL_SCANCODE_Q])
        {
            debug("quitting");
            running = 0;
        }

        FrameContext *frame = &state.context.frame_context[frame_index];

        validate(vkWaitForFences(
                   state.context.device, 1, &frame->fence, VK_TRUE, UINT64_MAX),
                 "wait for fence failed");

        validate(vkResetFences(state.context.device, 1, &frame->fence),
                 "reset fence failed");

        // OrbitLight(&state, frame_index, total_time);
        UpdateLightMatrix(&state, frame_index);
        UpdateCamera(&state, dt, frame_index);
        UpdateScene(&state, frame_index);
        Render(&state, frame_index);
        frame_index = (frame_index + 1) % FRAMES_IN_FLIGHT;
    }
    return 0;
}

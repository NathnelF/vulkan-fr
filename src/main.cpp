#include "headers.h"

#include "context.cpp"
#include "pipeline.cpp"
#include "render.cpp"
#include "swapchain.cpp"

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0); // disable buffering entirely
    setvbuf(stderr, NULL, _IONBF,
            0); // stderr too since validate() uses it
    debug("Hello, World");
    State state = {};
    InitContext(&state);
    CreateSwapchain(&state, VK_NULL_HANDLE);

    // Load shaders
    VkShaderModule basic_vert = LoadShaderModule(&state, "src/vert.spv");
    VkShaderModule basic_frag = LoadShaderModule(&state, "src/frag.spv");
    // Load pipelines
    PipelineDesc basic_pipeline_desc = DefaultPipelineDesc(&state);
    basic_pipeline_desc.vert = basic_vert;
    basic_pipeline_desc.frag = basic_frag;
    VkPipelineLayoutCreateInfo basic_pipeline_layout_desc = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = NULL,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    VkPipelineLayout basic_pipeline_layout;
    validate(vkCreatePipelineLayout(state.context.device,
                                    &basic_pipeline_layout_desc,
                                    NULL,
                                    &basic_pipeline_layout),
             "could not create pipeline layout");

    basic_pipeline_desc.layout = basic_pipeline_layout;

    state.pipelines[PIPELINE_BASIC] =
      BuildPipeline(&state, &basic_pipeline_desc);

    RenderContext render_context = {};
    InitRender(&state, &render_context);

    int frame_index = 0;
    int running = 1;
    SDL_Event event;
    while (running)
    {
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
        Render(&state, &render_context, frame_index);
        frame_index = (frame_index + 1) % FRAMES_IN_FLIGHT;
    }
    return 0;
}
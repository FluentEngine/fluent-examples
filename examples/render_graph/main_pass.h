#pragma once

struct RenderGraph;
struct Swapchain;
struct Camera;

void
register_main_pass( struct RenderGraph*     graph,
                    const struct Swapchain* swapchain,
                    const char*             backbuffer_source_name,
                    const struct Camera*    camera,
                    struct nk_context*      ui );

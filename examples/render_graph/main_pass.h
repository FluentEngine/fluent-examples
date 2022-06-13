#pragma once

#include <stdint.h>

struct RenderGraph;
struct Swapchain;

void
register_main_pass( struct RenderGraph*     graph,
                    const struct Swapchain* swapchain,
                    const char*             backbuffer_source_name );

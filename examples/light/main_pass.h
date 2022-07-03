#pragma once

struct ft_render_graph;
struct ft_swapchain;
struct ft_camera;

void
register_main_pass( struct ft_render_graph*    graph,
                    const struct ft_swapchain* swapchain,
                    const char*                backbuffer_source_name,
                    const struct ft_camera*    camera );

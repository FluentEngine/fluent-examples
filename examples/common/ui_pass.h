#pragma once

struct ft_render_graph;
struct ft_swapchain;

void
register_ui_pass( struct ft_render_graph*    graph,
                  const struct ft_swapchain* swapchain,
                  const char*                backbuffer_source_name,
                  struct nk_context*         ui );

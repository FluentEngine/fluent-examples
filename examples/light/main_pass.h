#pragma once

struct ft_render_graph;
struct ft_swapchain;
struct ft_camera;
struct ft_image;

struct pbr_maps
{
	struct ft_image* environment;
	struct ft_image* brdf_lut;
	struct ft_image* irradiance;
	struct ft_image* specular;
};

void
register_main_pass( struct ft_render_graph*    graph,
                    const struct ft_swapchain* swapchain,
                    const char*                backbuffer_source_name,
                    const struct ft_camera*    camera,
                    struct pbr_maps*           maps );

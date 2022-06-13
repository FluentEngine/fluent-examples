#include <fluent/renderer.h>
#include "main.vert.h"
#include "main.frag.h"
#include "main_pass.h"

struct MainPassData
{
	u32                         width;
	u32                         height;
	enum Format                 swapchain_format;
	struct DescriptorSetLayout* dsl;
	struct Pipeline*            pipeline;
} main_pass_data;

static void
main_pass_create( const struct Device* device, void* user_data )
{
	struct MainPassData* data = user_data;

	struct ShaderInfo shader_info = {
	    .vertex   = get_main_vert_shader( device->api ),
	    .fragment = get_main_frag_shader( device->api ),
	};

	struct Shader* shader;
	create_shader( device, &shader_info, &shader );

	create_descriptor_set_layout( device, shader, &data->dsl );

	struct PipelineInfo info = {
	    .shader                = shader,
	    .descriptor_set_layout = data->dsl,
	    .topology              = FT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	    .rasterizer_info =
	        {
	            .cull_mode    = FT_CULL_MODE_BACK,
	            .front_face   = FT_FRONT_FACE_COUNTER_CLOCKWISE,
	            .polygon_mode = FT_POLYGON_MODE_FILL,
	        },
	    .depth_state_info =
	        {
	            .compare_op  = FT_COMPARE_OP_LESS,
	            .depth_test  = 1,
	            .depth_write = 1,
	        },
	    .sample_count                  = 1,
	    .color_attachment_count        = 1,
	    .color_attachment_formats[ 0 ] = data->swapchain_format,
	    .depth_stencil_format          = FT_FORMAT_D32_SFLOAT,
	};

	create_graphics_pipeline( device, &info, &data->pipeline );

	destroy_shader( device, shader );
}

static void
main_pass_execute( struct CommandBuffer* cmd, void* user_data )
{
	struct MainPassData* data = user_data;
	cmd_set_scissor( cmd, 0, 0, data->width, data->height );
	cmd_set_viewport( cmd, 0, 0, data->width, data->height, 0, 1.0f );
	cmd_bind_pipeline( cmd, data->pipeline );
	cmd_draw( cmd, 3, 1, 0, 0 );
}

static void
main_pass_destroy( const struct Device* device, void* user_data )
{
	struct MainPassData* data = user_data;
	destroy_pipeline( device, data->pipeline );
	destroy_descriptor_set_layout( device, data->dsl );
}

static b32
main_pass_get_clear_color( u32 idx, ColorClearValue* color )
{
	switch ( idx )
	{
	case 0:
	{
		( *color )[ 0 ] = 0.1f;
		( *color )[ 1 ] = 0.2f;
		( *color )[ 2 ] = 0.3f;
		( *color )[ 3 ] = 1.0f;
		return 1;
	}
	default: return 0;
	}
}

static b32
main_pass_get_clear_depth_stencil(
    struct DepthStencilClearValue* depth_stencil )
{
	depth_stencil->depth   = 1.0f;
	depth_stencil->stencil = 0;

	return 1;
}

void
register_main_pass( struct RenderGraph*     graph,
                    const struct Swapchain* swapchain,
                    const char*             backbuffer_source_name )
{
	main_pass_data.width            = swapchain->width;
	main_pass_data.height           = swapchain->height;
	main_pass_data.swapchain_format = swapchain->format;

	struct RenderPass* pass;
	rg_add_pass( graph, "main", &pass );
	rg_set_user_data( pass, &main_pass_data );
	rg_set_pass_create_callback( pass, main_pass_create );
	rg_set_pass_execute_callback( pass, main_pass_execute );
	rg_set_pass_destroy_callback( pass, main_pass_destroy );
	rg_set_get_clear_color( pass, main_pass_get_clear_color );
	rg_set_get_clear_depth_stencil( pass, main_pass_get_clear_depth_stencil );

	struct ImageInfo back;
	rg_add_color_output( pass, backbuffer_source_name, &back );
	struct ImageInfo depth_image = {
	    .width        = main_pass_data.width,
	    .height       = main_pass_data.height,
	    .depth        = 1,
	    .format       = FT_FORMAT_D32_SFLOAT,
	    .layer_count  = 1,
	    .mip_levels   = 1,
	    .sample_count = 1,
	};
	rg_add_depth_stencil_output( pass, "depth", &depth_image );
}

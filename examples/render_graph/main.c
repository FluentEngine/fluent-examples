#include <fluent/os.h>
#include <fluent/renderer.h>

#include "main.vert.h"
#include "main.frag.h"

#define FRAME_COUNT   2
#define WINDOW_WIDTH  600
#define WINDOW_HEIGHT 480

struct FrameData
{
	struct Semaphore* present_semaphore;
	struct Semaphore* render_semaphore;
	struct Fence*     render_fence;

	struct CommandPool*   cmd_pool;
	struct CommandBuffer* cmd;
	b32                   cmd_recorded;
};

struct MainPassData
{
	struct DescriptorSetLayout* dsl;
	struct Pipeline*            pipeline;
} main_pass_data;

static enum RendererAPI        renderer_api   = FT_RENDERER_API_VULKAN;
static struct RendererBackend* backend        = NULL;
static struct Device*          device         = NULL;
static struct Queue*           graphics_queue = NULL;
static struct Swapchain*       swapchain      = NULL;
static struct FrameData        frames[ FRAME_COUNT ];
static u32                     frame_index = 0;
static u32                     image_index = 0;

struct RenderGraph* graph;

static void
init_renderer( void );
static void
shutdown_renderer( void );

static void
begin_frame( void );
static void
end_frame( void );

static void
main_pass_create( void* user_data )
{
	struct MainPassData* data = user_data;

	struct ShaderInfo shader_info = {
	    .vertex =
	        {
	            .bytecode      = shader_main_vert,
	            .bytecode_size = sizeof( shader_main_vert ),
	        },
	    .fragment =
	        {
	            .bytecode      = shader_main_frag,
	            .bytecode_size = sizeof( shader_main_frag ),
	        },
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
	    .color_attachment_formats[ 0 ] = swapchain->format,
	    .depth_stencil_format          = FT_FORMAT_D32_SFLOAT,
	};

	create_graphics_pipeline( device, &info, &data->pipeline );

	destroy_shader( device, shader );
}

static void
main_pass_execute( struct CommandBuffer* cmd, void* user_data )
{
	struct MainPassData* data = user_data;
	cmd_set_scissor( cmd, 0, 0, swapchain->width, swapchain->height );
	cmd_set_viewport( cmd, 0, 0, swapchain->width, swapchain->height, 0, 1.0f );
	cmd_bind_pipeline( cmd, data->pipeline );
	cmd_draw( cmd, 3, 1, 0, 0 );
}

static void
main_pass_destroy( void* user_data )
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

static void
on_init( void )
{
	init_renderer();

	rg_create( device, &graph );
	struct RenderPass* pass;
	rg_add_pass( graph, "main", &pass );
	rg_set_user_data( pass, &main_pass_data );
	rg_set_pass_create_callback( pass, main_pass_create );
	rg_set_pass_execute_callback( pass, main_pass_execute );
	rg_set_pass_destroy_callback( pass, main_pass_destroy );
	rg_set_get_clear_color( pass, main_pass_get_clear_color );
	rg_set_get_clear_depth_stencil( pass, main_pass_get_clear_depth_stencil );

	struct ImageInfo back;
	rg_add_color_output( pass, "back", &back );
	struct ImageInfo depth_image = {
	    .width        = swapchain->width,
	    .height       = swapchain->height,
	    .depth        = 1,
	    .format       = FT_FORMAT_D32_SFLOAT,
	    .layer_count  = 1,
	    .mip_levels   = 1,
	    .sample_count = 1,
	};
	rg_add_depth_stencil_output( pass, "depth", &depth_image );

	rg_set_backbuffer_source( graph, "back" );
	rg_set_swapchain_dimensions( graph, swapchain->width, swapchain->height );
	rg_build( graph );
}

static void
on_update( f32 delta_time )
{
	FT_UNUSED( delta_time );

	begin_frame();

	struct CommandBuffer* cmd = frames[ frame_index ].cmd;
	begin_command_buffer( cmd );

	rg_setup_attachments( graph, swapchain->images[ image_index ] );
	rg_execute( cmd, graph );

	end_command_buffer( cmd );

	end_frame();
}

static void
on_resize( u32 width, u32 height )
{
	queue_wait_idle( graphics_queue );
	resize_swapchain( device, swapchain, width, height );
}

static void
on_shutdown( void )
{
	queue_wait_idle( graphics_queue );
	rg_destroy( graph );
	shutdown_renderer();
}

int
main( int argc, char** argv )
{
	struct WindowInfo window_info = {
	    .title        = "fluent-sandbox",
	    .x            = 100,
	    .y            = 100,
	    .width        = WINDOW_WIDTH,
	    .height       = WINDOW_HEIGHT,
	    .resizable    = 0,
	    .centered     = 1,
	    .fullscreen   = 0,
	    .grab_mouse   = 0,
	    .renderer_api = renderer_api,
	};

	struct ApplicationConfig config = {
	    .argc        = argc,
	    .argv        = argv,
	    .window_info = window_info,
	    .log_level   = FT_TRACE,
	    .on_init     = on_init,
	    .on_update   = on_update,
	    .on_resize   = on_resize,
	    .on_shutdown = on_shutdown,
	};

	app_init( &config );
	app_run();
	app_shutdown();

	return EXIT_SUCCESS;
}

static void
init_renderer()
{
	struct RendererBackendInfo backend_info = {
	    backend_info.api      = renderer_api,
	    backend_info.wsi_info = get_ft_wsi_info(),
	};
	create_renderer_backend( &backend_info, &backend );

	struct DeviceInfo device_info = {
	    device_info.backend = backend,
	};
	create_device( backend, &device_info, &device );

	struct QueueInfo queue_info = {
	    queue_info.queue_type = FT_QUEUE_TYPE_GRAPHICS,
	};
	create_queue( device, &queue_info, &graphics_queue );

	for ( u32 i = 0; i < FRAME_COUNT; i++ )
	{
		frames[ i ].cmd_recorded = 0;
		create_semaphore( device, &frames[ i ].present_semaphore );
		create_semaphore( device, &frames[ i ].render_semaphore );
		create_fence( device, &frames[ i ].render_fence );

		struct CommandPoolInfo pool_info = {
		    .queue = graphics_queue,
		};

		create_command_pool( device, &pool_info, &frames[ i ].cmd_pool );
		create_command_buffers( device,
		                        frames[ i ].cmd_pool,
		                        1,
		                        &frames[ i ].cmd );
	}

	struct SwapchainInfo swapchain_info = {
	    .width           = window_get_framebuffer_width( get_app_window() ),
	    .height          = window_get_framebuffer_height( get_app_window() ),
	    .format          = FT_FORMAT_B8G8R8A8_SRGB,
	    .min_image_count = FRAME_COUNT,
	    .vsync           = 1,
	    .queue           = graphics_queue,
	    .wsi_info        = get_ft_wsi_info(),
	};
	create_swapchain( device, &swapchain_info, &swapchain );
}

static void
shutdown_renderer()
{
	queue_wait_idle( graphics_queue );

	destroy_swapchain( device, swapchain );

	for ( u32 i = 0; i < FRAME_COUNT; i++ )
	{
		destroy_command_buffers( device,
		                         frames[ i ].cmd_pool,
		                         1,
		                         &frames[ i ].cmd );
		destroy_command_pool( device, frames[ i ].cmd_pool );
		destroy_fence( device, frames[ i ].render_fence );
		destroy_semaphore( device, frames[ i ].render_semaphore );
		destroy_semaphore( device, frames[ i ].present_semaphore );
	}

	destroy_queue( graphics_queue );
	destroy_device( device );
	destroy_renderer_backend( backend );
}

static void
begin_frame()
{
	if ( !frames[ frame_index ].cmd_recorded )
	{
		wait_for_fences( device, 1, &frames[ frame_index ].render_fence );
		reset_fences( device, 1, &frames[ frame_index ].render_fence );
		frames[ frame_index ].cmd_recorded = 1;
	}

	acquire_next_image( device,
	                    swapchain,
	                    frames[ frame_index ].present_semaphore,
	                    NULL,
	                    &image_index );
}

static void
end_frame()
{
	struct QueueSubmitInfo submit_info = {
	    .wait_semaphore_count   = 1,
	    .wait_semaphores        = &frames[ frame_index ].present_semaphore,
	    .command_buffer_count   = 1,
	    .command_buffers        = &frames[ frame_index ].cmd,
	    .signal_semaphore_count = 1,
	    .signal_semaphores      = &frames[ frame_index ].render_semaphore,
	    .signal_fence           = frames[ frame_index ].render_fence,
	};

	queue_submit( graphics_queue, &submit_info );

	struct QueuePresentInfo queue_present_info = {
	    .wait_semaphore_count = 1,
	    .wait_semaphores      = &frames[ frame_index ].render_semaphore,
	    .swapchain            = swapchain,
	    .image_index          = image_index,
	};

	queue_present( graphics_queue, &queue_present_info );

	frames[ frame_index ].cmd_recorded = 0;
	frame_index                        = ( frame_index + 1 ) % FRAME_COUNT;
}

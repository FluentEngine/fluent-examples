#include <fluent/os.h>
#include <fluent/renderer.h>

#include "main_pass.h"

#define FRAME_COUNT   2
#define WINDOW_WIDTH  1400
#define WINDOW_HEIGHT 900

struct FrameData
{
	struct Semaphore* present_semaphore;
	struct Semaphore* render_semaphore;
	struct Fence*     render_fence;

	struct CommandPool*   cmd_pool;
	struct CommandBuffer* cmd;
	b32                   cmd_recorded;
};

static enum RendererAPI        renderer_api   = FT_RENDERER_API_VULKAN;
static struct RendererBackend* backend        = NULL;
static struct Device*          device         = NULL;
static struct Queue*           graphics_queue = NULL;
static struct Swapchain*       swapchain      = NULL;
static struct FrameData        frames[ FRAME_COUNT ];
static u32                     frame_index = 0;
static u32                     image_index = 0;

struct RenderGraph* graph;

static struct Camera           camera;
static struct CameraController camera_controller;

struct nk_context*    ctx;
struct nk_font_atlas* atlas;

static void
init_renderer( void );
static void
shutdown_renderer( void );

static void
begin_frame( void );
static void
end_frame( void );

static void
on_init( void )
{
	struct CameraInfo camera_info = {
	    .fov         = radians( 45.0f ),
	    .aspect      = window_get_aspect( get_app_window() ),
	    .near        = 0.1f,
	    .far         = 1000.0f,
	    .speed       = 5.0f,
	    .sensitivity = 0.12f,
	    .position    = { 0.0f, 0.0f, 3.0f },
	    .direction   = { 0.0f, 0.0f, -1.0f },
	    .up          = { 0.0f, 1.0f, 0.0f },
	};

	camera_init( &camera, &camera_info );
	camera_controller_init( &camera_controller, &camera );

	init_renderer();

	ctx = nk_ft_init( get_ft_wsi_info(),
	                  device,
	                  graphics_queue,
	                  swapchain->format,
	                  FT_FORMAT_D32_SFLOAT );

	nk_ft_font_stash_begin( &atlas );
	nk_ft_font_stash_end();

	rg_create( device, &graph );
	register_main_pass( graph, swapchain, "back", &camera, ctx );
	rg_set_backbuffer_source( graph, "back" );
	rg_set_swapchain_dimensions( graph, swapchain->width, swapchain->height );
	rg_build( graph );
}

static void
on_update( f32 delta_time )
{
	if ( is_key_pressed( FT_KEY_LEFT_ALT ) )
	{
		camera_controller_update( &camera_controller, delta_time );
	}
	else
	{
		camera_controller_reset( &camera_controller );
	}

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
	nk_ft_shutdown();
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
	    .api      = renderer_api,
	    .wsi_info = get_ft_wsi_info(),
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

#include <fluent/os.h>
#include <fluent/renderer.h>

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

static enum RendererAPI        renderer_api   = FT_RENDERER_API_VULKAN;
static struct RendererBackend* backend        = NULL;
static struct Device*          device         = NULL;
static struct Queue*           graphics_queue = NULL;
static struct Swapchain*       swapchain      = NULL;
static struct FrameData        frames[ FRAME_COUNT ];
static u32                     frame_index = 0;
static u32                     image_index = 0;

void
init_renderer( void );

void
shutdown_renderer( void );

void
begin_frame( void );
void
end_frame( void );

static void
on_init()
{
	init_renderer();
}

static void
on_update( f32 delta_time )
{
	FT_UNUSED( delta_time );

	begin_frame();

	struct CommandBuffer* cmd = frames[ frame_index ].cmd;
	begin_command_buffer( cmd );

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
on_shutdown()
{
	queue_wait_idle( graphics_queue );
	shutdown_renderer();
}

int
main( int argc, char** argv )
{
	struct WindowInfo window_info = {};
	window_info.title             = "fluent-sandbox";
	window_info.x                 = 100;
	window_info.y                 = 100;
	window_info.width             = WINDOW_WIDTH;
	window_info.height            = WINDOW_HEIGHT;
	window_info.resizable         = 0;
	window_info.centered          = 1;
	window_info.fullscreen        = 0;
	window_info.grab_mouse        = 0;

	struct ApplicationConfig config = {};
	config.argc                     = argc;
	config.argv                     = argv;
	config.window_info              = window_info;
	config.log_level                = FT_TRACE;
	config.on_init                  = on_init;
	config.on_update                = on_update;
	config.on_resize                = on_resize;
	config.on_shutdown              = on_shutdown;

	app_init( &config );
	app_run();
	app_shutdown();

	return EXIT_SUCCESS;
}

void
init_renderer()
{
	struct RendererBackendInfo backend_info = {};
	backend_info.api                        = renderer_api;
	backend_info.wsi_info                   = get_ft_wsi_info();
	create_renderer_backend( &backend_info, &backend );

	struct DeviceInfo device_info = {};
	device_info.backend           = backend;
	create_device( backend, &device_info, &device );

	struct QueueInfo queue_info = {};
	queue_info.queue_type       = FT_QUEUE_TYPE_GRAPHICS;
	create_queue( device, &queue_info, &graphics_queue );

	for ( u32 i = 0; i < FRAME_COUNT; i++ )
	{
		frames[ i ].cmd_recorded = 0;
		create_semaphore( device, &frames[ i ].present_semaphore );
		create_semaphore( device, &frames[ i ].render_semaphore );
		create_fence( device, &frames[ i ].render_fence );

		struct CommandPoolInfo pool_info = {};
		pool_info.queue                  = graphics_queue;
		create_command_pool( device, &pool_info, &frames[ i ].cmd_pool );
		create_command_buffers( device,
		                        frames[ i ].cmd_pool,
		                        1,
		                        &frames[ i ].cmd );
	}

	struct SwapchainInfo swapchain_info = {};
	window_get_size( get_app_window(),
	                 &swapchain_info.width,
	                 &swapchain_info.height );
	swapchain_info.format          = FT_FORMAT_B8G8R8A8_SRGB;
	swapchain_info.min_image_count = FRAME_COUNT;
	swapchain_info.vsync           = 1;
	swapchain_info.queue           = graphics_queue;
	swapchain_info.wsi_info        = get_ft_wsi_info();
	create_swapchain( device, &swapchain_info, &swapchain );

	resource_loader_init( device, 25 * 1024 * 1024 * 8 );
}

void
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

void
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

void
end_frame()
{
	struct QueueSubmitInfo submit_info = {};
	submit_info.wait_semaphore_count   = 1;
	submit_info.wait_semaphores      = &frames[ frame_index ].present_semaphore;
	submit_info.command_buffer_count = 1;
	submit_info.command_buffers      = &frames[ frame_index ].cmd;
	submit_info.signal_semaphore_count = 1;
	submit_info.signal_semaphores = &frames[ frame_index ].render_semaphore;
	submit_info.signal_fence      = frames[ frame_index ].render_fence;

	queue_submit( graphics_queue, &submit_info );

	struct QueuePresentInfo queue_present_info = {};
	queue_present_info.wait_semaphore_count    = 1;
	queue_present_info.wait_semaphores =
	    &frames[ frame_index ].render_semaphore;
	queue_present_info.swapchain   = swapchain;
	queue_present_info.image_index = image_index;

	queue_present( graphics_queue, &queue_present_info );

	frames[ frame_index ].cmd_recorded = 0;
	frame_index                        = ( frame_index + 1 ) % FRAME_COUNT;
}

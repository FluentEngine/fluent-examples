#include <fluent/os.h>
#include <fluent/renderer.h>

#include <main.vert.h>
#include <main.frag.h>

#define FRAME_COUNT   2
#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480

struct FrameData
{
	Semaphore* present_semaphore;
	Semaphore* render_semaphore;
	Fence*     render_fence;

	CommandPool*   cmd_pool;
	CommandBuffer* cmd;
	b32            cmd_recorded;
};

static enum RendererAPI        renderer_api   = FT_RENDERER_API_VULKAN;
static struct RendererBackend* backend        = NULL;
static struct Device*          device         = NULL;
static struct Queue*           graphics_queue = NULL;
static struct Swapchain*       swapchain      = NULL;
static struct FrameData        frames[ FRAME_COUNT ];
static u32                     frame_index = 0;
static u32                     image_index = 0;

static struct Pipeline*            pipeline   = NULL;
static struct DescriptorSetLayout* dsl        = NULL;
static struct Buffer*              ubo_buffer = NULL;
static struct DescriptorSet*       set        = NULL;

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

	struct ShaderInfo shader_info      = {};
	shader_info.vertex.bytecode_size   = sizeof( shader_main_vert );
	shader_info.vertex.bytecode        = shader_main_vert;
	shader_info.fragment.bytecode_size = sizeof( shader_main_frag );
	shader_info.fragment.bytecode      = shader_main_frag;

	struct Shader* shader;
	create_shader( device, &shader_info, &shader );

	create_descriptor_set_layout( device, shader, &dsl );

	struct PipelineInfo pipeline_info           = {};
	pipeline_info.shader                        = shader;
	pipeline_info.rasterizer_info.cull_mode     = FT_CULL_MODE_NONE;
	pipeline_info.rasterizer_info.front_face    = FT_FRONT_FACE_CLOCKWISE;
	pipeline_info.depth_state_info.depth_test   = 0;
	pipeline_info.depth_state_info.depth_write  = 0;
	pipeline_info.descriptor_set_layout         = dsl;
	pipeline_info.sample_count                  = 1;
	pipeline_info.color_attachment_count        = 1;
	pipeline_info.color_attachment_formats[ 0 ] = swapchain->format;
	pipeline_info.topology = FT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	create_graphics_pipeline( device, &pipeline_info, &pipeline );

	destroy_shader( device, shader );

	f32 color[ 4 ] = { 0.4f, 0.2f, 0.6f, 1.0f };

	struct BufferInfo buffer_info = {
		.descriptor_type = FT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.memory_usage    = FT_MEMORY_USAGE_CPU_TO_GPU,
		.size            = sizeof( color ),
	};

	create_buffer( device, &buffer_info, &ubo_buffer );
	map_memory( device, ubo_buffer );
	memcpy( ubo_buffer->mapped_memory, color, sizeof( color ) );
	unmap_memory( device, ubo_buffer );

	DescriptorSetInfo set_info = { .descriptor_set_layout = dsl, .set = 0 };

	BufferDescriptor buffer_descriptor = {
		.buffer = ubo_buffer,
		.offset = 0,
		.range  = sizeof( color ),
	};

	DescriptorWrite descriptor_write = {
		.buffer_descriptors  = &buffer_descriptor,
		.descriptor_count    = 1,
		.descriptor_name     = "ubo",
		.image_descriptors   = NULL,
		.sampler_descriptors = NULL,
	};

	create_descriptor_set( device, &set_info, &set );
	update_descriptor_set( device, set, 1, &descriptor_write );
}

static void
on_update( f32 delta_time )
{
	begin_frame();

	CommandBuffer* cmd = frames[ frame_index ].cmd;

	begin_command_buffer( cmd );

	ImageBarrier barrier = {};
	barrier.image        = swapchain->images[ image_index ];
	barrier.old_state    = FT_RESOURCE_STATE_UNDEFINED;
	barrier.new_state    = FT_RESOURCE_STATE_COLOR_ATTACHMENT;

	RenderPassBeginInfo rp_info            = {};
	rp_info.device                         = device;
	rp_info.width                          = WINDOW_WIDTH;
	rp_info.height                         = WINDOW_HEIGHT;
	rp_info.color_attachment_count         = 1;
	rp_info.color_attachments[ 0 ]         = swapchain->images[ image_index ];
	rp_info.color_attachment_load_ops[ 0 ] = FT_ATTACHMENT_LOAD_OP_CLEAR;
	rp_info.color_image_states[ 0 ]        = FT_RESOURCE_STATE_COLOR_ATTACHMENT;
	rp_info.clear_values[ 0 ].color[ 0 ]   = 0.38f;
	rp_info.clear_values[ 0 ].color[ 1 ]   = 0.30f;
	rp_info.clear_values[ 0 ].color[ 2 ]   = 0.35f;
	rp_info.clear_values[ 0 ].color[ 3 ]   = 1.0f;

	cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &barrier );

	cmd_begin_render_pass( cmd, &rp_info );
	cmd_set_scissor( cmd, 0, 0, swapchain->width, swapchain->height );
	cmd_set_viewport( cmd,
	                  0,
	                  0,
	                  ( f32 ) swapchain->width,
	                  ( f32 ) swapchain->height,
	                  0,
	                  1 );
	cmd_bind_descriptor_set( cmd, 0, set, pipeline );
	cmd_bind_pipeline( cmd, pipeline );
	cmd_draw( cmd, 6, 1, 0, 0 );
	cmd_end_render_pass( cmd );

	barrier.old_state = FT_RESOURCE_STATE_COLOR_ATTACHMENT;
	barrier.new_state = FT_RESOURCE_STATE_PRESENT;

	cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &barrier );

	end_command_buffer( cmd );

	end_frame();
}

static void
on_resize( u32 width, u32 height )
{
	// TODO:
}

static void
on_shutdown()
{
	queue_wait_idle( graphics_queue );
	destroy_buffer( device, ubo_buffer );
	destroy_descriptor_set( device, set );
	destroy_descriptor_set_layout( device, dsl );
	destroy_pipeline( device, pipeline );
	shutdown_renderer();
}

int
main( int argc, char** argv )
{
	WindowInfo window_info = {};
	window_info.title      = "fluent-sandbox";
	window_info.x          = 100;
	window_info.y          = 100;
	window_info.width      = WINDOW_WIDTH;
	window_info.height     = WINDOW_HEIGHT;
	window_info.resizable  = 0;
	window_info.centered   = 1;
	window_info.fullscreen = 0;
	window_info.grab_mouse = 0;

	ApplicationConfig config = {};
	config.argc              = argc;
	config.argv              = argv;
	config.window_info       = window_info;
	config.log_level         = FT_TRACE;
	config.on_init           = on_init;
	config.on_update         = on_update;
	config.on_resize         = on_resize;
	config.on_shutdown       = on_shutdown;

	app_init( &config );
	app_run();
	app_shutdown();

	return EXIT_SUCCESS;
}

void
init_renderer()
{
	RendererBackendInfo backend_info = {};
	backend_info.api                 = renderer_api;
	backend_info.wsi_info            = get_ft_wsi_info();
	create_renderer_backend( &backend_info, &backend );

	DeviceInfo device_info         = {};
	device_info.frame_in_use_count = FRAME_COUNT;
	create_device( backend, &device_info, &device );

	QueueInfo queue_info  = {};
	queue_info.queue_type = FT_QUEUE_TYPE_GRAPHICS;
	create_queue( device, &queue_info, &graphics_queue );

	for ( u32 i = 0; i < FRAME_COUNT; i++ )
	{
		frames[ i ].cmd_recorded = 0;
		create_semaphore( device, &frames[ i ].present_semaphore );
		create_semaphore( device, &frames[ i ].render_semaphore );
		create_fence( device, &frames[ i ].render_fence );

		CommandPoolInfo pool_info = {};
		pool_info.queue           = graphics_queue;
		create_command_pool( device, &pool_info, &frames[ i ].cmd_pool );
		create_command_buffers( device,
		                        frames[ i ].cmd_pool,
		                        1,
		                        &frames[ i ].cmd );
	}

	SwapchainInfo swapchain_info = {};
	window_get_size( get_app_window(),
	                 &swapchain_info.width,
	                 &swapchain_info.height );
	swapchain_info.format          = FT_FORMAT_B8G8R8A8_SRGB;
	swapchain_info.min_image_count = FRAME_COUNT;
	swapchain_info.vsync           = 1;
	swapchain_info.queue           = graphics_queue;
	swapchain_info.wsi_info        = get_ft_wsi_info();
	create_swapchain( device, &swapchain_info, &swapchain );
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
	QueueSubmitInfo submit_info      = {};
	submit_info.wait_semaphore_count = 1;
	submit_info.wait_semaphores      = &frames[ frame_index ].present_semaphore;
	submit_info.command_buffer_count = 1;
	submit_info.command_buffers      = &frames[ frame_index ].cmd;
	submit_info.signal_semaphore_count = 1;
	submit_info.signal_semaphores = &frames[ frame_index ].render_semaphore;
	submit_info.signal_fence      = frames[ frame_index ].render_fence;

	queue_submit( graphics_queue, &submit_info );

	QueuePresentInfo queue_present_info     = {};
	queue_present_info.wait_semaphore_count = 1;
	queue_present_info.wait_semaphores =
	    &frames[ frame_index ].render_semaphore;
	queue_present_info.swapchain   = swapchain;
	queue_present_info.image_index = image_index;

	queue_present( graphics_queue, &queue_present_info );

	frames[ frame_index ].cmd_recorded = 0;
	frame_index                        = ( frame_index + 1 ) % FRAME_COUNT;
}

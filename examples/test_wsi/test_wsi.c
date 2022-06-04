#include <math.h>
#include <volk/volk.h>
#include <GLFW/glfw3.h>
#include <fluent/renderer.h>

#include "main.vert.h"
#include "main.frag.h"

#define FRAME_COUNT   2
#define WINDOW_WIDTH  640
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

static GLFWwindow*    window   = NULL;
static struct WsiInfo glfw_wsi = {};

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
static struct Sampler*             sampler    = NULL;
static struct Image*               texture    = NULL;
static struct DescriptorSet*       set        = NULL;

static struct nk_context*    ctx;
static struct nk_font_atlas* atlas;

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
	pipeline_info.topology = FT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	create_graphics_pipeline( device, &pipeline_info, &pipeline );

	destroy_shader( device, shader );

	struct BufferInfo buffer_info = {
		.descriptor_type = FT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.memory_usage    = FT_MEMORY_USAGE_CPU_TO_GPU,
		.size            = sizeof( float[ 4 ] ),
	};

	create_buffer( device, &buffer_info, &ubo_buffer );

	f32 color[ 4 ] = { 0.3f, 0.0f, 0.4f, 1.0f };
	map_memory( device, ubo_buffer );
	memcpy( ubo_buffer->mapped_memory, color, sizeof( color ) );
	unmap_memory( device, ubo_buffer );

	struct SamplerInfo sampler_info = {
		.mag_filter        = FT_FILTER_LINEAR,
		.min_filter        = FT_FILTER_LINEAR,
		.mipmap_mode       = FT_SAMPLER_MIPMAP_MODE_NEAREST,
		.address_mode_u    = FT_SAMPLER_ADDRESS_MODE_REPEAT,
		.address_mode_v    = FT_SAMPLER_ADDRESS_MODE_REPEAT,
		.address_mode_w    = FT_SAMPLER_ADDRESS_MODE_REPEAT,
		.mip_lod_bias      = 0,
		.anisotropy_enable = 0,
		.max_anisotropy    = 0,
		.compare_enable    = 0,
		.compare_op        = FT_COMPARE_OP_ALWAYS,
		.min_lod           = 0,
		.max_lod           = 0,
	};

	create_sampler( device, &sampler_info, &sampler );

	struct ImageInfo image_info = {
		.width           = 2,
		.height          = 2,
		.depth           = 1,
		.format          = FT_FORMAT_R8G8B8A8_UNORM,
		.sample_count    = 1,
		.mip_levels      = 1,
		.layer_count     = 1,
		.descriptor_type = FT_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
	};

	create_image( device, &image_info, &texture );

	u8 image_data[ 4 ][ 4 ];
	for ( u32 i = 0; i < 4; ++i )
	{
		image_data[ i ][ 0 ] = 90;
		image_data[ i ][ 1 ] = 130;
		image_data[ i ][ 2 ] = 170;
		image_data[ i ][ 3 ] = 255;
	}
	upload_image( texture, sizeof( image_data ), image_data );

	struct DescriptorSetInfo set_info = { .descriptor_set_layout = dsl,
		                                  .set                   = 0 };

	struct BufferDescriptor buffer_descriptor = {
		.buffer = ubo_buffer,
		.offset = 0,
		.range  = sizeof( float[ 4 ] ),
	};

	struct SamplerDescriptor sampler_descriptor = { .sampler = sampler };

	struct ImageDescriptor image_descriptor = {
		.image          = texture,
		.resource_state = FT_RESOURCE_STATE_SHADER_READ_ONLY
	};

	struct DescriptorWrite descriptor_writes[ 3 ];
	memset( descriptor_writes, 0, sizeof( descriptor_writes ) );
	descriptor_writes[ 0 ].buffer_descriptors  = &buffer_descriptor;
	descriptor_writes[ 0 ].descriptor_count    = 1;
	descriptor_writes[ 0 ].descriptor_name     = "ubo";
	descriptor_writes[ 0 ].image_descriptors   = NULL;
	descriptor_writes[ 0 ].sampler_descriptors = NULL;

	descriptor_writes[ 1 ].buffer_descriptors  = NULL;
	descriptor_writes[ 1 ].descriptor_count    = 1;
	descriptor_writes[ 1 ].descriptor_name     = "u_sampler";
	descriptor_writes[ 1 ].image_descriptors   = NULL;
	descriptor_writes[ 1 ].sampler_descriptors = &sampler_descriptor;

	descriptor_writes[ 2 ].buffer_descriptors  = NULL;
	descriptor_writes[ 2 ].descriptor_count    = 1;
	descriptor_writes[ 2 ].descriptor_name     = "u_texture";
	descriptor_writes[ 2 ].image_descriptors   = &image_descriptor;
	descriptor_writes[ 2 ].sampler_descriptors = NULL;

	create_descriptor_set( device, &set_info, &set );
	update_descriptor_set( device, set, 3, descriptor_writes );
}

static void
on_update( f32 delta_time )
{
	begin_frame();

	struct CommandBuffer* cmd = frames[ frame_index ].cmd;

	begin_command_buffer( cmd );

	struct ImageBarrier barrier = {};
	barrier.image               = swapchain->images[ image_index ];
	barrier.old_state           = FT_RESOURCE_STATE_UNDEFINED;
	barrier.new_state           = FT_RESOURCE_STATE_COLOR_ATTACHMENT;

	struct RenderPassBeginInfo rp_info     = {};
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
	cmd_draw( cmd, 4, 1, 0, 0 );
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
	destroy_image( device, texture );
	destroy_sampler( device, sampler );
	destroy_buffer( device, ubo_buffer );
	destroy_descriptor_set( device, set );
	destroy_descriptor_set_layout( device, dsl );
	destroy_pipeline( device, pipeline );
	shutdown_renderer();
}

static void
create_vulkan_surface_glfw( void* w, void* i, void** s )
{
	GLFWwindow*  glfw_window = ( GLFWwindow* ) w;
	VkInstance   instance    = ( VkInstance ) i;
	VkSurfaceKHR surface;
	glfwCreateWindowSurface( instance, glfw_window, NULL, &surface );
	*s = surface;
}

int
main( int argc, char** argv )
{
	log_init( FT_TRACE );

	glfwInit();
	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
	glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );
	window =
	    glfwCreateWindow( WINDOW_WIDTH, WINDOW_HEIGHT, "test_wsi", NULL, NULL );
	glfw_wsi.vulkan_instance_extensions = glfwGetRequiredInstanceExtensions(
	    &glfw_wsi.vulkan_instance_extension_count );
	glfw_wsi.window                = window;
	glfw_wsi.create_vulkan_surface = create_vulkan_surface_glfw;

	on_init();

	while ( !glfwWindowShouldClose( window ) )
	{
		on_update( 0.0f );
		glfwPollEvents();
	}

	on_shutdown();

	glfwTerminate();

	log_shutdown();

	return EXIT_SUCCESS;
}

void
init_renderer()
{
	struct RendererBackendInfo backend_info = {};
	backend_info.api                        = renderer_api;
	backend_info.wsi_info                   = &glfw_wsi;
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
	swapchain_info.width                = WINDOW_WIDTH;
	swapchain_info.height               = WINDOW_HEIGHT;
	swapchain_info.format               = FT_FORMAT_B8G8R8A8_SRGB;
	swapchain_info.min_image_count      = FRAME_COUNT;
	swapchain_info.vsync                = 1;
	swapchain_info.queue                = graphics_queue;
	swapchain_info.wsi_info             = &glfw_wsi;
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

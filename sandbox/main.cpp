#include <functional>
#include <unordered_map>
#include "fluent/fluent.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
inline const char* MODEL_TEXTURE = "diablo.tga";
inline const char* MODEL_NAME    = "diablo.obj";
#include "model.hpp"
#include "render_graph.hpp"
#include "editor.hpp"

#include "shader_deffered_shading.vert.h"
#include "shader_deffered_shading.frag.h"
#include "shader_gbuffer.vert.h"
#include "shader_gbuffer.frag.h"

using namespace fluent;

inline constexpr u32 FRAME_COUNT        = 2;
inline constexpr u32 VERTEX_BUFFER_SIZE = 50 * 1024 * 8;
inline constexpr u32 INDEX_BUFFER_SIZE  = 30 * 1024 * 8;
inline constexpr u32 MAX_MODEL_COUNT    = 100;

struct FrameData
{
	Semaphore* present_semaphore;
	Semaphore* render_semaphore;
	Fence*     render_fence;

	CommandPool*   cmd_pool;
	CommandBuffer* cmd;
	bool           cmd_recorded = false;
};

u32 window_width  = 1400;
u32 window_height = 900;

RendererAPI      renderer_api = RendererAPI::VULKAN;
RendererBackend* backend;
Device*          device;
Queue*           graphics_queue;
Swapchain*       swapchain;
Image*           depth_image;
FrameData        frames[ FRAME_COUNT ];
u32              frame_index = 0;
u32              image_index = 0;

rg::RenderGraph graph;

Image* black_texture;

void
begin_frame( u32* image_index );
void
end_frame( u32 image_index );

void
on_init()
{
	fs::set_shaders_directory( "shaders/sandbox/" );
	fs::set_textures_directory( "../../sandbox/" );
	fs::set_models_directory( "../../sandbox/" );

	RendererBackendInfo backend_info {};
	backend_info.api = renderer_api;
	create_renderer_backend( &backend_info, &backend );

	DeviceInfo device_info {};
	device_info.frame_in_use_count = FRAME_COUNT;
	create_device( backend, &device_info, &device );

	QueueInfo queue_info {};
	queue_info.queue_type = QueueType::GRAPHICS;
	create_queue( device, &queue_info, &graphics_queue );

	for ( u32 i = 0; i < FRAME_COUNT; i++ )
	{
		create_semaphore( device, &frames[ i ].present_semaphore );
		create_semaphore( device, &frames[ i ].render_semaphore );
		create_fence( device, &frames[ i ].render_fence );

		CommandPoolInfo pool_info {};
		pool_info.queue = graphics_queue;
		create_command_pool( device, &pool_info, &frames[ i ].cmd_pool );
		create_command_buffers( device,
		                        frames[ i ].cmd_pool,
		                        1,
		                        &frames[ i ].cmd );
	}

	SwapchainInfo swapchain_info {};
	window_get_size( get_app_window(),
	                 &swapchain_info.width,
	                 &swapchain_info.height );
	swapchain_info.format          = Format::B8G8R8A8_SRGB;
	swapchain_info.min_image_count = FRAME_COUNT;
	swapchain_info.vsync           = true;
	swapchain_info.queue           = graphics_queue;
	create_swapchain( device, &swapchain_info, &swapchain );

	ResourceLoader::init( device, 5000 );

	ImageInfo image_info {};
	image_info.width           = 2;
	image_info.height          = 2;
	image_info.depth           = 1;
	image_info.format          = swapchain->format;
	image_info.layer_count     = 1;
	image_info.mip_levels      = 1;
	image_info.sample_count    = SampleCount::E1;
	image_info.descriptor_type = DescriptorType::SAMPLED_IMAGE;

	create_image( device, &image_info, &black_texture );
	
	struct Color
	{
		u8 r, g, b, a;
	} color { 0, 0, 0, 255 };

	Color image_data[4] = { color, color, color, color };
	ResourceLoader::upload_image( black_texture,
	                              sizeof( image_data ),
	                              image_data );

	UiInfo ui_info {};
	ui_info.backend                       = backend;
	ui_info.device                        = device;
	ui_info.min_image_count               = swapchain->min_image_count;
	ui_info.image_count                   = swapchain->image_count;
	ui_info.in_fly_frame_count            = FRAME_COUNT;
	ui_info.queue                         = graphics_queue;
	ui_info.sample_count                  = SampleCount::E1;
	ui_info.color_attachment_count        = 1;
	ui_info.color_attachment_formats[ 0 ] = swapchain->format;
	ui_info.window                        = get_app_window();

	init_ui( &ui_info );

	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	editor_init( renderer_api );

	begin_command_buffer( frames[ 0 ].cmd );
	ui_upload_resources( frames[ 0 ].cmd );
	end_command_buffer( frames[ 0 ].cmd );
	immediate_submit( graphics_queue, frames[ 0 ].cmd );
	ui_destroy_upload_objects();

	graph.init( device );

	ImageInfo            back;
	rg::RenderGraphPass* pass = graph.add_pass( "final" );
	pass->add_color_output( "back", back );
	pass->set_get_clear_color(
	    []( u32 idx, ColorClearValue* clear_value ) -> bool
	    {
		    clear_value[ idx ][ 0 ] = 0.2f;
		    clear_value[ idx ][ 1 ] = 0.3f;
		    clear_value[ idx ][ 2 ] = 0.4f;
		    clear_value[ idx ][ 3 ] = 1.0f;
		    return true;
	    } );

	pass->set_execute_callback(
	    []( CommandBuffer* cmd, void* )
	    {
		    editor_set_scene_image( black_texture );

		    ui_begin_frame( cmd );
		    editor_render();
		    ui_end_frame( cmd );

		    if ( editor_exit_requested() )
		    {
			    app_request_exit();
		    }
	    } );

	graph.set_backbuffer_source( "back" );
	graph.build();
}

void
on_update( f32 delta_time )
{
	begin_frame( &image_index );

	CommandBuffer* cmd = frames[ frame_index ].cmd;

	begin_command_buffer( cmd );

	graph.setup_attachments( swapchain->images[ image_index ] );
	graph.execute( cmd );

	end_command_buffer( cmd );

	end_frame( image_index );
}

void
on_resize( u32 width, u32 height )
{
	// TODO:
}

void
on_shutdown()
{
	queue_wait_idle( graphics_queue );

	graph.shutdown();

	shutdown_ui( device );

	ResourceLoader::shutdown();
	destroy_image( device, black_texture );

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

int
main( int argc, char** argv )
{
	WindowInfo window_info {};
	window_info.title      = "fluent-sandbox";
	window_info.x          = 100;
	window_info.y          = 100;
	window_info.width      = window_width;
	window_info.height     = window_height;
	window_info.resizable  = false;
	window_info.centered   = true;
	window_info.fullscreen = false;
	window_info.grab_mouse = false;

	ApplicationConfig config;
	config.argc        = argc;
	config.argv        = argv;
	config.window_info = window_info;
	config.log_level   = LogLevel::TRACE;
	config.on_init     = on_init;
	config.on_update   = on_update;
	config.on_resize   = on_resize;
	config.on_shutdown = on_shutdown;

	app_init( &config );
	app_run();
	app_shutdown();

	return EXIT_SUCCESS;
}

void
begin_frame( u32* image_index )
{
	if ( !frames[ frame_index ].cmd_recorded )
	{
		wait_for_fences( device, 1, &frames[ frame_index ].render_fence );
		reset_fences( device, 1, &frames[ frame_index ].render_fence );
		frames[ frame_index ].cmd_recorded = true;
	}

	acquire_next_image( device,
	                    swapchain,
	                    frames[ frame_index ].present_semaphore,
	                    nullptr,
	                    image_index );
}

void
end_frame( u32 image_index )
{
	QueueSubmitInfo submit_info {};
	submit_info.wait_semaphore_count = 1;
	submit_info.wait_semaphores      = &frames[ frame_index ].present_semaphore;
	submit_info.command_buffer_count = 1;
	submit_info.command_buffers      = &frames[ frame_index ].cmd;
	submit_info.signal_semaphore_count = 1;
	submit_info.signal_semaphores = &frames[ frame_index ].render_semaphore;
	submit_info.signal_fence      = frames[ frame_index ].render_fence;

	queue_submit( graphics_queue, &submit_info );

	QueuePresentInfo queue_present_info {};
	queue_present_info.wait_semaphore_count = 1;
	queue_present_info.wait_semaphores =
	    &frames[ frame_index ].render_semaphore;
	queue_present_info.swapchain   = swapchain;
	queue_present_info.image_index = image_index;

	queue_present( graphics_queue, &queue_present_info );

	frames[ frame_index ].cmd_recorded = false;
	frame_index                        = ( frame_index + 1 ) % FRAME_COUNT;
}

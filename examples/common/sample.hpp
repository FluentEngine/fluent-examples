#include "fluent/fluent.hpp"

using namespace fluent;

void
init_sample();
void
update_sample( CommandBuffer* cmd, f32 delta_time );
void
resize_sample( u32 width, u32 height );
void
shutdown_sample();

static constexpr u32 FRAME_COUNT = 2;
u32                  frame_index = 0;

RendererBackend* backend;
Device*          device;
Queue*           queue;
CommandPool*     command_pool;
Semaphore*       image_available_semaphores[ FRAME_COUNT ];
Semaphore*       rendering_finished_semaphores[ FRAME_COUNT ];
Fence*           in_flight_fences[ FRAME_COUNT ];
bool             command_buffers_recorded[ FRAME_COUNT ];

Swapchain*     swapchain;
CommandBuffer* command_buffers[ FRAME_COUNT ];

Image*       depth_image;
RenderPass** render_passes;

UiContext* ui;

RendererAPI current_api = RendererAPI::eVulkan;
std::string api_name    = "Vulkan";

void
api_switch( RendererAPI api );

static inline std::string
to_api_str( RendererAPI api )
{
	switch ( api )
	{
	case RendererAPI::eD3D12: return "/d3d12/";
	case RendererAPI::eVulkan: return "/vulkan/";
	case RendererAPI::eMetal: return "/metal/";
	}
	return "";
}

static inline Image*
load_image_from_file( const std::string& filename, b32 flip )
{
	u64       size = 0;
	void*     data = nullptr;
	ImageInfo image_info =
	    fs::read_image_data( fs::get_textures_directory() + filename,
	                         flip,
	                         &size,
	                         &data );
	image_info.format          = Format::eR8G8B8A8Srgb;
	image_info.descriptor_type = DescriptorType::eSampledImage;
	Image* image;
	create_image( device, &image_info, &image );
	ResourceLoader::upload_image( image, size, data );

	return image;
}

void
create_depth_image( u32 width, u32 height )
{
	ImageInfo depth_image_info {};
	depth_image_info.width           = width;
	depth_image_info.height          = height;
	depth_image_info.depth           = 1;
	depth_image_info.format          = Format::eD32Sfloat;
	depth_image_info.layer_count     = 1;
	depth_image_info.mip_levels      = 1;
	depth_image_info.sample_count    = SampleCount::e1;
	depth_image_info.descriptor_type = DescriptorType::eDepthStencilAttachment;

	create_image( device, &depth_image_info, &depth_image );

	ImageBarrier barrier {};
	barrier.image     = depth_image;
	barrier.old_state = ResourceState::eUndefined;
	barrier.new_state = ResourceState::eDepthStencilWrite;
	barrier.src_queue = queue;
	barrier.dst_queue = queue;

	begin_command_buffer( command_buffers[ 0 ] );
	cmd_barrier( command_buffers[ 0 ], 0, nullptr, 0, nullptr, 1, &barrier );
	end_command_buffer( command_buffers[ 0 ] );
	immediate_submit( queue, command_buffers[ 0 ] );
}

void
on_init()
{
	fs::set_shaders_directory( "../shaders/" SAMPLE_NAME );
	fs::set_textures_directory( "../../examples/textures/" );

	RendererBackendInfo backend_info {};
	backend_info.api = current_api;
	create_renderer_backend( &backend_info, &backend );

	DeviceInfo device_info {};
	device_info.frame_in_use_count = 2;
	create_device( backend, &device_info, &device );

	QueueInfo queue_info {};
	queue_info.queue_type = QueueType::eGraphics;
	create_queue( device, &queue_info, &queue );

	CommandPoolInfo command_pool_info {};
	command_pool_info.queue = queue;
	create_command_pool( device, &command_pool_info, &command_pool );
	create_command_buffers( device,
	                        command_pool,
	                        FRAME_COUNT,
	                        command_buffers );
	for ( u32 i = 0; i < FRAME_COUNT; ++i )
	{
		create_semaphore( device, &image_available_semaphores[ i ] );
		create_semaphore( device, &rendering_finished_semaphores[ i ] );
		create_fence( device, &in_flight_fences[ i ] );
		command_buffers_recorded[ i ] = false;
	}

	SwapchainInfo swapchain_info {};
	swapchain_info.width           = window_get_width( get_app_window() );
	swapchain_info.height          = window_get_height( get_app_window() );
	swapchain_info.format          = Format::eB8G8R8A8Srgb;
	swapchain_info.vsync           = true;
	swapchain_info.queue           = queue;
	swapchain_info.min_image_count = FRAME_COUNT;

	create_swapchain( device, &swapchain_info, &swapchain );
	create_depth_image( swapchain->width, swapchain->height );

	RenderPassInfo render_pass_info {};
	render_pass_info.width                          = swapchain->width;
	render_pass_info.height                         = swapchain->height;
	render_pass_info.color_attachment_count         = 1;
	render_pass_info.color_attachment_load_ops[ 0 ] = AttachmentLoadOp::eClear;
	render_pass_info.color_image_states[ 0 ] = ResourceState::eColorAttachment;
	render_pass_info.depth_stencil_state   = ResourceState::eDepthStencilWrite;
	render_pass_info.depth_stencil_load_op = AttachmentLoadOp::eClear;
	render_pass_info.depth_stencil         = depth_image;

	render_passes = new RenderPass*[ swapchain->image_count ];

	for ( u32 i = 0; i < swapchain->image_count; i++ )
	{
		render_pass_info.color_attachments[ 0 ] = swapchain->images[ i ];
		create_render_pass( device, &render_pass_info, &render_passes[ i ] );
	}

	ResourceLoader::init( device, 25 * 1024 * 1024 * 8 );

	UiInfo ui_info {};
	ui_info.backend            = backend;
	ui_info.device             = device;
	ui_info.min_image_count    = swapchain->min_image_count;
	ui_info.image_count        = swapchain->image_count;
	ui_info.in_fly_frame_count = FRAME_COUNT;
	ui_info.queue              = queue;
	ui_info.render_pass        = render_passes[ 0 ];
	ui_info.window             = get_app_window();

	create_ui_context( command_buffers[ 0 ], &ui_info, &ui );

	init_sample();
}

void
on_resize( u32 width, u32 height )
{
	queue_wait_idle( queue );
	destroy_image( device, depth_image );

	resize_swapchain( device, swapchain, width, height );

	create_depth_image( width, height );

	RenderPassInfo render_pass_info {};
	render_pass_info.width                          = width;
	render_pass_info.height                         = height;
	render_pass_info.color_attachment_count         = 1;
	render_pass_info.color_attachment_load_ops[ 0 ] = AttachmentLoadOp::eClear;
	render_pass_info.color_image_states[ 0 ] = ResourceState::eColorAttachment;
	render_pass_info.depth_stencil_state   = ResourceState::eDepthStencilWrite;
	render_pass_info.depth_stencil_load_op = AttachmentLoadOp::eClear;
	render_pass_info.depth_stencil         = depth_image;

	for ( uint32_t i = 0; i < swapchain->image_count; i++ )
	{
		render_pass_info.color_attachments[ 0 ] = swapchain->images[ i ];
		resize_render_pass( device, render_passes[ i ], &render_pass_info );
	}

	resize_sample( width, height );
}

u32
begin_frame()
{
	if ( !command_buffers_recorded[ frame_index ] )
	{
		wait_for_fences( device, 1, &in_flight_fences[ frame_index ] );
		reset_fences( device, 1, &in_flight_fences[ frame_index ] );
		command_buffers_recorded[ frame_index ] = true;
	}

	u32 image_index = 0;
	acquire_next_image( device,
	                    swapchain,
	                    image_available_semaphores[ frame_index ],
	                    nullptr,
	                    &image_index );

	auto& cmd = command_buffers[ frame_index ];

	begin_command_buffer( cmd );

	ImageBarrier to_clear_barrier {};
	to_clear_barrier.src_queue = queue;
	to_clear_barrier.dst_queue = queue;
	to_clear_barrier.image     = swapchain->images[ image_index ];
	to_clear_barrier.old_state = ResourceState::eUndefined;
	to_clear_barrier.new_state = ResourceState::eColorAttachment;

	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &to_clear_barrier );

	RenderPassBeginInfo render_pass_begin_info {};
	render_pass_begin_info.render_pass = render_passes[ image_index ];
	render_pass_begin_info.clear_values[ 0 ].color[ 0 ] = 1.0f;
	render_pass_begin_info.clear_values[ 0 ].color[ 1 ] = 0.8f;
	render_pass_begin_info.clear_values[ 0 ].color[ 2 ] = 0.4f;
	render_pass_begin_info.clear_values[ 0 ].color[ 3 ] = 1.0f;

	cmd_begin_render_pass( cmd, &render_pass_begin_info );
	cmd_set_viewport( cmd,
	                  0,
	                  0,
	                  swapchain->width,
	                  swapchain->height,
	                  0.0f,
	                  1.0f );
	cmd_set_scissor( cmd, 0, 0, swapchain->width, swapchain->height );

	return image_index;
}

void
end_frame( u32 image_index )
{
	auto& cmd = command_buffers[ frame_index ];

	ui_begin_frame( ui, cmd );

	ImGuiStyle* style              = &ImGui::GetStyle();
	auto        old_color          = style->Colors[ ImGuiCol_Text ];
	style->Colors[ ImGuiCol_Text ] = ImVec4( 0.0f, 0.0f, 0.0f, 1.00f );

	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoBackground |
	                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
	                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove;

	bool open_ptr = true;

	ImGui::SetNextWindowSize( { 400, 200 } );
	ImGui::Begin( "Performance", &open_ptr, window_flags );
	ImGui::Text( "FPS: %f", ImGui::GetIO().Framerate );
	ImGui::Text( "%s",
	             std::string( "Current API: " ).append( api_name ).c_str() );
	static int e    = static_cast<int>( current_api );
	auto       last = e;
	ImGui::RadioButton( "Vulkan", &e, 0 );
	ImGui::RadioButton( "D3D12", &e, 1 );
	ImGui::RadioButton( "Metal", &e, 2 );
	if ( last != e )
	{
		api_switch( static_cast<RendererAPI>( e ) );
		return;
	}
	ImGui::End();

	style->Colors[ ImGuiCol_Text ] = old_color;

	ui_end_frame( ui, cmd );
	cmd_end_render_pass( cmd );

	ImageBarrier to_present_barrier {};
	to_present_barrier.src_queue = queue;
	to_present_barrier.dst_queue = queue;
	to_present_barrier.image     = swapchain->images[ image_index ];
	to_present_barrier.old_state = ResourceState::eColorAttachment;
	to_present_barrier.new_state = ResourceState::ePresent;

	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &to_present_barrier );

	end_command_buffer( cmd );

	QueueSubmitInfo queue_submit_info {};
	queue_submit_info.wait_semaphore_count = 1;
	queue_submit_info.wait_semaphores =
	    &image_available_semaphores[ frame_index ];
	queue_submit_info.command_buffer_count   = 1;
	queue_submit_info.command_buffers        = &cmd;
	queue_submit_info.signal_semaphore_count = 1;
	queue_submit_info.signal_semaphores =
	    &rendering_finished_semaphores[ frame_index ];
	queue_submit_info.signal_fence = in_flight_fences[ frame_index ];

	queue_submit( queue, &queue_submit_info );

	QueuePresentInfo queue_present_info {};
	queue_present_info.wait_semaphore_count = 1;
	queue_present_info.wait_semaphores =
	    &rendering_finished_semaphores[ frame_index ];
	queue_present_info.swapchain   = swapchain;
	queue_present_info.image_index = image_index;

	queue_present( queue, &queue_present_info );

	command_buffers_recorded[ frame_index ] = false;
	frame_index                             = ( frame_index + 1 ) % FRAME_COUNT;
}

void
on_update( f32 delta_time )
{
	auto& cmd = command_buffers[ frame_index ];
	update_sample( cmd, delta_time );
}

void
on_shutdown()
{
	queue_wait_idle( queue );
	shutdown_sample();
	destroy_ui_context( device, ui );
	ResourceLoader::shutdown();
	destroy_image( device, depth_image );
	for ( uint32_t i = 0; i < swapchain->image_count; i++ )
	{
		destroy_render_pass( device, render_passes[ i ] );
	}
	delete[] render_passes;
	destroy_swapchain( device, swapchain );
	for ( u32 i = 0; i < FRAME_COUNT; ++i )
	{
		destroy_semaphore( device, image_available_semaphores[ i ] );
		destroy_semaphore( device, rendering_finished_semaphores[ i ] );
		destroy_fence( device, in_flight_fences[ i ] );
	}

	destroy_command_buffers( device,
	                         command_pool,
	                         FRAME_COUNT,
	                         command_buffers );
	destroy_command_pool( device, command_pool );
	destroy_queue( queue );
	destroy_device( device );
	destroy_renderer_backend( backend );
}

void
api_switch( RendererAPI api )
{
	switch ( api )
	{
	case RendererAPI::eD3D12:
	{
		api_name    = "D3D12";
		current_api = RendererAPI::eD3D12;
		break;
	}
	case RendererAPI::eVulkan:
	{
		api_name    = "Vulkan";
		current_api = RendererAPI::eVulkan;
		break;
	}
	case RendererAPI::eMetal:
	{
		api_name    = "Metal";
		current_api = RendererAPI::eMetal;
	}
	}

	on_shutdown();
	on_init();
}

int
main( int argc, char** argv )
{
	ApplicationConfig config;
	config.argc        = argc;
	config.argv        = argv;
	config.window_info = { SAMPLE_NAME, 100, 100, 1400, 900, false };
	config.log_level   = LogLevel::eTrace;
	config.on_init     = on_init;
	config.on_update   = on_update;
	config.on_resize   = on_resize;
	config.on_shutdown = on_shutdown;

	app_init( &config );
	app_run();
	app_shutdown();

	return 0;
}

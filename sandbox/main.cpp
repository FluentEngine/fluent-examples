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

Sampler*         sampler;
Camera           camera;
CameraController camera_controller;

struct ShaderData
{
	Matrix4 projection;
	Matrix4 view;
};

struct GBufferPassData
{
	rg::RenderGraph*     graph;
	Buffer*              ubo_buffer;
	Buffer*              vertex_buffer;
	Buffer*              index_buffer;
	Model                model;
	Image*               model_texture; // TODO:
	Pipeline*            pipeline;
	DescriptorSetLayout* dsl;
	DescriptorSet*       set;
} gbuffer_pass_data;

struct DefferedShadingPassData
{
	rg::RenderGraph*     graph;
	Pipeline*            pipeline;
	DescriptorSetLayout* dsl;
	DescriptorSet*       set;
} deffered_shading_pass_data;

void
begin_frame( u32* image_index );
void
end_frame( u32 image_index );

void
create_model_pipeline( GBufferPassData* );

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

	ResourceLoader::init( device, 20 * 1024 * 1024 * 8 );

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

	editor_init( renderer_api );

	begin_command_buffer( frames[ 0 ].cmd );
	ui_upload_resources( frames[ 0 ].cmd );
	end_command_buffer( frames[ 0 ].cmd );
	immediate_submit( graphics_queue, frames[ 0 ].cmd );
	ui_destroy_upload_objects();

	SamplerInfo sampler_info {};
	sampler_info.mipmap_mode = SamplerMipmapMode::LINEAR;
	sampler_info.min_lod     = 0;
	sampler_info.max_lod     = 1000;

	create_sampler( device, &sampler_info, &sampler );

	CameraInfo camera_info {};
	camera_info.aspect      = window_get_aspect( get_app_window() );
	camera_info.near        = 0.1f;
	camera_info.far         = 1000.0f;
	camera_info.position    = Vector3( 2.0f, -0.5f, 6.0f );
	camera_info.direction   = Vector3( 0.0f, 0.0f, -1.0f );
	camera_info.up          = Vector3( 0.0f, 1.0f, 0.0f );
	camera_info.speed       = 10.0f;
	camera_info.sensitivity = 0.12f;

	camera.init_camera( camera_info );
	camera_controller.init( camera );

	graph.init( device );

	{
		ImageInfo image_info {};
		image_info.width        = swapchain->width;
		image_info.height       = swapchain->height;
		image_info.depth        = 1;
		image_info.format       = swapchain->format;
		image_info.layer_count  = 1;
		image_info.mip_levels   = 1;
		image_info.sample_count = SampleCount::E1;

		ImageInfo depth_image_info {};
		depth_image_info.width        = swapchain->width;
		depth_image_info.height       = swapchain->height;
		depth_image_info.depth        = 1;
		depth_image_info.format       = Format::D32_SFLOAT;
		depth_image_info.layer_count  = 1;
		depth_image_info.mip_levels   = 1;
		depth_image_info.sample_count = SampleCount::E1;

		rg::RenderGraphPass* pass = graph.add_pass( "gbuffer-pass" );
		pass->add_color_output( "position", image_info );
		pass->add_color_output( "normal", image_info );
		pass->add_color_output( "albedo-spec", image_info );
		pass->add_depth_output( "depth", depth_image_info );
		pass->set_get_clear_color(
		    []( u32 idx, ColorClearValue* clear_value ) -> bool
		    {
			    auto& v = *clear_value;
			    v[ 0 ]  = 0.4f;
			    v[ 1 ]  = 0.3f;
			    v[ 2 ]  = 0.4f;
			    v[ 3 ]  = 1.0f;
			    return true;
		    } );
		pass->set_get_clear_depth_stencil(
		    []( DepthStencilClearValue* clear_value )
		    {
			    clear_value->depth   = 1.0f;
			    clear_value->stencil = 0;
			    return true;
		    } );
		pass->set_user_data( &gbuffer_pass_data );
		pass->set_create_callback(
		    []( void* p )
		    {
			    auto* data = static_cast<GBufferPassData*>( p );

			    BufferInfo buffer_info {};
			    buffer_info.descriptor_type = DescriptorType::UNIFORM_BUFFER;
			    buffer_info.memory_usage    = MemoryUsage::CPU_TO_GPU;
			    buffer_info.size            = sizeof( ShaderData );

			    create_buffer( device, &buffer_info, &data->ubo_buffer );

			    std::vector<Vertex> vertices;
			    std::vector<u32>    indices;
			    load_model( vertices, indices );

			    data->model.first_index  = 0;
			    data->model.first_vertex = 0;
			    data->model.index_count  = indices.size();

			    buffer_info                 = {};
			    buffer_info.descriptor_type = DescriptorType::VERTEX_BUFFER;
			    buffer_info.memory_usage    = MemoryUsage::GPU_ONLY;
			    buffer_info.size = vertices.size() * sizeof( vertices[ 0 ] );
			    create_buffer( device, &buffer_info, &data->vertex_buffer );
			    buffer_info.descriptor_type = DescriptorType::INDEX_BUFFER;
			    buffer_info.memory_usage    = MemoryUsage::GPU_ONLY;
			    buffer_info.size = indices.size() * sizeof( indices );
			    create_buffer( device, &buffer_info, &data->index_buffer );

			    u64       size;
			    void*     image_data;
			    ImageInfo image_info = fs::read_image_data(
			        ( fs::get_textures_directory() + MODEL_TEXTURE ),
			        false,
			        &size,
			        &image_data );
			    image_info.format          = Format::R8G8B8A8_SRGB;
			    image_info.descriptor_type = DescriptorType::SAMPLED_IMAGE;
			    create_image( device, &image_info, &data->model_texture );

			    ResourceLoader::begin_recording();
			    ResourceLoader::upload_buffer( data->vertex_buffer,
			                                   0,
			                                   data->vertex_buffer->size,
			                                   vertices.data() );
			    ResourceLoader::upload_buffer( data->index_buffer,
			                                   0,
			                                   data->index_buffer->size,
			                                   indices.data() );
			    ResourceLoader::upload_image( data->model_texture,
			                                  size,
			                                  image_data );
			    ResourceLoader::end_recording();

			    fs::release_image_data( image_data );

			    create_model_pipeline( data );
		    } );
		pass->set_execute_callback(
		    []( CommandBuffer* cmd, void* p )
		    {
			    auto* data        = static_cast<GBufferPassData*>( p );
			    auto* shader_data = static_cast<ShaderData*>(
			        ResourceLoader::begin_upload_buffer( data->ubo_buffer ) );
			    shader_data->view       = camera.get_view_matrix();
			    shader_data->projection = camera.get_projection_matrix();
			    ResourceLoader::end_upload_buffer( data->ubo_buffer );

			    cmd_set_viewport( cmd,
			                      0,
			                      0,
			                      swapchain->width,
			                      swapchain->height,
			                      0.0f,
			                      1.0f );
			    cmd_set_scissor( cmd,
			                     0,
			                     0,
			                     swapchain->width,
			                     swapchain->height );
			    cmd_bind_pipeline( cmd, data->pipeline );
			    cmd_bind_vertex_buffer( cmd, data->vertex_buffer, 0 );
			    cmd_bind_index_buffer_u32( cmd, data->index_buffer, 0 );
			    cmd_bind_descriptor_set( cmd, 0, data->set, data->pipeline );

			    cmd_draw_indexed( cmd, data->model.index_count, 1, 0, 0, 1 );
		    } );

		pass->set_destroy_callback(
		    []( void* p )
		    {
			    auto* data = static_cast<GBufferPassData*>( p );
			    destroy_descriptor_set( device, data->set );
			    destroy_descriptor_set_layout( device, data->dsl );
			    destroy_pipeline( device, data->pipeline );
			    destroy_image( device, data->model_texture );
			    destroy_buffer( device, data->ubo_buffer );
			    destroy_buffer( device, data->index_buffer );
			    destroy_buffer( device, data->vertex_buffer );
		    } );
	}
	{
		ImageInfo scene_image_info {};
		scene_image_info.width        = swapchain->width;
		scene_image_info.height       = swapchain->height;
		scene_image_info.depth        = 1;
		scene_image_info.format       = swapchain->format;
		scene_image_info.layer_count  = 1;
		scene_image_info.mip_levels   = 1;
		scene_image_info.sample_count = SampleCount::E1;

		rg::RenderGraphPass* pass = graph.add_pass( "deffered-shading-pass" );
		pass->add_texture_input( "position" );
		pass->add_texture_input( "normal" );
		pass->add_texture_input( "albedo-spec" );
		pass->add_color_output( "editor-viewport", scene_image_info );
		pass->set_user_data( &deffered_shading_pass_data );
		pass->set_create_callback(
		    []( void* p )
		    {
			    auto* data = static_cast<DefferedShadingPassData*>( p );

			    Shader* shader;

			    ShaderInfo shader_info {};
			    shader_info.vertex.bytecode_size =
			        sizeof( shader_deffered_shading_vert );
			    shader_info.vertex.bytecode = shader_deffered_shading_vert;
			    shader_info.fragment.bytecode_size =
			        sizeof( shader_deffered_shading_frag );
			    shader_info.fragment.bytecode = shader_deffered_shading_frag;

			    create_shader( device, &shader_info, &shader );

			    create_descriptor_set_layout( device, shader, &data->dsl );

			    PipelineInfo pipeline_info {};
			    pipeline_info.shader                       = shader;
			    pipeline_info.descriptor_set_layout        = data->dsl;
			    pipeline_info.rasterizer_info.cull_mode    = CullMode::NONE;
			    pipeline_info.rasterizer_info.polygon_mode = PolygonMode::FILL;
			    pipeline_info.topology = PrimitiveTopology::TRIANGLE_STRIP;
			    pipeline_info.color_attachment_count        = 1;
			    pipeline_info.color_attachment_formats[ 0 ] = swapchain->format;

			    create_graphics_pipeline( device,
			                              &pipeline_info,
			                              &data->pipeline );

			    DescriptorSetInfo set_info {};
			    set_info.set                   = 0;
			    set_info.descriptor_set_layout = data->dsl;
			    create_descriptor_set( device, &set_info, &data->set );

			    destroy_shader( device, shader );

			    // update sets

			    SamplerDescriptor sampler_descriptor {};
			    sampler_descriptor.sampler = sampler;
			    ImageDescriptor position_descriptor {};
			    position_descriptor.image = graph.get_image( "position" );
			    position_descriptor.resource_state =
			        ResourceState::SHADER_READ_ONLY;
			    ImageDescriptor normal_descriptor {};
			    normal_descriptor.image = graph.get_image( "normal" );
			    normal_descriptor.resource_state =
			        ResourceState::SHADER_READ_ONLY;
			    ImageDescriptor albedo_spec_descriptor {};
			    albedo_spec_descriptor.image = graph.get_image( "albedo-spec" );
			    albedo_spec_descriptor.resource_state =
			        ResourceState::SHADER_READ_ONLY;

			    DescriptorWrite descriptor_writes[ 4 ]  = {};
			    descriptor_writes[ 0 ].descriptor_name  = "u_sampler";
			    descriptor_writes[ 0 ].descriptor_count = 1;
			    descriptor_writes[ 0 ].sampler_descriptors =
			        &sampler_descriptor;
			    descriptor_writes[ 1 ].descriptor_name   = "u_position";
			    descriptor_writes[ 1 ].descriptor_count  = 1;
			    descriptor_writes[ 1 ].image_descriptors = &position_descriptor;
			    descriptor_writes[ 2 ].descriptor_name   = "u_normal";
			    descriptor_writes[ 2 ].descriptor_count  = 1;
			    descriptor_writes[ 2 ].image_descriptors = &normal_descriptor;
			    descriptor_writes[ 3 ].descriptor_name   = "u_albedo_spec";
			    descriptor_writes[ 3 ].descriptor_count  = 1;
			    descriptor_writes[ 3 ].image_descriptors =
			        &albedo_spec_descriptor;

			    update_descriptor_set( device,
			                           data->set,
			                           4,
			                           descriptor_writes );
		    } );

		pass->set_execute_callback(
		    []( CommandBuffer* cmd, void* p )
		    {
			    auto* data = static_cast<DefferedShadingPassData*>( p );

			    cmd_set_viewport( cmd,
			                      0,
			                      0,
			                      swapchain->width,
			                      swapchain->height,
			                      0.0f,
			                      1.0f );
			    cmd_set_scissor( cmd,
			                     0,
			                     0,
			                     swapchain->width,
			                     swapchain->height );
			    cmd_bind_pipeline( cmd, data->pipeline );
			    cmd_bind_descriptor_set( cmd, 0, data->set, data->pipeline );
			    cmd_draw( cmd, 4, 1, 0, 0 );
		    } );

		pass->set_destroy_callback(
		    []( void* p )
		    {
			    auto* data = static_cast<DefferedShadingPassData*>( p );
			    destroy_descriptor_set( device, data->set );
			    destroy_descriptor_set_layout( device, data->dsl );
			    destroy_pipeline( device, data->pipeline );
		    } );
	}
	{
		ImageInfo            back;
		rg::RenderGraphPass* pass = graph.add_pass( "editor-pass" );
		pass->set_user_data( &graph );
		pass->add_texture_input( "editor-viewport" );
		pass->add_color_output( "back", back );

		pass->set_execute_callback(
		    []( CommandBuffer* cmd, void* data )
		    {
			    auto* rg = static_cast<rg::RenderGraph*>( data );
			    editor_set_scene_image( rg->get_image( "editor-viewport" ) );

			    ui_begin_frame( cmd );
			    editor_render();
			    ui_end_frame( cmd );

			    if ( editor_exit_requested() )
			    {
				    app_request_exit();
			    }

			    if ( editor_api_change_requested( &renderer_api ) )
			    {
				    app_request_restart();
			    }
		    } );
	}
	graph.set_backbuffer_source( "back" );
	graph.build();
}

void
on_update( f32 delta_time )
{
	if ( is_key_pressed( Key::LeftAlt ) )
	{
		camera_controller.update( delta_time );
	}

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

	destroy_sampler( device, sampler );

	shutdown_ui( device );

	ResourceLoader::shutdown();

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

void
create_model_pipeline( GBufferPassData* data )
{
	Shader* shader;

	ShaderInfo shader_info {};
	shader_info.vertex.bytecode_size   = sizeof( shader_gbuffer_vert );
	shader_info.vertex.bytecode        = shader_gbuffer_vert;
	shader_info.fragment.bytecode_size = sizeof( shader_gbuffer_frag );
	shader_info.fragment.bytecode      = shader_gbuffer_frag;

	create_shader( device, &shader_info, &shader );

	create_descriptor_set_layout( device, shader, &data->dsl );

	PipelineInfo pipeline_info {};
	pipeline_info.shader                 = shader;
	pipeline_info.descriptor_set_layout  = data->dsl;
	auto& layout                         = pipeline_info.vertex_layout;
	layout.binding_info_count            = 1;
	layout.binding_infos[ 0 ].binding    = 0;
	layout.binding_infos[ 0 ].stride     = sizeof( Vertex );
	layout.binding_infos[ 0 ].input_rate = VertexInputRate::VERTEX;
	layout.attribute_info_count          = 3;
	layout.attribute_infos[ 0 ].binding  = 0;
	layout.attribute_infos[ 0 ].location = 0;
	layout.attribute_infos[ 0 ].format   = Format::R32G32B32_SFLOAT;
	layout.attribute_infos[ 0 ].offset   = offsetof( struct Vertex, position );
	layout.attribute_infos[ 1 ].binding  = 0;
	layout.attribute_infos[ 1 ].location = 1;
	layout.attribute_infos[ 1 ].format   = Format::R32G32B32_SFLOAT;
	layout.attribute_infos[ 1 ].offset   = offsetof( struct Vertex, normal );
	layout.attribute_infos[ 2 ].binding  = 0;
	layout.attribute_infos[ 2 ].location = 2;
	layout.attribute_infos[ 2 ].format   = Format::R32G32_SFLOAT;
	layout.attribute_infos[ 2 ].offset   = offsetof( struct Vertex, tex_coord );
	pipeline_info.rasterizer_info.cull_mode    = CullMode::BACK;
	pipeline_info.rasterizer_info.front_face   = FrontFace::COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_info.polygon_mode = PolygonMode::FILL;
	pipeline_info.topology = PrimitiveTopology::TRIANGLE_LIST;
	pipeline_info.depth_state_info.compare_op  = CompareOp::LESS;
	pipeline_info.depth_state_info.depth_test  = true;
	pipeline_info.depth_state_info.depth_write = true;

	pipeline_info.color_attachment_count        = 3;
	pipeline_info.color_attachment_formats[ 0 ] = swapchain->format;
	pipeline_info.color_attachment_formats[ 1 ] = swapchain->format;
	pipeline_info.color_attachment_formats[ 2 ] = swapchain->format;
	pipeline_info.depth_stencil_format          = Format::D32_SFLOAT;

	create_graphics_pipeline( device, &pipeline_info, &data->pipeline );

	DescriptorSetInfo set_info {};
	set_info.set                   = 0;
	set_info.descriptor_set_layout = data->dsl;
	create_descriptor_set( device, &set_info, &data->set );

	BufferDescriptor buffer_descriptor {};
	buffer_descriptor.buffer = data->ubo_buffer;
	buffer_descriptor.offset = 0;
	buffer_descriptor.range  = sizeof( ShaderData );

	SamplerDescriptor sampler_descriptor {};
	sampler_descriptor.sampler = sampler;

	ImageDescriptor image_descriptor {};
	image_descriptor.image          = data->model_texture;
	image_descriptor.resource_state = ResourceState::SHADER_READ_ONLY;

	DescriptorWrite descriptor_writes[ 3 ]     = {};
	descriptor_writes[ 0 ].descriptor_name     = "global_ubo";
	descriptor_writes[ 0 ].descriptor_count    = 1;
	descriptor_writes[ 0 ].buffer_descriptors  = &buffer_descriptor;
	descriptor_writes[ 1 ].descriptor_name     = "u_sampler";
	descriptor_writes[ 1 ].descriptor_count    = 1;
	descriptor_writes[ 1 ].sampler_descriptors = &sampler_descriptor;
	descriptor_writes[ 2 ].descriptor_name     = "u_texture";
	descriptor_writes[ 2 ].descriptor_count    = 1;
	descriptor_writes[ 2 ].image_descriptors   = &image_descriptor;

	update_descriptor_set( device, data->set, 3, descriptor_writes );

	destroy_shader( device, shader );
}

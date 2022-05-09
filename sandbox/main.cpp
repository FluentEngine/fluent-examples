#include <functional>
#include <unordered_map>
#include "fluent/fluent.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "render_graph.hpp"

#include "shader_deffered_shading.vert.h"
#include "shader_deffered_shading.frag.h"
#include "shader_gbuffer.vert.h"
#include "shader_gbuffer.frag.h"

#define RG 1

using namespace fluent;

inline constexpr u32 FRAME_COUNT        = 2;
inline constexpr u32 VERTEX_BUFFER_SIZE = 50 * 1024 * 8;
inline constexpr u32 INDEX_BUFFER_SIZE  = 30 * 1024 * 8;
inline constexpr u32 MAX_MODEL_COUNT    = 100;
inline const char*   MODEL_TEXTURE      = "diablo.tga";
inline const char*   MODEL_NAME         = "diablo.obj";

u32 window_width  = 1400;
u32 window_height = 900;

struct FrameData
{
	Semaphore* present_semaphore;
	Semaphore* render_semaphore;
	Fence*     render_fence;

	CommandPool*   cmd_pool;
	CommandBuffer* cmd;
	bool           cmd_recorded = false;
};

struct Vertex
{
	Vector3 position;
	Vector3 normal;
	Vector2 tex_coord;

	bool
	operator==( const Vertex& other ) const
	{
		return position == other.position && normal == other.normal &&
		       tex_coord == other.tex_coord;
	}
};

template <typename T>
struct VertexHash;

template <>
struct VertexHash<Vertex>
{
	std::size_t
	operator()( Vertex const& vertex ) const
	{
		auto p = std::hash<glm::vec3>()( vertex.position );
		auto n = ( std::hash<glm::vec3>()( vertex.normal ) << 1 );
		auto t = std::hash<glm::vec2>()( vertex.tex_coord ) << 1;
		return ( ( p ^ n ) >> 1 ) ^ ( t );
	}
};

using VertexHasher = VertexHash<Vertex>;

struct Model
{
	u32 first_vertex;
	u32 first_index;
	u32 index_count;
};

struct ShaderData
{
	Matrix4 projection;
	Matrix4 view;
};

RendererBackend* backend;
Device*          device;
Queue*           graphics_queue;
Swapchain*       swapchain;
Image*           depth_image;
RenderPass**     render_passes;
FrameData        frames[ FRAME_COUNT ];
u32              frame_index = 0;

Sampler* sampler;
Image*   position_image;
Image*   normal_image;
Image*   albedo_spec_image;

RenderPass* gbuffer_pass;

Buffer* ubo_buffer;

u64     vertex_buffer_offset = 0;
Buffer* vertex_buffer;
u64     index_buffer_offset = 0;
Buffer* index_buffer;

Image*               model_texture;
Pipeline*            model_pipeline;
DescriptorSetLayout* model_dsl;
DescriptorSet*       model_set;

#if RG
struct DefferedShadingPassData
{
	Pipeline*            pipeline;
	DescriptorSetLayout* dsl;
	DescriptorSet*       set;

	UiContext* ui_context;
} deffered_shading_pass_data;
#else
Pipeline*            deffered_shading_pipeline;
DescriptorSetLayout* deffered_shading_dsl;
DescriptorSet*       deffered_shading_set;
#endif
Camera           camera;
CameraController camera_controller;

u32   model_count = 0;
Model models[ MAX_MODEL_COUNT ];

rg::RenderGraph render_graph;

void
load_model( std::vector<Vertex>& vertices, std::vector<u32>& indices )
{
	tinyobj::attrib_t                attrib;
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      warn, err;

	auto model_path = fs::get_models_directory() + MODEL_NAME;

	auto res = tinyobj::LoadObj( &attrib,
	                             &shapes,
	                             &materials,
	                             &warn,
	                             &err,
	                             model_path.c_str() );

	FT_ASSERT( res && "failed to load model" );

	std::unordered_map<Vertex, u32, VertexHasher> unique_vertices {};

	for ( const auto& shape : shapes )
	{
		for ( const auto& index : shape.mesh.indices )
		{
			Vertex vertex {};

			vertex.position = { attrib.vertices[ 3 * index.vertex_index + 0 ],
				                attrib.vertices[ 3 * index.vertex_index + 1 ],
				                attrib.vertices[ 3 * index.vertex_index + 2 ] };

			vertex.tex_coord = {
				attrib.texcoords[ 2 * index.texcoord_index + 0 ],
				1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ]
			};

			vertex.normal = { attrib.normals[ 3 * index.vertex_index + 0 ],
				              attrib.normals[ 3 * index.vertex_index + 1 ],
				              attrib.normals[ 3 * index.vertex_index + 2 ] };

			if ( unique_vertices.count( vertex ) == 0 )
			{
				unique_vertices[ vertex ] =
				    static_cast<uint32_t>( vertices.size() );
				vertices.push_back( vertex );
			}

			indices.push_back( unique_vertices[ vertex ] );
		}
	}
}

void
create_ubo_buffer()
{
	BufferInfo desc {};
	desc.descriptor_type = DescriptorType::UNIFORM_BUFFER;
	desc.memory_usage    = MemoryUsage::CPU_TO_GPU;
	desc.size            = sizeof( ShaderData );

	create_buffer( device, &desc, &ubo_buffer );
}

void
create_model_pipeline()
{
	Shader* shader;

	ShaderInfo shader_info {};
	shader_info.vertex.bytecode_size   = sizeof( shader_gbuffer_vert );
	shader_info.vertex.bytecode        = shader_gbuffer_vert;
	shader_info.fragment.bytecode_size = sizeof( shader_gbuffer_frag );
	shader_info.fragment.bytecode      = shader_gbuffer_frag;

	create_shader( device, &shader_info, &shader );

	create_descriptor_set_layout( device, shader, &model_dsl );

	PipelineInfo pipeline_info {};
	pipeline_info.shader                 = shader;
	pipeline_info.descriptor_set_layout  = model_dsl;
	pipeline_info.render_pass            = gbuffer_pass;
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

	create_graphics_pipeline( device, &pipeline_info, &model_pipeline );

	DescriptorSetInfo set_info {};
	set_info.set                   = 0;
	set_info.descriptor_set_layout = model_dsl;
	create_descriptor_set( device, &set_info, &model_set );

	BufferDescriptor buffer_descriptor {};
	buffer_descriptor.buffer = ubo_buffer;
	buffer_descriptor.offset = 0;
	buffer_descriptor.range  = sizeof( ShaderData );

	SamplerDescriptor sampler_descriptor {};
	sampler_descriptor.sampler = sampler;

	ImageDescriptor image_descriptor {};
	image_descriptor.image          = model_texture;
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

	update_descriptor_set( device, model_set, 3, descriptor_writes );

	destroy_shader( device, shader );
}

void
destroy_model_pipeline()
{
	destroy_descriptor_set( device, model_set );
	destroy_pipeline( device, model_pipeline );
	destroy_descriptor_set_layout( device, model_dsl );
}

void
create_depth_image()
{
	CommandBuffer* cmd = frames[ frame_index ].cmd;

	ImageInfo depth_image_info {};
	depth_image_info.width           = swapchain->width;
	depth_image_info.height          = swapchain->height;
	depth_image_info.depth           = 1;
	depth_image_info.format          = Format::D32_SFLOAT;
	depth_image_info.layer_count     = 1;
	depth_image_info.mip_levels      = 1;
	depth_image_info.sample_count    = SampleCount::E1;
	depth_image_info.descriptor_type = DescriptorType::DEPTH_STENCIL_ATTACHMENT;

	create_image( device, &depth_image_info, &depth_image );

	ImageBarrier barrier {};
	barrier.image     = depth_image;
	barrier.old_state = ResourceState::UNDEFINED;
	barrier.new_state = ResourceState::DEPTH_STENCIL_WRITE;
	barrier.src_queue = graphics_queue;
	barrier.dst_queue = graphics_queue;

	begin_command_buffer( cmd );
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
	end_command_buffer( cmd );
	immediate_submit( graphics_queue, cmd );
}

static inline RenderPassInfo
gbuffer_pass_info()
{
	RenderPassInfo render_pass_info {};
	render_pass_info.width                          = swapchain->width;
	render_pass_info.height                         = swapchain->height;
	render_pass_info.color_attachment_count         = 3;
	render_pass_info.color_attachments[ 0 ]         = position_image;
	render_pass_info.color_attachment_load_ops[ 0 ] = AttachmentLoadOp::CLEAR;
	render_pass_info.color_image_states[ 0 ] = ResourceState::COLOR_ATTACHMENT;
	render_pass_info.color_attachments[ 1 ]  = normal_image;
	render_pass_info.color_attachment_load_ops[ 1 ] = AttachmentLoadOp::CLEAR;
	render_pass_info.color_image_states[ 1 ] = ResourceState::COLOR_ATTACHMENT;
	render_pass_info.color_attachments[ 2 ]  = albedo_spec_image;
	render_pass_info.color_attachment_load_ops[ 2 ] = AttachmentLoadOp::CLEAR;
	render_pass_info.color_image_states[ 2 ] = ResourceState::COLOR_ATTACHMENT;
	render_pass_info.depth_stencil           = depth_image;
	render_pass_info.depth_stencil_load_op   = AttachmentLoadOp::CLEAR;
	render_pass_info.depth_stencil_state = ResourceState::DEPTH_STENCIL_WRITE;

	return render_pass_info;
}

void
create_default_sampler()
{
	SamplerInfo sampler_info {};
	sampler_info.mipmap_mode = SamplerMipmapMode::LINEAR;
	sampler_info.min_lod     = 0;
	sampler_info.max_lod     = 1000;

	create_sampler( device, &sampler_info, &sampler );
}

void
create_attachment_images()
{
	ImageInfo desc {};
	desc.width        = swapchain->width;
	desc.height       = swapchain->height;
	desc.depth        = 1;
	desc.format       = swapchain->format;
	desc.layer_count  = 1;
	desc.mip_levels   = 1;
	desc.sample_count = SampleCount::E1;
	desc.descriptor_type =
	    DescriptorType::COLOR_ATTACHMENT | DescriptorType::SAMPLED_IMAGE;

	create_image( device, &desc, &position_image );
	create_image( device, &desc, &normal_image );
	create_image( device, &desc, &albedo_spec_image );
}

void
create_gbuffer_pass()
{
	auto desc = gbuffer_pass_info();
	create_render_pass( device, &desc, &gbuffer_pass );
}

void
draw_models()
{
	auto* shader_data = static_cast<ShaderData*>(
	    ResourceLoader::begin_upload_buffer( ubo_buffer ) );
	shader_data->view       = camera.get_view_matrix();
	shader_data->projection = camera.get_projection_matrix();
	ResourceLoader::end_upload_buffer( ubo_buffer );

	CommandBuffer* cmd = frames[ frame_index ].cmd;
	cmd_set_viewport( cmd,
	                  0,
	                  0,
	                  swapchain->width,
	                  swapchain->height,
	                  0.0f,
	                  1.0f );
	cmd_set_scissor( cmd, 0, 0, swapchain->width, swapchain->height );
	cmd_bind_pipeline( cmd, model_pipeline );
	cmd_bind_vertex_buffer( cmd, vertex_buffer, 0 );
	cmd_bind_index_buffer_u32( cmd, index_buffer, 0 );
	cmd_bind_descriptor_set( cmd, 0, model_set, model_pipeline );

	for ( u32 i = 0; i < model_count; ++i )
	{
		const auto& m = models[ i ];
		cmd_draw_indexed( cmd,
		                  m.index_count,
		                  1,
		                  m.first_index,
		                  m.first_vertex,
		                  1 );
	}
}

void
execute_gbuffer_pass()
{
	CommandBuffer* cmd = frames[ frame_index ].cmd;

	RenderPassBeginInfo pass_begin_info {};
	pass_begin_info.render_pass                  = gbuffer_pass;
	pass_begin_info.clear_values[ 0 ].color[ 0 ] = 0.2f;
	pass_begin_info.clear_values[ 0 ].color[ 1 ] = 0.3f;
	pass_begin_info.clear_values[ 0 ].color[ 2 ] = 0.4f;
	pass_begin_info.clear_values[ 0 ].color[ 3 ] = 1.0f;
	pass_begin_info.clear_values[ 1 ].color[ 0 ] = 0.2f;
	pass_begin_info.clear_values[ 1 ].color[ 1 ] = 0.3f;
	pass_begin_info.clear_values[ 1 ].color[ 2 ] = 0.4f;
	pass_begin_info.clear_values[ 1 ].color[ 3 ] = 1.0f;
	pass_begin_info.clear_values[ 2 ].color[ 0 ] = 0.2f;
	pass_begin_info.clear_values[ 2 ].color[ 1 ] = 0.3f;
	pass_begin_info.clear_values[ 2 ].color[ 2 ] = 0.4f;
	pass_begin_info.clear_values[ 2 ].color[ 3 ] = 1.0f;
	pass_begin_info.clear_values[ 3 ].depth      = 1.0f;
	pass_begin_info.clear_values[ 3 ].stencil    = 0;

	ImageBarrier barrier {};
	barrier.old_state = ResourceState::UNDEFINED;
	barrier.new_state = ResourceState::COLOR_ATTACHMENT;
	barrier.src_queue = graphics_queue;
	barrier.dst_queue = graphics_queue;

	barrier.image = position_image;
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
	barrier.image = normal_image;
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
	barrier.image = albedo_spec_image;
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );

	cmd_begin_render_pass( cmd, &pass_begin_info );
	draw_models();
	cmd_end_render_pass( cmd );

	barrier.old_state = ResourceState::COLOR_ATTACHMENT;
	barrier.new_state = ResourceState::SHADER_READ_ONLY;

	barrier.image = position_image;
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
	barrier.image = normal_image;
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
	barrier.image = albedo_spec_image;
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
}

void
on_init()
{
	fs::set_shaders_directory( "shaders/sandbox/" );
	fs::set_textures_directory( "../../sandbox/" );
	fs::set_models_directory( "../../sandbox/" );

	RendererBackendInfo backend_info {};
	backend_info.api = RendererAPI::VULKAN;
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

	create_depth_image();

	ResourceLoader::init( device, 30 * 1024 * 1024 * 8 );

	BufferInfo buffer_info {};
	buffer_info.descriptor_type = DescriptorType::VERTEX_BUFFER;
	buffer_info.memory_usage    = MemoryUsage::GPU_ONLY;
	buffer_info.size            = VERTEX_BUFFER_SIZE;
	create_buffer( device, &buffer_info, &vertex_buffer );
	buffer_info.descriptor_type = DescriptorType::INDEX_BUFFER;
	buffer_info.memory_usage    = MemoryUsage::GPU_ONLY;
	buffer_info.size            = INDEX_BUFFER_SIZE;
	create_buffer( device, &buffer_info, &index_buffer );

	std::vector<Vertex> vertices;
	std::vector<u32>    indices;
	load_model( vertices, indices );

	u64       size;
	void*     data;
	ImageInfo image_info =
	    fs::read_image_data( ( fs::get_textures_directory() + MODEL_TEXTURE ),
	                         false,
	                         &size,
	                         &data );
	image_info.format          = Format::R8G8B8A8_SRGB;
	image_info.descriptor_type = DescriptorType::SAMPLED_IMAGE;
	create_image( device, &image_info, &model_texture );

	ResourceLoader::begin_recording();
	ResourceLoader::upload_buffer( vertex_buffer,
	                               vertex_buffer_offset,
	                               vertices.size() * sizeof( vertices[ 0 ] ),
	                               vertices.data() );
	ResourceLoader::upload_buffer( index_buffer,
	                               index_buffer_offset,
	                               indices.size() * sizeof( indices[ 0 ] ),
	                               indices.data() );
	ResourceLoader::upload_image( model_texture, size, data );
	ResourceLoader::end_recording();

	fs::release_image_data( data );

	auto& model        = models[ model_count++ ];
	model.index_count  = indices.size();
	model.first_index  = index_buffer_offset / sizeof( u32 );
	model.first_vertex = vertex_buffer_offset / sizeof( Vertex );

	vertex_buffer_offset += vertices.size() * sizeof( vertices[ 0 ] );
	index_buffer_offset += indices.size() * sizeof( indices[ 0 ] );

	create_default_sampler();
	create_attachment_images();
	create_gbuffer_pass();

	create_ubo_buffer();
	create_model_pipeline();

	render_graph.init( device );

	auto* pass = render_graph.add_pass( "deffered_shading" );
	pass->set_color_clear_value( 0, Vector4( 0.2, 0.3, 0.4, 1.0 ) );
	pass->set_user_data( &deffered_shading_pass_data );
	pass->set_create_callback(
	    []( RenderPass* render_pass, void* user_data )
	    {
		    auto* data = static_cast<DefferedShadingPassData*>( user_data );

		    // TODO:
		    if ( data->pipeline )
		    {
			    return;
		    }

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
		    pipeline_info.render_pass                  = render_pass;
		    pipeline_info.rasterizer_info.cull_mode    = CullMode::NONE;
		    pipeline_info.rasterizer_info.polygon_mode = PolygonMode::FILL;
		    pipeline_info.topology = PrimitiveTopology::TRIANGLE_STRIP;

		    create_graphics_pipeline( device, &pipeline_info, &data->pipeline );

		    DescriptorSetInfo set_info {};
		    set_info.set                   = 0;
		    set_info.descriptor_set_layout = data->dsl;
		    create_descriptor_set( device, &set_info, &data->set );

		    destroy_shader( device, shader );

		    // update sets

		    SamplerDescriptor sampler_descriptor {};
		    sampler_descriptor.sampler = sampler;
		    ImageDescriptor position_descriptor {};
		    position_descriptor.image = position_image;
		    position_descriptor.resource_state =
		        ResourceState::SHADER_READ_ONLY;
		    ImageDescriptor normal_descriptor {};
		    normal_descriptor.image          = normal_image;
		    normal_descriptor.resource_state = ResourceState::SHADER_READ_ONLY;
		    ImageDescriptor albedo_spec_descriptor {};
		    albedo_spec_descriptor.image = albedo_spec_image;
		    albedo_spec_descriptor.resource_state =
		        ResourceState::SHADER_READ_ONLY;

		    DescriptorWrite descriptor_writes[ 4 ]     = {};
		    descriptor_writes[ 0 ].descriptor_name     = "u_sampler";
		    descriptor_writes[ 0 ].descriptor_count    = 1;
		    descriptor_writes[ 0 ].sampler_descriptors = &sampler_descriptor;
		    descriptor_writes[ 1 ].descriptor_name     = "u_position";
		    descriptor_writes[ 1 ].descriptor_count    = 1;
		    descriptor_writes[ 1 ].image_descriptors   = &position_descriptor;
		    descriptor_writes[ 2 ].descriptor_name     = "u_normal";
		    descriptor_writes[ 2 ].descriptor_count    = 1;
		    descriptor_writes[ 3 ].descriptor_name     = "u_albedo_spec";
		    descriptor_writes[ 2 ].image_descriptors   = &normal_descriptor;
		    descriptor_writes[ 3 ].descriptor_count    = 1;
		    descriptor_writes[ 3 ].image_descriptors = &albedo_spec_descriptor;

		    update_descriptor_set( device, data->set, 4, descriptor_writes );
	    } );

	pass->set_execute_callback(
	    []( CommandBuffer* cmd, void* user_data )
	    {
		    auto* data = static_cast<DefferedShadingPassData*>( user_data );

		    cmd_set_viewport( cmd,
		                      0,
		                      0,
		                      swapchain->width,
		                      swapchain->height,
		                      0.0f,
		                      1.0f );
		    cmd_set_scissor( cmd, 0, 0, swapchain->width, swapchain->height );
		    cmd_bind_pipeline( cmd, data->pipeline );
		    cmd_bind_descriptor_set( cmd, 0, data->set, data->pipeline );
		    cmd_draw( cmd, 4, 1, 0, 0 );
	    } );

	pass->set_destroy_callback(
	    []( void* user_data )
	    {
		    auto* data = static_cast<DefferedShadingPassData*>( user_data );
		    // TODO
		    if ( data->pipeline == nullptr )
		    {
			    return;
		    }
		    destroy_descriptor_set( device, data->set );
		    destroy_pipeline( device, data->pipeline );
		    destroy_descriptor_set_layout( device, data->dsl );
	    } );
	render_graph.build();

	CameraInfo camera_info {};
	camera_info.aspect      = window_get_aspect( get_app_window() );
	camera_info.near        = 0.1f;
	camera_info.far         = 1000.0f;
	camera_info.position    = Vector3( 0.0f, 0.0f, 3.0f );
	camera_info.direction   = Vector3( 0.0f, 0.0f, -1.0f );
	camera_info.up          = Vector3( 0.0f, 1.0f, 0.0f );
	camera_info.speed       = 10.0f;
	camera_info.sensitivity = 0.12f;

	camera.init_camera( camera_info );
	camera_controller.init( camera );
}

void
on_update( f32 delta_time )
{
	if ( is_key_pressed( Key::LeftAlt ) )
	{
		camera_controller.update( delta_time );
	}

	if ( !frames[ frame_index ].cmd_recorded )
	{
		wait_for_fences( device, 1, &frames[ frame_index ].render_fence );
		reset_fences( device, 1, &frames[ frame_index ].render_fence );
		frames[ frame_index ].cmd_recorded = true;
	}

	u32 image_index = 0;
	acquire_next_image( device,
	                    swapchain,
	                    frames[ frame_index ].present_semaphore,
	                    nullptr,
	                    &image_index );

	CommandBuffer* cmd = frames[ frame_index ].cmd;

	begin_command_buffer( cmd );
	execute_gbuffer_pass();
	render_graph.execute( cmd, swapchain->images[ image_index ] );
	end_command_buffer( cmd );

	QueueSubmitInfo submit_info {};
	submit_info.wait_semaphore_count = 1;
	submit_info.wait_semaphores      = &frames[ frame_index ].present_semaphore;
	submit_info.command_buffer_count = 1;
	submit_info.command_buffers      = &cmd;
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
on_resize( u32 width, u32 height )
{
	// TODO:
}

void
on_shutdown()
{
	queue_wait_idle( graphics_queue );

	render_graph.shutdown();

	destroy_model_pipeline();

	destroy_buffer( device, ubo_buffer );

	destroy_image( device, model_texture );
	destroy_buffer( device, vertex_buffer );
	destroy_buffer( device, index_buffer );

	ResourceLoader::shutdown();

	destroy_render_pass( device, gbuffer_pass );
	destroy_image( device, albedo_spec_image );
	destroy_image( device, normal_image );
	destroy_image( device, position_image );
	destroy_sampler( device, sampler );

	destroy_image( device, depth_image );

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
	window_info.resizable  = true;
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

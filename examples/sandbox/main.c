#include "common.h"

#include "renderer/scene/model_loader.h"

#include "main.vert.h"
#include "main.frag.h"

#define WINDOW_WIDTH  600
#define WINDOW_HEIGHT 480

#define MODEL_PATH "../examples/sandbox/dragon/DragonAttenuation.gltf"

#define VERTEX_BUFFER_SIZE 10 * 1024 * 1024 * 8
#define INDEX_BUFFER_SIZE  5 * 1024 * 1024 * 8
#define MAX_GEOMETRY_COUNT 100

struct ShaderData
{
	mat4x4 projection;
	mat4x4 view;
};

struct Vertex
{
	vec3 position;
	vec3 normal;
	vec2 texcoord;
};

struct Geometry
{
	i32    first_vertex;
	u32    first_index;
	u32    index_count;
	b32    is_32;
	mat4x4 model;
};

static struct Pipeline*            pipeline = NULL;
static struct DescriptorSetLayout* dsl      = NULL;

static struct DescriptorSet* set = NULL;

static struct Buffer*  vertex_buffer   = NULL;
static struct Buffer*  index_buffer_32 = NULL;
static struct Buffer*  index_buffer_16 = NULL;
static struct Buffer*  ubo_buffer      = NULL;
static struct Sampler* sampler         = NULL;

static struct Camera           camera;
static struct CameraController camera_controller;

static struct ShaderData shader_data;

u32                    geometry_count = 0;
static struct Geometry geometries[ MAX_GEOMETRY_COUNT ];
static struct Buffer*  transforms_buffer;

static void
write_descriptors();
static void
load_scene();

static void updata_camera_ubo( f32 );
static void
draw_scene( struct CommandBuffer* );

static void
on_init(void)
{
	init_renderer();

	vec3 position  = { 0.0f, 0.0f, 3.0f };
	vec3 direction = { 0.0f, 0.0f, -1.0f };
	vec3 up        = { 0.0f, 1.0f, 0.0f };

	struct CameraInfo camera_info = { 0 };
	camera_info.fov               = radians( 45.0f );
	camera_info.aspect            = window_get_aspect( get_app_window() );
	camera_info.near              = 0.1f;
	camera_info.far               = 1000.0f;
	vec3_dup( camera_info.position, position );
	vec3_dup( camera_info.direction, direction );
	vec3_dup( camera_info.up, up );
	camera_info.speed       = 5.0f;
	camera_info.sensitivity = 0.12f;

	camera_init( &camera, &camera_info );
	camera_controller_init( &camera_controller, &camera );

	mat4x4_dup( shader_data.projection, camera.projection );

	struct ShaderInfo shader_info      = { 0 };
	shader_info.vertex.bytecode_size   = sizeof( shader_main_vert );
	shader_info.vertex.bytecode        = shader_main_vert;
	shader_info.fragment.bytecode_size = sizeof( shader_main_frag );
	shader_info.fragment.bytecode      = shader_main_frag;

	struct Shader* shader;
	create_shader( device, &shader_info, &shader );

	create_descriptor_set_layout( device, shader, &dsl );

	struct PipelineInfo  pipeline_info = { 0 };
	struct VertexLayout* l             = &pipeline_info.vertex_layout;
	l->binding_info_count              = 1;
	l->binding_infos[ 0 ].binding      = 0;
	l->binding_infos[ 0 ].input_rate   = FT_VERTEX_INPUT_RATE_VERTEX;
	l->binding_infos[ 0 ].stride       = sizeof( struct Vertex );
	l->attribute_info_count            = 3;
	l->attribute_infos[ 0 ].binding    = 0;
	l->attribute_infos[ 0 ].format     = FT_FORMAT_R32G32B32_SFLOAT;
	l->attribute_infos[ 0 ].location   = 0;
	l->attribute_infos[ 0 ].offset     = offsetof( struct Vertex, position );
	l->attribute_infos[ 1 ].binding    = 0;
	l->attribute_infos[ 1 ].format     = FT_FORMAT_R32G32B32_SFLOAT;
	l->attribute_infos[ 1 ].location   = 1;
	l->attribute_infos[ 1 ].offset     = offsetof( struct Vertex, normal );
	l->attribute_infos[ 2 ].binding    = 0;
	l->attribute_infos[ 2 ].format     = FT_FORMAT_R32G32_SFLOAT;
	l->attribute_infos[ 2 ].location   = 2;
	l->attribute_infos[ 2 ].offset     = offsetof( struct Vertex, texcoord );

	pipeline_info.shader                      = shader;
	pipeline_info.rasterizer_info.cull_mode   = FT_CULL_MODE_NONE;
	pipeline_info.rasterizer_info.front_face  = FT_FRONT_FACE_COUNTER_CLOCKWISE;
	pipeline_info.depth_state_info.depth_test = 0;
	pipeline_info.depth_state_info.depth_write  = 0;
	pipeline_info.descriptor_set_layout         = dsl;
	pipeline_info.sample_count                  = 1;
	pipeline_info.color_attachment_count        = 1;
	pipeline_info.color_attachment_formats[ 0 ] = swapchain->format;
	pipeline_info.depth_stencil_format          = depth_image->format;
	pipeline_info.depth_state_info.compare_op   = FT_COMPARE_OP_LESS;
	pipeline_info.depth_state_info.depth_test   = 1;
	pipeline_info.depth_state_info.depth_write  = 1;
	pipeline_info.topology = FT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	create_graphics_pipeline( device, &pipeline_info, &pipeline );

	destroy_shader( device, shader );

	struct BufferInfo buffer_info = {
		.descriptor_type = FT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.memory_usage    = FT_MEMORY_USAGE_CPU_TO_GPU,
		.size            = sizeof( struct ShaderData ),
	};

	create_buffer( device, &buffer_info, &ubo_buffer );

	load_scene();
	write_descriptors();
}

static void
on_update( f32 delta_time )
{
	updata_camera_ubo( delta_time );

	begin_frame();

	struct CommandBuffer* cmd = frames[ frame_index ].cmd;

	begin_command_buffer( cmd );

	struct ImageBarrier barriers[ 2 ] = { 0 };
	barriers[ 0 ].image               = swapchain->images[ image_index ];
	barriers[ 0 ].old_state           = FT_RESOURCE_STATE_UNDEFINED;
	barriers[ 0 ].new_state           = FT_RESOURCE_STATE_COLOR_ATTACHMENT;
	barriers[ 1 ].image               = depth_image;
	barriers[ 1 ].old_state           = FT_RESOURCE_STATE_UNDEFINED;
	barriers[ 1 ].new_state           = FT_RESOURCE_STATE_DEPTH_STENCIL_WRITE;

	struct RenderPassBeginInfo rp_info     = { 0 };
	rp_info.device                         = device;
	rp_info.width                          = swapchain->width;
	rp_info.height                         = swapchain->height;
	rp_info.color_attachment_count         = 1;
	rp_info.color_attachments[ 0 ]         = swapchain->images[ image_index ];
	rp_info.color_attachment_load_ops[ 0 ] = FT_ATTACHMENT_LOAD_OP_CLEAR;
	rp_info.color_image_states[ 0 ]        = FT_RESOURCE_STATE_COLOR_ATTACHMENT;
	rp_info.depth_stencil                  = depth_image;
	rp_info.depth_stencil_state   = FT_RESOURCE_STATE_DEPTH_STENCIL_WRITE;
	rp_info.depth_stencil_load_op = FT_ATTACHMENT_LOAD_OP_CLEAR;
	rp_info.clear_values[ 0 ].color[ 0 ]            = 0.38f;
	rp_info.clear_values[ 0 ].color[ 1 ]            = 0.30f;
	rp_info.clear_values[ 0 ].color[ 2 ]            = 0.35f;
	rp_info.clear_values[ 0 ].color[ 3 ]            = 1.0f;
	rp_info.clear_values[ 1 ].depth_stencil.depth   = 1.0f;
	rp_info.clear_values[ 1 ].depth_stencil.stencil = 0;

	cmd_barrier( cmd, 0, NULL, 0, NULL, 2, barriers );

	cmd_begin_render_pass( cmd, &rp_info );
	draw_scene( cmd );
	cmd_end_render_pass( cmd );

	struct ImageBarrier barrier = {
		.image     = swapchain->images[ image_index ],
		.old_state = FT_RESOURCE_STATE_COLOR_ATTACHMENT,
		.new_state = FT_RESOURCE_STATE_PRESENT,
	};

	cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &barrier );

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
on_shutdown(void)
{
	queue_wait_idle( graphics_queue );
	unmap_memory( device, transforms_buffer );
	destroy_buffer( device, transforms_buffer );
	destroy_buffer( device, index_buffer_32 );
	destroy_buffer( device, index_buffer_16 );
	destroy_buffer( device, vertex_buffer );
	destroy_sampler( device, sampler );
	destroy_buffer( device, ubo_buffer );
	destroy_descriptor_set( device, set );
	destroy_descriptor_set_layout( device, dsl );
	destroy_pipeline( device, pipeline );
	shutdown_renderer();
}

int
main( int argc, char** argv )
{
	struct WindowInfo window_info = { 0 };
	window_info.title             = "fluent-sandbox";
	window_info.x                 = 100;
	window_info.y                 = 100;
	window_info.width             = WINDOW_WIDTH;
	window_info.height            = WINDOW_HEIGHT;
	window_info.resizable         = 0;
	window_info.centered          = 1;
	window_info.fullscreen        = 0;
	window_info.grab_mouse        = 0;

	struct ApplicationConfig config = { 0 };
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

static void
write_descriptors()
{
	struct DescriptorSetInfo set_info = {
		.descriptor_set_layout = dsl,
		.set                   = 0,
	};

	create_descriptor_set( device, &set_info, &set );

	struct BufferDescriptor buffer_descriptor = {
		.buffer = ubo_buffer,
		.offset = 0,
		.range  = sizeof( struct ShaderData ),
	};

	struct BufferDescriptor tbuffer_descriptor = {
		.buffer = transforms_buffer,
		.offset = 0,
		.range  = MAX_GEOMETRY_COUNT * sizeof( mat4x4 ),
	};

	struct DescriptorWrite descriptor_writes[ 2 ];
	memset( descriptor_writes, 0, sizeof( descriptor_writes ) );
	descriptor_writes[ 0 ].buffer_descriptors  = &buffer_descriptor;
	descriptor_writes[ 0 ].descriptor_count    = 1;
	descriptor_writes[ 0 ].descriptor_name     = "ubo";
	descriptor_writes[ 0 ].image_descriptors   = NULL;
	descriptor_writes[ 0 ].sampler_descriptors = NULL;

	descriptor_writes[ 1 ].buffer_descriptors  = NULL;
	descriptor_writes[ 1 ].descriptor_count    = 1;
	descriptor_writes[ 1 ].descriptor_name     = "u_transforms";
	descriptor_writes[ 1 ].buffer_descriptors  = &tbuffer_descriptor;
	descriptor_writes[ 1 ].sampler_descriptors = NULL;

	update_descriptor_set( device, set, 2, descriptor_writes );
}

static void
load_scene()
{
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
	
	struct BufferInfo buffer_info = {
		.descriptor_type = FT_DESCRIPTOR_TYPE_VERTEX_BUFFER,
		.memory_usage    = FT_MEMORY_USAGE_CPU_TO_GPU,
		.size            = VERTEX_BUFFER_SIZE,
	};

	create_buffer( device, &buffer_info, &vertex_buffer );

	buffer_info = ( struct BufferInfo ) {
		.descriptor_type = FT_DESCRIPTOR_TYPE_INDEX_BUFFER,
		.memory_usage    = FT_MEMORY_USAGE_CPU_TO_GPU,
		.size            = INDEX_BUFFER_SIZE,
	};

	create_buffer( device, &buffer_info, &index_buffer_16 );
	create_buffer( device, &buffer_info, &index_buffer_32 );

	buffer_info = ( struct BufferInfo ) {
		.descriptor_type = FT_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.memory_usage    = FT_MEMORY_USAGE_CPU_TO_GPU,
		.size            = sizeof( mat4x4 ) * MAX_GEOMETRY_COUNT,
	};

	create_buffer( device, &buffer_info, &transforms_buffer );

	map_memory( device, transforms_buffer );
	map_memory( device, vertex_buffer );
	map_memory( device, index_buffer_16 );
	map_memory( device, index_buffer_32 );

	struct Model model = load_gltf( MODEL_PATH );

	geometry_count = model.mesh_count;

	u32 first_vertex   = 0;
	u32 first_index_32 = 0;
	u32 first_index_16 = 0;

	for ( u32 m = 0; m < geometry_count; ++m )
	{
		const struct Mesh* mesh = &model.meshes[ m ];

		geometries[ m ].index_count  = mesh->index_count;
		geometries[ m ].first_vertex = first_vertex;

		struct Vertex* vertices = vertex_buffer->mapped_memory;
		vertices += first_vertex;

		u32 p = 0;

		for ( u32 v = 0; v < mesh->vertex_count; ++v )
		{
			vertices[ v ].position[ 0 ] = mesh->positions[ p ];
			vertices[ v ].position[ 1 ] = mesh->positions[ p + 1 ];
			vertices[ v ].position[ 2 ] = mesh->positions[ p + 2 ];

			vertices[ v ].normal[ 0 ] = mesh->normals[ p ];
			vertices[ v ].normal[ 1 ] = mesh->normals[ p + 1 ];
			vertices[ v ].normal[ 2 ] = mesh->normals[ p + 2 ];

			p += 3;
		}

		if ( model.meshes[ m ].is_32bit_indices )
		{
			geometries[ m ].first_index = first_index_32;
			geometries[ m ].is_32       = 1;

			u32* dst = index_buffer_32->mapped_memory;
			memcpy( &dst[ first_index_32 ],
			        mesh->indices,
			        mesh->index_count * sizeof( u32 ) );

			first_index_32 += mesh->index_count;
		}
		else
		{
			geometries[ m ].first_index = first_index_16;

			u16* dst = index_buffer_16->mapped_memory;
			memcpy( &dst[ first_index_16 ],
			        mesh->indices,
			        mesh->index_count * sizeof( u16 ) );

			first_index_16 += mesh->index_count;
		}

		first_vertex += mesh->vertex_count;

		mat4x4_dup( geometries[ m ].model, mesh->world );
	}

	free_model( &model );

	unmap_memory( device, vertex_buffer );
	unmap_memory( device, index_buffer_32 );
	unmap_memory( device, index_buffer_16 );
}

static void
updata_camera_ubo( f32 delta_time )
{
	if ( is_key_pressed( FT_KEY_LEFT_ALT ) )
	{
		camera_controller_update( &camera_controller, delta_time );
	}
	else
	{
		camera_controller_reset( &camera_controller );
	}

	mat4x4_dup( shader_data.view, camera.view );
	mat4x4_dup( shader_data.projection, camera.projection );

	map_memory( device, ubo_buffer );
	memcpy( ubo_buffer->mapped_memory,
	        &shader_data,
	        sizeof( struct ShaderData ) );
	unmap_memory( device, ubo_buffer );
}

static void
draw_scene( struct CommandBuffer* cmd )
{
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
	cmd_bind_vertex_buffer( cmd, vertex_buffer, 0 );
	for ( u32 i = 0; i < geometry_count; ++i )
	{
		mat4x4* t = transforms_buffer->mapped_memory;
		mat4x4_dup( t[ i ], geometries[ i ].model );

		cmd_push_constants( cmd, pipeline, 0, sizeof( u32 ), &i );

		if ( geometries[ i ].is_32 )
		{
			cmd_bind_index_buffer_u32( cmd, index_buffer_32, 0 );

			cmd_draw_indexed( cmd,
			                  geometries[ i ].index_count,
			                  1,
			                  geometries[ i ].first_index,
			                  geometries[ i ].first_vertex,
			                  0 );
		}
		else
		{
			cmd_bind_index_buffer_u16( cmd, index_buffer_16, 0 );

			cmd_draw_indexed( cmd,
			                  geometries[ i ].index_count,
			                  1,
			                  geometries[ i ].first_index,
			                  geometries[ i ].first_vertex,
			                  0 );
		}
	}
}

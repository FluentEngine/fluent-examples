#include <fluent/os.h>
#include <fluent/renderer.h>
#include "main.vert.h"
#include "main.frag.h"
#include "main_pass.h"

#define MODEL_PATH         MODEL_FOLDER "/BoxAnimated/glTF/BoxAnimated.gltf"
#define VERTEX_BUFFER_SIZE 10 * 1024 * 1024 * 8
#define INDEX_BUFFER_SIZE  10 * 1024 * 1024 * 8
#define MAX_DRAW_COUNT     5

struct Vertex
{
	vec3 position;
	vec3 normal;
	vec2 texcoord;
	vec4 joints;
	vec4 weights;
};

struct DrawData
{
	i32 first_vertex;
	u32 first_index;
	u32 index_count;
};

struct ShaderData
{
	mat4x4 projection;
	mat4x4 view;
};

struct MainPassData
{
	const struct Camera* camera;

	u32                         width;
	u32                         height;
	enum Format                 swapchain_format;
	struct DescriptorSetLayout* dsl;
	struct Pipeline*            pipeline;
	struct Buffer*              vertex_buffer;
	struct Buffer*              index_buffer;
	struct Buffer*              ubo_buffer;
	struct Buffer*              transforms_buffer;
	struct DescriptorSet*       set;

	struct Model model;

	struct ShaderData shader_data;
	u32               draw_count;
	struct DrawData   draws[ MAX_DRAW_COUNT ];

	struct Timer       timer;
	struct nk_context* ui;
} main_pass_data;

static inline void
main_pass_create_pipeline( const struct Device* device,
                           struct MainPassData* data )
{
	struct ShaderInfo shader_info = {
	    .vertex   = get_main_vert_shader( device->api ),
	    .fragment = get_main_frag_shader( device->api ),
	};

	struct Shader* shader;
	create_shader( device, &shader_info, &shader );

	create_descriptor_set_layout( device, shader, &data->dsl );

	struct PipelineInfo info = {
	    .type                  = FT_PIPELINE_TYPE_GRAPHICS,
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
	    .color_attachment_formats[ 0 ] = data->swapchain_format,
	    .depth_stencil_format          = FT_FORMAT_D32_SFLOAT,
	    .vertex_layout =
	        {
	            .binding_info_count            = 1,
	            .binding_infos[ 0 ].binding    = 0,
	            .binding_infos[ 0 ].input_rate = FT_VERTEX_INPUT_RATE_VERTEX,
	            .binding_infos[ 0 ].stride     = sizeof( struct Vertex ),
	            .attribute_info_count          = 3,
	            .attribute_infos[ 0 ].binding  = 0,
	            .attribute_infos[ 0 ].format   = FT_FORMAT_R32G32B32_SFLOAT,
	            .attribute_infos[ 0 ].location = 0,
	            .attribute_infos[ 0 ].offset =
	                offsetof( struct Vertex, position ),
	            .attribute_infos[ 1 ].binding  = 0,
	            .attribute_infos[ 1 ].format   = FT_FORMAT_R32G32B32_SFLOAT,
	            .attribute_infos[ 1 ].location = 1,
	            .attribute_infos[ 1 ].offset =
	                offsetof( struct Vertex, normal ),
	            .attribute_infos[ 2 ].binding  = 0,
	            .attribute_infos[ 2 ].format   = FT_FORMAT_R32G32_SFLOAT,
	            .attribute_infos[ 2 ].location = 2,
	            .attribute_infos[ 2 ].offset =
	                offsetof( struct Vertex, texcoord ),
	            .attribute_infos[ 3 ].binding  = 0,
	            .attribute_infos[ 3 ].format   = FT_FORMAT_R32G32B32A32_SFLOAT,
	            .attribute_infos[ 3 ].location = 3,
	            .attribute_infos[ 3 ].offset =
	                offsetof( struct Vertex, joints ),
	            .attribute_infos[ 4 ].binding  = 0,
	            .attribute_infos[ 4 ].format   = FT_FORMAT_R32G32B32A32_SFLOAT,
	            .attribute_infos[ 4 ].location = 4,
	            .attribute_infos[ 4 ].offset =
	                offsetof( struct Vertex, weights ),
	        },
	};

	create_pipeline( device, &info, &data->pipeline );

	destroy_shader( device, shader );
}

static inline void
main_pass_create_buffers( const struct Device* device,
                          struct MainPassData* data )
{
	struct BufferInfo info;
	info.memory_usage    = FT_MEMORY_USAGE_GPU_ONLY;
	info.descriptor_type = FT_DESCRIPTOR_TYPE_VERTEX_BUFFER;
	info.size            = VERTEX_BUFFER_SIZE;
	create_buffer( device, &info, &data->vertex_buffer );
	info.descriptor_type = FT_DESCRIPTOR_TYPE_INDEX_BUFFER;
	info.size            = INDEX_BUFFER_SIZE;
	create_buffer( device, &info, &data->index_buffer );
	info.descriptor_type = FT_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	info.memory_usage    = FT_MEMORY_USAGE_CPU_TO_GPU;
	info.size            = sizeof( struct ShaderData );
	create_buffer( device, &info, &data->ubo_buffer );
	info.descriptor_type = FT_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	info.size            = sizeof( mat4x4 ) * MAX_DRAW_COUNT;
	create_buffer( device, &info, &data->transforms_buffer );
}

static inline void
main_pass_load_scene( const struct Device* device, struct MainPassData* data )
{
	data->model = load_gltf( MODEL_PATH );

	data->draw_count = data->model.mesh_count;

	u32 first_vertex = 0;
	u32 first_index  = 0;

	begin_upload_batch();

	for ( u32 m = 0; m < data->draw_count; ++m )
	{
		const struct Mesh* mesh = &data->model.meshes[ m ];

		data->draws[ m ].index_count  = mesh->index_count;
		data->draws[ m ].first_vertex = first_vertex;

		struct Vertex* vertices =
		    malloc( sizeof( struct Vertex ) * mesh->vertex_count );

		for ( u32 v = 0; v < mesh->vertex_count; ++v )
		{
			memcpy( vertices[ v ].position,
			        &mesh->positions[ v * 3 ],
			        3 * sizeof( f32 ) );

			if ( mesh->normals )
			{
				memcpy( vertices[ v ].normal,
				        &mesh->normals[ v * 3 ],
				        3 * sizeof( f32 ) );
			}

			if ( mesh->texcoords )
			{
				memcpy( vertices[ v ].texcoord,
				        &mesh->texcoords[ v * 2 ],
				        2 * sizeof( f32 ) );
			}

			if ( mesh->joints )
			{
				memcpy( vertices[ v ].joints,
				        &mesh->joints[ v * 4 ],
				        4 * sizeof( f32 ) );
			}

			if ( mesh->weights )
			{
				memcpy( vertices[ v ].weights,
				        &mesh->joints[ v * 4 ],
				        4 * sizeof( f32 ) );
			}
		}

		data->draws[ m ].first_index = first_index;

		upload_buffer( data->vertex_buffer,
		               first_vertex * sizeof( struct Vertex ),
		               sizeof( struct Vertex ) * mesh->vertex_count,
		               vertices );
		upload_buffer( data->index_buffer,
		               first_index * sizeof( u16 ),
		               mesh->index_count * sizeof( u16 ),
		               mesh->indices );

		first_index += mesh->index_count;
		first_vertex += mesh->vertex_count;
	}

	end_upload_batch();
}

static inline void
main_pass_create_descriptor_sets( const struct Device* device,
                                  struct MainPassData* data )
{
	struct DescriptorSetInfo set_info = {
	    .descriptor_set_layout = data->dsl,
	    .set                   = 0,
	};

	create_descriptor_set( device, &set_info, &data->set );
}

static inline void
main_pass_write_descriptors( const struct Device* device,
                             struct MainPassData* data )
{
	struct BufferDescriptor buffer_descriptor = {
	    .buffer = data->ubo_buffer,
	    .offset = 0,
	    .range  = sizeof( struct ShaderData ),
	};

	struct BufferDescriptor tbuffer_descriptor = {
	    .buffer = data->transforms_buffer,
	    .offset = 0,
	    .range  = MAX_DRAW_COUNT * sizeof( mat4x4 ),
	};

	struct DescriptorWrite descriptor_writes[ 2 ] = {
	    [0] =
	        {
	            .buffer_descriptors  = &buffer_descriptor,
	            .descriptor_count    = 1,
	            .descriptor_name     = "ubo",
	            .image_descriptors   = NULL,
	            .sampler_descriptors = NULL,
	        },
	    [1] =
	        {
	            .buffer_descriptors  = NULL,
	            .descriptor_count    = 1,
	            .descriptor_name     = "u_transforms",
	            .buffer_descriptors  = &tbuffer_descriptor,
	            .sampler_descriptors = NULL,
	        },
	};
	update_descriptor_set( device, data->set, 2, descriptor_writes );
}

static inline void
main_pass_update_ubo( const struct Device* device, struct MainPassData* data )
{
	mat4x4_dup( data->shader_data.view, data->camera->view );
	mat4x4_dup( data->shader_data.projection, data->camera->projection );

	u8* dst = map_memory( device, data->ubo_buffer );
	memcpy( dst, &data->shader_data, sizeof( struct ShaderData ) );
	unmap_memory( device, data->ubo_buffer );
}

static void
main_pass_create( const struct Device* device, void* user_data )
{
	struct MainPassData* data = user_data;
	main_pass_create_pipeline( device, data );
	main_pass_create_buffers( device, data );
	main_pass_load_scene( device, data );
	main_pass_create_descriptor_sets( device, data );
	main_pass_write_descriptors( device, data );
}

static void
apply_animation( mat4x4                  r,
                 float                   current_time,
                 const struct Animation* animation )
{
	current_time = fmod( current_time, animation->duration );

	vec3 translation = { 0.0f, 0.0f, 0.0f };
	quat rotation    = { 0.0f, 0.0f, 0.0f, -1.0f };
	vec3 scale       = { 1.0f, 1.0f, 1.0f };

	for ( u32 ch = 0; ch < animation->channel_count; ++ch )
	{
		struct AnimationChannel* channel = &animation->channels[ ch ];
		struct AnimationSampler* sampler = channel->sampler;

		float previous_time = 0;
		float next_time     = 0;

		u32 previous_frame = 0;
		u32 next_frame     = 0;

		for ( u32 f = 0; f < sampler->frame_count - 1; f++ )
		{
			if ( sampler->times[ f + 1 ] > current_time )
			{
				previous_time  = sampler->times[ f ];
				next_time      = sampler->times[ f + 1 ];
				previous_frame = f;
				next_frame     = f + 1;
				break;
			}
		}

		float interpolation_value =
		    ( current_time - previous_time ) / ( next_time - previous_time );

		switch ( channel->transform_type )
		{
		case FT_TRANSFORM_TYPE_TRANSLATION:
		{
			vec3* translations = ( vec3* ) sampler->values;
			vec3_lerp( translation,
			           translations[ previous_frame ],
			           translations[ next_frame ],
			           interpolation_value );
			break;
		}
		case FT_TRANSFORM_TYPE_ROTATION:
		{
			quat* quats = ( quat* ) sampler->values;
			slerp( rotation,
			       quats[ previous_frame ],
			       quats[ next_frame ],
			       interpolation_value );
			break;
		}
		}
	}

	mat4x4_compose( r, translation, rotation, scale );
}

static void
main_pass_draw_ui( struct CommandBuffer* cmd, struct MainPassData* data )
{
	static struct Timer fps_timer;
	static b32          first_time = 1;
	static u64          frames     = 0;
	static f64          fps        = 0;

	if ( first_time )
	{
		timer_reset( &fps_timer );
		first_time = 0;
	}

	nk_ft_new_frame();
	if ( nk_begin( data->ui,
	               "Debug Menu",
	               nk_rect( 0, 0, 200, data->width ),
	               NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR |
	                   NK_WINDOW_NO_INPUT | NK_WINDOW_NOT_INTERACTIVE ) )
	{
		char fps_str[ 30 ];
		sprintf( fps_str, "FPS: %.04f", fps );
		nk_layout_row_static( data->ui, 20, 100, 1 );
		nk_label( data->ui, fps_str, NK_TEXT_ALIGN_LEFT );
	}
	nk_end( data->ui );
	nk_ft_render( cmd, NK_ANTI_ALIASING_ON );

	frames++;

	if ( frames > 5 )
	{
		fps = frames / ( ( ( f64 ) timer_get_ticks( &fps_timer ) ) / 1000.0f );
		frames = 0;
		timer_reset( &fps_timer );
	}
}

static void
main_pass_execute( const struct Device*  device,
                   struct CommandBuffer* cmd,
                   void*                 user_data )
{
	struct MainPassData* data = user_data;

	main_pass_update_ubo( device, data );

	cmd_set_scissor( cmd, 0, 0, data->width, data->height );
	cmd_set_viewport( cmd, 0, 0, data->width, data->height, 0, 1.0f );

	cmd_bind_pipeline( cmd, data->pipeline );
	cmd_bind_descriptor_set( cmd, 0, data->set, data->pipeline );
	cmd_bind_vertex_buffer( cmd, data->vertex_buffer, 0 );
	cmd_bind_index_buffer( cmd, data->index_buffer, 0, FT_INDEX_TYPE_U16 );

	for ( u32 i = 0; i < data->draw_count; ++i )
	{
		struct DrawData* draw = &data->draws[ i ];

		mat4x4* transforms = map_memory( device, data->transforms_buffer );
		mat4x4_dup( transforms[ i ], data->model.meshes[ i ].world );

		for ( u32 a = 0; a < data->model.animation_count; ++a )
		{
			if ( i == 1 )
			{
				apply_animation(
				    transforms[ i ],
				    ( ( f32 ) ( timer_get_ticks( &data->timer ) ) ) / 1000.0f,
				    &data->model.animations[ a ] );
			}
		}

		unmap_memory( device, data->transforms_buffer );

		cmd_push_constants( cmd, data->pipeline, 0, sizeof( u32 ), &i );
		cmd_draw_indexed( cmd,
		                  draw->index_count,
		                  1,
		                  draw->first_index,
		                  draw->first_vertex,
		                  0 );
	}

	main_pass_draw_ui( cmd, data );
}

static void
main_pass_destroy( const struct Device* device, void* user_data )
{
	struct MainPassData* data = user_data;
	destroy_descriptor_set( device, data->set );
	free_gltf( &data->model );
	destroy_buffer( device, data->transforms_buffer );
	destroy_buffer( device, data->ubo_buffer );
	destroy_buffer( device, data->index_buffer );
	destroy_buffer( device, data->vertex_buffer );
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

void
register_main_pass( struct RenderGraph*     graph,
                    const struct Swapchain* swapchain,
                    const char*             backbuffer_source_name,
                    const struct Camera*    camera,
                    struct nk_context*      ui )
{
	main_pass_data.width            = swapchain->width;
	main_pass_data.height           = swapchain->height;
	main_pass_data.swapchain_format = swapchain->format;
	main_pass_data.camera           = camera;
	main_pass_data.ui               = ui;
	timer_reset( &main_pass_data.timer );

	struct RenderPass* pass;
	rg_add_pass( graph, "main", &pass );
	rg_set_user_data( pass, &main_pass_data );
	rg_set_pass_create_callback( pass, main_pass_create );
	rg_set_pass_execute_callback( pass, main_pass_execute );
	rg_set_pass_destroy_callback( pass, main_pass_destroy );
	rg_set_get_clear_color( pass, main_pass_get_clear_color );
	rg_set_get_clear_depth_stencil( pass, main_pass_get_clear_depth_stencil );

	struct ImageInfo back;
	rg_add_color_output( pass, backbuffer_source_name, &back );
	struct ImageInfo depth_image = {
	    .width        = main_pass_data.width,
	    .height       = main_pass_data.height,
	    .depth        = 1,
	    .format       = FT_FORMAT_D32_SFLOAT,
	    .layer_count  = 1,
	    .mip_levels   = 1,
	    .sample_count = 1,
	};
	rg_add_depth_stencil_output( pass, "depth", &depth_image );
}

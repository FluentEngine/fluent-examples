#include <fluent/os.h>
#include <fluent/renderer.h>
#include "main.vert.h"
#include "main.frag.h"
#include "main_pass.h"

#define MODEL_PATH         MODEL_FOLDER "/DamagedHelmet/glTF/DamagedHelmet.gltf"
#define VERTEX_BUFFER_SIZE 20 * 1024 * 1024 * 8
#define INDEX_BUFFER_SIZE  20 * 1024 * 1024 * 8
#define MAX_DRAW_COUNT     200

struct vertex
{
	float3 position;
	float3 normal;
	float3 tangent;
	float2 texcoord;
};

struct shader_data
{
	float4x4 projection;
	float4x4 view;
	float4   view_pos;
};

struct push_constant
{
	uint32_t instance_id;
	int32_t  textures[ FT_TEXTURE_TYPE_COUNT ];
	int32_t  pad[ 2 ];
	float4   base_color_factor;
};

struct draw_data
{
	int32_t                   first_vertex;
	uint32_t                  first_index;
	uint32_t                  index_count;
	enum ft_index_type        index_type;
	struct ft_descriptor_set* material_set;
	struct push_constant      push_constant;
};

struct main_pass_data
{
	const struct ft_camera* camera;

	uint32_t                         width;
	uint32_t                         height;
	enum ft_format                   swapchain_format;
	struct ft_descriptor_set_layout* dsl;
	struct ft_pipeline*              pipeline;
	struct ft_buffer*                vertex_buffer;
	struct ft_buffer*                index_buffer;
	struct ft_buffer*                ubo_buffer;
	struct ft_buffer*                transforms_buffer;
	struct ft_descriptor_set*        set;

	struct ft_model model;

	struct shader_data shader_data;
	uint32_t           draw_count;
	struct draw_data   draws[ MAX_DRAW_COUNT ];
	uint32_t           model_image_count;
	struct ft_sampler* sampler;
	struct ft_image**  model_images;
	struct ft_image*   unbound_image;

	struct ft_timer timer;
} main_pass_data;

FT_INLINE void
main_pass_create_pipeline( const struct ft_device* device,
                           struct main_pass_data*  data )
{
	struct ft_shader_info shader_info = {
	    .vertex   = get_main_vert_shader( device->api ),
	    .fragment = get_main_frag_shader( device->api ),
	};

	struct ft_shader* shader;
	ft_create_shader( device, &shader_info, &shader );

	ft_create_descriptor_set_layout( device, shader, &data->dsl );

	struct ft_pipeline_info info = {
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
	            .binding_infos[ 0 ].stride     = sizeof( struct vertex ),
	            .attribute_info_count          = 4,
	            .attribute_infos[ 0 ].binding  = 0,
	            .attribute_infos[ 0 ].format   = FT_FORMAT_R32G32B32_SFLOAT,
	            .attribute_infos[ 0 ].location = 0,
	            .attribute_infos[ 0 ].offset =
	                offsetof( struct vertex, position ),
	            .attribute_infos[ 1 ].binding  = 0,
	            .attribute_infos[ 1 ].format   = FT_FORMAT_R32G32B32_SFLOAT,
	            .attribute_infos[ 1 ].location = 1,
	            .attribute_infos[ 1 ].offset =
	                offsetof( struct vertex, normal ),
	            .attribute_infos[ 2 ].binding  = 0,
	            .attribute_infos[ 2 ].format   = FT_FORMAT_R32G32B32_SFLOAT,
	            .attribute_infos[ 2 ].location = 2,
	            .attribute_infos[ 2 ].offset =
	                offsetof( struct vertex, tangent ),
	            .attribute_infos[ 3 ].binding  = 0,
	            .attribute_infos[ 3 ].format   = FT_FORMAT_R32G32_SFLOAT,
	            .attribute_infos[ 3 ].location = 3,
	            .attribute_infos[ 3 ].offset =
	                offsetof( struct vertex, texcoord ),
	        },
	};

	ft_create_pipeline( device, &info, &data->pipeline );

	ft_destroy_shader( device, shader );
}

FT_INLINE void
main_pass_create_buffers( const struct ft_device* device,
                          struct main_pass_data*  data )
{
	struct ft_buffer_info info;
	info.memory_usage    = FT_MEMORY_USAGE_GPU_ONLY;
	info.descriptor_type = FT_DESCRIPTOR_TYPE_VERTEX_BUFFER;
	info.size            = VERTEX_BUFFER_SIZE;
	ft_create_buffer( device, &info, &data->vertex_buffer );
	info.descriptor_type = FT_DESCRIPTOR_TYPE_INDEX_BUFFER;
	info.size            = INDEX_BUFFER_SIZE;
	ft_create_buffer( device, &info, &data->index_buffer );
	info.descriptor_type = FT_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	info.memory_usage    = FT_MEMORY_USAGE_CPU_TO_GPU;
	info.size            = sizeof( struct shader_data );
	ft_create_buffer( device, &info, &data->ubo_buffer );
	info.descriptor_type = FT_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	info.size            = sizeof( float4x4 ) * MAX_DRAW_COUNT;
	ft_create_buffer( device, &info, &data->transforms_buffer );
}

FT_INLINE void
main_pass_create_sampler( const struct ft_device* device,
                          struct main_pass_data*  data )
{
	struct ft_sampler_info info = {
	    .mag_filter        = FT_FILTER_LINEAR,
	    .min_filter        = FT_FILTER_LINEAR,
	    .mipmap_mode       = FT_SAMPLER_MIPMAP_MODE_LINEAR,
	    .address_mode_u    = FT_SAMPLER_ADDRESS_MODE_REPEAT,
	    .address_mode_v    = FT_SAMPLER_ADDRESS_MODE_REPEAT,
	    .address_mode_w    = FT_SAMPLER_ADDRESS_MODE_REPEAT,
	    .mip_lod_bias      = 0,
	    .anisotropy_enable = 0,
	    .max_anisotropy    = 16,
	    .compare_enable    = 0,
	    .compare_op        = FT_COMPARE_OP_ALWAYS,
	    .min_lod           = 0,
	    .max_lod           = 16,
	};

	ft_create_sampler( device, &info, &data->sampler );
}

FT_INLINE void
main_pass_create_unbound_resources( const struct ft_device* device,
                                    struct main_pass_data*  data )
{
	float4 image_data[ 4 ];
	memset( image_data, 0, sizeof( image_data ) );

	struct ft_image_info info = {
	    .width           = 2,
	    .height          = 2,
	    .depth           = 1,
	    .format          = FT_FORMAT_R8G8B8A8_SRGB,
	    .sample_count    = 1,
	    .layer_count     = 1,
	    .mip_levels      = 1,
	    .descriptor_type = FT_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
	};

	ft_create_image( device, &info, &data->unbound_image );
	ft_upload_image( data->unbound_image, sizeof( image_data ), image_data );
}

FT_INLINE void
main_pass_load_scene( const struct ft_device* device,
                      struct main_pass_data*  data )
{
	data->model = ft_load_gltf( MODEL_PATH, FT_MODEL_GENERATE_TANGENTS );

	data->draw_count = data->model.mesh_count;

	uint32_t first_vertex = 0;
	uint32_t first_index  = 0;

	data->model_image_count = data->model.texture_count;
	if ( data->model_image_count != 0 )
	{
		data->model_images =
		    calloc( data->model.texture_count, sizeof( struct ft_image* ) );
	}

	for ( uint32_t t = 0; t < data->model.texture_count; ++t )
	{
		struct ft_texture* texture = &data->model.textures[ t ];

		struct ft_image_info image_info = {
		    .width           = texture->width,
		    .height          = texture->height,
		    .depth           = 1,
		    .format          = FT_FORMAT_R8G8B8A8_UNORM,
		    .sample_count    = 1,
		    .layer_count     = 1,
		    .mip_levels      = 1,
		    .descriptor_type = FT_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		};

		ft_create_image( device, &image_info, &data->model_images[ t ] );

		ft_upload_image( data->model_images[ t ],
		                 texture->width * texture->height * 4,
		                 texture->data );
	}

	ft_begin_upload_batch();

	for ( uint32_t m = 0; m < data->draw_count; ++m )
	{
		const struct ft_mesh* mesh = &data->model.meshes[ m ];
		struct push_constant* pc   = &data->draws[ m ].push_constant;

		data->draws[ m ].index_count  = mesh->index_count;
		data->draws[ m ].first_vertex = first_vertex;

		struct vertex* vertices =
		    malloc( sizeof( struct vertex ) * mesh->vertex_count );

		for ( uint32_t v = 0; v < mesh->vertex_count; ++v )
		{
			memcpy( vertices[ v ].position,
			        &mesh->positions[ v * 3 ],
			        3 * sizeof( float ) );

			if ( mesh->normals )
			{
				memcpy( vertices[ v ].normal,
				        &mesh->normals[ v * 3 ],
				        3 * sizeof( float ) );
			}

			if ( mesh->tangents )
			{
				memcpy( vertices[ v ].tangent,
				        &mesh->tangents[ v * 3 ],
				        3 * sizeof( float ) );
			}

			if ( mesh->texcoords )
			{
				memcpy( vertices[ v ].texcoord,
				        &mesh->texcoords[ v * 2 ],
				        2 * sizeof( float ) );
			}
		}

		data->draws[ m ].first_index = first_index;

		ft_upload_buffer( data->vertex_buffer,
		                  first_vertex * sizeof( struct vertex ),
		                  sizeof( struct vertex ) * mesh->vertex_count,
		                  vertices );
		ft_upload_buffer( data->index_buffer,
		                  first_index * sizeof( uint16_t ),
		                  mesh->index_count * sizeof( uint16_t ),
		                  mesh->indices );

		first_index += mesh->index_count;
		first_vertex += mesh->vertex_count;

		free( vertices );

		pc->instance_id = m;

		struct ft_descriptor_set_info set_info = {
		    .set                   = 1,
		    .descriptor_set_layout = data->dsl,
		};

		struct ft_descriptor_set* set;
		ft_create_descriptor_set( device, &set_info, &set );

		struct ft_sampler_descriptor sampler_descriptor = {
		    .sampler = data->sampler,
		};

		struct ft_image_descriptor image_descriptors[ FT_TEXTURE_TYPE_COUNT ];
		memset( image_descriptors, 0, sizeof( image_descriptors ) );
		for ( uint32_t i = 0; i < FT_TEXTURE_TYPE_COUNT; ++i )
		{
			image_descriptors[ i ].resource_state =
			    FT_RESOURCE_STATE_SHADER_READ_ONLY;
			if ( mesh->material.textures[ i ] != -1 )
			{
				image_descriptors[ i ].image =
				    data->model_images[ mesh->material.textures[ i ] ];
				pc->textures[ i ] = i;
			}
			else
			{
				image_descriptors[ i ].image = data->unbound_image;
				pc->textures[ i ]            = -1;
			}
		}

		float4_dup( pc->base_color_factor, mesh->material.base_color_factor );

		struct ft_descriptor_write descriptor_writes[ 2 ];
		memset( descriptor_writes, 0, sizeof( descriptor_writes ) );
		descriptor_writes[ 0 ].descriptor_count    = 1;
		descriptor_writes[ 0 ].descriptor_name     = "u_sampler";
		descriptor_writes[ 0 ].sampler_descriptors = &sampler_descriptor;
		descriptor_writes[ 1 ].descriptor_count    = FT_TEXTURE_TYPE_COUNT;
		descriptor_writes[ 1 ].descriptor_name     = "u_textures";
		descriptor_writes[ 1 ].image_descriptors   = image_descriptors;

		ft_update_descriptor_set( device, set, 2, descriptor_writes );

		data->draws[ m ].material_set = set;
	}

	ft_end_upload_batch();
}

FT_INLINE void
main_pass_create_descriptor_sets( const struct ft_device* device,
                                  struct main_pass_data*  data )
{
	struct ft_descriptor_set_info set_info = {
	    .descriptor_set_layout = data->dsl,
	    .set                   = 0,
	};

	ft_create_descriptor_set( device, &set_info, &data->set );
}

FT_INLINE void
main_pass_write_descriptors( const struct ft_device* device,
                             struct main_pass_data*  data )
{
	struct ft_buffer_descriptor buffer_descriptor = {
	    .buffer = data->ubo_buffer,
	    .offset = 0,
	    .range  = sizeof( struct shader_data ),
	};

	struct ft_buffer_descriptor tbuffer_descriptor = {
	    .buffer = data->transforms_buffer,
	    .offset = 0,
	    .range  = MAX_DRAW_COUNT * sizeof( float4x4 ),
	};

	struct ft_descriptor_write descriptor_writes[ 2 ] = {
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
	ft_update_descriptor_set( device, data->set, 2, descriptor_writes );
}

FT_INLINE void
main_pass_update_ubo( const struct ft_device* device,
                      struct main_pass_data*  data )
{
	float4x4_dup( data->shader_data.view, data->camera->view );
	float4x4_dup( data->shader_data.projection, data->camera->projection );
	float3_dup( data->shader_data.view_pos, data->camera->position );

	uint8_t* dst = ft_map_memory( device, data->ubo_buffer );
	memcpy( dst, &data->shader_data, sizeof( struct shader_data ) );
	ft_unmap_memory( device, data->ubo_buffer );
}

static void
main_pass_create( const struct ft_device* device, void* user_data )
{
	struct main_pass_data* data = user_data;
	main_pass_create_pipeline( device, data );
	main_pass_create_buffers( device, data );
	main_pass_create_sampler( device, data );
	main_pass_create_unbound_resources( device, data );
	main_pass_load_scene( device, data );
	main_pass_create_descriptor_sets( device, data );
	main_pass_write_descriptors( device, data );
}

static void
main_pass_execute( const struct ft_device*   device,
                   struct ft_command_buffer* cmd,
                   void*                     user_data )
{
	struct main_pass_data* data = user_data;

	main_pass_update_ubo( device, data );

	ft_cmd_set_scissor( cmd, 0, 0, data->width, data->height );
	ft_cmd_set_viewport( cmd, 0, 0, data->width, data->height, 0, 1.0f );

	ft_cmd_bind_pipeline( cmd, data->pipeline );
	ft_cmd_bind_descriptor_set( cmd, 0, data->set, data->pipeline );
	ft_cmd_bind_vertex_buffer( cmd, data->vertex_buffer, 0 );
	ft_cmd_bind_index_buffer( cmd, data->index_buffer, 0, FT_INDEX_TYPE_U16 );

	float4x4* transforms = ft_map_memory( device, data->transforms_buffer );

	for ( uint32_t i = 0; i < data->draw_count; ++i )
	{
		float4x4_dup( transforms[ i ], data->model.meshes[ i ].world );
	}

	for ( uint32_t a = 0; a < data->model.animation_count; ++a )
	{
		struct ft_animation* animation    = &data->model.animations[ a ];
		float                current_time = ft_timer_get_ticks( &data->timer );
		current_time /= 1000.0f;
		apply_animation( transforms, current_time, animation );
	}

	ft_unmap_memory( device, data->transforms_buffer );

	for ( uint32_t i = 0; i < data->draw_count; ++i )
	{
		struct draw_data* draw = &data->draws[ i ];

		ft_cmd_push_constants( cmd,
		                       data->pipeline,
		                       0,
		                       sizeof( struct push_constant ),
		                       &draw->push_constant );

		ft_cmd_bind_descriptor_set( cmd,
		                            1,
		                            draw->material_set,
		                            data->pipeline );

		ft_cmd_draw_indexed( cmd,
		                     draw->index_count,
		                     1,
		                     draw->first_index,
		                     draw->first_vertex,
		                     0 );
	}
}

static void
main_pass_destroy( const struct ft_device* device, void* user_data )
{
	struct main_pass_data* data = user_data;
	ft_destroy_descriptor_set( device, data->set );
	for ( uint32_t i = 0; i < data->draw_count; i++ )
	{
		if ( data->draws[ i ].material_set )
		{
			ft_destroy_descriptor_set( device, data->draws[ i ].material_set );
		}
	}

	for ( uint32_t i = 0; i < data->model.texture_count; i++ )
	{
		ft_destroy_image( device, data->model_images[ i ] );
	}
	ft_safe_free( data->model_images );
	ft_destroy_image( device, data->unbound_image );
	ft_destroy_sampler( device, data->sampler );
	ft_free_gltf( &data->model );
	ft_destroy_buffer( device, data->transforms_buffer );
	ft_destroy_buffer( device, data->ubo_buffer );
	ft_destroy_buffer( device, data->index_buffer );
	ft_destroy_buffer( device, data->vertex_buffer );
	ft_destroy_pipeline( device, data->pipeline );
	ft_destroy_descriptor_set_layout( device, data->dsl );
}

static bool
main_pass_get_clear_color( uint32_t idx, ft_color_clear_value* color )
{
	switch ( idx )
	{
	case 0:
	{
		( *color )[ 0 ] = 0.1f;
		( *color )[ 1 ] = 0.2f;
		( *color )[ 2 ] = 0.3f;
		( *color )[ 3 ] = 1.0f;
		return true;
	}
	default: return false;
	}
}

static bool
main_pass_get_clear_depth_stencil(
    struct ft_depth_stencil_clear_value* depth_stencil )
{
	depth_stencil->depth   = 1.0f;
	depth_stencil->stencil = 0;

	return true;
}

void
register_main_pass( struct ft_render_graph*    graph,
                    const struct ft_swapchain* swapchain,
                    const char*                backbuffer_source_name,
                    const struct ft_camera*    camera )
{
	main_pass_data.width            = swapchain->width;
	main_pass_data.height           = swapchain->height;
	main_pass_data.swapchain_format = swapchain->format;
	main_pass_data.camera           = camera;
	ft_timer_reset( &main_pass_data.timer );

	struct ft_render_pass* pass;
	ft_rg_add_pass( graph, "main", &pass );
	ft_rg_set_user_data( pass, &main_pass_data );
	ft_rg_set_pass_create_callback( pass, main_pass_create );
	ft_rg_set_pass_execute_callback( pass, main_pass_execute );
	ft_rg_set_pass_destroy_callback( pass, main_pass_destroy );
	ft_rg_set_get_clear_color( pass, main_pass_get_clear_color );
	ft_rg_set_get_clear_depth_stencil( pass,
	                                   main_pass_get_clear_depth_stencil );

	struct ft_image_info back;
	ft_rg_add_color_output( pass, backbuffer_source_name, &back );
	struct ft_image_info depth_image = {
	    .width        = main_pass_data.width,
	    .height       = main_pass_data.height,
	    .depth        = 1,
	    .format       = FT_FORMAT_D32_SFLOAT,
	    .layer_count  = 1,
	    .mip_levels   = 1,
	    .sample_count = 1,
	};
	ft_rg_add_depth_stencil_output( pass, "depth", &depth_image );
}

#include <fluent/os.h>
#include <fluent/renderer.h>

#include "ui_pass.h"
#include "main_pass.h"
#include "eq_to_cubemap.comp.h"
#include "brdf.comp.h"
#include "irradiance.comp.h"
#include "specular.comp.h"

#define FRAME_COUNT   2
#define WINDOW_WIDTH  1400
#define WINDOW_HEIGHT 900

struct frame_data
{
	struct ft_semaphore* present_semaphore;
	struct ft_semaphore* render_semaphore;
	struct ft_fence*     render_fence;

	struct ft_command_pool*   cmd_pool;
	struct ft_command_buffer* cmd;
	bool                      cmd_recorded;
};

struct app_data
{
	enum ft_renderer_api        renderer_api;
	struct ft_renderer_backend* backend;
	struct ft_device*           device;
	struct ft_queue*            graphics_queue;
	struct ft_swapchain*        swapchain;
	struct frame_data           frames[ FRAME_COUNT ];
	uint32_t                    frame_index;
	uint32_t                    image_index;

	struct ft_render_graph* graph;

	struct ft_camera            camera;
	struct ft_camera_controller camera_controller;

	struct nk_context*    ctx;
	struct nk_font_atlas* atlas;

	struct pbr_maps pbr;
};

static void
init_renderer( struct app_data* );
static void
shutdown_renderer( struct app_data* );

static void
begin_frame( struct app_data* );
static void
end_frame( struct app_data* );

static void
compute_pbr_maps( struct app_data* );
static void
free_pbr_maps( struct app_data* );

static void
on_init( const struct ft_application_callback_data* cb_data )
{
	struct app_data* app = cb_data->user_data;

	struct ft_camera_info camera_info = {
	    .fov         = radians( 45.0f ),
	    .aspect      = ft_window_get_aspect( ft_get_app_window() ),
	    .near        = 0.1f,
	    .far         = 1000.0f,
	    .speed       = 5.0f,
	    .sensitivity = 0.12f,
	    .position    = { 0.0f, 0.0f, 3.0f },
	    .direction   = { 0.0f, 0.0f, -1.0f },
	    .up          = { 0.0f, 1.0f, 0.0f },
	};

	ft_camera_init( &app->camera, &camera_info );
	ft_camera_controller_init( &app->camera_controller, &app->camera );

	init_renderer( app );

	app->ctx = nk_ft_init( ft_get_wsi_info(),
	                       app->device,
	                       app->graphics_queue,
	                       app->swapchain->format,
	                       FT_FORMAT_UNDEFINED );

	nk_ft_font_stash_begin( &app->atlas );
	nk_ft_font_stash_end();

	compute_pbr_maps( app );

	ft_rg_create( app->device, &app->graph );
	register_main_pass( app->graph,
	                    app->swapchain,
	                    "back",
	                    &app->camera,
	                    &app->pbr );
	register_ui_pass( app->graph, app->swapchain, "back", app->ctx );
	ft_rg_set_backbuffer_source( app->graph, "back" );
	ft_rg_set_swapchain_dimensions( app->graph,
	                                app->swapchain->width,
	                                app->swapchain->height );
	ft_rg_build( app->graph );
}

static void
on_update( const struct ft_application_callback_data* cb_data )
{
	struct app_data* app = cb_data->user_data;

	if ( ft_is_key_pressed( FT_KEY_LEFT_ALT ) )
	{
		ft_camera_controller_update( &app->camera_controller,
		                             cb_data->delta_time );
	}
	else
	{
		ft_camera_controller_reset( &app->camera_controller );
	}

	begin_frame( app );

	struct ft_command_buffer* cmd = app->frames[ app->frame_index ].cmd;
	ft_begin_command_buffer( cmd );

	ft_rg_setup_attachments( app->graph,
	                         app->swapchain->images[ app->image_index ] );
	ft_rg_execute( cmd, app->graph );

	ft_end_command_buffer( cmd );

	end_frame( app );
}

static void
on_resize( const struct ft_application_callback_data* cb_data )
{
	struct app_data* app = cb_data->user_data;

	ft_queue_wait_idle( app->graphics_queue );
	ft_resize_swapchain( app->device,
	                     app->swapchain,
	                     cb_data->width,
	                     cb_data->height );

	ft_rg_set_swapchain_dimensions( app->graph,
	                                app->swapchain->width,
	                                app->swapchain->height );
	ft_rg_build( app->graph );
}

static void
on_shutdown( const struct ft_application_callback_data* cb_data )
{
	struct app_data* app = cb_data->user_data;
	ft_queue_wait_idle( app->graphics_queue );
	ft_rg_destroy( app->graph );
	free_pbr_maps( app );
	nk_ft_shutdown();
	shutdown_renderer( app );
}

int
main( int argc, char** argv )
{
	struct app_data data = {
	    .renderer_api = FT_RENDERER_API_VULKAN,
	};

	struct ft_window_info window_info = {
	    .title        = "fluent-sandbox",
	    .x            = 100,
	    .y            = 100,
	    .width        = WINDOW_WIDTH,
	    .height       = WINDOW_HEIGHT,
	    .resizable    = 0,
	    .centered     = 1,
	    .fullscreen   = 0,
	    .grab_mouse   = 0,
	    .renderer_api = data.renderer_api,
	};

	struct ft_application_info app_info = {
	    .argc        = argc,
	    .argv        = argv,
	    .window_info = window_info,
	    .log_level   = FT_LOG_LEVEL_TRACE,
	    .on_init     = on_init,
	    .on_update   = on_update,
	    .on_resize   = on_resize,
	    .on_shutdown = on_shutdown,
	    .user_data   = &data,
	};

	ft_app_init( &app_info );
	ft_app_run();
	ft_app_shutdown();

	return EXIT_SUCCESS;
}

static void
init_renderer( struct app_data* app )
{
	struct ft_renderer_backend_info backend_info = {
	    .api      = app->renderer_api,
	    .wsi_info = ft_get_wsi_info(),
	};

	ft_create_renderer_backend( &backend_info, &app->backend );

	struct ft_device_info device_info = {
	    device_info.backend = app->backend,
	};
	ft_create_device( app->backend, &device_info, &app->device );

	struct ft_queue_info queue_info = {
	    queue_info.queue_type = FT_QUEUE_TYPE_GRAPHICS,
	};
	ft_create_queue( app->device, &queue_info, &app->graphics_queue );

	for ( uint32_t i = 0; i < FRAME_COUNT; i++ )
	{
		app->frames[ i ].cmd_recorded = 0;
		ft_create_semaphore( app->device, &app->frames[ i ].present_semaphore );
		ft_create_semaphore( app->device, &app->frames[ i ].render_semaphore );
		ft_create_fence( app->device, &app->frames[ i ].render_fence );

		struct ft_command_pool_info pool_info = {
		    .queue = app->graphics_queue,
		};

		ft_create_command_pool( app->device,
		                        &pool_info,
		                        &app->frames[ i ].cmd_pool );
		ft_create_command_buffers( app->device,
		                           app->frames[ i ].cmd_pool,
		                           1,
		                           &app->frames[ i ].cmd );
	}

	struct ft_swapchain_info swapchain_info = {
	    .width  = ft_window_get_framebuffer_width( ft_get_app_window() ),
	    .height = ft_window_get_framebuffer_height( ft_get_app_window() ),
	    .format = FT_FORMAT_B8G8R8A8_SRGB,
	    .min_image_count = FRAME_COUNT,
	    .vsync           = 1,
	    .queue           = app->graphics_queue,
	    .wsi_info        = ft_get_wsi_info(),
	};
	ft_create_swapchain( app->device, &swapchain_info, &app->swapchain );
}

static void
shutdown_renderer( struct app_data* app )
{
	ft_queue_wait_idle( app->graphics_queue );

	ft_destroy_swapchain( app->device, app->swapchain );

	for ( uint32_t i = 0; i < FRAME_COUNT; i++ )
	{
		ft_destroy_command_buffers( app->device,
		                            app->frames[ i ].cmd_pool,
		                            1,
		                            &app->frames[ i ].cmd );
		ft_destroy_command_pool( app->device, app->frames[ i ].cmd_pool );
		ft_destroy_fence( app->device, app->frames[ i ].render_fence );
		ft_destroy_semaphore( app->device, app->frames[ i ].render_semaphore );
		ft_destroy_semaphore( app->device, app->frames[ i ].present_semaphore );
	}

	ft_destroy_queue( app->graphics_queue );
	ft_destroy_device( app->device );
	ft_destroy_renderer_backend( app->backend );
}

static void
begin_frame( struct app_data* app )
{
	if ( !app->frames[ app->frame_index ].cmd_recorded )
	{
		ft_wait_for_fences( app->device,
		                    1,
		                    &app->frames[ app->frame_index ].render_fence );
		ft_reset_fences( app->device,
		                 1,
		                 &app->frames[ app->frame_index ].render_fence );
		app->frames[ app->frame_index ].cmd_recorded = 1;
	}

	ft_acquire_next_image( app->device,
	                       app->swapchain,
	                       app->frames[ app->frame_index ].present_semaphore,
	                       NULL,
	                       &app->image_index );
}

static void
end_frame( struct app_data* app )
{
	struct ft_queue_submit_info submit_info = {
	    .wait_semaphore_count = 1,
	    .wait_semaphores = &app->frames[ app->frame_index ].present_semaphore,
	    .command_buffer_count   = 1,
	    .command_buffers        = &app->frames[ app->frame_index ].cmd,
	    .signal_semaphore_count = 1,
	    .signal_semaphores = &app->frames[ app->frame_index ].render_semaphore,
	    .signal_fence      = app->frames[ app->frame_index ].render_fence,
	};

	ft_queue_submit( app->graphics_queue, &submit_info );

	struct ft_queue_present_info queue_present_info = {
	    .wait_semaphore_count = 1,
	    .wait_semaphores = &app->frames[ app->frame_index ].render_semaphore,
	    .swapchain       = app->swapchain,
	    .image_index     = app->image_index,
	};

	ft_queue_present( app->graphics_queue, &queue_present_info );

	app->frames[ app->frame_index ].cmd_recorded = 0;
	app->frame_index = ( app->frame_index + 1 ) % FRAME_COUNT;
}

static struct ft_image*
load_environment_map( const struct ft_device* device, const char* filename )
{
	struct ft_image_info info = {
	    .depth           = 1,
	    .format          = FT_FORMAT_R32G32B32A32_SFLOAT,
	    .mip_levels      = 1,
	    .layer_count     = 1,
	    .sample_count    = 1,
	    .descriptor_type = FT_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
	};

	struct ft_image* image;
	void* data = ft_read_image_from_file( filename, &info.width, &info.height );

	ft_create_image( device, &info, &image );

	struct ft_image_upload_job job = {
	    .image     = image,
	    .width     = info.width,
	    .height    = info.height,
	    .mip_level = 0,
	    .data      = data,
	};

	ft_upload_image( &job );

	ft_loader_wait_idle();

	ft_free_image_data( data );

	return image;
}

static void
compute_pbr_maps( struct app_data* app )
{
	static const uint32_t SKYBOX_SIZE     = 2048;
	uint32_t              SKYBOX_MIPS     = ( uint ) log2( SKYBOX_SIZE ) + 1;
	static const uint32_t IRRADIANCE_SIZE = 32;
	static const uint32_t SPECULAR_SIZE   = 128;
	uint32_t              SPECULAR_MIPS   = ( uint ) log2( SPECULAR_SIZE ) + 1;
	static const uint32_t BRDF_LUT_SIZE   = 512;

	const struct ft_device*   device = app->device;
	struct pbr_maps*          pbr    = &app->pbr;
	struct ft_command_buffer* cmd    = app->frames[ 0 ].cmd;

	struct ft_sampler_info sampler_info = {
	    .mag_filter        = FT_FILTER_LINEAR,
	    .min_filter        = FT_FILTER_LINEAR,
	    .mipmap_mode       = FT_SAMPLER_MIPMAP_MODE_LINEAR,
	    .address_mode_u    = FT_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	    .address_mode_v    = FT_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	    .address_mode_w    = FT_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	    .mip_lod_bias      = 0,
	    .anisotropy_enable = 1,
	    .max_anisotropy    = 16.0f,
	    .compare_enable    = 0,
	    .compare_op        = FT_COMPARE_OP_ALWAYS,
	    .min_lod           = 0,
	    .max_lod           = 16,
	};

	struct ft_sampler* skybox_sampler;
	ft_create_sampler( device, &sampler_info, &skybox_sampler );

	struct ft_image* environment_eq =
	    load_environment_map( device, "LA_Downtown_Helipad_GoldenHour_3k.hdr" );

	struct ft_image_info image_info;
	memset( &image_info, 0, sizeof( image_info ) );
	image_info.width        = SKYBOX_SIZE;
	image_info.height       = SKYBOX_SIZE;
	image_info.depth        = 1;
	image_info.format       = FT_FORMAT_R32G32B32A32_SFLOAT;
	image_info.mip_levels   = SKYBOX_MIPS;
	image_info.layer_count  = 6;
	image_info.sample_count = 1;
	image_info.descriptor_type =
	    FT_DESCRIPTOR_TYPE_SAMPLED_IMAGE | FT_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	ft_create_image( device, &image_info, &pbr->environment );
	image_info.width        = IRRADIANCE_SIZE;
	image_info.height       = IRRADIANCE_SIZE;
	image_info.depth        = 1;
	image_info.format       = FT_FORMAT_R32G32B32A32_SFLOAT;
	image_info.layer_count  = 6;
	image_info.mip_levels   = 1;
	image_info.sample_count = 1;
	image_info.descriptor_type =
	    FT_DESCRIPTOR_TYPE_STORAGE_IMAGE | FT_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	ft_create_image( device, &image_info, &pbr->irradiance );
	image_info.width      = SPECULAR_SIZE;
	image_info.height     = SPECULAR_SIZE;
	image_info.mip_levels = SPECULAR_MIPS;
	ft_create_image( device, &image_info, &pbr->specular );
	image_info.width       = BRDF_LUT_SIZE;
	image_info.height      = BRDF_LUT_SIZE;
	image_info.layer_count = 1;
	image_info.mip_levels  = 1;
	image_info.format      = FT_FORMAT_R32G32_SFLOAT;
	ft_create_image( device, &image_info, &pbr->brdf_lut );

	struct ft_shader* eq_to_cubemap_shader;
	struct ft_shader* brdf_shader;
	struct ft_shader* irradiance_shader;
	struct ft_shader* specular_shader;

	struct ft_shader_info shader_info;
	memset( &shader_info, 0, sizeof( shader_info ) );
	shader_info.compute = get_eq_to_cubemap_comp_shader( device->api );
	ft_create_shader( device, &shader_info, &eq_to_cubemap_shader );
	shader_info.compute = get_brdf_comp_shader( device->api );
	ft_create_shader( device, &shader_info, &brdf_shader );
	shader_info.compute = get_irradiance_comp_shader( device->api );
	ft_create_shader( device, &shader_info, &irradiance_shader );
	shader_info.compute = get_specular_comp_shader( device->api );
	ft_create_shader( device, &shader_info, &specular_shader );

	struct ft_descriptor_set_layout* eq_to_cubemap_dsl;
	struct ft_descriptor_set_layout* brdf_dsl;
	struct ft_descriptor_set_layout* irradiance_dsl;
	struct ft_descriptor_set_layout* specular_dsl;

	ft_create_descriptor_set_layout( device,
	                                 eq_to_cubemap_shader,
	                                 &eq_to_cubemap_dsl );
	ft_create_descriptor_set_layout( device, brdf_shader, &brdf_dsl );
	ft_create_descriptor_set_layout( device,
	                                 irradiance_shader,
	                                 &irradiance_dsl );
	ft_create_descriptor_set_layout( device, specular_shader, &specular_dsl );

	struct ft_pipeline* eq_to_cubemap_pipeline;
	struct ft_pipeline* brdf_pipeline;
	struct ft_pipeline* irradiance_pipeline;
	struct ft_pipeline* specular_pipeline;

	struct ft_pipeline_info pipeline_info;
	memset( &pipeline_info, 0, sizeof( pipeline_info ) );
	pipeline_info.type                  = FT_PIPELINE_TYPE_COMPUTE;
	pipeline_info.shader                = eq_to_cubemap_shader;
	pipeline_info.descriptor_set_layout = eq_to_cubemap_dsl;
	ft_create_pipeline( device, &pipeline_info, &eq_to_cubemap_pipeline );
	pipeline_info.shader                = brdf_shader;
	pipeline_info.descriptor_set_layout = brdf_dsl;
	ft_create_pipeline( device, &pipeline_info, &brdf_pipeline );
	pipeline_info.shader                = irradiance_shader;
	pipeline_info.descriptor_set_layout = irradiance_dsl;
	ft_create_pipeline( device, &pipeline_info, &irradiance_pipeline );
	pipeline_info.shader                = specular_shader;
	pipeline_info.descriptor_set_layout = specular_dsl;
	ft_create_pipeline( device, &pipeline_info, &specular_pipeline );

	struct ft_descriptor_set* eq_to_cubemap_set[ 16 ];
	memset( eq_to_cubemap_set, 0, sizeof( eq_to_cubemap_set ) );
	struct ft_descriptor_set* brdf_set;
	struct ft_descriptor_set* irradiance_set;
	struct ft_descriptor_set* specular_set[ 16 ];
	memset( specular_set, 0, sizeof( specular_set ) );

	struct ft_descriptor_set_info set_info;
	memset( &set_info, 0, sizeof( set_info ) );
	set_info.descriptor_set_layout = eq_to_cubemap_dsl;
	set_info.set                   = 0;
	for ( uint32_t mip = 0; mip < SKYBOX_MIPS; ++mip )
	{
		ft_create_descriptor_set( device,
		                          &set_info,
		                          &eq_to_cubemap_set[ mip ] );
	}
	set_info.descriptor_set_layout = brdf_dsl;
	set_info.set                   = 0;
	ft_create_descriptor_set( device, &set_info, &brdf_set );
	set_info.descriptor_set_layout = irradiance_dsl;
	set_info.set                   = 0;
	ft_create_descriptor_set( device, &set_info, &irradiance_set );
	set_info.descriptor_set_layout = specular_dsl;
	set_info.set                   = 0;
	for ( uint32_t mip = 0; mip < SPECULAR_MIPS; ++mip )
	{
		ft_create_descriptor_set( device, &set_info, &specular_set[ mip ] );
	}

	struct ft_sampler_descriptor sampler_descriptor;
	sampler_descriptor.sampler = skybox_sampler;
	struct ft_image_descriptor image_descriptors[ 2 ];
	memset( image_descriptors, 0, sizeof( image_descriptors ) );
	struct ft_descriptor_write writes[ 3 ];
	memset( writes, 0, sizeof( writes ) );

	image_descriptors[ 0 ].image          = pbr->brdf_lut;
	image_descriptors[ 0 ].resource_state = FT_RESOURCE_STATE_GENERAL;
	writes[ 0 ].descriptor_count          = 1;
	writes[ 0 ].descriptor_name           = "u_dst";
	writes[ 0 ].image_descriptors         = &image_descriptors[ 0 ];
	ft_update_descriptor_set( device, brdf_set, 1, &writes[ 0 ] );

	memset( writes, 0, sizeof( writes ) );
	image_descriptors[ 0 ].image          = environment_eq;
	image_descriptors[ 0 ].resource_state = FT_RESOURCE_STATE_SHADER_READ_ONLY;
	image_descriptors[ 1 ].image          = pbr->environment;
	image_descriptors[ 1 ].resource_state = FT_RESOURCE_STATE_GENERAL;
	writes[ 0 ].descriptor_count          = 1;
	writes[ 0 ].descriptor_name           = "u_sampler";
	writes[ 0 ].sampler_descriptors       = &sampler_descriptor;
	writes[ 1 ].descriptor_count          = 1;
	writes[ 1 ].descriptor_name           = "u_src";
	writes[ 1 ].image_descriptors         = &image_descriptors[ 0 ];
	writes[ 2 ].descriptor_count          = 1;
	writes[ 2 ].descriptor_name           = "u_dst";
	writes[ 2 ].image_descriptors         = &image_descriptors[ 1 ];

	for ( uint32_t mip = 0; mip < pbr->environment->mip_levels; ++mip )
	{
		image_descriptors[ 1 ].mip_level = mip;
		ft_update_descriptor_set( device,
		                          eq_to_cubemap_set[ mip ],
		                          FT_ARRAY_SIZE( writes ),
		                          writes );
	}

	memset( image_descriptors, 0, sizeof( image_descriptors ) );
	memset( writes, 0, sizeof( writes ) );
	image_descriptors[ 0 ].image          = pbr->environment;
	image_descriptors[ 0 ].resource_state = FT_RESOURCE_STATE_SHADER_READ_ONLY;
	image_descriptors[ 1 ].image          = pbr->irradiance;
	image_descriptors[ 1 ].resource_state = FT_RESOURCE_STATE_GENERAL;
	writes[ 0 ].descriptor_count          = 1;
	writes[ 0 ].descriptor_name           = "u_sampler";
	writes[ 0 ].sampler_descriptors       = &sampler_descriptor;
	writes[ 1 ].descriptor_count          = 1;
	writes[ 1 ].descriptor_name           = "u_src";
	writes[ 1 ].image_descriptors         = &image_descriptors[ 0 ];
	writes[ 2 ].descriptor_count          = 1;
	writes[ 2 ].descriptor_name           = "u_dst";
	writes[ 2 ].image_descriptors         = &image_descriptors[ 1 ];
	ft_update_descriptor_set( device,
	                          irradiance_set,
	                          FT_ARRAY_SIZE( writes ),
	                          writes );

	memset( image_descriptors, 0, sizeof( image_descriptors ) );
	memset( writes, 0, sizeof( writes ) );
	image_descriptors[ 0 ].image          = pbr->environment;
	image_descriptors[ 0 ].resource_state = FT_RESOURCE_STATE_SHADER_READ_ONLY;
	image_descriptors[ 1 ].image          = pbr->specular;
	image_descriptors[ 1 ].resource_state = FT_RESOURCE_STATE_GENERAL;
	writes[ 0 ].descriptor_count          = 1;
	writes[ 0 ].descriptor_name           = "u_sampler";
	writes[ 0 ].sampler_descriptors       = &sampler_descriptor;
	writes[ 1 ].descriptor_count          = 1;
	writes[ 1 ].descriptor_name           = "u_src";
	writes[ 1 ].image_descriptors         = &image_descriptors[ 0 ];
	writes[ 2 ].descriptor_count          = 1;
	writes[ 2 ].descriptor_name           = "u_dst";
	writes[ 2 ].image_descriptors         = &image_descriptors[ 1 ];

	for ( uint32_t mip = 0; mip < SPECULAR_MIPS; ++mip )
	{
		image_descriptors[ 1 ].mip_level = mip;
		ft_update_descriptor_set( device,
		                          specular_set[ mip ],
		                          FT_ARRAY_SIZE( writes ),
		                          writes );
	}

	ft_begin_command_buffer( cmd );

	struct ft_image_barrier image_barrier;
	memset( &image_barrier, 0, sizeof( image_barrier ) );

	image_barrier.image     = pbr->brdf_lut;
	image_barrier.old_state = FT_RESOURCE_STATE_UNDEFINED;
	image_barrier.new_state = FT_RESOURCE_STATE_GENERAL;
	ft_cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &image_barrier );

	ft_cmd_bind_pipeline( cmd, brdf_pipeline );
	ft_cmd_bind_descriptor_set( cmd, 0, brdf_set, brdf_pipeline );
	ft_cmd_dispatch( cmd, BRDF_LUT_SIZE / 16, BRDF_LUT_SIZE / 16, 1 );

	image_barrier.old_state = FT_RESOURCE_STATE_GENERAL;
	image_barrier.new_state = FT_RESOURCE_STATE_SHADER_READ_ONLY;
	ft_cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &image_barrier );

	image_barrier.image     = pbr->environment;
	image_barrier.old_state = FT_RESOURCE_STATE_UNDEFINED;
	image_barrier.new_state = FT_RESOURCE_STATE_GENERAL;
	ft_cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &image_barrier );

	ft_cmd_bind_pipeline( cmd, eq_to_cubemap_pipeline );

	struct
	{
		uint32_t mip;
		uint32_t texture_size;
	} eq_to_cubemap_pc = { 0, SKYBOX_SIZE };

	for ( uint32_t i = 0; i < SKYBOX_MIPS; ++i )
	{
		ft_cmd_bind_descriptor_set( cmd,
		                            0,
		                            eq_to_cubemap_set[ i ],
		                            eq_to_cubemap_pipeline );

		eq_to_cubemap_pc.mip = i;
		ft_cmd_push_constants( cmd,
		                       eq_to_cubemap_pipeline,
		                       0,
		                       sizeof( eq_to_cubemap_pc ),
		                       &eq_to_cubemap_pc );

		ft_cmd_dispatch( cmd,
		                 FT_MAX( 1u, ( SKYBOX_SIZE >> i ) / 16 ),
		                 FT_MAX( 1u, ( SKYBOX_SIZE >> i ) / 16 ),
		                 6 );
	}

	image_barrier.old_state = FT_RESOURCE_STATE_GENERAL;
	image_barrier.new_state = FT_RESOURCE_STATE_SHADER_READ_ONLY;
	ft_cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &image_barrier );
	image_barrier.image     = pbr->irradiance;
	image_barrier.old_state = FT_RESOURCE_STATE_UNDEFINED;
	image_barrier.new_state = FT_RESOURCE_STATE_GENERAL;
	ft_cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &image_barrier );

	ft_cmd_bind_pipeline( cmd, irradiance_pipeline );
	ft_cmd_bind_descriptor_set( cmd, 0, irradiance_set, irradiance_pipeline );
	ft_cmd_dispatch( cmd, IRRADIANCE_SIZE / 16, IRRADIANCE_SIZE / 16, 6 );

	image_barrier.old_state = FT_RESOURCE_STATE_GENERAL;
	image_barrier.new_state = FT_RESOURCE_STATE_SHADER_READ_ONLY;
	ft_cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &image_barrier );

	image_barrier.image     = pbr->specular;
	image_barrier.old_state = FT_RESOURCE_STATE_UNDEFINED;
	image_barrier.new_state = FT_RESOURCE_STATE_GENERAL;
	ft_cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &image_barrier );

	ft_cmd_bind_pipeline( cmd, specular_pipeline );

	struct
	{
		uint32_t mip_size;
		float    roughness;
	} specular_pc;

	for ( uint32_t i = 0; i < SPECULAR_MIPS; ++i )
	{
		specular_pc.roughness =
		    ( float ) i / FT_MAX( ( float ) ( SPECULAR_MIPS - 1 ), 0.00001f );
		specular_pc.mip_size = SPECULAR_SIZE >> i;

		ft_cmd_bind_descriptor_set( cmd,
		                            0,
		                            specular_set[ i ],
		                            specular_pipeline );

		ft_cmd_push_constants( cmd,
		                       specular_pipeline,
		                       0,
		                       sizeof( specular_pc ),
		                       &specular_pc );

		ft_cmd_dispatch( cmd,
		                 FT_MAX( 1u, ( SPECULAR_SIZE >> i ) / 16 ),
		                 FT_MAX( 1u, ( SPECULAR_SIZE >> i ) / 16 ),
		                 6 );
	}

	image_barrier.old_state = FT_RESOURCE_STATE_GENERAL;
	image_barrier.new_state = FT_RESOURCE_STATE_SHADER_READ_ONLY;
	ft_cmd_barrier( cmd, 0, NULL, 0, NULL, 1, &image_barrier );

	ft_end_command_buffer( cmd );

	ft_immediate_submit( app->graphics_queue, cmd );

	for ( uint32_t mip = 0; mip < pbr->specular->mip_levels; ++mip )
	{
		ft_destroy_descriptor_set( device, specular_set[ mip ] );
	}

	ft_destroy_descriptor_set( device, irradiance_set );
	ft_destroy_descriptor_set( device, brdf_set );
	for ( uint32_t mip = 0; mip < pbr->environment->mip_levels; ++mip )
	{
		ft_destroy_descriptor_set( device, eq_to_cubemap_set[ mip ] );
	}

	ft_destroy_pipeline( device, specular_pipeline );
	ft_destroy_pipeline( device, irradiance_pipeline );
	ft_destroy_pipeline( device, brdf_pipeline );
	ft_destroy_pipeline( device, eq_to_cubemap_pipeline );

	ft_destroy_descriptor_set_layout( device, specular_dsl );
	ft_destroy_descriptor_set_layout( device, irradiance_dsl );
	ft_destroy_descriptor_set_layout( device, brdf_dsl );
	ft_destroy_descriptor_set_layout( device, eq_to_cubemap_dsl );

	ft_destroy_shader( device, specular_shader );
	ft_destroy_shader( device, irradiance_shader );
	ft_destroy_shader( device, brdf_shader );
	ft_destroy_shader( device, eq_to_cubemap_shader );

	ft_destroy_image( device, environment_eq );
	ft_destroy_sampler( device, skybox_sampler );
}

static void
free_pbr_maps( struct app_data* app )
{
	ft_destroy_image( app->device, app->pbr.environment );
	ft_destroy_image( app->device, app->pbr.brdf_lut );
	ft_destroy_image( app->device, app->pbr.irradiance );
	ft_destroy_image( app->device, app->pbr.specular );
}
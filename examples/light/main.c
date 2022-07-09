#include <fluent/os.h>
#include <fluent/renderer.h>

#include "ui_pass.h"
#include "main_pass.h"

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

	ft_rg_create( app->device, &app->graph );
	register_main_pass( app->graph, app->swapchain, "back", &app->camera );
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

	struct ft_application_config config = {
	    .argc        = argc,
	    .argv        = argv,
	    .window_info = window_info,
	    .log_level   = FT_TRACE,
	    .on_init     = on_init,
	    .on_update   = on_update,
	    .on_resize   = on_resize,
	    .on_shutdown = on_shutdown,
	    .user_data   = &data,
	};

	ft_app_init( &config );
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

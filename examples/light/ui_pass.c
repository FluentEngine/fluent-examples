#include <stdio.h>
#include <fluent/fluent.h>
#include "ui_pass.h"

struct ui_pass_data
{
	uint32_t           width;
	uint32_t           height;
	struct nk_context* ui;
} ui_pass_data;

static void
ui_pass_execute( const struct ft_device*   device,
                 struct ft_command_buffer* cmd,
                 void*                     user_data )
{
	struct ui_pass_data* data = user_data;

	static struct ft_timer fps_timer;
	static bool            first_time = 1;
	static uint64_t        frames     = 0;
	static double          fps        = 0;

	if ( first_time )
	{
		ft_timer_reset( &fps_timer );
		first_time = 0;
	}

	nk_ft_new_frame();

	if ( nk_begin( data->ui,
	               "Debug Menu",
	               nk_rect( 0, 0, 200, data->height ),
	               NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR |
	                   NK_WINDOW_NO_INPUT | NK_WINDOW_NOT_INTERACTIVE ) )
	{
		char fps_str[ 30 ];
		sprintf( fps_str, "FPS: %.04f", fps );
		nk_layout_row_static( data->ui, 20, 100, 1 );
		nk_label( data->ui, fps_str, NK_TEXT_ALIGN_LEFT );
	}
	nk_end( data->ui );
	nk_ft_render( cmd, NK_ANTI_ALIASING_OFF );

	frames++;

	if ( frames > 5 )
	{
		fps = frames /
		      ( ( ( double ) ft_timer_get_ticks( &fps_timer ) ) / 1000.0f );
		frames = 0;
		ft_timer_reset( &fps_timer );
	}
}

void
register_ui_pass( struct ft_render_graph*    graph,
                  const struct ft_swapchain* swapchain,
                  const char*                backbuffer_source_name,
                  struct nk_context*         ui )
{
	ft_get_swapchain_size( swapchain,
	                       &ui_pass_data.width,
	                       &ui_pass_data.height );
	ui_pass_data.ui = ui;

	struct ft_render_pass* pass;
	ft_rg_add_pass( graph, "ui", &pass );
	ft_rg_set_user_data( pass, &ui_pass_data );
	ft_rg_set_pass_execute_callback( pass, ui_pass_execute );

	struct ft_image_info back;
	ft_rg_add_color_output( pass, backbuffer_source_name, &back );
}

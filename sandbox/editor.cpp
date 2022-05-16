#include "editor.hpp"

namespace fluent
{
struct EditorData
{
	bool               dockspace_open = true;
	ImGuiDockNodeFlags dockspace_flags;
	ImGuiWindowFlags   dockspace_window_flags;
	bool               request_exit = false;
	RendererAPI        current_api;
	bool               api_change_requested = false;
	bool               log_open             = true;
	Image*             scene_image          = nullptr;
	Vector2            viewport_size        = { 0.0f, 0.0f };
	Vector2            viewport_bounds[ 2 ];
	b32                viewport_focused = false;
	b32                viewport_hovered = false;
};

static EditorData editor_data;

std::string
to_api_name( RendererAPI api )
{
	switch ( api )
	{
	case RendererAPI::VULKAN: return "Vulkan";
	case RendererAPI::D3D12: return "D3D12";
	case RendererAPI::METAL: return "Metal";
	default: FT_ASSERT( false );
	}
}

void
editor_init( RendererAPI api )
{
	auto& io = ImGui::GetIO();
	FT_ASSERT( io.ConfigFlags & ImGuiConfigFlags_DockingEnable );
	editor_data.dockspace_flags = ImGuiDockNodeFlags_None;
	editor_data.dockspace_window_flags =
	    ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
	    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	editor_data.current_api = api;
}

void
render_dockspace()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();

	// Set the parent window's position, size, and viewport to match that of
	// the main viewport. This is so the parent window completely covers the
	// main viewport, giving it a "full-screen" feel.
	ImGui::SetNextWindowPos( viewport->WorkPos );
	ImGui::SetNextWindowSize( viewport->WorkSize );
	ImGui::SetNextWindowViewport( viewport->ID );

	ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );

	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );

	ImGui::Begin( "DockSpace Demo",
	              &editor_data.dockspace_open,
	              editor_data.dockspace_window_flags );

	ImGui::PopStyleVar();

	ImGui::PopStyleVar( 2 );

	ImGuiID dockspace_id = ImGui::GetID( "MyDockSpace" );
	ImGui::DockSpace( dockspace_id,
	                  ImVec2( 0.0f, 0.0f ),
	                  editor_data.dockspace_flags );

	if ( ImGui::BeginMenuBar() )
	{
		if ( ImGui::BeginMenu( "Options" ) )
		{
			editor_data.request_exit = ImGui::MenuItem( "Exit" );
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::End();
}

void
editor_render()
{
	render_dockspace();
	bool open_ptr = true;
	ImGui::Begin( "Settings", &open_ptr );
	ImGui::Text( "FPS: %f", ImGui::GetIO().Framerate );

	ImGui::Separator();

	ImGui::Text( "%s",
	             std::string( "Current API: " )
	                 .append( to_api_name( editor_data.current_api ) )
	                 .c_str() );
	static i32 api = static_cast<int>( editor_data.current_api );
	ImGui::RadioButton( "Vulkan", &api, 0 );
	ImGui::RadioButton( "D3D12", &api, 1 );
	ImGui::RadioButton( "Metal", &api, 2 );
	if ( static_cast<i32>( editor_data.current_api ) != api )
	{
		editor_data.api_change_requested = true;
	}

	ImGui::Separator();

	ImGui::End();
	
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2 { 0, 0 } );
	ImGui::Begin( "Viewport" );
	auto viewport_min_region         = ImGui::GetWindowContentRegionMin();
	auto viewport_max_region         = ImGui::GetWindowContentRegionMax();
	auto viewport_offset             = ImGui::GetWindowPos();
	editor_data.viewport_bounds[ 0 ] = {
		viewport_min_region.x + viewport_offset.x,
		viewport_min_region.y + viewport_offset.y
	};
	editor_data.viewport_bounds[ 1 ] = {
		viewport_max_region.x + viewport_offset.x,
		viewport_max_region.y + viewport_offset.y
	};

	editor_data.viewport_focused = ImGui::IsWindowFocused();
	editor_data.viewport_hovered = ImGui::IsWindowHovered();

	ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
	editor_data.viewport_size  = { viewport_panel_size.x,
                                  viewport_panel_size.y };

	ImGui::Image(
	    get_imgui_texture_id( editor_data.scene_image ),
	    ImVec2 { editor_data.viewport_size.x, editor_data.viewport_size.y },
	    ImVec2 { 0, 1 },
	    ImVec2 { 1, 0 } );

	ImGui::End();
	ImGui::PopStyleVar();
	
	ImGui::Begin( "Log", &editor_data.log_open );
	ImGui::End();
}

void
editor_set_scene_image( Image* image )
{
	editor_data.scene_image = image;
}

b32
editor_exit_requested()
{
	return editor_data.request_exit;
}

b32
api_change_requested( RendererAPI* api )
{
	b32 res                          = editor_data.api_change_requested;
	*api                             = editor_data.current_api;
	editor_data.api_change_requested = false;
	return res;
}

} // namespace fluent

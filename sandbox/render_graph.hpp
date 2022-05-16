#pragma once

#include <list>
#include <unordered_map>
#include "fluent/fluent.hpp"

namespace fluent::rg
{

struct RenderGraph;

struct RenderGraphImage
{
	ImageInfo info;
	u32       index;
};

using GetClearColorCallback = bool ( * )( u32 idx, ColorClearValue* );
using ExecuteCallback       = void ( * )( CommandBuffer*, void* );

struct RenderGraphPass
{
	RenderGraph* graph;
	std::vector<RenderGraphImage*> color_outputs;
	void* user_data;
	
	GetClearColorCallback get_color_clear_value;
	ExecuteCallback execute_callback;

	void
	add_color_output( std::string const& name, ImageInfo const& info );

	void
	execute( CommandBuffer* cmd )
	{
		execute_callback( cmd, user_data );
	}

	void
	set_user_data(void* data)
	{
		user_data = data;
	}
	
	void
	set_get_clear_color( GetClearColorCallback&& cb )
	{
		get_color_clear_value = cb;
	}
	
	void
	set_execute_callback( ExecuteCallback&& cb )
	{
		execute_callback = cb;
	}
};

struct PassBarriers
{
	u32                       image_barrier_count;
	std::vector<ImageBarrier> image_barriers;
};

struct RenderGraph
{
	Device const*                        device;
	std::string                          backbuffer_source_name;
	std::unordered_map<std::string, u32> pass_name_to_index;
	std::unordered_map<std::string, u32> image_name_to_index;

	std::vector<RenderGraphImage> images;
	std::vector<RenderGraphPass>  passes;

	std::vector<PassBarriers>        pass_barriers;
	std::vector<RenderPassBeginInfo> pass_infos;

	Image* backbuffer_image;

	RenderGraphImage*
	get_image( std::string const& name );

	// public:

	void
	init( Device const* device );

	void
	shutdown();

	RenderGraphPass*
	add_pass( std::string const& name );

	void
	set_backbuffer_source( std::string const& name );

	void
	build();

	void
	setup_attachments( Image* image );

	void
	execute( CommandBuffer* cmd );
};

} // namespace fluent::rg

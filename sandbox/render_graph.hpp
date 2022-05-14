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

using GetClearColorCallback = bool ( * )( u32              idx,
                                          ColorClearValue* clear_values );

struct RenderGraphPass
{
	RenderGraph* graph;

	GetClearColorCallback get_color_clear_value;

	std::vector<RenderGraphImage*> color_outputs;

	void
	add_color_output( std::string const& name, ImageInfo const& info );

	void
	set_get_clear_color( GetClearColorCallback&& cb )
	{
		get_color_clear_value = cb;
	}
};

struct RenderGraph
{
	Device const*                        device;
	std::string                          backbuffer_source_name;
	std::unordered_map<std::string, u32> pass_name_to_index;
	std::unordered_map<std::string, u32> image_name_to_index;

	std::vector<RenderGraphImage> images;
	std::vector<RenderGraphPass>  passes;

	std::vector<RenderPassBeginInfo> pass_infos;

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

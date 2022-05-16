#include "render_graph.hpp"

namespace fluent::rg
{

void
RenderGraphPass::add_color_output( std::string const& name,
                                   ImageInfo const&   info )
{
	RenderGraphImage* image = graph->get_image( name );
	image->info             = info;
	color_outputs.emplace_back( image );
}

RenderGraphImage*
RenderGraph::get_image( std::string const& name )
{
	auto it = image_name_to_index.find( name );
	if ( it != image_name_to_index.cend() )
	{
		return &images[ image_name_to_index[ name ] ];
	}
	else
	{
		u32               idx       = images.size();
		RenderGraphImage* image     = &images.emplace_back();
		image->index                = idx;
		image_name_to_index[ name ] = idx;
		return image;
	}
}

void
RenderGraph::init( Device const* device )
{
	this->device = device;
}

void
RenderGraph::shutdown()
{
}

RenderGraphPass*
RenderGraph::add_pass( std::string const& name )
{
	u32 idx                    = passes.size();
	pass_name_to_index[ name ] = idx;

	RenderGraphPass* pass       = &passes.emplace_back();
	pass->graph                 = this;
	pass->get_color_clear_value = []( u32 idx, ColorClearValue* clear_values )
	{ return false; };
	pass->execute_callback = []( CommandBuffer*, void* ) {};

	return pass;
}

void
RenderGraph::set_backbuffer_source( std::string const& name )
{
	backbuffer_source_name = name;
}

void
RenderGraph::build()
{
	for ( u32 pass = 0; pass < passes.size(); ++pass )
	{
		RenderPassBeginInfo pass_info {};
		pass_info.device                 = device;
		pass_info.color_attachment_count = passes[ pass ].color_outputs.size();

		PassBarriers barriers {};
		barriers.image_barrier_count = pass_info.color_attachment_count;

		for ( u32 att = 0; att < pass_info.color_attachment_count; ++att )
		{
			bool need_clear = passes[ pass ].get_color_clear_value(
			    att,
			    &pass_info.clear_values[ att ].color );

			if ( need_clear )
			{
				pass_info.color_attachment_load_ops[ att ] =
				    AttachmentLoadOp::CLEAR;
			}
			else
			{
				pass_info.color_attachment_load_ops[ att ] =
				    AttachmentLoadOp::DONT_CARE;
			}

			pass_info.color_image_states[ att ] =
			    ResourceState::COLOR_ATTACHMENT;

			ImageBarrier barrier {};
			barrier.old_state = ResourceState::UNDEFINED;
			barrier.new_state = ResourceState::COLOR_ATTACHMENT;

			barriers.image_barriers.emplace_back( barrier );
		}

		pass_barriers.emplace_back( std::move( barriers ) );
		pass_infos.emplace_back( std::move( pass_info ) );
	}
}

void
RenderGraph::setup_attachments( Image* image )
{
	backbuffer_image = image;

	for ( u32 pass = 0; pass < pass_infos.size(); ++pass )
	{
		for ( u32 att = 0; att < pass_infos[ pass ].color_attachment_count;
		      ++att )
		{
			if ( image_name_to_index[ backbuffer_source_name ] ==
			     passes[ pass ].color_outputs[ att ]->index )
			{
				pass_infos[ pass ].color_attachments[ att ]       = image;
				pass_barriers[ pass ].image_barriers[ att ].image = image;
			}
			pass_infos[ pass ].width =
			    pass_infos[ pass ].color_attachments[ att ]->width;
			pass_infos[ pass ].height =
			    pass_infos[ pass ].color_attachments[ att ]->height;
		}
	}
}

void
RenderGraph::execute( CommandBuffer* cmd )
{
	for ( u32 pass = 0; pass < pass_infos.size(); ++pass )
	{
		cmd_barrier( cmd,
		             0,
		             nullptr,
		             0,
		             nullptr,
		             pass_barriers[ pass ].image_barrier_count,
		             pass_barriers[ pass ].image_barriers.data() );

		cmd_begin_render_pass( cmd, &pass_infos[ pass ] );
		passes[ pass ].execute( cmd );
		cmd_end_render_pass( cmd );
	}

	ImageBarrier barrier {};
	barrier.image     = backbuffer_image;
	barrier.old_state = ResourceState::COLOR_ATTACHMENT;
	barrier.new_state = ResourceState::PRESENT;
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
}

} // namespace fluent::rg

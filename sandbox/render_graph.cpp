#include "render_graph.hpp"

namespace fluent::rg
{

void
RenderGraph::init( const Device* device )
{
	this->device = device;
}

void
RenderGraph::shutdown()
{
	for ( auto& pass : passes_to_execute )
	{
		pass.destroy_callback( pass.user_data );
	}

	for ( auto& [ hash, pass ] : passes )
	{
		destroy_render_pass( device, pass );
	}
}

void
RenderGraph::build() {};

void
RenderGraph::execute( CommandBuffer* cmd, Image* image )
{
	ImageBarrier barrier {};
	barrier.image     = image;
	barrier.old_state = ResourceState::UNDEFINED;
	barrier.new_state = ResourceState::COLOR_ATTACHMENT;

	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );

	// swap pass
	{
		auto& pass                               = passes_to_execute.back();
		pass.info.width                          = image->width;
		pass.info.height                         = image->height;
		pass.info.color_attachment_load_ops[ 0 ] = AttachmentLoadOp::CLEAR;
		pass.info.color_image_states[ 0 ] = ResourceState::COLOR_ATTACHMENT;
		pass.info.color_attachments[ 0 ]  = image;

		u32 hash = GraphPassHasher()( pass.info );
		if ( passes.find( hash ) == passes.cend() )
		{
			create_render_pass( device, &pass.info, &pass.pass );
			passes[ hash ] = pass.pass;
			if ( pass.create_callback )
			{
				pass.create_callback( pass.pass, pass.user_data );
			}
		}
		else
		{
			pass.pass = passes[ hash ];
		}
	}

	for ( auto& pass : passes_to_execute )
	{
		pass.begin_info.render_pass = pass.pass;
		cmd_begin_render_pass( cmd, &pass.begin_info );
		if ( pass.execute_callback )
		{
			pass.execute_callback( cmd, pass.user_data );
		}
		cmd_end_render_pass( cmd );
	}

	barrier.old_state = ResourceState::COLOR_ATTACHMENT;
	barrier.new_state = ResourceState::PRESENT;

	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
};

GraphPass*
RenderGraph::add_pass( const std::string& name )
{
	auto* pass       = &passes_to_execute.emplace_back();
	pass->info       = {};
	pass->begin_info = {};
	return pass;
}

} // namespace fluent::rg

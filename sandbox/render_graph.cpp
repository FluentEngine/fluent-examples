#include "render_graph.hpp"

namespace fluent::rg
{

GraphPass*
RenderGraph::get_render_pass( const RenderPassInfo& info )
{
	GraphPass* pass = nullptr;

	u32 hash = GraphPassHasher()( info );

	if ( passes.find( hash ) == passes.cend() )
	{
		pass = &passes[ hash ];
		pass->create( device, info );
	}
	else
	{
		pass = &passes[ hash ];
	}

	return pass;
}

void
RenderGraph::init( const Device* device )
{
	this->device = device;
}

void
RenderGraph::shutdown()
{
	for ( auto& [ hash, pass ] : passes ) { pass.destroy( device ); }
}

void
RenderGraph::build() {};

void
RenderGraph::execute( CommandBuffer* cmd, Image* image )
{
	ImageBarrier barrier {};
	barrier.image     = image;
	barrier.old_state = ResourceState::eUndefined;
	barrier.new_state = ResourceState::eColorAttachment;

	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );

	RenderPassInfo render_pass_info {};
	render_pass_info.width                          = image->width;
	render_pass_info.height                         = image->height;
	render_pass_info.color_attachment_count         = 1;
	render_pass_info.color_attachments[ 0 ]         = image;
	render_pass_info.color_attachment_load_ops[ 0 ] = AttachmentLoadOp::eClear;
	render_pass_info.color_image_states[ 0 ] = ResourceState::eColorAttachment;

	GraphPass* pass = get_render_pass( render_pass_info );

	RenderPassBeginInfo pass_begin_info {};
	pass_begin_info.render_pass                  = pass->pass;
	pass_begin_info.clear_values[ 0 ].color[ 0 ] = 0.2f;
	pass_begin_info.clear_values[ 0 ].color[ 1 ] = 0.3f;
	pass_begin_info.clear_values[ 0 ].color[ 2 ] = 0.4f;
	pass_begin_info.clear_values[ 0 ].color[ 3 ] = 1.0f;

	cmd_begin_render_pass( cmd, &pass_begin_info );
	cmd_end_render_pass( cmd );

	barrier.old_state = ResourceState::eColorAttachment;
	barrier.new_state = ResourceState::ePresent;

	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
};

} // namespace fluent::rg

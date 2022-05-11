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

	for ( auto* color_output : color_outputs )
	{
		destroy_image( device, color_output );
	}

	for ( auto* depth_output : depth_outputs )
	{
		destroy_image( device, depth_output );
	}

	for ( auto& [ hash, pass ] : passes )
	{
		destroy_render_pass( device, pass );
	}
}

void
RenderGraph::build( Queue* queue, CommandBuffer* cmd )
{
	ImageInfo desc {};
	desc.width        = 1400;
	desc.height       = 900;
	desc.depth        = 1;
	desc.format       = Format::R8G8B8A8_SRGB;
	desc.layer_count  = 1;
	desc.mip_levels   = 1;
	desc.sample_count = SampleCount::E1;
	desc.descriptor_type =
	    DescriptorType::COLOR_ATTACHMENT | DescriptorType::SAMPLED_IMAGE;

	create_image( device, &desc, &color_outputs.emplace_back() );
	create_image( device, &desc, &color_outputs.emplace_back() );
	create_image( device, &desc, &color_outputs.emplace_back() );

	ImageInfo depth_image_info {};
	depth_image_info.width           = 1400;
	depth_image_info.height          = 900;
	depth_image_info.depth           = 1;
	depth_image_info.format          = Format::D32_SFLOAT;
	depth_image_info.layer_count     = 1;
	depth_image_info.mip_levels      = 1;
	depth_image_info.sample_count    = SampleCount::E1;
	depth_image_info.descriptor_type = DescriptorType::DEPTH_STENCIL_ATTACHMENT;

	create_image( device, &depth_image_info, &depth_outputs.emplace_back() );

	ImageBarrier barrier {};
	barrier.image     = depth_outputs.back();
	barrier.old_state = ResourceState::UNDEFINED;
	barrier.new_state = ResourceState::DEPTH_STENCIL_WRITE;

	begin_command_buffer( cmd );
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
	end_command_buffer( cmd );
	immediate_submit( queue, cmd );

	RenderPassInfo render_pass_info {};
	render_pass_info.width                          = 1400;
	render_pass_info.height                         = 900;
	render_pass_info.color_attachment_count         = 3;
	render_pass_info.color_attachments[ 0 ]         = color_outputs[ 0 ];
	render_pass_info.color_attachment_load_ops[ 0 ] = AttachmentLoadOp::CLEAR;
	render_pass_info.color_image_states[ 0 ] = ResourceState::COLOR_ATTACHMENT;
	render_pass_info.color_attachments[ 1 ]  = color_outputs[ 1 ];
	render_pass_info.color_attachment_load_ops[ 1 ] = AttachmentLoadOp::CLEAR;
	render_pass_info.color_image_states[ 1 ] = ResourceState::COLOR_ATTACHMENT;
	render_pass_info.color_attachments[ 2 ]  = color_outputs[ 2 ];
	render_pass_info.color_attachment_load_ops[ 2 ] = AttachmentLoadOp::CLEAR;
	render_pass_info.color_image_states[ 2 ] = ResourceState::COLOR_ATTACHMENT;
	render_pass_info.depth_stencil           = depth_outputs[ 0 ];
	render_pass_info.depth_stencil_load_op   = AttachmentLoadOp::CLEAR;
	render_pass_info.depth_stencil_state = ResourceState::DEPTH_STENCIL_WRITE;

	create_render_pass( device,
	                    &render_pass_info,
	                    &passes_to_execute[ 0 ].pass );

	passes_to_execute[ 0 ].create_callback( passes_to_execute[ 0 ].pass,
	                                        passes_to_execute[ 0 ].user_data );

	passes[ GraphPassHasher()( render_pass_info ) ] =
	    passes_to_execute[ 0 ].pass;
};

void
RenderGraph::execute( CommandBuffer* cmd, Image* image )
{
	ImageBarrier barrier {};
	barrier.image     = image;
	barrier.old_state = ResourceState::UNDEFINED;
	barrier.new_state = ResourceState::COLOR_ATTACHMENT;

	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );

	barrier.image = color_outputs[ 0 ];
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
	barrier.image = color_outputs[ 1 ];
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
	barrier.image = color_outputs[ 2 ];
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

	u32 i = 0;

	for ( auto& pass : passes_to_execute )
	{
		pass.begin_info.render_pass = pass.pass;
		cmd_begin_render_pass( cmd, &pass.begin_info );
		if ( pass.execute_callback )
		{
			pass.execute_callback( cmd, pass.user_data );
		}
		cmd_end_render_pass( cmd );

		i++;
		if ( i == 1 )
		{
			barrier.old_state = ResourceState::COLOR_ATTACHMENT;
			barrier.new_state = ResourceState::SHADER_READ_ONLY;

			barrier.image = color_outputs[ 0 ];
			cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
			barrier.image = color_outputs[ 1 ];
			cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
			barrier.image = color_outputs[ 2 ];
			cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
		}
	}

	barrier.image     = image;
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

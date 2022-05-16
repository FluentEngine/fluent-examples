#include "render_graph.hpp"

namespace fluent::rg
{

void
RenderGraphPass::add_color_output( std::string const& name,
                                   ImageInfo const&   info )
{
	RenderGraphImage* image = graph->get_graph_image( name );
	image->info             = info;
	image->info.descriptor_type |= DescriptorType::COLOR_ATTACHMENT;
	color_outputs.emplace_back( image );
}

void
RenderGraphPass::add_depth_output( std::string const& name,
                                   ImageInfo const&   info )
{
	RenderGraphImage* image     = graph->get_graph_image( name );
	image->info                 = info;
	image->info.descriptor_type = DescriptorType::DEPTH_STENCIL_ATTACHMENT;
	depth_output                = image;
}

void
RenderGraphPass::add_texture_input( std::string const& name )
{
	RenderGraphImage* image = graph->get_graph_image( name );
	image->info.descriptor_type |= DescriptorType::SAMPLED_IMAGE;
	texture_inputs.emplace_back( image );
}

RenderGraphImage*
RenderGraph::get_graph_image( std::string const& name )
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
		image->info                 = {};
		image->index                = idx;
		image_name_to_index[ name ] = idx;
		physical_images.emplace_back( nullptr );
		return image;
	}
}

void
RenderGraph::init( Device const* device )
{
	this->device = device;
	// TODO: temporary solution to avoid invalidation
	passes.reserve( 10 );
	images.reserve( 10 );
	pass_barriers.reserve( 10 );
	pass_infos.reserve( 10 );
	physical_images.reserve( 10 );
}

void
RenderGraph::shutdown()
{
	for ( u32 i = 0; i < passes.size(); ++i )
	{
		passes[ i ].destroy_callback( passes[ i ].user_data );
	}

	for ( u32 i = 0; i < physical_images.size(); ++i )
	{
		if ( physical_images[ i ] )
		{
			destroy_image( device, physical_images[ i ] );
		}
	}
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
	pass->get_depth_stencil_clear_value =
	    []( DepthStencilClearValue* clear_value ) { return false; };
	pass->create_callback  = []( void* ) {};
	pass->execute_callback = []( CommandBuffer*, void* ) {};
	pass->destroy_callback = []( void* ) {};
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

			if ( passes[ pass ].color_outputs[ att ]->index !=
			     image_name_to_index[ backbuffer_source_name ] )
			{
				auto& graph_image =
				    images[ passes[ pass ].color_outputs[ att ]->index ];

				create_image(
				    device,
				    &graph_image.info,
				    &physical_images
				        [ passes[ pass ].color_outputs[ att ]->index ] );
			}

			pass_info.color_attachments[ att ] =
			    physical_images[ passes[ pass ].color_outputs[ att ]->index ];
		}

		if ( passes[ pass ].depth_output )
		{
			auto& graph_image = images[ passes[ pass ].depth_output->index ];
			create_image(
			    device,
			    &graph_image.info,
			    &physical_images[ passes[ pass ].depth_output->index ] );

			b32 need_clear = passes[ pass ].get_depth_stencil_clear_value(
			    &pass_info.clear_values[ pass_info.color_attachment_count ]
			         .depth_stencil );

			if ( need_clear )
			{
				pass_info.depth_stencil_load_op = AttachmentLoadOp::CLEAR;
			}
			else
			{
				pass_info.depth_stencil_load_op = AttachmentLoadOp::DONT_CARE;
			}

			pass_info.depth_stencil =
			    physical_images[ passes[ pass ].depth_output->index ];
			pass_info.depth_stencil_state = ResourceState::DEPTH_STENCIL_WRITE;
		}

		pass_infos.emplace_back( std::move( pass_info ) );
	}

	for ( u32 pass = 0; pass < passes.size(); ++pass )
	{
		PassBarriers barriers {};
		barriers.image_barriers.reserve( passes[ pass ].color_outputs.size() +
		                                 passes[ pass ].texture_inputs.size() );

		for ( u32 att = 0; att < passes[ pass ].color_outputs.size(); ++att )
		{
			ImageBarrier image_barrier {};
			image_barrier.old_state = ResourceState::UNDEFINED; // TODO
			image_barrier.new_state = ResourceState::COLOR_ATTACHMENT;
			image_barrier.image =
			    physical_images[ passes[ pass ].color_outputs[ att ]->index ];

			barriers.image_barrier_count++;
			barriers.image_barriers.emplace_back( image_barrier );
		}

		if ( passes[ pass ].depth_output )
		{
			ImageBarrier image_barrier {};
			image_barrier.old_state = ResourceState::UNDEFINED;
			image_barrier.new_state = ResourceState::DEPTH_STENCIL_WRITE;
			image_barrier.image =
			    physical_images[ passes[ pass ].depth_output->index ];
			barriers.image_barrier_count++;
			barriers.image_barriers.emplace_back( image_barrier );
		}

		for ( u32 att = 0; att < passes[ pass ].texture_inputs.size(); ++att )
		{
			ImageBarrier image_barrier {};
			image_barrier.old_state = ResourceState::COLOR_ATTACHMENT; // TODO
			image_barrier.new_state = ResourceState::SHADER_READ_ONLY;
			image_barrier.image =
			    physical_images[ passes[ pass ].texture_inputs[ att ]->index ];

			barriers.image_barrier_count++;
			barriers.image_barriers.emplace_back( image_barrier );
		}

		pass_barriers.emplace_back( std::move( barriers ) );
	}

	for ( u32 i = 0; i < passes.size(); ++i )
	{
		passes[ i ].create_callback( passes[ i ].user_data );
	}
}

void
RenderGraph::setup_attachments( Image* image )
{
	backbuffer_image = image;

	for ( u32 pass = 0; pass < passes.size(); ++pass )
	{
		for ( u32 att = 0; att < passes[ pass ].color_outputs.size(); ++att )
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

		for ( u32 att = passes[ pass ].color_outputs.size();
		      att < passes[ pass ].color_outputs.size() +
		                passes[ pass ].texture_inputs.size();
		      ++att )
		{
			if ( image_name_to_index[ backbuffer_source_name ] ==
			     passes[ pass ]
			         .texture_inputs[ att -
			                          passes[ pass ].color_outputs.size() ]
			         ->index )
			{
				pass_barriers[ pass ].image_barriers[ att ].image = image;
			}
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
	barrier.old_state = pass_infos.empty() ? ResourceState::UNDEFINED
	                                       : ResourceState::COLOR_ATTACHMENT;
	barrier.new_state = ResourceState::PRESENT;
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &barrier );
}

Image*
RenderGraph::get_image( const std::string& name )
{
	return physical_images[ image_name_to_index[ name ] ];
}

} // namespace fluent::rg

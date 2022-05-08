#define SAMPLE_NAME "03_compute"
#include "../common/sample.hpp"

struct Vertex
{
	Vector3 position;
	Vector3 normal;
	Vector2 uv;
};

struct CameraUBO
{
	Matrix4 projection;
	Matrix4 view;
	Matrix4 model;
};

struct PushConstantBlock
{
	f32 time;
	f32 mouse_x;
	f32 mouse_y;
};

DescriptorSetLayout* descriptor_set_layout;
Pipeline*            pipeline;

Image* output_texture;

Buffer* uniform_buffer;

DescriptorSet* descriptor_set;

void
create_output_texture()
{
	ImageInfo image_info {};
	image_info.layer_count = 1;
	image_info.mip_levels  = 1;
	image_info.depth       = 1;
	image_info.format      = Format::R8G8B8A8_UNORM;
	image_info.width       = 1024;
	image_info.height      = 1024;
	image_info.descriptor_type =
	    DescriptorType::SAMPLED_IMAGE | DescriptorType::STORAGE_IMAGE;

	create_image( device, &image_info, &output_texture );

	ImageBarrier image_barrier {};
	image_barrier.image     = output_texture;
	image_barrier.src_queue = queue;
	image_barrier.dst_queue = queue;
	image_barrier.old_state = ResourceState::UNDEFINED;
	image_barrier.new_state = ResourceState::GENERAL;

	CommandBuffer* cmd = command_buffers[ 0 ];

	begin_command_buffer( cmd );
	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &image_barrier );
	end_command_buffer( cmd );
	immediate_submit( queue, cmd );
}

void
init_sample()
{
	auto comp_code = read_shader( "main.comp" );

	ShaderInfo shader_info {};
	shader_info.compute.bytecode_size =
	    comp_code.size() * sizeof( comp_code[ 0 ] );
	shader_info.compute.bytecode = comp_code.data();

	Shader* shader;
	create_shader( device, &shader_info, &shader );

	create_descriptor_set_layout( device, shader, &descriptor_set_layout );

	PipelineInfo pipeline_info {};
	pipeline_info.shader                = shader;
	pipeline_info.descriptor_set_layout = descriptor_set_layout;

	create_compute_pipeline( device, &pipeline_info, &pipeline );

	destroy_shader( device, shader );

	BufferInfo buffer_info {};
	buffer_info.size            = sizeof( CameraUBO );
	buffer_info.descriptor_type = DescriptorType::UNIFORM_BUFFER;

	create_buffer( device, &buffer_info, &uniform_buffer );

	create_output_texture();

	DescriptorSetInfo descriptor_set_info {};
	descriptor_set_info.descriptor_set_layout = descriptor_set_layout;
	descriptor_set_info.set                   = 0;

	create_descriptor_set( device, &descriptor_set_info, &descriptor_set );

	ImageDescriptor image_descriptor {};
	image_descriptor.resource_state = ResourceState::GENERAL;
	image_descriptor.image          = output_texture;

	DescriptorWrite descriptor_write {};
	descriptor_write.descriptor_name   = "u_output_image";
	descriptor_write.descriptor_count  = 1;
	descriptor_write.image_descriptors = &image_descriptor;

	update_descriptor_set( device, descriptor_set, 1, &descriptor_write );
}

void
resize_sample( u32, u32 )
{
}

void
update_sample( CommandBuffer* cmd, f32 )
{
	if ( !command_buffers_recorded[ frame_index ] )
	{
		wait_for_fences( device, 1, &in_flight_fences[ frame_index ] );
		reset_fences( device, 1, &in_flight_fences[ frame_index ] );
		command_buffers_recorded[ frame_index ] = true;
	}

	u32 image_index = 0;
	acquire_next_image( device,
	                    swapchain,
	                    image_available_semaphores[ frame_index ],
	                    {},
	                    &image_index );

	begin_command_buffer( cmd );

	cmd_set_viewport( cmd,
	                  0,
	                  0,
	                  swapchain->width,
	                  swapchain->height,
	                  0.0f,
	                  1.0f );

	cmd_set_scissor( cmd, 0, 0, swapchain->width, swapchain->height );
	cmd_bind_pipeline( cmd, pipeline );
	cmd_bind_descriptor_set( cmd, 0, descriptor_set, pipeline );

	PushConstantBlock pcb;
	pcb.time    = get_time() / 400.0f;
	pcb.mouse_x = get_mouse_pos_x();
	pcb.mouse_y = get_mouse_pos_y();
	cmd_push_constants( cmd, pipeline, 0, sizeof( PushConstantBlock ), &pcb );
	cmd_dispatch( cmd,
	              output_texture->width / 16,
	              output_texture->height / 16,
	              1 );

	cmd_blit_image( cmd,
	                output_texture,
	                ResourceState::GENERAL,
	                swapchain->images[ image_index ],
	                ResourceState::UNDEFINED,
	                Filter::LINEAR );

	ImageBarrier to_present_barrier {};
	to_present_barrier.src_queue = queue;
	to_present_barrier.dst_queue = queue;
	to_present_barrier.image     = swapchain->images[ image_index ];
	to_present_barrier.old_state = ResourceState::TRANSFER_DST;
	to_present_barrier.new_state = ResourceState::PRESENT;

	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &to_present_barrier );

	ImageBarrier to_storage_barrier {};
	to_storage_barrier.src_queue = queue;
	to_storage_barrier.dst_queue = queue;
	to_storage_barrier.image     = output_texture;
	to_storage_barrier.old_state = ResourceState::TRANSFER_SRC;
	to_storage_barrier.new_state = ResourceState::GENERAL;

	cmd_barrier( cmd, 0, nullptr, 0, nullptr, 1, &to_storage_barrier );

	end_command_buffer( cmd );

	QueueSubmitInfo queue_submit_info {};
	queue_submit_info.wait_semaphore_count = 1;
	queue_submit_info.wait_semaphores =
	    &image_available_semaphores[ frame_index ];
	queue_submit_info.command_buffer_count   = 1;
	queue_submit_info.command_buffers        = &cmd;
	queue_submit_info.signal_semaphore_count = 1;
	queue_submit_info.signal_semaphores =
	    &rendering_finished_semaphores[ frame_index ];
	queue_submit_info.signal_fence = in_flight_fences[ frame_index ];

	queue_submit( queue, &queue_submit_info );

	QueuePresentInfo queue_present_info {};
	queue_present_info.wait_semaphore_count = 1;
	queue_present_info.wait_semaphores =
	    &rendering_finished_semaphores[ frame_index ];
	queue_present_info.swapchain   = swapchain;
	queue_present_info.image_index = image_index;

	queue_present( queue, &queue_present_info );

	command_buffers_recorded[ frame_index ] = false;
	frame_index                             = ( frame_index + 1 ) % FRAME_COUNT;
}

void
shutdown_sample()
{
	destroy_image( device, output_texture );
	destroy_buffer( device, uniform_buffer );
	destroy_pipeline( device, pipeline );
	destroy_descriptor_set_layout( device, descriptor_set_layout );
}

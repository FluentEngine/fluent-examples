#define SAMPLE_NAME "01_hello_triangle"
#include "../common/sample.hpp"

DescriptorSetLayout* descriptor_set_layout;
Pipeline*            pipeline;

void
init_sample()
{
	auto vert_code = read_shader( "main.vert" );
	auto frag_code = read_shader( "main.frag" );

	ShaderInfo shader_info {};
	shader_info.vertex.bytecode_size =
	    vert_code.size() * sizeof( vert_code[ 0 ] );
	shader_info.vertex.bytecode = vert_code.data();
	shader_info.fragment.bytecode_size =
	    frag_code.size() * sizeof( frag_code[ 0 ] );
	shader_info.fragment.bytecode = frag_code.data();

	Shader* shader;
	create_shader( device, &shader_info, &shader );

	create_descriptor_set_layout( device, shader, &descriptor_set_layout );

	PipelineInfo pipeline_info {};
	pipeline_info.shader                       = shader;
	pipeline_info.rasterizer_info.cull_mode    = CullMode::NONE;
	pipeline_info.rasterizer_info.front_face   = FrontFace::COUNTER_CLOCKWISE;
	pipeline_info.depth_state_info.depth_test  = false;
	pipeline_info.depth_state_info.depth_write = false;
	pipeline_info.descriptor_set_layout        = descriptor_set_layout;
	pipeline_info.render_pass                  = render_passes[ 0 ];
	pipeline_info.topology = PrimitiveTopology::TRIANGLE_LIST;

	create_graphics_pipeline( device, &pipeline_info, &pipeline );

	destroy_shader( device, shader );
}

void
resize_sample( u32, u32 )
{
}

void
update_sample( CommandBuffer* cmd, f32 )
{
	u32 image_index = begin_frame();

	cmd_bind_pipeline( cmd, pipeline );
	cmd_draw( cmd, 3, 1, 0, 0 );

	end_frame( image_index );
}

void
shutdown_sample()
{
	destroy_pipeline( device, pipeline );
	destroy_descriptor_set_layout( device, descriptor_set_layout );
}

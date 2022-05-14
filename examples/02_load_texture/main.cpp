#define SAMPLE_NAME "02_load_texture"
#include "../common/sample.hpp"

#include "shader_main.vert.h"
#include "shader_main.frag.h"

DescriptorSetLayout* descriptor_set_layout;
Pipeline*            pipeline;

Buffer* vertex_buffer;

// clang-format off
static const f32 vertices[] = {
    -0.5f, -0.5f, 0.0f, 0.0f,
    0.5f, -0.5f, 1.0f, 0.0f,
    0.5f, 0.5f, 1.0f, 1.0f,
    0.5f, 0.5f, 1.0f, 1.0f,
    -0.5f, 0.5f, 0.0f, 1.0f,
    -0.5f, -0.5f, 0.0f, 0.0f,
};
// clang-format on

DescriptorSet* descriptor_set;
Sampler*       sampler;
Image*         texture;

void
init_sample()
{
	ShaderInfo shader_info {};
	shader_info.vertex.bytecode_size   = sizeof( shader_main_vert );
	shader_info.vertex.bytecode        = shader_main_vert;
	shader_info.fragment.bytecode_size = sizeof( shader_main_frag );
	shader_info.fragment.bytecode      = shader_main_frag;

	Shader* shader;
	create_shader( device, &shader_info, &shader );

	create_descriptor_set_layout( device, shader, &descriptor_set_layout );

	PipelineInfo  pipeline_info {};
	VertexLayout& vertex_layout                 = pipeline_info.vertex_layout;
	vertex_layout.binding_info_count            = 1;
	vertex_layout.binding_infos[ 0 ].binding    = 0;
	vertex_layout.binding_infos[ 0 ].input_rate = VertexInputRate::VERTEX;
	vertex_layout.binding_infos[ 0 ].stride     = 4 * sizeof( float );
	vertex_layout.attribute_info_count          = 2;
	vertex_layout.attribute_infos[ 0 ].binding  = 0;
	vertex_layout.attribute_infos[ 0 ].format   = Format::R32G32_SFLOAT;
	vertex_layout.attribute_infos[ 0 ].location = 0;
	vertex_layout.attribute_infos[ 0 ].offset   = 0;
	vertex_layout.attribute_infos[ 1 ].binding  = 0;
	vertex_layout.attribute_infos[ 1 ].format   = Format::R32G32_SFLOAT;
	vertex_layout.attribute_infos[ 1 ].location = 1;
	vertex_layout.attribute_infos[ 1 ].offset   = 2 * sizeof( float );
	pipeline_info.shader                        = shader;
	pipeline_info.rasterizer_info.cull_mode     = CullMode::NONE;
	pipeline_info.rasterizer_info.front_face    = FrontFace::COUNTER_CLOCKWISE;
	pipeline_info.depth_state_info.depth_test   = false;
	pipeline_info.depth_state_info.depth_write  = false;
	pipeline_info.descriptor_set_layout         = descriptor_set_layout;
	pipeline_info.sample_count                  = SampleCount::E1;
	pipeline_info.color_attachment_count        = 1;
	pipeline_info.color_attachment_formats[ 0 ] = swapchain->format;
	pipeline_info.depth_stencil_format          = depth_image->format;
	pipeline_info.topology = PrimitiveTopology::TRIANGLE_LIST;

	create_graphics_pipeline( device, &pipeline_info, &pipeline );

	BufferInfo buffer_info {};
	buffer_info.size            = sizeof( vertices );
	buffer_info.descriptor_type = DescriptorType::VERTEX_BUFFER;

	create_buffer( device, &buffer_info, &vertex_buffer );

	ResourceLoader::upload_buffer( vertex_buffer,
	                               0,
	                               sizeof( vertices ),
	                               vertices );

	SamplerInfo sampler_info {};
	sampler_info.mipmap_mode = SamplerMipmapMode::LINEAR;
	sampler_info.min_lod     = 0;
	sampler_info.max_lod     = 1000;

	create_sampler( device, &sampler_info, &sampler );

	texture = load_image_from_file( "statue.jpg", true );

	ImageDescriptor image_descriptor {};
	image_descriptor.image          = texture;
	image_descriptor.resource_state = ResourceState::SHADER_READ_ONLY;

	SamplerDescriptor sampler_descriptor {};
	sampler_descriptor.sampler = sampler;

	DescriptorWrite descriptor_writes[ 2 ]     = {};
	descriptor_writes[ 0 ].descriptor_name     = "u_sampler";
	descriptor_writes[ 0 ].descriptor_count    = 1;
	descriptor_writes[ 0 ].sampler_descriptors = &sampler_descriptor;
	descriptor_writes[ 1 ].descriptor_name     = "u_texture";
	descriptor_writes[ 1 ].descriptor_count    = 1;
	descriptor_writes[ 1 ].image_descriptors   = &image_descriptor;

	DescriptorSetInfo descriptor_set_info {};
	descriptor_set_info.set                   = 0;
	descriptor_set_info.descriptor_set_layout = descriptor_set_layout;

	create_descriptor_set( device, &descriptor_set_info, &descriptor_set );
	update_descriptor_set( device, descriptor_set, 2, descriptor_writes );

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
	cmd_bind_descriptor_set( cmd, 0, descriptor_set, pipeline );
	cmd_bind_vertex_buffer( cmd, vertex_buffer, 0 );
	cmd_draw( cmd, 6, 1, 0, 0 );

	end_frame( image_index );
}

void
shutdown_sample()
{
	destroy_descriptor_set( device, descriptor_set );
	destroy_image( device, texture );
	destroy_sampler( device, sampler );
	destroy_buffer( device, vertex_buffer );
	destroy_pipeline( device, pipeline );
	destroy_descriptor_set_layout( device, descriptor_set_layout );
}

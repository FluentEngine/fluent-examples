#version 460

layout( set = 0, binding = 0 ) uniform ubo
{
	mat4 projection;
	mat4 view;
} u;

layout( std140, set = 0, binding = 1 ) readonly buffer u_transforms
{
	mat4 transforms[];
} transforms;

layout( push_constant ) uniform constants
{
	uint instance_id; // DirectX12 compatibility
}
pc;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_texcoord;

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec3 out_frag_pos;

void
main()
{
	mat4 transform = transforms.transforms[ pc.instance_id ];
	out_normal     = mat3( transform ) * in_normal;
	out_frag_pos   = ( transform * vec4( in_position, 1.0 ) ).xyz;
	gl_Position =
	    u.projection * u.view * transform * vec4( in_position, 1.0 );
}

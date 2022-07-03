#version 460

layout( set = 0, binding = 0 ) uniform ubo
{
	mat4 projection;
	mat4 view;
	vec4 view_pos;
}
u;

layout( std140, set = 0, binding = 1 ) readonly buffer u_transforms
{
	mat4 transforms[];
}
transforms;

layout( push_constant ) uniform constants
{
	uint instance_id; // DirectX12 compatibility
	int  base_color_texture;
	int  normal_texture;
	int  ambient_occlusion_texture;
	int  metal_rougness_texture;
	int  emissive_texture;
	int  pad[ 2 ];
	vec4 base_color_factor;
}
pc;

layout( location = 0 ) in vec3 in_position;
layout( location = 1 ) in vec3 in_normal;
layout( location = 2 ) in vec3 in_tangent;
layout( location = 3 ) in vec2 in_texcoord;

layout( location = 0 ) out vec3 out_normal;
layout( location = 1 ) out vec2 out_tex_coord;
layout( location = 2 ) out vec3 out_frag_pos;
layout( location = 3 ) out vec3 out_view_pos;
layout( location = 4 ) out mat3 out_tbn;

void
main()
{
	mat4 transform     = transforms.transforms[ pc.instance_id ];
	mat3 normal_matrix = mat3( transform );

	vec3 T = normalize( vec3( transform * vec4( in_tangent, 0.0 ) ) );
	vec3 N = normalize( vec3( transform * vec4( in_normal, 0.0 ) ) );
	T      = normalize( T - dot( T, N ) * N );
	vec3 B = cross( N, T );

	out_tex_coord = in_texcoord;
	out_normal    = N;
	out_frag_pos  = ( transform * vec4( in_position, 1.0 ) ).xyz;
	out_view_pos  = u.view_pos.xyz;
	out_tbn       = mat3( T, B, N );

	gl_Position = u.projection * u.view * transform * vec4( in_position, 1.0 );
}

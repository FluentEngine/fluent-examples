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
	int base_color_texture;
	int normal_texture;
	int pad;
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

void
main()
{
	mat4 transform = transforms.transforms[ pc.instance_id ];
	out_normal     = mat3( transform ) * in_normal;
	out_tex_coord  = in_texcoord;
	out_frag_pos   = ( transform * vec4( in_position, 1.0 ) ).xyz;

//	mat3 normal_matrix = mat3( transform );
//	vec3 T             = normalize( normal_matrix * in_tangent );
//	vec3 N             = normalize( normal_matrix * in_normal );
//	T                  = normalize( T - dot( T, N ) * N );
//	vec3 B             = cross( N, T );
//	mat3 TBN           = transpose( mat3( T, B, N ) );

//	out_normal = N;

	gl_Position =
	    u.projection * u.view * transform * vec4( in_position, 1.0 );
}

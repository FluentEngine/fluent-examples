#version 460

layout( set = 0, binding = 1 ) uniform sampler u_sampler;
layout( set = 0, binding = 2 ) uniform textureCube u_environment_map;

layout( location = 0 ) in vec3 in_frag_pos;
layout( location = 0 ) out vec4 out_frag_color;

void
main()
{
	vec3 color =
	    texture( samplerCube( u_environment_map, u_sampler ), in_frag_pos ).rgb;

	color = color / ( color + vec3( 1.0 ) );

	out_frag_color = vec4( color, 1.0 );
}
#version 460

layout (location = 0) in vec3 in_normal;
layout (location = 1) in vec2 in_tex_coord;
layout (location = 2) in vec3 in_frag_pos;

layout (location = 0) out vec4 out_color;

#define TEXTURE_COUNT 1
#define BASE_COLOR_TEXTURE 0

layout( set = 1, binding = 0 ) uniform sampler u_sampler;
layout( set = 1, binding = 1 ) uniform texture2D u_textures[ TEXTURE_COUNT ];

void
main()
{
	vec3 view_pos    = vec3( 0.0, 1.0, 3.0 );
	vec3 light_color = vec3( 1.0, 1.0, 1.0 );
	vec3 light_pos   = vec3( 0.0, 1.0, 0.0 );

	// ambient
	float ambient_strength = 0.1;
	vec3  ambient          = ambient_strength * light_color;

	// diffuse
	vec3  N         = normalize( in_normal );
	vec3  light_dir = normalize( light_pos - in_frag_pos );
	float diff      = max( dot( N, light_dir ), 0.0 );
	vec3  diffuse   = diff * light_color;

	// specular
	float specular_strength = 0.5;
	vec3  view_dir          = normalize( view_pos - in_frag_pos );
	vec3  reflect_dir       = reflect( -light_dir, N );
	float spec     = pow( max( dot( view_dir, reflect_dir ), 0.0 ), 32 );
	vec3  specular = specular_strength * spec * light_color;

	vec3 result = ( ambient + diffuse + specular ) * vec3( 1.0, 1.0, 1.0 );
	result = texture(sampler2D(u_textures[ BASE_COLOR_TEXTURE ], u_sampler), in_tex_coord).rgb;

	out_color = vec4( result, 1.0 );
}

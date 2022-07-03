#version 460

layout( location = 0 ) in vec3 in_normal;
layout( location = 1 ) in vec2 in_tex_coord;
layout( location = 2 ) in vec3 in_frag_pos;

layout( location = 0 ) out vec4 out_color;

#define TEXTURE_COUNT 2

layout( push_constant ) uniform constants
{
	uint instance_id; // DirectX12 compatibility
	int base_color_texture;
	int normal_texture;
	int pad;
	vec4 base_color_factor;
}
pc;

layout( set = 1, binding = 0 ) uniform sampler u_sampler;
layout( set = 1, binding = 1 ) uniform texture2D u_textures[ TEXTURE_COUNT ];

void
main()
{
	vec3 view_pos    = vec3( 0.0, 1.0, 3.0 );
	vec3 light_color = vec3( 1.0, 1.0, 1.0 );
	vec3 light_pos   = vec3( 0.0, 1.0, 0.0 );

	vec3 base_color;
	if (pc.base_color_texture != -1)
	{
		base_color = texture(sampler2D(u_textures[ pc.base_color_texture ], u_sampler), in_tex_coord).rgb;
	}
	else
	{
		base_color = pc.base_color_factor.rgb;
	}

	vec3 normal;
	if (pc.normal_texture != -1)
	{
		normal = texture(sampler2D(u_textures[ pc.normal_texture ], u_sampler), in_tex_coord).rgb;
	}
	else
	{
		normal = in_normal;
	}

	// ambient
	float ambient_strength = 0.1;
	vec3  ambient          = ambient_strength * light_color;

	// diffuse
	normal          = normalize( normal );
	vec3  light_dir = normalize( light_pos - in_frag_pos );
	float diff      = max( dot( normal, light_dir ), 0.0 );
	vec3  diffuse   = diff * light_color;

	// specular
	float specular_strength = 0.5;
	vec3  view_dir          = normalize( view_pos - in_frag_pos );
	vec3  reflect_dir       = reflect( -light_dir, normal );
	float spec     = pow( max( dot( view_dir, reflect_dir ), 0.0 ), 32 );
	vec3  specular = specular_strength * spec * light_color;

	vec3 result = ( ambient + diffuse + specular ) * base_color;

	out_color = vec4( result, 1.0 );
}

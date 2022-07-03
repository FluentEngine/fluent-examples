#version 460

layout( location = 0 ) in vec3 in_normal;
layout( location = 1 ) in vec2 in_tex_coord;
layout( location = 2 ) in vec3 in_frag_pos;
layout( location = 3 ) in vec3 in_view_pos;
layout( location = 4 ) in mat3 in_tbn;

layout( location = 0 ) out vec4 out_color;

#define TEXTURE_COUNT 5

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

layout( set = 1, binding = 0 ) uniform sampler u_sampler;
layout( set = 1, binding = 1 ) uniform texture2D u_textures[ TEXTURE_COUNT ];

void
main()
{
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
		normal = in_tbn * ( normal * 2.0 - 1.0 );
	}
	else
	{
		normal = in_normal;
	}

	vec3 result = base_color;
	out_color = vec4( result, 1.0 );
}

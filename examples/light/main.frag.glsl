#version 460

layout( location = 0 ) in vec3 in_normal;
layout( location = 1 ) in vec2 in_tex_coord;
layout( location = 2 ) in vec3 in_frag_pos;
layout( location = 3 ) in vec3 in_view_pos;
layout( location = 4 ) in mat3 in_tbn;

layout( location = 0 ) out vec4 out_color;

struct Material
{
	vec4  base_color_factor;
	vec4  emissive_factor;
	float metallic_factor;
	float roughness_factor;
	float emissive_strength;
	float alpha_cutoff;
	int   base_color_texture;
	int   normal_texture;
	int   ambient_occlusion_texture;
	int   metallic_roughness_texture;
	int   emissive_texture;
	int   pad0;
	int   pad1;
	int   pad2;
};

layout( std140, set = 0, binding = 2 ) readonly buffer u_materials
{
	Material materials[];
}
materials;

layout( push_constant ) uniform constants
{
	uint instance_id; // DirectX12 compatibility
}
pc;

#define TEXTURE_COUNT 5
layout( set = 1, binding = 0 ) uniform sampler u_sampler;
layout( set = 1, binding = 1 ) uniform texture2D u_textures[ TEXTURE_COUNT ];

const float PI = 3.14159265359;

vec3
fresnel_schlick( float cos_theta, vec3 f0 )
{
	return f0 + ( 1.0 - f0 ) * pow( clamp( 1.0 - cos_theta, 0.0, 1.0 ), 5.0 );
}

float
distribution_ggx( vec3 n, vec3 h, float roughness )
{
	float a      = roughness * roughness;
	float a2     = a * a;
	float ndoth  = max( dot( n, h ), 0.0 );
	float ndoth2 = ndoth * ndoth;

	float num   = a2;
	float denom = ( ndoth2 * ( a2 - 1.0 ) + 1.0 );
	denom       = PI * denom * denom;

	return num / denom;
}

float
geometry_schlick_ggx( float ndotv, float roughness )
{
	float r = ( roughness + 1.0 );
	float k = ( r * r ) / 8.0;

	float num   = ndotv;
	float denom = ndotv * ( 1.0 - k ) + k;

	return num / denom;
}

float
geometry_smith( vec3 n, vec3 v, vec3 l, float roughness )
{
	float ndotv = max( dot( n, v ), 0.0 );
	float ndotl = max( dot( n, l ), 0.0 );
	float ggx2  = geometry_schlick_ggx( ndotv, roughness );
	float ggx1  = geometry_schlick_ggx( ndotl, roughness );

	return ggx1 * ggx2;
}

void
main()
{
	vec3 light_color    = vec3( 1.0, 0.5, 0 );
	vec3 light_position = vec3( -1.0, 0.0, 0.0 );

	Material mat = materials.materials[ pc.instance_id ];

	vec4 base_color = mat.base_color_factor;
	if ( mat.base_color_texture != -1 )
	{
		base_color = texture(sampler2D(u_textures[ mat.base_color_texture ], u_sampler), in_tex_coord);
	}

	if ( base_color.a < mat.alpha_cutoff )
	{
		discard;
	}

	vec3 n = in_normal;
	if ( mat.normal_texture != -1 )
	{
		n = texture(sampler2D(u_textures[ mat.normal_texture ], u_sampler), in_tex_coord).rgb;
		n = in_tbn * ( n * 2.0 - 1.0 );
	}

	float metallic  = mat.metallic_factor;
	float roughness = mat.roughness_factor;
	if ( mat.metallic_roughness_texture != -1 )
	{
		vec3 metallic_roughness = texture(sampler2D(u_textures[ mat.metallic_roughness_texture ], u_sampler), in_tex_coord).rgb;
		metallic                = metallic_roughness.b * metallic;
		roughness               = metallic_roughness.g * roughness;
	}
	else
	{
		roughness = clamp( roughness, 0.004, 1.0 );
		metallic  = clamp( metallic, 0.0, 1.0 );
	}

	n      = normalize( n );
	vec3 v = normalize( in_view_pos - in_frag_pos );

	vec3 f0 = vec3( 0.04 );
	f0      = mix( f0, base_color.rgb, metallic );

	vec3 color = vec3( 0.0 );

	{
		vec3  l           = normalize( light_position - in_frag_pos );
		vec3  h           = normalize( v + l );
		float distance    = length( light_position - in_frag_pos );
		float attenuation = 1.0 / ( distance * distance );
		vec3  radiance    = light_color * attenuation;

		float ndf = distribution_ggx( n, h, roughness );
		float g   = geometry_smith( n, v, l, roughness );
		vec3  f   = fresnel_schlick( max( dot( h, v ), 0.0 ), f0 );

		vec3 ks = f;
		vec3 kd = vec3( 1.0 ) - ks;
		kd *= 1.0 - metallic;

		vec3  num = ndf * g * f;
		float denom =
		    4.0 * max( dot( n, v ), 0.0 ) * max( dot( n, l ), 0.0 ) + 0.0001;
		vec3 specular = num / denom;

		float ndotl = max( dot( n, l ), 0.0 );
		color += ( kd * base_color.rgb / PI + specular ) * radiance * ndotl;
	}

	if ( mat.ambient_occlusion_texture != -1 )
	{
		float ao = texture(sampler2D(u_textures[ mat.ambient_occlusion_texture ], u_sampler), in_tex_coord).r;
		color    = mix( color, color * ao, 1.0 ); // TODO:
	}

	vec3 emissive_factor = vec3( 0.0 );
	if ( mat.emissive_texture != -1 )
	{
		emissive_factor = texture(sampler2D(u_textures[ mat.emissive_texture ], u_sampler), in_tex_coord).rgb;
	}

	color += emissive_factor * mat.emissive_strength;

	out_color = vec4( color, 1.0 );
}

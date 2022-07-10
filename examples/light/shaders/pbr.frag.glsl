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

layout( set = 0, binding = 3 ) uniform texture2D u_brdf_lut;
layout( set = 0, binding = 4 ) uniform textureCube u_irradiance_map;
layout( set = 0, binding = 5 ) uniform textureCube u_specular_map;

layout( push_constant ) uniform constants
{
	uint instance_id; // DirectX12 compatibility
}
pc;

#define TEXTURE_COUNT 5
layout( set = 1, binding = 0 ) uniform sampler u_sampler;
layout( set = 1, binding = 1 ) uniform texture2D u_textures[ TEXTURE_COUNT ];

const float PI = 3.14159265359;

struct Light
{
	vec3 color;
	vec3 position;
};

#define LIGHT_COUNT 1
const Light lights[ LIGHT_COUNT ] = {
    { vec3( 100.0 ), vec3( -5.0, 2.0, 0.0 ) },
};

vec4
srgb_to_linear( vec4 c )
{
	return vec4( pow( c.rgb, vec3( 2.2 ) ), c.a );
}

vec3
fresnel_schlick( float cos_theta, vec3 f0 )
{
	return f0 + ( 1.0 - f0 ) * pow( clamp( 1.0 - cos_theta, 0.0, 1.0 ), 5.0 );
}

vec3
fresnel_schlick_roughness( float cos_theta, vec3 f0, float roughness )
{
	return f0 + ( max( vec3( 1.0 - roughness ), f0 ) - f0 ) *
	                pow( clamp( 1.0 - cos_theta, 0.0, 1.0 ), 5.0 );
}

float
distribution_ggx( float ndoth, float roughness )
{
	float a      = roughness * roughness;
	float a2     = a * a;
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
geometry_smith( float ndotv, float ndotl, float roughness )
{
	float ggx2 = geometry_schlick_ggx( ndotv, roughness );
	float ggx1 = geometry_schlick_ggx( ndotl, roughness );

	return ggx1 * ggx2;
}

void
main()
{
	Material mat = materials.materials[ pc.instance_id ];

	vec4 base_color = mat.base_color_factor;
	if ( mat.base_color_texture != -1 )
	{
		base_color = srgb_to_linear( texture(
		    sampler2D( u_textures[ mat.base_color_texture ], u_sampler ),
		    in_tex_coord ) );
	}

	if ( base_color.a < mat.alpha_cutoff )
	{
		discard;
	}

	vec3 n = in_normal;
	if ( mat.normal_texture != -1 )
	{
		n = texture( sampler2D( u_textures[ mat.normal_texture ], u_sampler ),
		             in_tex_coord )
		        .rgb;
		n.z = sqrt( 1.0 - dot( n.xy, n.xy ) );
		n   = in_tbn * ( n * 2.0 - 1.0 );
	}
	n = normalize( n );

	float metallic  = mat.metallic_factor;
	float roughness = mat.roughness_factor;
	if ( mat.metallic_roughness_texture != -1 )
	{
		vec3 metallic_roughness =
		    texture( sampler2D( u_textures[ mat.metallic_roughness_texture ],
		                        u_sampler ),
		             in_tex_coord )
		        .rgb;
		metallic  = metallic_roughness.b * metallic;
		roughness = metallic_roughness.g * roughness;
	}
	else
	{
		roughness = clamp( roughness, 0.004, 1.0 );
		metallic  = clamp( metallic, 0.0, 1.0 );
	}

	vec3 v = normalize( in_view_pos - in_frag_pos );
	vec3 r = reflect( -v, n );

	vec3 f0 = vec3( 0.04 );
	f0      = mix( f0, base_color.rgb, metallic );

	float ndotv = clamp( abs( dot( n, v ) ), 0.001, 1.0 );

	vec3 lo = vec3( 0.0 );
	for ( int i = 0; i < LIGHT_COUNT; ++i )
	{
		vec3  l           = normalize( lights[ i ].position - in_frag_pos );
		vec3  h           = normalize( v + l );
		float distance    = length( lights[ i ].position - in_frag_pos );
		float attenuation = 1.0 / ( distance * distance );
		vec3  radiance    = lights[ i ].color * attenuation;

		float ndotl = clamp( dot( n, l ), 0.001, 1.0 );
		float hdotv = clamp( dot( h, v ), 0.0, 1.0 );

		float ndf = distribution_ggx( ndotl, roughness );
		float g   = geometry_smith( ndotv, ndotl, roughness );
		vec3  f   = fresnel_schlick( hdotv, f0 );

		vec3 ks = f;
		vec3 kd = vec3( 1.0 ) - ks;
		kd *= 1.0 - metallic;

		vec3  num      = ndf * g * f;
		float denom    = 4.0 * ndotv * ndotl + 0.0001;
		vec3  specular = num / denom;

		lo += ( kd * base_color.rgb / PI + specular ) * radiance * ndotl;
	}

	vec3 f = fresnel_schlick_roughness( ndotv, f0, roughness );

	vec3 ks = f;
	vec3 kd = 1.0 - ks;
	kd *= 1.0 - metallic;

	vec3 irradiance =
	    texture( samplerCube( u_irradiance_map, u_sampler ), n ).rgb;
	vec3 diffuse = irradiance * base_color.rgb;

	const float MAX_REFLECTION_LOD = 4.0;
	vec3        prefiltered_color =
	    textureLod( samplerCube( u_specular_map, u_sampler ),
	                r,
	                roughness * MAX_REFLECTION_LOD )
	        .rgb;
	vec2 brdf =
	    texture( sampler2D( u_brdf_lut, u_sampler ), vec2( ndotv, roughness ) )
	        .rg;
	vec3 specular = prefiltered_color * ( f * brdf.x + brdf.y );

	vec3 ambient = ( kd * diffuse + specular );

	vec3 color = ambient + lo;

	if ( mat.ambient_occlusion_texture != -1 )
	{
		float ao =
		    texture( sampler2D( u_textures[ mat.ambient_occlusion_texture ],
		                        u_sampler ),
		             in_tex_coord )
		        .r;
		color = mix( color, color * ao, 1.0 ); // TODO:
	}

	vec3 emissive_factor = vec3( 0.0 );
	if ( mat.emissive_texture != -1 )
	{
		emissive_factor =
		    srgb_to_linear(
		        texture(
		            sampler2D( u_textures[ mat.emissive_texture ], u_sampler ),
		            in_tex_coord ) )
		        .rgb;
	}

	color += emissive_factor * mat.emissive_strength;

	color = color / ( color + vec3( 1.0 ) );

	out_color = vec4( color, base_color.a );
}

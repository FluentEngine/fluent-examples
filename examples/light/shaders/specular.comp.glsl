#version 460

layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( set = 0, binding = 0 ) uniform sampler u_sampler;
layout( set = 0, binding = 1 ) uniform textureCube u_src;
layout( set = 0, binding = 2, rgba32f ) uniform image2DArray u_dst;

#define IMPORTANCE_SAMPLE_COUNT 64
#define PI                      3.14159265359

layout( push_constant ) uniform constants
{
	uint  mip_size;
	float roughness;
}
pc;

float
radical_inverse_vdc( uint bits )
{
	bits = ( bits << 16u ) | ( bits >> 16u );
	bits = ( ( bits & 0x55555555u ) << 1u ) | ( ( bits & 0xAAAAAAAAu ) >> 1u );
	bits = ( ( bits & 0x33333333u ) << 2u ) | ( ( bits & 0xCCCCCCCCu ) >> 2u );
	bits = ( ( bits & 0x0F0F0F0Fu ) << 4u ) | ( ( bits & 0xF0F0F0F0u ) >> 4u );
	bits = ( ( bits & 0x00FF00FFu ) << 8u ) | ( ( bits & 0xFF00FF00u ) >> 8u );
	return float( bits ) * 2.3283064365386963e-10;
}

vec2
hammersley( uint i, uint n )
{
	return vec2( float( i ) / float( n ), radical_inverse_vdc( i ) );
}

float
distribution_ggx( vec3 n, vec3 h, float roughness )
{
	float a      = roughness * roughness;
	float a2     = a * a;
	float ndoth  = max( dot( n, h ), 0.0 );
	float ndoth2 = ndoth * ndoth;

	float nom   = a2;
	float denom = ( ndoth2 * ( a2 - 1.0 ) + 1.0 );
	denom       = PI * denom * denom;

	return nom / denom;
}

vec3
importance_sample_ggx( vec2 xi, vec3 n, float roughness )
{
	float a = roughness * roughness;

	float phi       = 2.0 * PI * xi.x;
	float cos_theta = sqrt( ( 1.0 - xi.y ) / ( 1.0 + ( a * a - 1.0 ) * xi.y ) );
	float sin_theta = sqrt( 1.0 - cos_theta * cos_theta );

	vec3 h;
	h.x = cos( phi ) * sin_theta;
	h.y = sin( phi ) * sin_theta;
	h.z = cos_theta;

	vec3 up =
	    abs( n.z ) < 0.999 ? vec3( 0.0, 0.0, 1.0 ) : vec3( 1.0, 0.0, 0.0 );
	vec3 tangent   = normalize( cross( up, n ) );
	vec3 bitangent = cross( n, tangent );

	vec3 sample_vec = tangent * h.x + bitangent * h.y + n * h.z;
	return normalize( sample_vec );
}

void
main()
{
	uvec3 thread_pos = uvec3( gl_GlobalInvocationID );

	float mip_roughness = pc.roughness;
	uint  mip_size      = pc.mip_size;

	if ( thread_pos.x >= mip_size || thread_pos.y >= mip_size )
		return;

	vec2 texcoords = vec2( float( thread_pos.x + 0.5 ) / mip_size,
	                       float( thread_pos.y + 0.5 ) / mip_size );

	vec3 sphere_dir = vec3( 1.0 );

	if ( thread_pos.z <= 0 )
		sphere_dir = normalize(
		    vec3( 0.5, -( texcoords.y - 0.5 ), -( texcoords.x - 0.5 ) ) );
	else if ( thread_pos.z <= 1 )
		sphere_dir = normalize(
		    vec3( -0.5, -( texcoords.y - 0.5 ), texcoords.x - 0.5 ) );
	else if ( thread_pos.z <= 2 )
		sphere_dir =
		    normalize( vec3( texcoords.x - 0.5, 0.5, texcoords.y - 0.5 ) );
	else if ( thread_pos.z <= 3 )
		sphere_dir = normalize(
		    vec3( texcoords.x - 0.5, -0.5, -( texcoords.y - 0.5 ) ) );
	else if ( thread_pos.z <= 4 )
		sphere_dir =
		    normalize( vec3( texcoords.x - 0.5, -( texcoords.y - 0.5 ), 0.5 ) );
	else if ( thread_pos.z <= 5 )
		sphere_dir = normalize(
		    vec3( -( texcoords.x - 0.5 ), -( texcoords.y - 0.5 ), -0.5 ) );

	vec3 n = sphere_dir;
	vec3 r = n;
	vec3 v = r;

	float total_weight      = 0.0;
	vec4  prefiltered_color = vec4( 0.0, 0.0, 0.0, 0.0 );

	vec2 dim = vec2( textureSize( samplerCube( u_src, u_sampler ), 0 ) );

	float src_texture_size = max( dim[ 0 ], dim[ 1 ] );

	for ( int i = 0; i < IMPORTANCE_SAMPLE_COUNT; ++i )
	{
		vec2 xi = hammersley( i, IMPORTANCE_SAMPLE_COUNT );
		vec3 h  = importance_sample_ggx( xi, n, mip_roughness );
		vec3 l  = normalize( 2.0 * dot( v, h ) * h - v );

		float ndotl = max( dot( n, l ), 0.0 );
		if ( ndotl > 0.0 )
		{
			float d     = distribution_ggx( n, h, mip_roughness );
			float ndoth = max( dot( n, h ), 0.0 );
			float hdotv = max( dot( h, v ), 0.0 );
			float pdf   = d * ndoth / ( 4.0 * hdotv ) + 0.0001;

			float sa_texel =
			    4.0 * PI / ( 6.0 * src_texture_size * src_texture_size );
			float sa_sample =
			    1.0 / ( float( IMPORTANCE_SAMPLE_COUNT ) * pdf + 0.0001 );

			float mip_level =
			    mip_roughness == 0.0
			        ? 0.0
			        : max( 0.5 * log2( sa_sample / sa_texel ) + 1.0f, 0.0f );

			prefiltered_color +=
			    textureLod( samplerCube( u_src, u_sampler ), l, mip_level ) *
			    ndotl;

			total_weight += ndotl;
		}
	}

	prefiltered_color = prefiltered_color / total_weight;
	imageStore( u_dst, ivec3( thread_pos ), prefiltered_color );
}
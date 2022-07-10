#version 460

#define SAMPLE_DELTA 0.25
#define PI           3.14159265359

layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( set = 0, binding = 0 ) uniform sampler u_sampler;
layout( set = 0, binding = 1 ) uniform textureCube u_src;
layout( set = 0, binding = 2, rgba32f ) uniform image2DArray u_dst;

vec4
compute_irradiance( vec3 n )
{
	vec4 irradiance = vec4( 0.0, 0.0, 0.0, 0.0 );

	vec3 up    = vec3( 0.0, 1.0, 0.0 );
	vec3 right = cross( up, n );
	up         = cross( n, right );

	float nr_samples = 0.0;

	for ( float phi = 0.0; phi < 2.0 * PI; phi += SAMPLE_DELTA )
	{
		for ( float theta = 0.0; theta < 0.5 * PI; theta += SAMPLE_DELTA )
		{
			vec3 tangent_sample = vec3( sin( theta ) * cos( phi ),
			                            sin( theta ) * sin( phi ),
			                            cos( theta ) );

			vec3 sample_vec = tangent_sample.x * right + tangent_sample.y * up +
			                  tangent_sample.z * n;

			vec4 sampled_value =
			    texture( samplerCube( u_src, u_sampler ), sample_vec );

			irradiance += vec4( sampled_value.rgb * cos( theta ) * sin( theta ),
			                    sampled_value.a );
			nr_samples++;
		}
	}

	return PI * irradiance * ( 1.0 / float( nr_samples ) );
}

void
main()
{
	vec3 thread_pos   = vec3( gl_GlobalInvocationID );
	uint pixel_offset = 0;

	for ( uint i = 0; i < thread_pos.z; ++i ) { pixel_offset += 32 * 32; }

	vec2 texcoords = vec2( float( thread_pos.x + 0.5 ) / 32.0f,
	                       float( thread_pos.y + 0.5 ) / 32.0f );

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

	vec4 irradiance = compute_irradiance( sphere_dir );

	imageStore( u_dst, ivec3( thread_pos ), irradiance );
}
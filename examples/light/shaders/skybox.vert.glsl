#version 450

const vec3 positions[] = {
    vec3( -1.0, -1.0, -1.0 ),
    vec3( 1.0, 1.0, -1.0 ),
    vec3( 1.0, -1.0, -1.0 ),
    vec3( 1.0, 1.0, -1.0 ),
    vec3( -1.0, -1.0, -1.0 ),
    vec3( -1.0, 1.0, -1.0 ),
    // front face
    vec3( -1.0, -1.0, 1.0 ),
    vec3( 1.0, -1.0, 1.0 ),
    vec3( 1.0, 1.0, 1.0 ),
    vec3( 1.0, 1.0, 1.0 ),
    vec3( -1.0, 1.0, 1.0 ),
    vec3( -1.0, -1.0, 1.0 ),
    // left face
    vec3( -1.0, 1.0, 1.0 ),
    vec3( -1.0, 1.0, -1.0 ),
    vec3( -1.0, -1.0, -1.0 ),
    vec3( -1.0, -1.0, -1.0 ),
    vec3( -1.0, -1.0, 1.0 ),
    vec3( -1.0, 1.0, 1.0 ),
    // right face
    vec3( 1.0, 1.0, 1.0 ),
    vec3( 1.0, -1.0, -1.0 ),
    vec3( 1.0, 1.0, -1.0 ),
    vec3( 1.0, -1.0, -1.0 ),
    vec3( 1.0, 1.0, 1.0 ),
    vec3( 1.0, -1.0, 1.0 ),
    // bottom face
    vec3( -1.0, -1.0, -1.0 ),
    vec3( 1.0, -1.0, -1.0 ),
    vec3( 1.0, -1.0, 1.0 ),
    vec3( 1.0, -1.0, 1.0 ),
    vec3( -1.0, -1.0, 1.0 ),
    vec3( -1.0, -1.0, -1.0 ),
    // top face
    vec3( -1.0, 1.0, -1.0 ),
    vec3( 1.0, 1.0, 1.0 ),
    vec3( 1.0, 1.0, -1.0 ),
    vec3( 1.0, 1.0, 1.0 ),
    vec3( -1.0, 1.0, -1.0 ),
    vec3( -1.0, 1.0, 1.0 ),
};

layout( set = 0, binding = 0 ) uniform ubo
{
	mat4 projection;
	mat4 view;
	vec4 view_pos;
}
u;

layout( location = 0 ) out vec3 out_world_pos;

void
main()
{
	out_world_pos = positions[ gl_VertexIndex ];

	mat4 rot_view = mat4( mat3( u.view ) );
	vec4 clip_pos = u.projection * rot_view * vec4( out_world_pos, 1.0 );

	gl_Position = clip_pos.xyww;
}
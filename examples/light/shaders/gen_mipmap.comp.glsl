#version 460

layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( set = 0, binding = 0 ) sampler u_sampler;
layout( set = 0, binding = 1 ) texture2D u_src;
layout( set = 0, binding = 2 ) image2D u_dst;

void main()
{

}
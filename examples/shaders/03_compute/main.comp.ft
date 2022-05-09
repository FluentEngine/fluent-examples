#include "fluent.hlsl"

rwtexture2D( float4, u_output_image, 1 );

struct Input
{
    uint3 invocation_id : SV_DispatchThreadID;
};

[ numthreads( 16, 16, 1 ) ] void
main( Input stage_input )
{
	int2 coord              = int2( stage_input.invocation_id.xy );
	u_output_image[ coord ] = float4( 1.0 );
}

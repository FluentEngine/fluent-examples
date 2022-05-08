#include "fluent.hlsl"

cbuf(global_ubo, 0)
{
    float4x4 projection;
    float4x4 view;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0;
    float3 frag_pos : TEXCOORD1;
};

VertexOutput main(VertexInput stage_input)
{
    VertexOutput stage_output;
    stage_output.position = mul(projection, mul(view, float4(stage_input.position, 1.0f)));
    stage_output.normal = stage_input.normal;
    stage_output.tex_coord = stage_input.tex_coord;
    stage_output.frag_pos = stage_input.position.xyz;
    return stage_output;
}

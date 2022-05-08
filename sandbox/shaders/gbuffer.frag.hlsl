#include "fluent.hlsl"

sampler(u_sampler, 1);
texture2D(float4, u_texture, 2);

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD0;
    float3 frag_pos : TEXCOORD1;
};

struct FragmentOutput
{
    float4 position : SV_Target0;
    float4 normal : SV_Target1;
    float4 albedo_spec : SV_Target2;
};

FragmentOutput main(VertexOutput stage_input)
{
    FragmentOutput output;
    output.position = float4(stage_input.frag_pos, 1.0);
    output.normal = float4(normalize(stage_input.normal), 1.0);
    output.albedo_spec = float4(u_texture.Sample(u_sampler, stage_input.tex_coord).rgb, 0.2);
    return output;
}

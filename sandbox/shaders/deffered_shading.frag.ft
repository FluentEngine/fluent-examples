#include "fluent.hlsl"

sampler(u_sampler, 0);
texture2D(float4, u_position, 1);
texture2D(float4, u_normal, 2);
texture2D(float4, u_albedo_spec, 3);

struct VertexOutput
{
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

float4 main(VertexOutput stage_input)
{
    // TODO:
    float3 light_pos = float3(0.0, 6.0, 0.0);
    float3 view_pos = float3(0.0, 0.0, 3.0);

    float3 frag_pos = u_position.Sample(u_sampler, stage_input.tex_coord).rgb;
    float3 normal = u_normal.Sample(u_sampler, stage_input.tex_coord).rgb;
    float3 diffuse = u_albedo_spec.Sample(u_sampler, stage_input.tex_coord).rgb;
    float3 specular = u_albedo_spec.Sample(u_sampler, stage_input.tex_coord).a;

    float3 ambient = diffuse * 0.05f;

    float3 light_dir = normalize(light_pos - frag_pos);
    float3 view_dir = normalize(view_pos - frag_pos);
    float3 reflect_dir = reflect(-light_dir, normal);

    float3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(normal, halfway_dir), 0.0), 32.0);

    return float4(ambient + diffuse + specular * spec, 1.0);
}

static const float2 positions[4] = { float2(-1.0f, 1.0f), float2(-1.0f, -1.0f), float2(1.0f, 1.0f), float2(1.0f, -1.0f) };
static const float2 tex_coords[4] = { float2(0.0f, 0.0f), float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(1.0f, 1.0f) };

struct VertexInput
{
    uint vertex_id : SV_VertexID;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

VertexOutput main(VertexInput stage_input)
{
    VertexOutput stage_output;
    stage_output.position = float4(positions[stage_input.vertex_id], 0.0, 1.0);
    stage_output.tex_coord = tex_coords[stage_input.vertex_id];
    return stage_output;
}

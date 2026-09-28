struct SKYBOX_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

TextureCube skybox_texture : register(t0);
SamplerState skybox_sampler : register(s0);

SKYBOX_OUTPUT VS_Skybox(uint vertex_id : SV_VertexID)
{
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };

    SKYBOX_OUTPUT output;
    output.position = float4(positions[vertex_id], 0.9999f, 1.0f);
    output.uv = positions[vertex_id] * 0.5f + 0.5f;
    output.uv.y = 1.0f - output.uv.y;
    return output;
}

float4 PS_Skybox(float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float3 direction = normalize(float3(uv * 2.0f - 1.0f, 1.0f));
    return skybox_texture.Sample(skybox_sampler, direction);
}

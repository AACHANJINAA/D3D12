cbuffer OBJECT_DATA : register(b0)
{
    row_major float4x4 transform;
    float3 light_direction;
    float light_intensity;
    float3 light_color;
    float padding;
};

Texture2D albedo_texture : register(t0);
SamplerState texture_sampler : register(s0);

struct VERTEX_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VERTEX_OUTPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VERTEX_OUTPUT VS_Triangle(VERTEX_INPUT input)
{
    VERTEX_OUTPUT output;
    output.position = mul(float4(input.position, 1.0f), transform);
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}

float4 PS_Color(float4 position : SV_POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD) : SV_TARGET
{
    float4 albedo = albedo_texture.Sample(texture_sampler, uv);
    float3 surface_normal = normalize(normal);
    float3 to_light = normalize(-light_direction);
    float diffuse = saturate(dot(surface_normal, to_light));
    float3 lighting = 0.15f + diffuse * light_color * light_intensity;
    return float4(albedo.rgb * lighting, albedo.a);
}

float4 PS_Black(float4 position : SV_POSITION) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}

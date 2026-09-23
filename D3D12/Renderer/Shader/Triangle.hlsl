cbuffer OBJECT_DATA : register(b0)
{
    row_major float4x4 transform;
};

struct VERTEX_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VERTEX_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VERTEX_OUTPUT VS_Triangle(VERTEX_INPUT input)
{
    VERTEX_OUTPUT output;
    output.position = mul(float4(input.position, 1.0f), transform);
    output.color = input.color;
    return output;
}

float4 PS_White(float4 position : SV_POSITION, float4 color : COLOR) : SV_TARGET
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}

float4 PS_Black(float4 position : SV_POSITION, float4 color : COLOR) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}

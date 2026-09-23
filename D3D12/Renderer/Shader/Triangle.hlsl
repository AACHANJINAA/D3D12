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
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

float4 PS_Triangle(float4 position : SV_POSITION, float4 color : COLOR) : SV_TARGET
{
    return color;
}

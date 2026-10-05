#include "Mesh.hlsl"

struct INSTANCE_DATA
{
    row_major float4x4 world;
    row_major float4x4 normal_matrix;
    float tangent_orientation;
    float selection;
    float2 padding;
};

StructuredBuffer<INSTANCE_DATA> instances : register(t8);

VERTEX_OUTPUT VS_Instanced(VERTEX_INPUT input, uint instance_id : SV_InstanceID)
{
    INSTANCE_DATA instance = instances[instance_id];
    VERTEX_OUTPUT output;
    output.world_position = mul(float4(input.position, 1), instance.world).xyz;
    output.position = mul(float4(output.world_position, 1), transform);
    output.normal = normalize(mul(input.normal, (float3x3)instance.normal_matrix));
    output.tangent = float4(normalize(mul(input.tangent.xyz, (float3x3)instance.world)),
        input.tangent.w * instance.tangent_orientation);
    output.uv = input.uv;
    output.selection = instance.selection;
    return output;
}

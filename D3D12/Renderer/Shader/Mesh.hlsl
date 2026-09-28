cbuffer OBJECT_DATA : register(b0)
{
    row_major float4x4 transform;
    float3 light_direction;
    float light_intensity;
    float3 light_color;
    float padding;
    float3 camera_position;
    float metallic;
    float roughness;
    float ambient_strength;
    float2 reserved;
};

Texture2D albedo_texture : register(t0);
Texture2D normal_texture : register(t1);
Texture2D metal_roughness_texture : register(t2);
Texture2D ao_texture : register(t3);
Texture2D emissive_texture : register(t4);
TextureCube environment_texture : register(t5);
Texture2D brdf_lut : register(t6);
TextureCube specular_environment_texture : register(t7);
SamplerState texture_sampler : register(s0);

struct VERTEX_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct VERTEX_OUTPUT
{
    float4 position : SV_POSITION;
    float3 world_position : POSITION1;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

VERTEX_OUTPUT VS_Mesh(VERTEX_INPUT input)
{
    VERTEX_OUTPUT output;
    output.world_position = input.position;
    output.position = mul(float4(output.world_position, 1.0f), transform);
    output.normal = input.normal;
    output.tangent = input.tangent;
    output.uv = input.uv;
    return output;
}

float3 fresnel_schlick(float cos_theta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - saturate(cos_theta), 5.0f);
}

float distribution_ggx(float n_dot_h, float alpha)
{
    float alpha_squared = alpha * alpha;
    float denominator = n_dot_h * n_dot_h * (alpha_squared - 1.0f) + 1.0f;
    return alpha_squared / max(3.14159265f * denominator * denominator, 0.0001f);
}

float geometry_schlick_ggx(float n_dot_v, float roughness_value)
{
    float k = (roughness_value + 1.0f) * (roughness_value + 1.0f) / 8.0f;
    return n_dot_v / max(n_dot_v * (1.0f - k) + k, 0.0001f);
}

float geometry_smith(float n_dot_v, float n_dot_l, float roughness_value)
{
    return geometry_schlick_ggx(n_dot_v, roughness_value) *
        geometry_schlick_ggx(n_dot_l, roughness_value);
}

float3 calculate_irradiance_sh(float3 normal_value)
{
    const float3 l00 = float3(0.384992f, 0.443180f, 0.523824f);
    const float3 l1_1 = float3(0.048705f, 0.059345f, 0.076326f);
    const float3 l10 = float3(-0.089855f, -0.106461f, -0.133221f);
    const float3 l11 = float3(0.012586f, 0.014529f, 0.017559f);
    const float3 l2_2 = float3(-0.017367f, -0.021074f, -0.027003f);
    const float3 l2_1 = float3(-0.006745f, -0.007963f, -0.010173f);
    const float3 l20 = float3(0.038166f, 0.044129f, 0.052675f);
    const float3 l21 = float3(0.002820f, 0.003429f, 0.004455f);
    const float3 l22 = float3(-0.040182f, -0.049419f, -0.064508f);
    const float c1 = 0.429043f;
    const float c2 = 0.511664f;
    const float c3 = 0.743125f;
    const float c4 = 0.886227f;
    const float c5 = 0.247708f;
    return max(
        c4 * l00 -
        c1 * l22 * (normal_value.x * normal_value.x - normal_value.y * normal_value.y) +
        c3 * l20 * normal_value.z * normal_value.z - c5 * l20 +
        2.0f * c1 * l2_2 * normal_value.x * normal_value.y +
        2.0f * c1 * l21 * normal_value.x * normal_value.z +
        2.0f * c1 * l2_1 * normal_value.y * normal_value.z +
        2.0f * c2 * l11 * normal_value.x +
        2.0f * c2 * l1_1 * normal_value.y +
        2.0f * c2 * l10 * normal_value.z,
        0.0f);
}

float4 PS_Mesh(
    float4 position : SV_POSITION,
    float3 world_position : POSITION1,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 uv : TEXCOORD) : SV_TARGET
{
    float4 albedo = albedo_texture.Sample(texture_sampler, uv);
    float4 metal_roughness = metal_roughness_texture.Sample(texture_sampler, uv);
    float metallic_value = metal_roughness.b;
    float roughness_value = max(metal_roughness.g, 0.04f);
    float ambient_occlusion = ao_texture.Sample(texture_sampler, uv).r;
    float3 emissive = emissive_texture.Sample(texture_sampler, uv).rgb;

    float3 geometric_normal = normalize(normal);
    float3 surface_tangent = normalize(
        tangent.xyz - geometric_normal * dot(geometric_normal, tangent.xyz));
    float3 bitangent = normalize(cross(geometric_normal, surface_tangent) * tangent.w);
    float3 tangent_normal = normal_texture.Sample(texture_sampler, uv).xyz * 2.0f - 1.0f;
    float3 surface_normal = normalize(
        surface_tangent * tangent_normal.x +
        bitangent * tangent_normal.y +
        geometric_normal * tangent_normal.z);
    float3 to_light = normalize(-light_direction);
    float3 to_view = normalize(camera_position - world_position);
    float3 half_vector = normalize(to_view + to_light);

    float n_dot_l = saturate(dot(surface_normal, to_light));
    float n_dot_v = saturate(dot(surface_normal, to_view));
    float n_dot_h = saturate(dot(surface_normal, half_vector));
    float v_dot_h = saturate(dot(to_view, half_vector));
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metallic_value);
    float3 fresnel = fresnel_schlick(v_dot_h, f0);
    float distribution = distribution_ggx(n_dot_h, roughness_value);
    float geometry = geometry_smith(n_dot_v, n_dot_l, roughness_value);
    float3 specular = distribution * fresnel * geometry /
        max(4.0f * n_dot_v * max(n_dot_l, 0.0001f), 0.0001f);

    float3 diffuse_weight = (1.0f - fresnel) * (1.0f - metallic_value);
    float3 diffuse = diffuse_weight * albedo.rgb / 3.14159265f;
    float3 radiance = light_color * light_intensity;
    float3 direct_lighting = (diffuse + specular) * radiance * n_dot_l;
    if (n_dot_l <= 0.0f || n_dot_v <= 0.0f)
    {
        direct_lighting = 0.0f;
    }
    float3 environment_direction = reflect(-to_view, surface_normal);
    float3 environment_diffuse = calculate_irradiance_sh(surface_normal);
    float3 environment_specular = specular_environment_texture.SampleLevel(
        texture_sampler, environment_direction, roughness_value * 8.0f).rgb;
    float2 brdf = brdf_lut.Sample(texture_sampler, float2(n_dot_v, roughness_value)).rg;
    float3 environment_fresnel = fresnel_schlick(n_dot_v, f0);
    float3 split_sum_specular = environment_specular *
        (environment_fresnel * brdf.x + brdf.y);
    float3 environment_lighting =
        environment_diffuse * albedo.rgb * (1.0f - metallic_value) +
        split_sum_specular;
    float3 hdr_color =
        albedo.rgb * ambient_strength * ambient_occlusion +
        direct_lighting + environment_lighting + emissive;
    float3 tone_mapped_color = hdr_color / (hdr_color + 1.0f);
    tone_mapped_color = pow(max(tone_mapped_color, 0.0f), 1.0f / 2.2f);
    return float4(tone_mapped_color, albedo.a);
}

float4 PS_Wireframe(float4 position : SV_POSITION) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}

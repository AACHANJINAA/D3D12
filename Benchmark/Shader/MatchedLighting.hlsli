#ifndef MATCHED_LIGHTING_INCLUDED
#define MATCHED_LIGHTING_INCLUDED
struct BENCH_POINT { float4 position_radius; float4 radiance; };
cbuffer BENCH_LIGHTS : register(b6)
{
    uint bench_count;
    uint bench_matched;
    uint bench_directional;
    float bench_environment;
    BENCH_POINT bench_points[256];
    uint bench_compact;
    float bench_width, bench_height, bench_padding;
    row_major float4x4 bench_inverse_view_projection;
};

float3 bench_brdf(float3 albedo, float3 n, float3 v, float3 l, float metallic, float roughness)
{
    float nl = saturate(dot(n, l)), nv = saturate(dot(n, v));
    float3 h = normalize(v + l + 1e-8);
    float nh = saturate(dot(n, h)), vh = saturate(dot(v, h));
    float3 f0 = lerp(0.04.xxx, albedo, metallic);
    float3 f = f0 + (1 - f0) * pow(1 - vh, 5);
    float a = roughness * roughness, a2 = a * a;
    float d = a2 / max(3.14159265 * pow(nh * nh * (a2 - 1) + 1, 2), 1e-7);
    float k = (roughness + 1) * (roughness + 1) / 8;
    float g = nv / max(nv * (1 - k) + k, 1e-6) * nl / max(nl * (1 - k) + k, 1e-6);
    return ((1 - f) * (1 - metallic) * albedo / 3.14159265 + d * f * g / max(4 * nv * nl, 1e-5)) * nl;
}

float3 bench_irradiance(float3 n)
{
    const float3 sh[9] = {
        float3(.384992,.443180,.523824), float3(.048705,.059345,.076326),
        float3(-.089855,-.106461,-.133221), float3(.012586,.014529,.017559),
        float3(-.017367,-.021074,-.027003), float3(-.006745,-.007963,-.010173),
        float3(.038166,.044129,.052675), float3(.002820,.003429,.004455),
        float3(-.040182,-.049419,-.064508) };
    return max(.886227 * sh[0] - .429043 * sh[8] * (n.x*n.x-n.y*n.y) +
        .743125 * sh[6] * n.z*n.z - .247708 * sh[6] +
        .858086 * (sh[4]*n.x*n.y + sh[7]*n.x*n.z + sh[5]*n.y*n.z) +
        1.023328 * (sh[3]*n.x + sh[1]*n.y + sh[2]*n.z), 0);
}

float4 bench_shade(float4 albedo, float3 world, float3 n, float metallic, float roughness,
    float ao, float3 emissive, float3 camera, float3 direction, float3 directional_radiance)
{
    n = normalize(n);
    float3 v = normalize(camera - world);
    float3 color = 0;
    if (bench_directional)
        color = bench_brdf(albedo.rgb, n, v, normalize(-direction), metallic, roughness) * directional_radiance;
    // Identical full light loop in both paths; no tile, distance or light-list culling.
    [loop] for (uint i = 0; i < bench_count; ++i)
    {
        float3 delta = bench_points[i].position_radius.xyz - world;
        float distance2 = dot(delta, delta);
        float radius = bench_points[i].position_radius.w;
        float falloff = saturate(1 - distance2 / (radius * radius));
        float attenuation = falloff * falloff / max(distance2, 1.0);
        color += bench_brdf(albedo.rgb, n, v, delta * rsqrt(max(distance2, 1e-8)), metallic, roughness) *
            bench_points[i].radiance.rgb * attenuation;
    }
    float nv = saturate(dot(n, v));
    float3 f0 = lerp(0.04.xxx, albedo.rgb, metallic);
    float3 f = f0 + (1 - f0) * pow(1 - nv, 5);
    float3 environment = BENCH_SPECULAR.SampleLevel(BENCH_SAMPLER, reflect(-v, n), roughness * 8).rgb;
    float2 brdf = BENCH_BRDF.SampleLevel(BENCH_SAMPLER, float2(nv, roughness), 0).rg;
    color += bench_environment * ao * (bench_irradiance(n) * albedo.rgb * (1 - metallic) +
        environment * (f * brdf.x + brdf.y));
    color = max(color + emissive, 0);
    return float4(pow(color / (1 + color), 1.0 / 2.2), albedo.a);
}
#endif

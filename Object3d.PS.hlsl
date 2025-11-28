#include "Particle.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    // テクスチャ色
    float4 texColor = gTexture.Sample(gSampler, input.texcoord);

    // 基本色（マテリアル × テクスチャ）
    float3 baseColor = gMaterial.color.rgb * texColor.rgb;

    // ライティングONのときだけ計算
    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(input.normal);
        float3 L = normalize(-gLight.direction); // 光の来る方向

        float NdotL = saturate(dot(N, L));
        float3 diffuse = baseColor * gLight.color.rgb * (NdotL * gLight.intensity);

        baseColor = diffuse;
    }

    float alpha = gMaterial.color.a * texColor.a;
    return float4(baseColor, alpha);
}

#include "Particle.hlsli"

// PS 用：テクスチャ (RootParameter[2], t0)
Texture2D gTexture : register(t0);
// サンプラはルートシグネチャで static sampler s0 として作っている想定
SamplerState gSampler : register(s0);

float4 main(VertexOutput pin) : SV_TARGET
{
    // テクスチャ取得
    float4 texColor = gTexture.Sample(gSampler, pin.texcoord);

    // とりあえず「ライティング ON なら平行光源をかける」くらいの簡易処理
    float3 color = texColor.rgb * gMaterial.color.rgb;
    float alpha = texColor.a * gMaterial.color.a;

    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(pin.normal);
        float3 L = -normalize(gDirectionalLight.direction); // 光の逆方向
        float NdotL = saturate(dot(N, L));

        float3 lightColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
        color *= (NdotL * lightColor);
    }

    return float4(color, alpha);
}

#include "object3d.hlsli"


ConstantBuffer<Material> gMaterial : register(b0);


ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 5番目のスロット(t1)に届いているシャドウマップを受け取る
Texture2D<float> gShadowMap : register(t1);

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャのサンプリング（既存）
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // ==========================================================
    // 影の計算
    // ==========================================================
    // 1. ライト視点の座標を [0, 1] の UV 空間に変換します
    float3 lightPos = input.lightSpacePosition.xyz / input.lightSpacePosition.w;
    float2 shadowUV = lightPos.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    float currentDepth = lightPos.z; // このピクセルのライトからの距離

    float shadowFactor = 1.0f; // 影なし（明るい）

    // 2. シャドウマップの範囲内の場合のみ判定します
    if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f && shadowUV.y >= 0.0f && shadowUV.y <= 1.0f)
    {
        // 遮蔽物までの最短距離を読み取ります
        float mapDepth = gShadowMap.Sample(gSampler, shadowUV);
        
        // 3. 自分の距離の方が奥にあれば影です。
        // ※ 0.0005f は「シャドウアクネ（シマシマ）」を防ぐためのバイアスです。
        if (currentDepth > mapDepth + 0.0005f)
        {
            shadowFactor = 0.6f; // 影の中なら暗くする（0.6〜0.7 くらいが綺麗です）
        }
    }

    // ライティング計算
    if (gMaterial.enableLighting != 0)
    {
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float ambient = 0.35f;
        float shade = cos + ambient;
        
        // ★修正：計算した shade に影の係数（shadowFactor）を掛けます
        output.color = (shade * shadowFactor) * gMaterial.color * textureColor;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}
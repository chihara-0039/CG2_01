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
        // 3. PCF (Percentage Closer Filtering) を用いたソフトシャドウ
        float shadowSum = 0.0f;
        float2 texelSize = 1.0f / 4096.0f; // ShadowMapの解像度に合わせて1ピクセルのサイズを計算
        
        // 周囲3x3ピクセルをサンプリング
        for (int y = -1; y <= 1; ++y)
        {
            for (int x = -1; x <= 1; ++x)
            {
                float2 offset = float2(x, y) * texelSize;
                float mapDepth = gShadowMap.Sample(gSampler, shadowUV + offset);
                
                // 自分の距離の方が奥にあれば影と判定（バイアスを加味）
                float depthDiff = currentDepth - mapDepth;
                if (depthDiff > 0.0005f)
                {
                    // 距離が離れるほど影を薄くする（NDCで0.05以上離れると完全に消える）
                    float fade = saturate(1.0f - (depthDiff / 0.05f));
                    shadowSum += fade;
                }
            }
        }
        
        // 影の濃さを計算（9回のサンプリングのうち、影になった割合）
        // 完全な影(9/9)なら 0.6f、全く影でない(0/9)なら 1.0f に補間
        shadowFactor = lerp(1.0f, 0.6f, shadowSum / 9.0f);
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
#include "Particle.hlsli"

// PS の CBV（b0, b1）
ConstantBuffer<Material> gMaterial : register(b0);


// gTexture / gSampler は hlsli で宣言済み。ここで再宣言しない。

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV 変換
    float4 uv = float4(input.texcoord, 0.0f, 1.0f);
    float4 uvTransformed = mul(uv, gMaterial.uvTransform);

    // サンプル
    float32_t4 tex = gTexture.Sample(gSampler, uvTransformed.xy);

    //色合成
    output.color = gMaterial.color * tex;

    // 透過破棄（必要に応じて閾値調整）
    if (output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャサンプリング
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // テクスチャの色 * パーティクルの色
    output.color = textureColor * input.color;
    
    // アルファテスト (完全に透明な部分は描画しない)
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
    return output;
}
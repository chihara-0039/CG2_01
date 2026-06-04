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

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float2 centeredUv = input.texcoord * 2.0f - 1.0f;
    float softCircleAlpha = saturate((1.0f - length(centeredUv)) * 3.0f);

    output.color = textureColor * input.color;
    output.color.a *= softCircleAlpha;

    if (output.color.a <= 0.001f)
    {
        discard;
    }

    return output;
}

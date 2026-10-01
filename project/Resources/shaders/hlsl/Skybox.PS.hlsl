#include "Skybox.hlsli"

struct Material {
    float32_t4 color;
};
ConstantBuffer<Material> gMaterial : register(b0);

TextureCube<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float3 direction = normalize(input.texcoord);
    float horizon = pow(saturate(1.0f - abs(direction.y)), 3.0f);
    float upperSky = saturate(direction.y * 0.5f + 0.5f);
    float3 skyGradient = lerp(
        float3(0.72f, 0.84f, 1.02f),
        float3(0.88f, 0.95f, 1.05f),
        upperSky);
    float3 atmosphericColor = textureColor.rgb * skyGradient;
    atmosphericColor = lerp(atmosphericColor, float3(1.0f, 0.91f, 0.76f), horizon * 0.20f);
    PixelShaderOutput output;
    output.color = float4(atmosphericColor, textureColor.a) * gMaterial.color;
    return output;
}

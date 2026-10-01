#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float3 ExtractHighlight(float3 color)
{
    const float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    const float peak = max(color.r, max(color.g, color.b));
    // Peak intensity also catches strongly emissive blue stars without making
    // the ordinary blue background bloom across the whole screen.
    const float brightness = max(luminance, peak * 0.82f);
    // Only the hottest highlights contribute. This keeps bright cloud quads
    // and the blue background from revealing their rectangular boundaries.
    const float mask = smoothstep(0.80f, 0.985f, brightness);
    return color * mask;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    uint width;
    uint height;
    gTexture.GetDimensions(width, height);
    const float2 texel = 1.0f / float2(width, height);

    const float4 source = gTexture.Sample(gSampler, input.texcoord);
    float3 glow = 0.0f;
    float weightSum = 0.0f;

    // Closely packed 7x7 samples remove the visible square/grid pattern that a
    // sparse wide kernel produced around strong emissive objects.
    [unroll]
    for (int y = -3; y <= 3; ++y)
    {
        [unroll]
        for (int x = -3; x <= 3; ++x)
        {
            const float2 kernelPos = float2((float)x, (float)y);
            const float weight = exp(-dot(kernelPos, kernelPos) * 0.28f);
            const float2 uv = input.texcoord + kernelPos * texel * 3.25f;
            glow += ExtractHighlight(gTexture.Sample(gSampler, uv).rgb) * weight;
            weightSum += weight;
        }
    }

    glow /= max(weightSum, 0.0001f);
    // Preserve the crisp source image and add only a restrained soft halo.
    output.color = float4(source.rgb + glow * 0.68f, source.a);
    return output;
}

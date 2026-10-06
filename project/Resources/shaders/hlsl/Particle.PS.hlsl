struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    nointerpolation float4 lightColor : COLOR1;
    nointerpolation float2 lightDirection : TEXCOORD2;
    nointerpolation float ambientLight : TEXCOORD3;
    nointerpolation float shape : TEXCOORD1;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float CloudDensity(float2 uv)
{
    // 低周波の丸い塊を滑らかにつなぐ。細かな縞ノイズは使用しない。
    float2 a = (uv - float2(-0.32f, 0.06f)) / float2(0.58f, 0.62f);
    float2 b = (uv - float2(0.30f, 0.02f)) / float2(0.60f, 0.58f);
    float2 c = (uv - float2(0.02f, -0.26f)) / float2(0.50f, 0.56f);
    return exp(-dot(a, a) * 2.6f) + exp(-dot(b, b) * 2.6f) +
        exp(-dot(c, c) * 2.8f);
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャサンプリング
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    float2 centeredUv = input.texcoord * 2.0f - 1.0f;
    float softCircleAlpha = saturate((1.0f - length(centeredUv)) * 3.0f);
    float lineSideAlpha = saturate((1.0f - abs(centeredUv.x)) * 4.0f);
    float lineEndAlpha = saturate((1.0f - abs(centeredUv.y)) * 5.0f);
    float solidLineAlpha = lineSideAlpha * lineEndAlpha;
    float density = CloudDensity(centeredUv);
    float cloudAlpha = smoothstep(0.035f, 0.90f, density) * 0.72f;
    cloudAlpha *= 1.0f - smoothstep(0.78f, 1.0f, max(abs(centeredUv.x), abs(centeredUv.y)));
    float shapeAlpha = input.shape > 1.5f
        ? cloudAlpha
        : input.shape > 0.5f ? solidLineAlpha : softCircleAlpha;

    // テクスチャの色 * パーティクルの色
    output.color = textureColor * input.color;
    output.color.a *= shapeAlpha;

    // 雲は発光物ではないため、ブルーム抽出閾値より十分低い明度に抑える。
    // 雷や通常パーティクルには適用せず、太陽・月など他の発光表現を維持する。
    if (input.shape > 1.5f)
    {
        const float epsilon = 0.025f;
        const float gradientX = (CloudDensity(centeredUv + float2(epsilon, 0.0f)) -
            CloudDensity(centeredUv - float2(epsilon, 0.0f))) / (2.0f * epsilon);
        const float gradientY = (CloudDensity(centeredUv + float2(0.0f, epsilon)) -
            CloudDensity(centeredUv - float2(0.0f, epsilon))) / (2.0f * epsilon);
        const float3 cloudNormal = normalize(float3(-gradientX * 0.65f, gradientY * 0.65f, 1.0f));
        const float2 direction2D = normalize(input.lightDirection + float2(0.0001f, 0.0001f));
        const float3 cloudLightDirection = normalize(float3(-direction2D.x, -direction2D.y, 0.35f));
        const float diffuse = saturate(dot(cloudNormal, cloudLightDirection));
        const float3 lighting = input.ambientLight.xxx + input.lightColor.rgb * diffuse * 0.80f;
        output.color.rgb *= lighting;
        output.color.rgb = min(output.color.rgb, float3(0.76f, 0.76f, 0.76f));
    }
    
    // アルファテスト (完全に透明な部分は描画しない)
    if (output.color.a <= 0.001f)
    {
        discard;
    }
    
    return output;
}

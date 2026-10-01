struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 lightSpacePosition : POSITION0;
    float4 color : COLOR0;
    float3 worldPosition : POSITION1; // ワールド空間の位置を追加
};

struct Material
{
    float4 color;
    int enableLighting;
    float shininess;
    float metallic;
    float emissive;
    float4x4 uvTransform;
    float environmentCoefficient;
};

static const uint MAX_POINT_LIGHTS = 8;

struct PointLight
{
    float3 position;
    float intensity;
    float4 color;
    float radius;
    float3 padding;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
    float3 cameraPosition; // カメラの位置を追加
    float paddingLight;        // アライメント用パディング
    PointLight pointLights[MAX_POINT_LIGHTS];
    uint pointLightCount;
    float3 pointLightPadding;
};

ConstantBuffer<Material> gMaterial : register(b0);


ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 5番目のスロット(t1)に届いているシャドウマップを受け取る
Texture2D<float> gShadowMap : register(t1);
TextureCube<float4> gEnvironmentTexture : register(t2);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

float3 ApplyFantasyAtmosphere(float3 color, float3 worldPosition)
{
    // 遠景を明るい空色へなじませ、空中世界らしい奥行きを作る。
    float distanceFromCamera = distance(gDirectionalLight.cameraPosition, worldPosition);
    float distanceFog = smoothstep(18.0f, 58.0f, distanceFromCamera);
    float heightHaze = saturate((worldPosition.y + 4.0f) / 42.0f) * 0.12f;
    float fogAmount = saturate(distanceFog * 0.72f + heightHaze * distanceFog);
    float3 fogColor = lerp(float3(0.48f, 0.70f, 0.96f), gDirectionalLight.color.rgb, 0.18f);
    color = lerp(color, fogColor, fogAmount);

    // 彩度を少し持ち上げつつ、強い光を緩やかに圧縮して白飛びを防ぐ。
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(luminance.xxx, color, 1.08f);
    return max(color, 0.0f) / (1.0f + max(color, 0.0f) * 0.12f);
}

float CalculateSoftShadow(float4 lightSpacePosition, float3 normal, float3 lightDirection)
{
    float3 lightPosition = lightSpacePosition.xyz / lightSpacePosition.w;
    float2 shadowUV = lightPosition.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f || shadowUV.y < 0.0f || shadowUV.y > 1.0f) {
        return 1.0f;
    }

    uint shadowWidth;
    uint shadowHeight;
    gShadowMap.GetDimensions(shadowWidth, shadowHeight);
    float2 texelSize = rcp(float2(shadowWidth, shadowHeight));
    float NdotL = saturate(dot(normal, lightDirection));
    float bias = max(0.00035f, 0.0016f * (1.0f - NdotL));
    float visibility = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float sampledDepth = gShadowMap.SampleLevel(
                gSampler, shadowUV + float2(x, y) * texelSize, 0.0f);
            visibility += lightPosition.z - bias <= sampledDepth ? 1.0f : 0.0f;
        }
    }
    visibility /= 9.0f;
    return lerp(0.58f, 1.0f, visibility);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャのサンプリング（既存）
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy) * input.color;
    
    // ライティング計算
    if (gMaterial.enableLighting != 0)
    {
        // ==========================================================
        // 影の計算（ライティング有効時のみ実行）
        // ==========================================================
        float3 normal = normalize(input.normal);
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float shadowFactor = CalculateSoftShadow(input.lightSpacePosition, normal, lightDir);
        float NdotL = dot(normal, lightDir);
        float wrappedDiffuse = saturate((NdotL + 0.22f) / 1.22f);

        // 空からは淡い青、地面側からは暖かい色が回り込む半球環境光。
        float hemisphere = normal.y * 0.5f + 0.5f;
        float3 ambientColor = lerp(
            float3(0.20f, 0.16f, 0.13f),
            float3(0.30f, 0.43f, 0.62f),
            hemisphere);
        
        // 1. 拡散反射光 (Diffuse) - ハーフランバートにソフトシャドウを適用
        float3 albedo = gMaterial.color.rgb * textureColor.rgb;
        float3 diffuseColor =
            albedo * ambientColor +
            albedo * gDirectionalLight.color.rgb *
            wrappedDiffuse * shadowFactor * gDirectionalLight.intensity * 0.82f;

        // 複数ポイントライト。プレイヤー、雷、松明などを同時に合成できる。
        const uint pointLightCount = min(gDirectionalLight.pointLightCount, MAX_POINT_LIGHTS);
        for (uint lightIndex = 0; lightIndex < pointLightCount; ++lightIndex)
        {
            PointLight pointLight = gDirectionalLight.pointLights[lightIndex];
            float3 toLight = pointLight.position - input.worldPosition;
            float distanceToLight = length(toLight);
            if (pointLight.intensity > 0.0f && distanceToLight < pointLight.radius)
            {
                float3 pointDirection = toLight / max(distanceToLight, 0.0001f);
                float pointNdotL = saturate(dot(normalize(input.normal), pointDirection));
                float attenuation = saturate(1.0f - distanceToLight / pointLight.radius);
                attenuation *= attenuation;
                diffuseColor += pointLight.color.rgb * pointNdotL * attenuation * pointLight.intensity * gMaterial.color.rgb * textureColor.rgb;
            }
        }
        
        // 2. スペキュラー反射光 (Blinn-Phong Specular) - 影の中ではハイライトを減衰して自然に見せる
        float3 viewDir = normalize(gDirectionalLight.cameraPosition - input.worldPosition);
        float3 halfDir = normalize(lightDir + viewDir);
        
        // gMaterial.shininess を反射光の広がり（指数）にマッピング
        // 0.0f の時は 8.0f（鈍い光）、1.0f の時は 256.0f（非常に鋭いハイライト）
        float specPower = lerp(8.0f, 256.0f, gMaterial.shininess);
        float specular = pow(saturate(dot(normalize(input.normal), halfDir)), specPower);
        
        // metallic が高いほど、スペキュラー色にマテリアルの色を強く混ぜる（金属特有の反射光）
        float3 specBaseColor = lerp(float3(1.0f, 1.0f, 1.0f), gMaterial.color.rgb, gMaterial.metallic);
        
        // shininess に応じて反射の強さを調節
        float fresnel = pow(1.0f - saturate(dot(normal, viewDir)), 5.0f);
        float specIntensity = lerp(0.10f, 0.62f, gMaterial.shininess) * (0.75f + fresnel * 0.45f);
        
        float3 sunHighlight = gDirectionalLight.color.rgb * float3(1.04f, 0.98f, 0.88f);
        float3 specColor = sunHighlight * gDirectionalLight.intensity * specular * specBaseColor * specIntensity * shadowFactor;
        
        // 3. リムライト (Rim Light) - 物体の輪郭を光らせて立体感を極限まで高める
        float rim = pow(1.0f - saturate(dot(normal, viewDir)), 4.0f);
        // emissive に応じてリムライトの光り方を補強
        float3 rimColor = float3(0.68f, 0.84f, 1.0f) * rim * (0.18f + gMaterial.emissive * 0.42f) * gDirectionalLight.intensity;
        
        // 4. 自発光 (Emission) - 光源がなくても自己発光する
        float3 emissiveColor = gMaterial.color.rgb * gMaterial.emissive;

        // 5. Environment Map - CPU 側の Inspector スライダー値を反射量に使用する。
        float3 reflectionDirection = reflect(-viewDir, normalize(input.normal));
        float3 environmentColor =
            gEnvironmentTexture.Sample(gSampler, reflectionDirection).rgb;
        float3 environmentReflection =
            environmentColor * saturate(gMaterial.environmentCoefficient);
        
        // 最終カラー合成
        output.color.rgb =
            diffuseColor + specColor + rimColor + emissiveColor +
            environmentReflection;
        output.color.rgb = ApplyFantasyAtmosphere(output.color.rgb, input.worldPosition);
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}

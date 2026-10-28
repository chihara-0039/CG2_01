struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 lightSpacePosition : POSITION0;
    float3 worldPosition : POSITION1;
    
    float4 color : COLOR0;
    float shininess : SHININESS0;
    float metallic : METALLIC0;
    float emissive : EMISSIVE0;
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
    float3 cameraPosition;
    float paddingLight;
    PointLight pointLights[MAX_POINT_LIGHTS];
    uint pointLightCount;
    float3 pointLightPadding;
    float4 fogSettings;
    float4 fogOrigin;
    float4 fogDirection;
    float4 fogScan;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
Texture2D<float> gShadowMap : register(t1);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

float3 ApplyFantasyAtmosphere(float3 color, float3 worldPosition)
{
    float distanceFromCamera = distance(gDirectionalLight.cameraPosition, worldPosition);
    // 通常モデルと同じ距離・濃度にし、ブロックだけ霧が濃くなる差を作らない。
    float distanceFog = smoothstep(22.0f, 100.0f, distanceFromCamera);
    float heightHaze = saturate((worldPosition.y + 4.0f) / 42.0f) * 0.04f;
    float fogAmount = saturate(distanceFog * (0.48f + heightHaze));
    float3 fogColor = lerp(float3(0.48f, 0.70f, 0.96f), gDirectionalLight.color.rgb, 0.18f);
    if (gDirectionalLight.fogSettings.x > .5f) {
        // 専用探索ステージのみ。通常のタイトルや他ステージの霧は従来どおり。
        float4 setting = gDirectionalLight.fogSettings;
        fogAmount = smoothstep(setting.y, setting.z, distanceFromCamera) * setting.w;
        fogColor = float3(.48f,.64f,.76f);
        float2 offset = worldPosition.xz - gDirectionalLight.fogOrigin.xz;
        float radial = length(offset);
        float mode = gDirectionalLight.fogOrigin.w;
        float reveal = mode >= .5f ? 1-smoothstep(4,8,radial) : 0;
        float scanMask = 0;
        if (mode >= 1.5f) {
            float2 direction = gDirectionalLight.fogDirection.xz;
            float along = clamp(dot(offset,direction),0,gDirectionalLight.fogDirection.w);
            float corridor = 1-smoothstep(4,8,length(offset-direction*along));
            float wave = 1-smoothstep(gDirectionalLight.fogScan.y-2,gDirectionalLight.fogScan.y+2,radial);
            scanMask = corridor * wave * gDirectionalLight.fogScan.x;
            reveal = max(reveal,scanMask);
        }
        // 高さの違う遠景まで一括で消さず、足場と同じ高さ付近に作用する。
        float heightMask = 1-smoothstep(4,9,abs(worldPosition.y-gDirectionalLight.fogOrigin.y));
        fogAmount *= 1-saturate(reveal*heightMask)*.94f;
        // スキャン境界を淡く発光させ、何が起きたかを画面でも伝える。
        float rim = (1-smoothstep(0,.8f,abs(radial-gDirectionalLight.fogScan.y))) * scanMask;
        color += float3(.08f,.26f,.30f)*rim;
    }
    color = lerp(color, fogColor, fogAmount);
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
    
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    float4 matColor = input.color;
    float matShininess = input.shininess;
    float matMetallic = input.metallic;
    float matEmissive = input.emissive;
    
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-gDirectionalLight.direction);
    float shadowFactor = CalculateSoftShadow(input.lightSpacePosition, normal, lightDir);
    float NdotL = dot(normal, lightDir);
    float wrappedDiffuse = saturate((NdotL + 0.22f) / 1.22f);
    float hemisphere = normal.y * 0.5f + 0.5f;
    float3 ambientColor = lerp(
        float3(0.20f, 0.16f, 0.13f),
        float3(0.30f, 0.43f, 0.62f),
        hemisphere);
    
    // 1. Diffuse
    float3 albedo = matColor.rgb * textureColor.rgb;
    float3 diffuseColor =
        albedo * ambientColor +
        albedo * gDirectionalLight.color.rgb *
        wrappedDiffuse * shadowFactor * gDirectionalLight.intensity * 0.82f;
    
    // Multiple Point Light Contributions
    const uint pointLightCount = min(gDirectionalLight.pointLightCount, MAX_POINT_LIGHTS);
    for (uint lightIndex = 0; lightIndex < pointLightCount; ++lightIndex) {
        PointLight pointLight = gDirectionalLight.pointLights[lightIndex];
        float3 plDir = pointLight.position - input.worldPosition;
        float plDist = length(plDir);
        if (pointLight.intensity > 0.0f && plDist < pointLight.radius) {
            plDir /= max(plDist, 0.0001f);
            float plNdotL = max(0.0f, dot(normalize(input.normal), plDir));
            float plAtten = saturate(1.0f - (plDist / pointLight.radius));
            plAtten *= plAtten;
            float3 plContrib = pointLight.color.rgb * plNdotL * plAtten * pointLight.intensity;
            diffuseColor += plContrib * matColor.rgb * textureColor.rgb;
        }
    }
    
    // 2. Specular
    float3 viewDir = normalize(gDirectionalLight.cameraPosition - input.worldPosition);
    float3 halfDir = normalize(lightDir + viewDir);
    
    float specPower = lerp(8.0f, 256.0f, matShininess);
    float specular = pow(saturate(dot(normalize(input.normal), halfDir)), specPower);
    
    float3 specBaseColor = lerp(float3(1.0f, 1.0f, 1.0f), matColor.rgb, matMetallic);
    float fresnel = pow(1.0f - saturate(dot(normal, viewDir)), 5.0f);
    float specIntensity = lerp(0.10f, 0.62f, matShininess) * (0.75f + fresnel * 0.45f);
    
    float3 sunHighlight = gDirectionalLight.color.rgb * float3(1.04f, 0.98f, 0.88f);
    float3 specColor = sunHighlight * gDirectionalLight.intensity * specular * specBaseColor * specIntensity * shadowFactor;
    
    // 3. Rim
    float rim = pow(1.0f - saturate(dot(normal, viewDir)), 4.0f);
    float3 rimColor = float3(0.68f, 0.84f, 1.0f) * rim * (0.18f + matEmissive * 0.42f) * gDirectionalLight.intensity;
    
    // 4. Emission
    float3 emissiveColor = matColor.rgb * matEmissive;
    
    output.color.rgb = ApplyFantasyAtmosphere(
        diffuseColor + specColor + rimColor + emissiveColor,
        input.worldPosition);
    output.color.a = matColor.a * textureColor.a;
    
    return output;
}

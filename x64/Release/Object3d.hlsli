struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

struct VSInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    uint instanceId : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

// C++側 Material と対応
struct Material
{
    float4 color;
    int enableLighting;
    float3 padding; // アライメント合わせ
};

// C++側 DirectionalLight と対応
struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

// ルートパラメータ0：マテリアル
cbuffer MaterialCB : register(b0)
{
    Material gMaterial;
}

// ルートパラメータ3：平行光源
cbuffer DirectionalLightCB : register(b3)
{
    DirectionalLight gLight;
}

// ルートパラメータ2：テクスチャ（t0）
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

// ルートパラメータ1：インスタンシングデータ（t1）
StructuredBuffer<TransformationMatrix> gInstancingData : register(t1);
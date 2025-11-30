// 共通構造体

struct VertexInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

// C++側の struct とレイアウトを合わせる
struct Material
{
    float4 color;
    int enableLighting;
    float3 padding;
    float4x4 uvTransform;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

// b0 : マテリアル
cbuffer CbMaterial : register(b0)
{
    Material gMaterial;
};

// b1 : 平行光源
cbuffer CbDirectionalLight : register(b1)
{
    DirectionalLight gDirectionalLight;
}

// Particle.hlsli
struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
};

struct Material
{
    float4 color;
    int enableLighting; // �g��Ȃ������C�A�E�g���킹�Ŏc��
    float3 _pad;
    float4x4 uvTransform;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};
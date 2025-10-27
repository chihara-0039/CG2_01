// Particle.hlsli

// === VS I/O ===
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

// === ConstantBuffer 用の構造体 ===
struct Material
{
    float4 color;
    int enableLighting; // レイアウト合わせ
    float3 _pad;
    float4x4 uvTransform;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

// === ピクセルシェーダ用リソース（PS: t0 / s0）===
// ※ ここでだけ宣言する。PS側で二重に宣言しない。
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

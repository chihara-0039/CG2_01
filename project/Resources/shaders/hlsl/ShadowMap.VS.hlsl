#include "object3d.hlsli"
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
};

struct ShaderVertexOutput
{
    float32_t4 position : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // ★重要：WVPではなく「World行列 × ライト行列」を直接計算して使う
    float32_t4x4 lightWVP = mul(gTransformationMatrix.World, gTransformationMatrix.lightViewProjection);
    output.position = mul(input.position, lightWVP);
    return output;
}
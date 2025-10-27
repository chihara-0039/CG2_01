#include "Particle.hlsli"

// VS 側インスタンシング行列（t0）
// ルートシグネチャで「頂点シェーダ可視の SRV テーブル t0」に紐付ける
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput o;

    float4x4 wvp = gTransformationMatrices[instanceId].WVP;
    float4x4 world = gTransformationMatrices[instanceId].World;

    o.position = mul(input.position, wvp);

    // ※ 正確には法線行列（world の 3x3 逆転置）だが、ひとまず world の 3x3 でOK
    o.normal = normalize(mul(input.normal, (float3x3) world));
    o.texcoord = input.texcoord;

    return o;
}

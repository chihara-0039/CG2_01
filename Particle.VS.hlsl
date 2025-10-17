#include "Particle.hlsli"

// ��VS�p��SRV�� space1 ��
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0, space1);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    float4x4 wvp = gTransformationMatrices[instanceId].WVP;
    float4x4 world = gTransformationMatrices[instanceId].World;

    output.position = mul(input.position, wvp);
    output.texcoord = input.texcoord;

    // �@���� World ��3x3��
    output.normal = normalize(mul(input.normal, (float3x3) world));
    return output;
}

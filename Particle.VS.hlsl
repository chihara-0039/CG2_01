#include "Particle.hlsli"

// ÅöVSópÇÃSRVÇÕ space1 Ç…
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0, space1);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    float4x4 wvp = gTransformationMatrices[instanceId].WVP;
    float4x4 world = gTransformationMatrices[instanceId].World;

    output.position = mul(input.position, wvp);
    output.texcoord = input.texcoord;

    // ñ@ê¸ÇÕ World ÇÃ3x3Ç≈
    output.normal = normalize(mul(input.normal, (float3x3) world));
    return output;
}

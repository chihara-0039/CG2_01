#include "Particle.hlsli"

// VS 用：インスタンシング情報 (RootParameter[1], t0)
//  → C++ 側の instancingSrvDesc で StructuredBuffer<TransformationMatrix> として作っている
StructuredBuffer<TransformationMatrix> gInstanceMatrices : register(t0);

VertexOutput main(VertexInput vin, uint instanceId : SV_InstanceID)
{
    VertexOutput vout;

    // このインスタンスの行列を取得
    TransformationMatrix m = gInstanceMatrices[instanceId];

    // 位置変換（C++ 側で row-major で作っているので mul(v, M) 形式）
    vout.position = mul(vin.position, m.WVP);

    // 法線をワールド空間に変換（回転＋スケール部分だけ取り出す）
    float3x3 world3x3 = (float3x3) m.World;
    vout.normal = mul(vin.normal, world3x3);

    vout.texcoord = vin.texcoord;

    return vout;
}

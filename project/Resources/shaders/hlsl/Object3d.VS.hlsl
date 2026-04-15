#include "object3d.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexSgaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};


VertexShaderOutput main(VertexSgaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul((float32_t3x3) gTransformationMatrix.World, input.normal));
    
    // モデルの頂点を「世界のどこにいるか」に変換
    float32_t4 worldPos = mul(input.position, gTransformationMatrix.World);
    
    // その世界座標を「ライト視点の行列」で変換してピクセルシェーダーに送る
    output.lightSpacePosition = mul(worldPos, gTransformationMatrix.lightViewProjection);
    
    return output;
}
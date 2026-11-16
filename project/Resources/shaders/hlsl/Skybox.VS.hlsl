#include "Skybox.hlsli"

struct TransformationMatrix {
    float32_t4x4 WVP;
    float32_t4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// 空の頂点をカメラへ合わせ、キューブマップの参照方向を渡す。
VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    // 遠景表現のため、射影変換後のz値をwに置換して常に最遠(z=w)になるようにする
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    output.texcoord = input.position.xyz;
    return output;
}

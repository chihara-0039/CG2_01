struct TransformationMatrix {
    float4x4 WVP;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// 2D頂点へ変換を適用し、画面位置とUVを求める。
VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
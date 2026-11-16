// Only fragments behind scene depth are drawn by the highlight pipeline.
struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 lightSpacePosition : POSITION0;
    float4 color : COLOR0;
    float3 worldPosition : POSITION1;
};
float4 main(VertexShaderOutput input) : SV_TARGET {
    return float4(0.12f, 0.85f, 1.0f, 0.55f);
}

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 lightSpacePosition : POSITION0;
    float3 worldPosition : POSITION1;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 lightViewProjection;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

StructuredBuffer<Well> gMatrixPalette : register(t2);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

struct Skinned
{
    float4 position;
    float3 normal;
};

Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;
    
    skinned.position =
        mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x +
        mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y +
        mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z +
        mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.position.w = 1.0f;

    skinned.normal =
        mul(input.normal, (float3x3)gMatrixPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weight.x +
        mul(input.normal, (float3x3)gMatrixPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weight.y +
        mul(input.normal, (float3x3)gMatrixPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weight.z +
        mul(input.normal, (float3x3)gMatrixPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weight.w;
    skinned.normal = normalize(skinned.normal);

    return skinned;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    Skinned skinned = Skinning(input);
    
    output.position = mul(skinned.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinned.normal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    
    float4 worldPos = mul(skinned.position, gTransformationMatrix.World);
    output.lightSpacePosition = mul(worldPos, gTransformationMatrix.lightViewProjection);
    output.worldPosition = worldPos.xyz;
    
    return output;
}





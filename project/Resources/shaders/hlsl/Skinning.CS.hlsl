struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

struct Vertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
    float4 color;
};

struct VertexInfluence
{
    float4 weight;
    int4 index;
};

struct SkinningInformation
{
    uint numVertices;
    uint numJoints;
    uint2 padding;
};

StructuredBuffer<Well> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;
    if (vertexIndex >= gSkinningInformation.numVertices)
    {
        return;
    }

    Vertex input = gInputVertices[vertexIndex];
    VertexInfluence influence = gInfluences[vertexIndex];

    // glTFのJOINTS_0に壊れた番号や、別スキン用の番号が含まれていても
    // GPUのStructuredBuffer範囲外を読まないようにする。
    // Root SRVにはバッファの要素数が格納されないため、
    // StructuredBuffer::GetDimensions()を境界判定に使ってはいけない。
    // CPUが検証済みのJoint数を定数バッファで明示的に渡す。
    if (gSkinningInformation.numJoints == 0)
    {
        gOutputVertices[vertexIndex] = input;
        return;
    }

    int maximumJointIndex = int(gSkinningInformation.numJoints - 1);
    influence.index.x = clamp(influence.index.x, 0, maximumJointIndex);
    influence.index.y = clamp(influence.index.y, 0, maximumJointIndex);
    influence.index.z = clamp(influence.index.z, 0, maximumJointIndex);
    influence.index.w = clamp(influence.index.w, 0, maximumJointIndex);

    float totalWeight =
        influence.weight.x +
        influence.weight.y +
        influence.weight.z +
        influence.weight.w;
    if (totalWeight <= 0.000001f)
    {
        gOutputVertices[vertexIndex] = input;
        return;
    }
    influence.weight /= totalWeight;

    Vertex skinned;
    skinned.position =
        mul(input.position, gMatrixPalette[influence.index.x].skeletonSpaceMatrix) * influence.weight.x +
        mul(input.position, gMatrixPalette[influence.index.y].skeletonSpaceMatrix) * influence.weight.y +
        mul(input.position, gMatrixPalette[influence.index.z].skeletonSpaceMatrix) * influence.weight.z +
        mul(input.position, gMatrixPalette[influence.index.w].skeletonSpaceMatrix) * influence.weight.w;
    skinned.position.w = 1.0f;

    skinned.texcoord = input.texcoord;
    skinned.color = input.color;
    skinned.normal =
        mul(input.normal, (float3x3)gMatrixPalette[influence.index.x].skeletonSpaceInverseTransposeMatrix) * influence.weight.x +
        mul(input.normal, (float3x3)gMatrixPalette[influence.index.y].skeletonSpaceInverseTransposeMatrix) * influence.weight.y +
        mul(input.normal, (float3x3)gMatrixPalette[influence.index.z].skeletonSpaceInverseTransposeMatrix) * influence.weight.z +
        mul(input.normal, (float3x3)gMatrixPalette[influence.index.w].skeletonSpaceInverseTransposeMatrix) * influence.weight.w;
    float normalLengthSquared = dot(skinned.normal, skinned.normal);
    if (normalLengthSquared > 0.000001f)
    {
        skinned.normal *= rsqrt(normalLengthSquared);
    }
    else
    {
        skinned.normal = input.normal;
    }

    gOutputVertices[vertexIndex] = skinned;
}

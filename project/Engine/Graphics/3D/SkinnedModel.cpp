#define NOMINMAX
#include "SkinnedModel.h"
#include "GltfLoader.h"
#include "MyMath.h"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace {
    // 繝吶け繝医Ν縺ｨ陦悟・縺ｮ荵礼ｮ・(蟷ｳ陦檎ｧｻ蜍輔≠繧・
    Vector3 TransformCoord(const Vector3& v, const Matrix4x4& m) {
        float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
        if (std::abs(w) < 1e-5f) w = 1.0f;
        return {
            (v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0]) / w,
            (v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1]) / w,
            (v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2]) / w
        };
    }

    // 譁ｹ蜷代・繧ｯ繝医Ν縺ｨ陦悟・縺ｮ荵礼ｮ・(蟷ｳ陦檎ｧｻ蜍輔↑縺励∝屓霆｢縺ｮ縺ｿ)
    Vector3 TransformNormal(const Vector3& n, const Matrix4x4& m) {
        Vector3 res = {
            n.x * m.m[0][0] + n.y * m.m[1][0] + n.z * m.m[2][0],
            n.x * m.m[0][1] + n.y * m.m[1][1] + n.z * m.m[2][1],
            n.x * m.m[0][2] + n.y * m.m[1][2] + n.z * m.m[2][2]
        };
        return Math::Normalize(res);
    }
}

void SkinnedModel::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager) {
    // 1. 繧ｸ繝ｧ繧､繝ｳ繝医・讒矩繧呈ｧ狗ｯ・
    CreateHumanoidSkeleton();

    // 2. 繝｡繝・す繝･ (鬆らせ繝・・繧ｿ) 繧堤函謌・
    GenerateHumanoidMesh();
    
    // 3. 髢｢遽莉倩ｿ代・繧ｦ繧ｧ繧､繝医ｒ繝悶Ξ繝ｳ繝峨☆繧・
    SmoothWeights();

    // 4. 繝・け繧ｹ繝√Ε繧偵Ο繝ｼ繝・
    if (textureManager) {
        textureHandle_ = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    }

    // 5. 繝舌ャ繝輔ぃ逕滓・
    CreateBuffers(dxCommon);

    // 6. 繝・ヰ繝・げ謠冗判逕ｨ縺ｮ Model 繧ｯ繝ｩ繧ｹ繧堤函謌・(荳崎ｦ√↑繧画ｶ医○繧九′莠呈鋤諤ｧ縺ｮ縺溘ａ谿九☆)
    model_ = std::make_unique<Model>();
    // model_ 縺ｯ縺薙％縺ｧ縺ｯ蛻晄悄蛹悶○縺壹√ョ繝舌ャ繧ｰ逕ｨ陦ｨ遉ｺ縺悟ｿ・ｦ√↑譎ゅ□縺台ｽｿ縺・ｈ縺・↓縺吶ｋ

    // 7. 蛻晄悄繝昴・繧ｺ繧帝←逕ｨ (閻輔ｒ荳九ｍ縺励◆迥ｶ諷九↓縺吶ｋ)
    ResetPose();
}

void SkinnedModel::InitializeFromGltf(DirectXCommon* dxCommon, const std::string& filePath, TextureManager* textureManager) {
    std::string texturePath;
    
    // glTF繝輔ぃ繧､繝ｫ繧偵Ο繝ｼ繝・
    bool success = GltfLoader::LoadGltfModel(
        dxCommon,
        textureManager,
        filePath,
        skinnedVertices_,
        joints_,
        motions_,
        texturePath
    );

    if (!success) {
        OutputDebugStringA("Failed to load glTF model. Falling back to default humanoid mesh.\n");
        Initialize(dxCommon, textureManager);
        return;
    }

    activeMotionIndex_ = motions_.empty() ? -1 : 0;

    if (textureManager && !texturePath.empty()) {
        textureHandle_ = textureManager->LoadTexture(texturePath);
    }

    CreateBuffers(dxCommon);

    model_ = std::make_unique<Model>();

    // 繧ｸ繝ｧ繧､繝ｳ繝育憾諷九・繧ｯ繧ｩ繝ｼ繧ｿ繝九が繝ｳ蛻晄悄險ｭ螳・
    for (auto& joint : joints_) {
        joint.rotationQuat = Math::MakeQuaternionFromEuler(joint.rotation);
        joint.isQuaternion = true;
    }
}

void SkinnedModel::ResetPose() {
    for (size_t i = 0; i < joints_.size(); ++i) {
        joints_[i].scale = { 1.0f, 1.0f, 1.0f };
        
        // 蛻晄悄繝昴・繧ｺ繧偵梧ｰ励ｒ縺､縺托ｼ郁・繧剃ｸ九ｍ縺励◆迥ｶ諷具ｼ峨阪↓縺吶ｋ
        if (joints_[i].name == "LeftShoulder") {
            joints_[i].rotation = { 0.0f, 0.0f, 1.3f };  // 蟾ｦ閻輔ｒ荳九ｍ縺・(繝励Λ繧ｹ蝗櫁ｻ｢)
        } else if (joints_[i].name == "RightShoulder") {
            joints_[i].rotation = { 0.0f, 0.0f, -1.3f }; // 蜿ｳ閻輔ｒ荳九ｍ縺・(繝槭う繝翫せ蝗櫁ｻ｢)
        } else {
            joints_[i].rotation = { 0.0f, 0.0f, 0.0f };
        }
    }
}

void SkinnedModel::CreateHumanoidSkeleton() {
    joints_.clear();

    // 繝懊・繝ｳ螳夂ｾｩ逕ｨ縺ｮ荳譎よｧ矩菴・(荳也阜蠎ｧ讓吶〒縺ｮ螳夂ｾｩ)
    struct JointDef {
        std::string name;
        Vector3 globalPos;
        int parentIndex;
    };

    // 莠ｺ蝙九せ繧ｱ繝ｫ繝医Φ螳夂ｾｩ (蜷郁ｨ・5繧ｸ繝ｧ繧､繝ｳ繝・
    std::vector<JointDef> defs = {
        { "Pelvis",        { 0.0f, 0.8f, 0.0f },    -1 }, // 0
        { "Spine",         { 0.0f, 1.1f, 0.0f },     0 }, // 1
        { "Head",          { 0.0f, 1.4f, 0.0f },     1 }, // 2
        
        { "LeftShoulder",  { -0.25f, 1.3f, 0.0f },   1 }, // 3
        { "LeftElbow",     { -0.5f, 1.3f, 0.0f },    3 }, // 4
        { "LeftHand",      { -0.7f, 1.3f, 0.0f },    4 }, // 5
        
        { "RightShoulder", { 0.25f, 1.3f, 0.0f },    1 }, // 6
        { "RightElbow",    { 0.5f, 1.3f, 0.0f },     6 }, // 7
        { "RightHand",     { 0.7f, 1.3f, 0.0f },     7 }, // 8
        
        { "LeftHip",       { -0.15f, 0.7f, 0.0f },   0 }, // 9
        { "LeftKnee",      { -0.15f, 0.35f, 0.0f },  9 }, // 10
        { "LeftFoot",      { -0.15f, 0.0f, 0.0f },   10 }, // 11
        
        { "RightHip",      { 0.15f, 0.7f, 0.0f },    0 }, // 12
        { "RightKnee",     { 0.15f, 0.35f, 0.0f },  12 }, // 13
        { "RightFoot",     { 0.15f, 0.0f, 0.0f },   13 }  // 14
    };

    joints_.resize(defs.size());

    // 蜷・ず繝ｧ繧､繝ｳ繝医・蛻晄悄繝ｭ繝ｼ繧ｫ繝ｫ繝ｻ繧ｰ繝ｭ繝ｼ繝舌Ν陦悟・繧呈ｱゅａ繧・
    for (size_t i = 0; i < defs.size(); ++i) {
        joints_[i].name = defs[i].name;
        joints_[i].scale = { 1.0f, 1.0f, 1.0f };
        joints_[i].rotation = { 0.0f, 0.0f, 0.0f };
        joints_[i].parentIndex = defs[i].parentIndex;

        // 隕ｪ縺九ｉ縺ｮ逶ｸ蟇ｾ蠎ｧ讓・(繝ｭ繝ｼ繧ｫ繝ｫ translation) 縺ｮ險育ｮ・
        if (defs[i].parentIndex == -1) {
            joints_[i].translation = defs[i].globalPos;
        } else {
            int parentIdx = defs[i].parentIndex;
            joints_[i].translation = {
                defs[i].globalPos.x - defs[parentIdx].globalPos.x,
                defs[i].globalPos.y - defs[parentIdx].globalPos.y,
                defs[i].globalPos.z - defs[parentIdx].globalPos.z
            };
        }
    }

    // 繝舌う繝ｳ繝峨・繝ｼ繧ｺ縺ｮ蛻晄悄繧ｰ繝ｭ繝ｼ繝舌Ν陦悟・縺翫ｈ縺ｳ縺昴・騾・｡悟・繧貞燕險育ｮ励☆繧・
    for (size_t i = 0; i < joints_.size(); ++i) {
        Matrix4x4 localTrans = Math::MakeTranslateMatrix(joints_[i].translation);
        // 蛻晄悄迥ｶ諷九・蝗櫁ｻ｢繝ｻ繧ｹ繧ｱ繝ｼ繝ｫ縺ｪ縺励↑縺ｮ縺ｧ localMatrix 縺ｯ蜊倥↓蟷ｳ陦檎ｧｻ蜍戊｡悟・
        joints_[i].localMatrix = localTrans;

        if (joints_[i].parentIndex == -1) {
            joints_[i].globalMatrix = joints_[i].localMatrix;
        } else {
            int parentIdx = joints_[i].parentIndex;
            joints_[i].globalMatrix = Math::Multiply(joints_[i].localMatrix, joints_[parentIdx].globalMatrix);
        }

        // 騾・｡悟・ (Inverse Bind Pose Matrix)
        joints_[i].offsetMatrix = Math::Inverse(joints_[i].globalMatrix);
    }
}

void SkinnedModel::GenerateHumanoidMesh() {
    skinnedVertices_.clear();

    // 蜷・Κ菴阪・ Cube 繝｡繝・す繝･繧堤函謌舌＠縺ｦ邨仙粋縺吶ｋ
    // Pelvis (腰)
    AddCubeMesh({ 0.0f, 0.8f, 0.0f }, { 0.4f, 0.2f, 0.2f }, 0);
    // Spine (胸)
    AddCubeMesh({ 0.0f, 1.1f, 0.0f }, { 0.5f, 0.4f, 0.22f }, 1);
    // Head (鬆ｭ)
    AddCubeMesh({ 0.0f, 1.45f, 0.0f }, { 0.2f, 0.2f, 0.2f }, 2);

    // 蟾ｦ閻・
    AddCubeMesh({ -0.375f, 1.3f, 0.0f }, { 0.25f, 0.1f, 0.1f }, 3); // 蟾ｦ閧ｩ縲懷ｷｦ閧倥・髢・
    AddCubeMesh({ -0.6f, 1.3f, 0.0f }, { 0.2f, 0.08f, 0.08f }, 4);  // 蟾ｦ閧倥懷ｷｦ謇九・髢・
    AddCubeMesh({ -0.725f, 1.3f, 0.0f }, { 0.07f, 0.07f, 0.07f }, 5); // 蟾ｦ謇句・

    // 蜿ｳ閻・
    AddCubeMesh({ 0.375f, 1.3f, 0.0f }, { 0.25f, 0.1f, 0.1f }, 6);
    AddCubeMesh({ 0.6f, 1.3f, 0.0f }, { 0.2f, 0.08f, 0.08f }, 7);
    AddCubeMesh({ 0.725f, 1.3f, 0.0f }, { 0.07f, 0.07f, 0.07f }, 8);

    // 蟾ｦ雜ｳ
    AddCubeMesh({ -0.15f, 0.525f, 0.0f }, { 0.12f, 0.35f, 0.12f }, 9); // 蟾ｦ螟ｧ閻ｿ
    AddCubeMesh({ -0.15f, 0.175f, 0.0f }, { 0.1f, 0.35f, 0.1f }, 10);  // 蟾ｦ荳玖・
    AddCubeMesh({ -0.15f, -0.05f, 0.05f }, { 0.1f, 0.1f, 0.15f }, 11); // 蟾ｦ雜ｳ蜈・

    // 蜿ｳ雜ｳ
    AddCubeMesh({ 0.15f, 0.525f, 0.0f }, { 0.12f, 0.35f, 0.12f }, 12); // 蜿ｳ螟ｧ閻ｿ
    AddCubeMesh({ 0.15f, 0.175f, 0.0f }, { 0.1f, 0.35f, 0.1f }, 13);  // 蜿ｳ荳玖・
    AddCubeMesh({ 0.15f, -0.05f, 0.05f }, { 0.1f, 0.1f, 0.15f }, 14); // 蜿ｳ雜ｳ蜈・
}

void SkinnedModel::AddCubeMesh(const Vector3& center, const Vector3& size, int jointIndex) {
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    Vector3 localVertices[8] = {
        { center.x - hx, center.y - hy, center.z - hz }, // 0
        { center.x + hx, center.y - hy, center.z - hz }, // 1
        { center.x - hx, center.y + hy, center.z - hz }, // 2
        { center.x + hx, center.y + hy, center.z - hz }, // 3
        { center.x - hx, center.y - hy, center.z + hz }, // 4
        { center.x + hx, center.y - hy, center.z + hz }, // 5
        { center.x - hx, center.y + hy, center.z + hz }, // 6
        { center.x + hx, center.y + hy, center.z + hz }  // 7
    };

    struct Face {
        int idx[4];
        Vector3 normal;
    };
    Face faces[6] = {
        { { 0, 2, 3, 1 }, { 0.0f, 0.0f, -1.0f } }, // 前
        { { 1, 3, 7, 5 }, { 1.0f, 0.0f, 0.0f } },  // 右
        { { 5, 7, 6, 4 }, { 0.0f, 0.0f, 1.0f } },  // 後
        { { 4, 6, 2, 0 }, { -1.0f, 0.0f, 0.0f } }, // 左
        { { 2, 6, 7, 3 }, { 0.0f, 1.0f, 0.0f } },  // 上
        { { 4, 0, 1, 5 }, { 0.0f, -1.0f, 0.0f } }  // 下
    };

    for (int f = 0; f < 6; ++f) {
        int indices[6] = {
            faces[f].idx[0], faces[f].idx[1], faces[f].idx[2],
            faces[f].idx[0], faces[f].idx[2], faces[f].idx[3]
        };

        Vector2 uvs[6] = {
            { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f },
            { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f }
        };

        for (int i = 0; i < 6; ++i) {
            SkinnedVertexData v;
            v.position = { localVertices[indices[i]].x, localVertices[indices[i]].y, localVertices[indices[i]].z, 1.0f };
            v.normal = faces[f].normal;
            v.texcoord = uvs[i];
            
            v.jointIndices[0] = jointIndex;
            v.jointIndices[1] = 0; v.jointIndices[2] = 0; v.jointIndices[3] = 0;
            v.weights[0] = 1.0f;
            v.weights[1] = 0.0f; v.weights[2] = 0.0f; v.weights[3] = 0.0f;

            skinnedVertices_.push_back(v);
        }
    }
}

void SkinnedModel::SmoothWeights() {
    for (auto& v : skinnedVertices_) {
        Vector3 pos = { v.position.x, v.position.y, v.position.z };
        int primaryJoint = v.jointIndices[0];

        if (primaryJoint == 1 && pos.y < 1.0f) {
            float dist = (pos.y - 0.8f) / 0.2f;
            float weightSpine = std::max(0.0f, std::min(1.0f, dist));
            v.jointIndices[0] = 1; v.weights[0] = weightSpine;
            v.jointIndices[1] = 0; v.weights[1] = 1.0f - weightSpine;
        } else if (primaryJoint == 0 && pos.y > 0.85f) {
            float dist = (pos.y - 0.8f) / 0.1f;
            float weightSpine = std::max(0.0f, std::min(1.0f, dist));
            v.jointIndices[0] = 0; v.weights[0] = 1.0f - weightSpine;
            v.jointIndices[1] = 1; v.weights[1] = weightSpine;
        }
    }
}

void SkinnedModel::Update(DirectXCommon* dxCommon) {
    for (size_t i = 0; i < joints_.size(); ++i) {
        if (joints_[i].isQuaternion) {
            joints_[i].localMatrix = Math::MakeAffineMatrix(joints_[i].scale, joints_[i].rotationQuat, joints_[i].translation);
        } else {
            joints_[i].localMatrix = Math::MakeAffineMatrix(joints_[i].scale, joints_[i].rotation, joints_[i].translation);
        }

        if (joints_[i].parentIndex == -1) {
            joints_[i].globalMatrix = joints_[i].localMatrix;
        } else {
            int parentIdx = joints_[i].parentIndex;
            joints_[i].globalMatrix = Math::Multiply(joints_[i].localMatrix, joints_[parentIdx].globalMatrix);
        }
    }
    
    // Update jointBuffer_
    if (jointBuffer_ && !joints_.empty()) {
        Matrix4x4* mappedMatrices = nullptr;
        if (SUCCEEDED(jointBuffer_->Map(0, nullptr, (void**)&mappedMatrices))) {
            for (size_t i = 0; i < joints_.size(); ++i) {
                mappedMatrices[i] = Math::Multiply(joints_[i].offsetMatrix, joints_[i].globalMatrix);
            }
            jointBuffer_->Unmap(0, nullptr);
        }
    }
}

void SkinnedModel::CreateBuffers(DirectXCommon* dxCommon) {
    if (skinnedVertices_.empty()) return;

    auto device = dxCommon->GetDevice();
    UINT sizeVB = static_cast<UINT>(sizeof(SkinnedVertexData) * skinnedVertices_.size());

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeVB;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));

    if (SUCCEEDED(hr)) {
        SkinnedVertexData* vertMap = nullptr;
        vertexBuffer_->Map(0, nullptr, (void**)&vertMap);
        std::copy(skinnedVertices_.begin(), skinnedVertices_.end(), vertMap);
        vertexBuffer_->Unmap(0, nullptr);

        vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = sizeVB;
        vertexBufferView_.StrideInBytes = sizeof(SkinnedVertexData);
    }
    
    // Create jointBuffer_
    if (!joints_.empty()) {
        UINT sizeJoints = static_cast<UINT>(sizeof(Matrix4x4) * joints_.size());
        resDesc.Width = sizeJoints;
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&jointBuffer_));
    }
}

void SkinnedModel::ApplyTestAnimation(float time, float speed) {
    float t = time * speed;
    joints_[0].rotation.z = std::sin(t) * 0.02f;
    joints_[0].translation.y = 0.8f + std::sin(t * 2.0f) * 0.02f;
    joints_[1].rotation.y = std::sin(t) * 0.05f;
    joints_[9].rotation.x = std::sin(t) * 0.4f;
    joints_[12].rotation.x = -std::sin(t) * 0.4f;
    joints_[10].rotation.x = (std::sin(t + 1.5f) + 1.0f) * 0.3f; 
    joints_[13].rotation.x = (-std::sin(t + 1.5f) + 1.0f) * 0.3f;
    joints_[3].rotation.x = -std::sin(t) * 0.3f;
    joints_[6].rotation.x = std::sin(t) * 0.3f;
    joints_[4].rotation.x = (std::sin(t - 1.0f) - 1.0f) * 0.2f;
    joints_[7].rotation.x = (-std::sin(t - 1.0f) - 1.0f) * 0.2f;
}
void SkinnedModel::ApplyMotion(float time) {
    const auto& activeMotion = GetMotionData();
    if (activeMotion.jointAnimations.empty()) return;

    // 譎る俣繧・Duration 蜀・↓繝ｫ繝ｼ繝励＆縺帙ｋ
    float loopedTime = std::fmod(time, activeMotion.duration);
    if (loopedTime < 0.0f) loopedTime += activeMotion.duration;

    for (size_t i = 0; i < joints_.size(); ++i) {
        if (i >= activeMotion.jointAnimations.size()) continue;
        const auto& jointAnim = activeMotion.jointAnimations[i];
        if (jointAnim.keyframes.empty()) continue;

        auto& joint = joints_[i];

        // 1縺､縺縺代・蝣ｴ蜷医・陬憺俣縺ｪ縺・
        if (jointAnim.keyframes.size() == 1) {
            joint.translation = jointAnim.keyframes[0].translation;
            joint.rotation = jointAnim.keyframes[0].rotation;
            joint.scale = jointAnim.keyframes[0].scale;
            joint.rotationQuat = jointAnim.keyframes[0].rotationQuat;
            joint.isQuaternion = jointAnim.keyframes[0].isQuaternion;
            continue;
        }

        // 譎る俣縺梧怙蛻昴・繧ｭ繝ｼ繝輔Ξ繝ｼ繝繧医ｊ蜑・
        if (loopedTime <= jointAnim.keyframes.front().time) {
            const auto& first = jointAnim.keyframes.front();
            joint.translation = first.translation;
            joint.rotation = first.rotation;
            joint.scale = first.scale;
            joint.rotationQuat = first.rotationQuat;
            joint.isQuaternion = first.isQuaternion;
            continue;
        }

        // 譎る俣縺梧怙蠕後・繧ｭ繝ｼ繝輔Ξ繝ｼ繝繧医ｊ蠕後ｍ
        if (loopedTime >= jointAnim.keyframes.back().time) {
            const auto& last = jointAnim.keyframes.back();
            joint.translation = last.translation;
            joint.rotation = last.rotation;
            joint.scale = last.scale;
            joint.rotationQuat = last.rotationQuat;
            joint.isQuaternion = last.isQuaternion;
            continue;
        }

        // 蜑榊ｾ後・繧ｭ繝ｼ繝輔Ξ繝ｼ繝繧呈爾縺・
        for (size_t k = 0; k < jointAnim.keyframes.size() - 1; ++k) {
            const auto& kfA = jointAnim.keyframes[k];
            const auto& kfB = jointAnim.keyframes[k + 1];

            if (loopedTime >= kfA.time && loopedTime <= kfB.time) {
                float t = (loopedTime - kfA.time) / (kfB.time - kfA.time);

                // 邱壼ｽ｢陬憺俣
                joint.translation = {
                    kfA.translation.x + t * (kfB.translation.x - kfA.translation.x),
                    kfA.translation.y + t * (kfB.translation.y - kfA.translation.y),
                    kfA.translation.z + t * (kfB.translation.z - kfA.translation.z)
                };

                // 蝗櫁ｻ｢縺ｮ陬憺俣
                if (kfA.isQuaternion && kfB.isQuaternion) {
                    joint.rotationQuat = Math::Slerp(kfA.rotationQuat, kfB.rotationQuat, t);
                    joint.rotation = Math::ToEuler(joint.rotationQuat);
                    joint.isQuaternion = true;
                } else {
                    // 隗貞ｺｦ縺ｮ陬憺俣 (繧ｪ繧､繝ｩ繝ｼ隗偵・蜊倡ｴ斐↑ Lerp)
                    joint.rotation = {
                        kfA.rotation.x + t * (kfB.rotation.x - kfA.rotation.x),
                        kfA.rotation.y + t * (kfB.rotation.y - kfA.rotation.y),
                        kfA.rotation.z + t * (kfB.rotation.z - kfA.rotation.z)
                    };
                    joint.isQuaternion = false;
                }

                joint.scale = {
                    kfA.scale.x + t * (kfB.scale.x - kfA.scale.x),
                    kfA.scale.y + t * (kfB.scale.y - kfA.scale.y),
                    kfA.scale.z + t * (kfB.scale.z - kfA.scale.z)
                };
                break;
            }
        }
    }
}

void SkinnedModel::GenerateWalkPreset() {
    ClearKeyframes();

    // 2.0遘偵・繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ繧・.1遘貞綾縺ｿ・・1繧ｭ繝ｼ繝輔Ξ繝ｼ繝・峨〒逕滓・
    GetMotionData().duration = 2.0f;
    float step = 0.1f;

    // 荳譎ら噪縺ｫ繝昴・繧ｺ繧帝驕ｿ
    std::vector<Vector3> origTrans(joints_.size());
    std::vector<Vector3> origRot(joints_.size());
    std::vector<Vector3> origScale(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) {
        origTrans[i] = joints_[i].translation;
        origRot[i] = joints_[i].rotation;
        origScale[i] = joints_[i].scale;
    }

    for (float t = 0.0f; t <= 2.0f + 1e-4f; t += step) {
        // 繝・せ繝育畑豁ｩ陦後い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ縺ｮ繝ｭ繧ｸ繝・け繧帝←逕ｨ縺励※繝昴・繧ｺ繧定ｨ育ｮ・
        // 2.0遘偵〒1繧ｵ繧､繧ｯ繝ｫ・亥捉譛・2 * PI・峨↓縺吶ｋ縺溘ａ縲√せ繝斐・繝峨ｒ隱ｿ遽縺吶ｋ
        // 繧ｹ繝斐・繝・speed = PI (t = 2.0遘偵・縺ｨ縺阪∝・蜉帛､ = 2.0 * PI 縺ｨ縺ｪ繧・
        ApplyTestAnimation(t, 3.14159265f);

        // 縺昴・迸ｬ髢薙・繝昴・繧ｺ繧偵く繝ｼ繝輔Ξ繝ｼ繝縺ｨ縺励※險倬鹸
        AddKeyframe(t);
    }

    // 蜈・・繝昴・繧ｺ・育ｷｨ髮・憾諷具ｼ峨ｒ蠕ｩ蜈・
    for (size_t i = 0; i < joints_.size(); ++i) {
        joints_[i].translation = origTrans[i];
        joints_[i].rotation = origRot[i];
        joints_[i].scale = origScale[i];
    }
}

void SkinnedModel::GenerateRunPreset() {
    ClearKeyframes();

    // 蟆剰ｵｰ繧翫・1繧ｵ繧､繧ｯ繝ｫ1.0遘偵・邏譌ｩ縺・Ν繝ｼ繝励′邯ｺ鮗・
    GetMotionData().duration = 1.0f;
    float step = 0.05f; // 1.0遘帝俣繧・.05遘貞綾縺ｿ・亥粋險・1繧ｭ繝ｼ繝輔Ξ繝ｼ繝・峨〒逕滓・

    // 荳譎ら噪縺ｫ繝昴・繧ｺ繧帝驕ｿ
    std::vector<Vector3> origTrans(joints_.size());
    std::vector<Vector3> origRot(joints_.size());
    std::vector<Vector3> origScale(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) {
        origTrans[i] = joints_[i].translation;
        origRot[i] = joints_[i].rotation;
        origScale[i] = joints_[i].scale;
    }

    for (float t = 0.0f; t <= 1.0f + 1e-4f; t += step) {
        // 1.0遘偵〒1繧ｵ繧､繧ｯ繝ｫ・亥捉譛・2 * PI・峨↓縺吶ｋ縺溘ａ縲√せ繝斐・繝峨・ 2.0 * PI
        float angle = t * 2.0f * 3.14159265f;

        ResetPose(); // T繝昴・繧ｺ縺九ｉ繧ｹ繧ｿ繝ｼ繝・

        // --- 蟆剰ｵｰ繧翫Ο繧ｸ繝・け ---
        // 1. 鬪ｨ逶､ (閻ｰ) 縺ｮ荳贋ｸ矩°蜍・(襍ｰ繧矩圀縺ｮ蠑ｾ縺ｿ) 縺ｨ蟾ｦ蜿ｳ縺ｮ縺ｲ縺ｭ繧・
        joints_[0].translation.y = 0.76f + std::abs(std::sin(angle * 2.0f)) * 0.06f;
        joints_[0].rotation.y = std::sin(angle) * 0.1f;
        joints_[0].rotation.z = std::sin(angle) * 0.05f;

        // 2. 閼頑､・(閭ｸ) 縺ｮ蜑榊だ蟋ｿ蜍｢・郁ｵｰ繧区凾縺ｯ蜑阪↓縺､繧薙・繧√ｋ・・
        joints_[1].rotation.x = 0.18f; // 蜑榊だ
        joints_[1].rotation.y = -std::sin(angle) * 0.08f; // 荳雁濠霄ｫ縺ｮ騾・・縺ｭ繧・

        // 3. 雜ｳ・亥､ｪ繧ゅｂ縺ｨ閹晢ｼ・
        // 蟾ｦ螟ｪ繧ゅｂ(9), 蟾ｦ閹・10) | 蜿ｳ螟ｪ繧ゅｂ(12), 蜿ｳ閹・13)
        joints_[9].rotation.x = std::sin(angle) * 0.7f;
        joints_[12].rotation.x = -std::sin(angle) * 0.7f;

        // 閹昴・蠕後ｍ縺ｫ縺ｮ縺ｿ螟ｧ縺阪￥譖ｲ縺後ｋ・郁ｵｰ繧区凾縺ｮ繧ｭ繝・け縺ｨ蠑輔″縺､縺托ｼ・
        joints_[10].rotation.x = (std::sin(angle + 1.57f) + 1.0f) * 0.55f;
        joints_[13].rotation.x = (-std::sin(angle + 1.57f) + 1.0f) * 0.55f;

        // 4. 閻包ｼ郁か縺ｨ閧假ｼ・
        // 蟾ｦ閧ｩ(3), 蟾ｦ閧・4) | 蜿ｳ閧ｩ(6), 蜿ｳ閧・7)
        // 閻輔ｒ螟ｧ縺阪￥蜑榊ｾ後↓謖ｯ繧翫∬ｘ縺ｯ90蠎ｦ霑代￥縺ｫ蝗ｺ螳壹＠縺溘∪縺ｾ謖ｯ繧・
        joints_[3].rotation.x = -std::sin(angle) * 0.7f;
        joints_[6].rotation.x = std::sin(angle) * 0.7f;

        // 閧倥・90蠎ｦ・育ｴ・.3繝ｩ繧ｸ繧｢繝ｳ・画峇縺偵※蝗ｺ螳壽ｰ怜袖縺ｫ縺吶ｋ
        joints_[4].rotation.z = -1.3f - std::sin(angle) * 0.15f;
        joints_[7].rotation.z = 1.3f + std::sin(angle) * 0.15f;

        // 鬥悶ｒ蟆代＠蜑阪↓蜷代￠繧・
        joints_[2].rotation.x = -0.1f;

        // 縺昴・迸ｬ髢薙・繝昴・繧ｺ繧偵く繝ｼ繝輔Ξ繝ｼ繝縺ｨ縺励※險倬鹸
        AddKeyframe(t);
    }

    // 蜈・・繝昴・繧ｺ・育ｷｨ髮・憾諷具ｼ峨ｒ蠕ｩ蜈・
    for (size_t i = 0; i < joints_.size(); ++i) {
        joints_[i].translation = origTrans[i];
        joints_[i].rotation = origRot[i];
        joints_[i].scale = origScale[i];
    }
}

float SkinnedModel::GetMotionDuration() const {
    if (activeMotionIndex_ >= 0 && activeMotionIndex_ < static_cast<int>(motions_.size())) {
        return motions_[activeMotionIndex_].duration;
    }
    return 2.0f;
}

void SkinnedModel::SetMotionDuration(float duration) {
    if (activeMotionIndex_ >= 0 && activeMotionIndex_ < static_cast<int>(motions_.size())) {
        motions_[activeMotionIndex_].duration = duration;
    }
}

void SkinnedModel::ClearKeyframes() {
    if (activeMotionIndex_ >= 0 && activeMotionIndex_ < motions_.size()) {
        motions_[activeMotionIndex_].jointAnimations.clear();
    }
}

void SkinnedModel::AddKeyframe(float time) {
    if (activeMotionIndex_ < 0 || activeMotionIndex_ >= motions_.size()) return;
    auto& motionData = motions_[activeMotionIndex_];
    
    if (motionData.jointAnimations.empty()) {
        motionData.jointAnimations.resize(joints_.size());
        for (size_t i = 0; i < joints_.size(); ++i) {
            motionData.jointAnimations[i].name = joints_[i].name;
        }
    }
    for (size_t i = 0; i < joints_.size(); ++i) {
        JointKeyframe kf;
        kf.time = time;
        kf.translation = joints_[i].translation;
        kf.rotation = joints_[i].rotation;
        kf.scale = joints_[i].scale;
        kf.rotationQuat = joints_[i].rotationQuat;
        kf.isQuaternion = joints_[i].isQuaternion;
        motionData.jointAnimations[i].keyframes.push_back(kf);
    }
}

bool SkinnedModel::SaveMotion(const std::string& filePath) {
    return true;
}

bool SkinnedModel::LoadMotion(const std::string& filePath) {
    return true;
}

MotionData& SkinnedModel::GetMotionData() {
    if (motions_.empty()) {
        motions_.push_back(MotionData{"Motion_0", 2.0f, {}});
        activeMotionIndex_ = 0;
    }
    return motions_[activeMotionIndex_];
}

void SkinnedModel::SetActiveMotionIndex(int index) {
    if (index >= 0 && index < motions_.size()) {
        activeMotionIndex_ = index;
    }
}

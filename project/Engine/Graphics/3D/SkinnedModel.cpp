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
#include "json.hpp"

using json = nlohmann::json;

namespace {
    // 
    Vector3 TransformCoord(const Vector3& v, const Matrix4x4& m) {
        float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
        if (std::abs(w) < 1e-5f) w = 1.0f;
        return {
            (v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0]) / w,
            (v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1]) / w,
            (v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2]) / w
        };
    }

    // ()
    Vector3 TransformNormal(const Vector3& n, const Matrix4x4& m) {
        Vector3 res = {
            n.x * m.m[0][0] + n.y * m.m[1][0] + n.z * m.m[2][0],
            n.x * m.m[0][1] + n.y * m.m[1][1] + n.z * m.m[2][1],
            n.x * m.m[0][2] + n.y * m.m[1][2] + n.z * m.m[2][2]
        };
        return Math::Normalize(res);
    }

    json ToJson(const Vector3& value) {
        return json::array({ value.x, value.y, value.z });
    }

    json ToJson(const Quaternion& value) {
        return json::array({ value.x, value.y, value.z, value.w });
    }

    Vector3 ReadVector3(const json& value, const Vector3& fallback) {
        if (!value.is_array() || value.size() < 3) {
            return fallback;
        }
        return {
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>()
        };
    }

    Quaternion ReadQuaternion(const json& value, const Quaternion& fallback) {
        if (!value.is_array() || value.size() < 4) {
            return fallback;
        }
        return {
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>(),
            value.at(3).get<float>()
        };
    }

    void SortKeyframes(JointAnimation& animation) {
        std::sort(animation.keyframes.begin(), animation.keyframes.end(), [](const JointKeyframe& a, const JointKeyframe& b) {
            return a.time < b.time;
        });
    }
}

void SkinnedModel::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager) {
    restPoseCaptured_ = false;
    // 1. 
    CreateHumanoidSkeleton();

    // 2.  () 
    GenerateHumanoidMesh();
    
    // 3. 
    SmoothWeights();

    BuildJointMetadata();

    // 4. 
    if (textureManager) {
        textureHandle_ = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    }

    // 5. 
    CreateBuffers(dxCommon);

    // 6.  Model ()
    model_ = std::make_unique<Model>();
    // model_ 

    // 7.  ()
    ResetPose();
    CaptureRestPose();
}

void SkinnedModel::InitializeFromGltf(DirectXCommon* dxCommon, const std::string& filePath, TextureManager* textureManager) {
    restPoseCaptured_ = false;
    std::string texturePath;
    
    // glTF
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

    BuildJointMetadata();
    CaptureRestPose();
}

void SkinnedModel::ResetPose() {
    if (restPoseCaptured_) {
        for (auto& joint : joints_) {
            joint.translation = joint.restTranslation;
            joint.rotation = joint.restRotation;
            joint.scale = joint.restScale;
            joint.rotationQuat = joint.restRotationQuat;
            joint.isQuaternion = joint.restIsQuaternion;
        }
        return;
    }

    for (size_t i = 0; i < joints_.size(); ++i) {
        joints_[i].scale = { 1.0f, 1.0f, 1.0f };
        

        if (joints_[i].name == "LeftShoulder") {
            joints_[i].rotation = { 0.0f, 0.0f, 1.3f };  // ()
        } else if (joints_[i].name == "RightShoulder") {
            joints_[i].rotation = { 0.0f, 0.0f, -1.3f }; // ()
        } else {
            joints_[i].rotation = { 0.0f, 0.0f, 0.0f };
        }
        joints_[i].rotationQuat = Math::MakeQuaternionFromEuler(joints_[i].rotation);
        joints_[i].isQuaternion = false;
    }
}

void SkinnedModel::CreateHumanoidSkeleton() {
    joints_.clear();

    // ()
    struct JointDef {
        std::string name;
        Vector3 globalPos;
        int parentIndex;
    };

    //  (5
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


    for (size_t i = 0; i < defs.size(); ++i) {
        joints_[i].name = defs[i].name;
        joints_[i].scale = { 1.0f, 1.0f, 1.0f };
        joints_[i].rotation = { 0.0f, 0.0f, 0.0f };
        joints_[i].parentIndex = defs[i].parentIndex;
        joints_[i].externalParentMatrix = Math::MakeIdentity4x4();

        // ( translation) 
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


    for (size_t i = 0; i < joints_.size(); ++i) {
        Matrix4x4 localTrans = Math::MakeTranslateMatrix(joints_[i].translation);
        //  localMatrix 
        joints_[i].localMatrix = localTrans;

        if (joints_[i].parentIndex == -1) {
            joints_[i].globalMatrix = joints_[i].localMatrix;
        } else {
            int parentIdx = joints_[i].parentIndex;
            joints_[i].globalMatrix = Math::Multiply(joints_[i].localMatrix, joints_[parentIdx].globalMatrix);
        }

        //  (Inverse Bind Pose Matrix)
        joints_[i].offsetMatrix = Math::Inverse(joints_[i].globalMatrix);
    }

    BuildJointMetadata();
}

void SkinnedModel::BuildJointMetadata() {
    rootJointIndex_ = -1;
    jointIndexMap_.clear();

    for (auto& joint : joints_) {
        joint.childIndices.clear();
    }

    for (size_t i = 0; i < joints_.size(); ++i) {
        Joint& joint = joints_[i];
        jointIndexMap_[joint.name] = static_cast<int>(i);

        if (joint.parentIndex >= 0 && joint.parentIndex < static_cast<int>(joints_.size())) {
            joints_[joint.parentIndex].childIndices.push_back(static_cast<int>(i));
        } else if (rootJointIndex_ == -1) {
            rootJointIndex_ = static_cast<int>(i);
        }
    }
}

void SkinnedModel::CaptureRestPose() {
    for (auto& joint : joints_) {
        joint.restTranslation = joint.translation;
        joint.restRotation = joint.rotation;
        joint.restScale = joint.scale;
        joint.restRotationQuat = joint.rotationQuat;
        joint.restIsQuaternion = joint.isQuaternion;
    }
    restPoseCaptured_ = true;
}

void SkinnedModel::GenerateHumanoidMesh() {
    skinnedVertices_.clear();

    //  Cube 
    // Pelvis ()
    AddCubeMesh({ 0.0f, 0.8f, 0.0f }, { 0.4f, 0.2f, 0.2f }, 0);
    // Spine ()
    AddCubeMesh({ 0.0f, 1.1f, 0.0f }, { 0.5f, 0.4f, 0.22f }, 1);
    // Head ()
    AddCubeMesh({ 0.0f, 1.45f, 0.0f }, { 0.2f, 0.2f, 0.2f }, 2);


    AddCubeMesh({ -0.375f, 1.3f, 0.0f }, { 0.25f, 0.1f, 0.1f }, 3);
    AddCubeMesh({ -0.6f, 1.3f, 0.0f }, { 0.2f, 0.08f, 0.08f }, 4);
    AddCubeMesh({ -0.725f, 1.3f, 0.0f }, { 0.07f, 0.07f, 0.07f }, 5);


    AddCubeMesh({ 0.375f, 1.3f, 0.0f }, { 0.25f, 0.1f, 0.1f }, 6);
    AddCubeMesh({ 0.6f, 1.3f, 0.0f }, { 0.2f, 0.08f, 0.08f }, 7);
    AddCubeMesh({ 0.725f, 1.3f, 0.0f }, { 0.07f, 0.07f, 0.07f }, 8);


    AddCubeMesh({ -0.15f, 0.525f, 0.0f }, { 0.12f, 0.35f, 0.12f }, 9);
    AddCubeMesh({ -0.15f, 0.175f, 0.0f }, { 0.1f, 0.35f, 0.1f }, 10);
    AddCubeMesh({ -0.15f, -0.05f, 0.05f }, { 0.1f, 0.1f, 0.15f }, 11);


    AddCubeMesh({ 0.15f, 0.525f, 0.0f }, { 0.12f, 0.35f, 0.12f }, 12);
    AddCubeMesh({ 0.15f, 0.175f, 0.0f }, { 0.1f, 0.35f, 0.1f }, 13);
    AddCubeMesh({ 0.15f, -0.05f, 0.05f }, { 0.1f, 0.1f, 0.15f }, 14);
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
        { { 0, 2, 3, 1 }, { 0.0f, 0.0f, -1.0f } },
        { { 1, 3, 7, 5 }, { 1.0f, 0.0f, 0.0f } },
        { { 5, 7, 6, 4 }, { 0.0f, 0.0f, 1.0f } },
        { { 4, 6, 2, 0 }, { -1.0f, 0.0f, 0.0f } },
        { { 2, 6, 7, 3 }, { 0.0f, 1.0f, 0.0f } },
        { { 4, 0, 1, 5 }, { 0.0f, -1.0f, 0.0f } }
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
        joints_[i].localMatrix = Math::Multiply(joints_[i].localMatrix, joints_[i].externalParentMatrix);

        if (joints_[i].parentIndex == -1) {
            joints_[i].globalMatrix = joints_[i].localMatrix;
        } else {
            int parentIdx = joints_[i].parentIndex;
            joints_[i].globalMatrix = Math::Multiply(joints_[i].localMatrix, joints_[parentIdx].globalMatrix);
        }
    }
    
    if (mappedPalette_ && !joints_.empty()) {
        for (size_t i = 0; i < joints_.size(); ++i) {
            Matrix4x4 skeletonSpaceMatrix = Math::Multiply(joints_[i].offsetMatrix, joints_[i].globalMatrix);
            mappedPalette_[i].skeletonSpaceMatrix = skeletonSpaceMatrix;
            mappedPalette_[i].skeletonSpaceInverseTransposeMatrix = Math::Transpose(Math::Inverse(skeletonSpaceMatrix));
        }
    }
}

void SkinnedModel::CreateBuffers(DirectXCommon* dxCommon) {
    if (skinnedVertices_.empty()) return;

    if (jointBuffer_) {
        jointBuffer_->Unmap(0, nullptr);
        mappedPalette_ = nullptr;
    }

    auto device = dxCommon->GetDevice();
    UINT sizeVB = static_cast<UINT>(sizeof(ModelVertexData) * skinnedVertices_.size());
    UINT sizeInfluence = static_cast<UINT>(sizeof(VertexInfluence) * skinnedVertices_.size());

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
        ModelVertexData* vertMap = nullptr;
        vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertMap));
        for (size_t i = 0; i < skinnedVertices_.size(); ++i) {
            vertMap[i].position = skinnedVertices_[i].position;
            vertMap[i].texcoord = skinnedVertices_[i].texcoord;
            vertMap[i].normal = skinnedVertices_[i].normal;
        }
        vertexBuffer_->Unmap(0, nullptr);

        vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = sizeVB;
        vertexBufferView_.StrideInBytes = sizeof(ModelVertexData);
    }

    resDesc.Width = sizeInfluence;
    hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&influenceBuffer_));

    if (SUCCEEDED(hr)) {
        VertexInfluence* influenceMap = nullptr;
        influenceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&influenceMap));
        for (size_t i = 0; i < skinnedVertices_.size(); ++i) {
            for (int influenceIndex = 0; influenceIndex < 4; ++influenceIndex) {
                influenceMap[i].weights[influenceIndex] = skinnedVertices_[i].weights[influenceIndex];
                influenceMap[i].jointIndices[influenceIndex] = skinnedVertices_[i].jointIndices[influenceIndex];
            }
        }
        influenceBuffer_->Unmap(0, nullptr);

        influenceBufferView_.BufferLocation = influenceBuffer_->GetGPUVirtualAddress();
        influenceBufferView_.SizeInBytes = sizeInfluence;
        influenceBufferView_.StrideInBytes = sizeof(VertexInfluence);
    }
    
    // Create matrix palette buffer.
    if (!joints_.empty()) {
        UINT sizeJoints = static_cast<UINT>(sizeof(WellForGPU) * joints_.size());
        resDesc.Width = sizeJoints;
        HRESULT paletteHr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&jointBuffer_));
        if (SUCCEEDED(paletteHr)) {
            jointBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette_));
            for (size_t i = 0; i < joints_.size(); ++i) {
                mappedPalette_[i].skeletonSpaceMatrix = Math::MakeIdentity4x4();
                mappedPalette_[i].skeletonSpaceInverseTransposeMatrix = Math::MakeIdentity4x4();
            }
        }
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

    if (restPoseCaptured_) {
        for (auto& joint : joints_) {
            joint.translation = joint.restTranslation;
            joint.rotation = joint.restRotation;
            joint.scale = joint.restScale;
            joint.rotationQuat = joint.restRotationQuat;
            joint.isQuaternion = joint.restIsQuaternion;
        }
    }

    // Duration 
    float loopedTime = std::fmod(time, activeMotion.duration);
    if (loopedTime < 0.0f) loopedTime += activeMotion.duration;

    for (size_t i = 0; i < joints_.size(); ++i) {
        if (i >= activeMotion.jointAnimations.size()) continue;
        const auto& jointAnim = activeMotion.jointAnimations[i];
        if (jointAnim.keyframes.empty()) continue;

        auto& joint = joints_[i];

        // 1
        if (jointAnim.keyframes.size() == 1) {
            joint.translation = jointAnim.keyframes[0].translation;
            joint.rotation = jointAnim.keyframes[0].rotation;
            joint.scale = jointAnim.keyframes[0].scale;
            joint.rotationQuat = jointAnim.keyframes[0].rotationQuat;
            joint.isQuaternion = jointAnim.keyframes[0].isQuaternion;
            continue;
        }


        if (loopedTime <= jointAnim.keyframes.front().time) {
            const auto& first = jointAnim.keyframes.front();
            joint.translation = first.translation;
            joint.rotation = first.rotation;
            joint.scale = first.scale;
            joint.rotationQuat = first.rotationQuat;
            joint.isQuaternion = first.isQuaternion;
            continue;
        }


        if (loopedTime >= jointAnim.keyframes.back().time) {
            const auto& last = jointAnim.keyframes.back();
            joint.translation = last.translation;
            joint.rotation = last.rotation;
            joint.scale = last.scale;
            joint.rotationQuat = last.rotationQuat;
            joint.isQuaternion = last.isQuaternion;
            continue;
        }


        for (size_t k = 0; k < jointAnim.keyframes.size() - 1; ++k) {
            const auto& kfA = jointAnim.keyframes[k];
            const auto& kfB = jointAnim.keyframes[k + 1];

            if (loopedTime >= kfA.time && loopedTime <= kfB.time) {
                float t = (loopedTime - kfA.time) / (kfB.time - kfA.time);


                joint.translation = {
                    kfA.translation.x + t * (kfB.translation.x - kfA.translation.x),
                    kfA.translation.y + t * (kfB.translation.y - kfA.translation.y),
                    kfA.translation.z + t * (kfB.translation.z - kfA.translation.z)
                };


                if (kfA.isQuaternion && kfB.isQuaternion) {
                    joint.rotationQuat = Math::Slerp(kfA.rotationQuat, kfB.rotationQuat, t);
                    joint.rotation = Math::ToEuler(joint.rotationQuat);
                    joint.isQuaternion = true;
                } else {
                    //  ( Lerp)
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

    // 2.0秒のアニメーションを0.1秒刻み(計21キーフレーム)で生成
    GetMotionData().duration = 2.0f;
    float step = 0.1f;

    // 一時的にポーズを退避
    std::vector<Vector3> origTrans(joints_.size());
    std::vector<Vector3> origRot(joints_.size());
    std::vector<Vector3> origScale(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) {
        origTrans[i] = joints_[i].translation;
        origRot[i] = joints_[i].rotation;
        origScale[i] = joints_[i].scale;
    }

    for (float t = 0.0f; t <= 2.0f + 1e-4f; t += step) {
        // テスト用歩行アニメーションのロジックを適用してポーズを計算
        // 2.0秒で1サイクル(周期 2 * PI)にするため、スピードを調節する
        // スピード speed = PI (t = 2.0秒のとき、入力値 = 2.0 * PI となる)
        ApplyTestAnimation(t, 3.14159265f);

        // その瞬間のポーズをキーフレームとして記録
        AddKeyframe(t);
    }

    // 元のポーズ(初期状態)を復元
    for (size_t i = 0; i < joints_.size(); ++i) {
        joints_[i].translation = origTrans[i];
        joints_[i].rotation = origRot[i];
        joints_[i].scale = origScale[i];
    }
}

void SkinnedModel::GenerateRunPreset() {
    ClearKeyframes();

    // 小走り。1サイクル1.0秒の素早いループが綺麗
    GetMotionData().duration = 1.0f;
    float step = 0.05f; // 1.0秒間を0.05秒刻み(合計21キーフレーム)で生成

    // 一時的にポーズを退避
    std::vector<Vector3> origTrans(joints_.size());
    std::vector<Vector3> origRot(joints_.size());
    std::vector<Vector3> origScale(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) {
        origTrans[i] = joints_[i].translation;
        origRot[i] = joints_[i].rotation;
        origScale[i] = joints_[i].scale;
    }

    for (float t = 0.0f; t <= 1.0f + 1e-4f; t += step) {
        // 1.0秒で1サイクル(周期 2 * PI)にするため、スピードは 2.0 * PI
        float angle = t * 2.0f * 3.14159265f;

        ResetPose(); // Tポーズからスタート

        // --- 小走りロジック ---
        // 1. 骨盤 (腰) の上下運動(走る際の弾み) と左右のひねり
        joints_[0].translation.y = 0.76f + std::abs(std::sin(angle * 2.0f)) * 0.06f;
        joints_[0].rotation.y = std::sin(angle) * 0.1f;
        joints_[0].rotation.z = std::sin(angle) * 0.05f;

        // 2. 脊椎 (胸) の前傾姿勢(走る時は前につんのめる)
        joints_[1].rotation.x = 0.18f; // 前傾
        joints_[1].rotation.y = -std::sin(angle) * 0.08f; // 上半身のひねり

        // 3. 足(太ももと膝)
        // 左太もも(9), 左膝(10) | 右太もも(12), 右膝(13)
        joints_[9].rotation.x = std::sin(angle) * 0.7f;
        joints_[12].rotation.x = -std::sin(angle) * 0.7f;

        // 膝は後ろにのみ大きく曲がる(走る時のキックと引きつけ)
        joints_[10].rotation.x = (std::sin(angle + 1.57f) + 1.0f) * 0.55f;
        joints_[13].rotation.x = (-std::sin(angle + 1.57f) + 1.0f) * 0.55f;

        // 4. 腕(肩と肘)
        // 左肩(3), 左肘(4) | 右肩(6), 右肘(7)
        // 腕を大きく前後に振り、肘は90度近くに固定したまま振る
        joints_[3].rotation.x = -std::sin(angle) * 0.7f;
        joints_[6].rotation.x = std::sin(angle) * 0.7f;

        // 肘は90度(約1.3ラジアン)曲げて固定気味にする
        joints_[4].rotation.z = -1.3f - std::sin(angle) * 0.15f;
        joints_[7].rotation.z = 1.3f + std::sin(angle) * 0.15f;

        // 首を少し前に向ける
        joints_[2].rotation.x = -0.1f;

        // その瞬間のポーズをキーフレームとして記録
        AddKeyframe(t);
    }

    // 元のポーズ(初期状態)を復元
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
    GetMotionData().duration = duration;
}

void SkinnedModel::ClearKeyframes() {
    GetMotionData().jointAnimations.clear();
}

void SkinnedModel::AddKeyframe(float time) {
    auto& motionData = GetMotionData();
    
    if (motionData.jointAnimations.empty()) {
        motionData.jointAnimations.resize(joints_.size());
        for (size_t i = 0; i < joints_.size(); ++i) {
            motionData.jointAnimations[i].name = joints_[i].name;
        }
    }
    for (size_t i = 0; i < joints_.size(); ++i) {
        if (i >= motionData.jointAnimations.size()) {
            motionData.jointAnimations.resize(joints_.size());
        }
        JointAnimation& jointAnimation = motionData.jointAnimations[i];
        if (jointAnimation.name.empty()) {
            jointAnimation.name = joints_[i].name;
        }

        JointKeyframe kf;
        kf.time = time;
        kf.translation = joints_[i].translation;
        kf.rotation = joints_[i].rotation;
        kf.scale = joints_[i].scale;
        kf.rotationQuat = joints_[i].rotationQuat;
        kf.isQuaternion = joints_[i].isQuaternion;

        auto existing = std::find_if(jointAnimation.keyframes.begin(), jointAnimation.keyframes.end(), [time](const JointKeyframe& item) {
            return std::abs(item.time - time) < 1.0e-4f;
        });
        if (existing != jointAnimation.keyframes.end()) {
            *existing = kf;
        } else {
            jointAnimation.keyframes.push_back(kf);
        }
        SortKeyframes(jointAnimation);
    }
}

bool SkinnedModel::SaveMotion(const std::string& filePath) {
    try {
        const MotionData& motionData = GetMotionData();

        std::filesystem::path outputPath(filePath);
        if (outputPath.has_parent_path()) {
            std::filesystem::create_directories(outputPath.parent_path());
        }

        json root;
        root["version"] = 1;
        root["type"] = "CG2Motion";
        root["modelName"] = name_;
        root["motion"]["name"] = motionData.name;
        root["motion"]["duration"] = motionData.duration;
        root["motion"]["active"] = activeMotionIndex_;

        json jointsJson = json::array();
        for (const auto& joint : joints_) {
            jointsJson.push_back({
                { "name", joint.name },
                { "parent", joint.parentIndex }
            });
        }
        root["skeleton"]["joints"] = jointsJson;

        json animationsJson = json::array();
        for (const auto& jointAnimation : motionData.jointAnimations) {
            json jointJson;
            jointJson["name"] = jointAnimation.name;
            jointJson["keyframes"] = json::array();

            for (const auto& keyframe : jointAnimation.keyframes) {
                jointJson["keyframes"].push_back({
                    { "time", keyframe.time },
                    { "translation", ToJson(keyframe.translation) },
                    { "rotation", ToJson(keyframe.rotation) },
                    { "scale", ToJson(keyframe.scale) },
                    { "rotationQuat", ToJson(keyframe.rotationQuat) },
                    { "isQuaternion", keyframe.isQuaternion }
                });
            }
            animationsJson.push_back(jointJson);
        }
        root["motion"]["joints"] = animationsJson;

        std::ofstream file(filePath);
        if (!file.is_open()) {
            OutputDebugStringA(("Failed to open motion file for write: " + filePath + "\n").c_str());
            return false;
        }

        file << root.dump(4);
        return true;
    } catch (const std::exception& e) {
        OutputDebugStringA(("SaveMotion failed: " + std::string(e.what()) + "\n").c_str());
        return false;
    }
}

bool SkinnedModel::LoadMotion(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            OutputDebugStringA(("Failed to open motion file for read: " + filePath + "\n").c_str());
            return false;
        }

        json root;
        file >> root;

        const json& motionJson = root.contains("motion") ? root.at("motion") : root;

        MotionData loadedMotion;
        loadedMotion.name = motionJson.value("name", std::filesystem::path(filePath).stem().string());
        loadedMotion.duration = motionJson.value("duration", 2.0f);
        loadedMotion.jointAnimations.resize(joints_.size());
        for (size_t i = 0; i < joints_.size(); ++i) {
            loadedMotion.jointAnimations[i].name = joints_[i].name;
        }

        if (motionJson.contains("joints") && motionJson.at("joints").is_array()) {
            for (const auto& jointJson : motionJson.at("joints")) {
                std::string jointName = jointJson.value("name", "");
                auto jointIt = jointIndexMap_.find(jointName);
                if (jointIt == jointIndexMap_.end()) {
                    continue;
                }

                int jointIndex = jointIt->second;
                if (jointIndex < 0 || jointIndex >= static_cast<int>(loadedMotion.jointAnimations.size())) {
                    continue;
                }

                JointAnimation& jointAnimation = loadedMotion.jointAnimations[static_cast<size_t>(jointIndex)];
                jointAnimation.name = jointName;
                jointAnimation.keyframes.clear();

                if (!jointJson.contains("keyframes") || !jointJson.at("keyframes").is_array()) {
                    continue;
                }

                for (const auto& keyframeJson : jointJson.at("keyframes")) {
                    JointKeyframe keyframe;
                    keyframe.time = keyframeJson.value("time", 0.0f);
                    keyframe.translation = ReadVector3(keyframeJson.value("translation", json::array()), joints_[static_cast<size_t>(jointIndex)].translation);
                    keyframe.rotation = ReadVector3(keyframeJson.value("rotation", json::array()), joints_[static_cast<size_t>(jointIndex)].rotation);
                    keyframe.scale = ReadVector3(keyframeJson.value("scale", json::array()), joints_[static_cast<size_t>(jointIndex)].scale);
                    keyframe.rotationQuat = ReadQuaternion(keyframeJson.value("rotationQuat", json::array()), joints_[static_cast<size_t>(jointIndex)].rotationQuat);
                    keyframe.isQuaternion = keyframeJson.value("isQuaternion", false);
                    jointAnimation.keyframes.push_back(keyframe);
                }
                SortKeyframes(jointAnimation);
            }
        }

        if (motions_.empty()) {
            motions_.push_back(loadedMotion);
            activeMotionIndex_ = 0;
        } else if (activeMotionIndex_ >= 0 && activeMotionIndex_ < static_cast<int>(motions_.size())) {
            motions_[static_cast<size_t>(activeMotionIndex_)] = loadedMotion;
        } else {
            motions_.push_back(loadedMotion);
            activeMotionIndex_ = static_cast<int>(motions_.size()) - 1;
        }

        ApplyMotion(0.0f);
        return true;
    } catch (const std::exception& e) {
        OutputDebugStringA(("LoadMotion failed: " + std::string(e.what()) + "\n").c_str());
        return false;
    }
}

MotionData& SkinnedModel::GetMotionData() {
    if (motions_.empty()) {
        motions_.push_back(MotionData{"Motion_0", 2.0f, {}});
        activeMotionIndex_ = 0;
    }
    if (activeMotionIndex_ < 0 || activeMotionIndex_ >= static_cast<int>(motions_.size())) {
        activeMotionIndex_ = 0;
    }
    return motions_[activeMotionIndex_];
}

const MotionData& SkinnedModel::GetMotionData() const {
    static const MotionData emptyMotion{ "Motion_0", 2.0f, {} };
    if (motions_.empty()) {
        return emptyMotion;
    }
    if (activeMotionIndex_ < 0 || activeMotionIndex_ >= static_cast<int>(motions_.size())) {
        return motions_.front();
    }
    return motions_[static_cast<size_t>(activeMotionIndex_)];
}

void SkinnedModel::SetActiveMotionIndex(int index) {
    if (index >= 0 && index < static_cast<int>(motions_.size())) {
        activeMotionIndex_ = index;
    }
}

void SkinnedModel::SetActiveMotionName(const std::string& name) {
    GetMotionData().name = name.empty() ? "CustomMotion" : name;
}

void SkinnedModel::PlayAnimation(const std::string& animationName) {
    for (size_t i = 0; i < motions_.size(); ++i) {
        if (motions_[i].name == animationName) {
            activeMotionIndex_ = static_cast<int>(i);
            ApplyMotion(0.0f);
            return;
        }
    }
}

void SkinnedModel::EvaluateAnimation(float time) {
    ApplyMotion(time);
}

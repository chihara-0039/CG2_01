#include "SkinnedObject.h"
#include "MyMath.h"
#include <cmath>

void SkinnedObject::Initialize(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, TextureManager* textureManager) {
    // 1. 郢ｧ・ｹ郢ｧ・ｭ郢昜ｹ斟ｦ郢ｧ・ｰ郢晢ｽ｢郢昴・ﾎ晉ｸｺ・ｮ騾墓ｻ薙・
    skinnedModel_ = std::make_unique<SkinnedModel>();
    skinnedModel_->Initialize(dxCommon, textureManager);

    // 2. 髯ｦ・ｨ驕会ｽｺ騾包ｽｨ邵ｺ・ｮObject3d郢ｧ雋槭・隴帶ｺｷ蝟ｧ邵ｺ蜉ｱ窶ｻ邵ｲ繝ｾkinnedModel陷繝ｻﾎ夂ｸｺ・ｮModel郢ｧ蝣､蛹ｳ鬪ｭ・ｲ
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(skinnedModel_->GetModel());
}

void SkinnedObject::InitializeFromGltf(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, const std::string& filePath, TextureManager* textureManager) {
    // 1. 郢ｧ・ｹ郢ｧ・ｭ郢昜ｹ斟ｦ郢ｧ・ｰ郢晢ｽ｢郢昴・ﾎ晉ｹｧ證僕TF邵ｺ荵晢ｽ蛾墓ｻ薙・
    skinnedModel_ = std::make_unique<SkinnedModel>();
    skinnedModel_->InitializeFromGltf(dxCommon, filePath, textureManager);

    // 2. 髯ｦ・ｨ驕会ｽｺ騾包ｽｨ邵ｺ・ｮObject3d郢ｧ雋槭・隴帶ｺｷ蝟ｧ邵ｺ蜉ｱ窶ｻ邵ｲ繝ｾkinnedModel陷繝ｻﾎ夂ｸｺ・ｮModel郢ｧ蝣､蛹ｳ鬪ｭ・ｲ
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(skinnedModel_->GetModel());
}

void SkinnedObject::Update(DirectXCommon* dxCommon, const Matrix4x4& lightVP) {
    // 1. 郢ｧ・｢郢昜ｹ斟鍋ｹ晢ｽｼ郢ｧ・ｷ郢晢ｽｧ郢晢ｽｳ邵ｺ・ｮ陷蜥ｲ蜃ｽ
    if (playAnimation_) {
        // 60FPS隲・ｳ陞ｳ螢ｹ縲定ｭ弱ｋ菫｣郢ｧ蟶敖・ｲ郢ｧ竏夲ｽ・
        animationTime_ += (1.0f / 60.0f);
        skinnedModel_->ApplyTestAnimation(animationTime_, animationSpeed_);
    } else if (playCustomAnimation_) {
        // 郢ｧ・ｫ郢ｧ・ｹ郢ｧ・ｿ郢晢｣ｰ郢ｧ・ｭ郢晢ｽｼ郢晁ｼ釆樒ｹ晢ｽｼ郢晢｣ｰ郢晢ｽ｢郢晢ｽｼ郢ｧ・ｷ郢晢ｽｧ郢晢ｽｳ邵ｺ・ｮ陷蜥ｲ蜃ｽ
        animationTime_ += (1.0f / 60.0f) * animationSpeed_;
        float dur = skinnedModel_->GetMotionDuration();
        currentKeyframeTime_ = std::fmod(animationTime_, dur);
        if (currentKeyframeTime_ < 0.0f) currentKeyframeTime_ += dur;
        skinnedModel_->ApplyMotion(currentKeyframeTime_);
    }

    // 2. 郢ｧ・ｹ郢ｧ・ｭ郢昜ｹ斟ｦ郢ｧ・ｰ鬯・ｉ縺幃坎閧ｲ・ｮ蜉ｱ繝ｻ陞ｳ貅ｯ・｡蠕娯・GPU邵ｺ・ｸ邵ｺ・ｮ髴・ｽ｢鬨ｾ繝ｻ
    skinnedModel_->Update(dxCommon);

    // 3. Object3d 邵ｺ・ｮ郢晏現ﾎ帷ｹ晢ｽｳ郢ｧ・ｹ郢晁ｼ斐°郢晢ｽｼ郢晢｣ｰ髫ｪ・ｭ陞ｳ螢ｹ竊帝勗謔溘・隴厄ｽｴ隴・ｽｰ
    if (object3d_) {
        object3d_->SetPosition(position_);
        object3d_->SetRotation(rotation_);
        object3d_->SetScale(scale_);
        object3d_->SetCamera(viewMatrix_, projectionMatrix_);
        object3d_->Update(lightVP);
    }
}

void SkinnedObject::Draw() {
    if (!skinnedModel_ || !object3d_) return;
    
    auto commandList = object3d_->GetObject3dCommon()->GetDxCommon()->GetCommandList();
    
    // pipeline setup
    if (object3d_->GetObject3dCommon()->GetSkinnedPipelineState()) {
        commandList->SetPipelineState(object3d_->GetObject3dCommon()->GetSkinnedPipelineState());
    }

    // 0. 繝槭ユ繝ｪ繧｢繝ｫ
    commandList->SetGraphicsRootConstantBufferView(0, object3d_->GetMaterialResource()->GetGPUVirtualAddress());
    // 1. Transform (繝医Λ繝ｳ繧ｹ繝輔か繝ｼ繝)
    commandList->SetGraphicsRootConstantBufferView(1, object3d_->GetTransformationResource()->GetGPUVirtualAddress());
    // 2. Light (蟷ｳ陦悟・貅・ (Common縺梧戟縺､)
    commandList->SetGraphicsRootConstantBufferView(2, object3d_->GetObject3dCommon()->GetLightGPUVirtualAddress());
    // 3. Texture (繝・け繧ｹ繝√Ε)
    if (object3d_->GetObject3dCommon()->GetTextureManager()) {
        auto gpuHandle = object3d_->GetObject3dCommon()->GetTextureManager()->GetSrvHandleGPU(skinnedModel_->GetTextureHandle());
        commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);
    }
    
    // 5. JointMatrices (繧ｸ繝ｧ繧､繝ｳ繝郁｡悟・繝舌ャ繝輔ぃ)
    if (skinnedModel_->GetJointBuffer()) {
        commandList->SetGraphicsRootShaderResourceView(5, skinnedModel_->GetJointBuffer()->GetGPUVirtualAddress());
    }
    
    // 鬆らせ繝舌ャ繝輔ぃ繧偵ヰ繧､繝ｳ繝峨＠縺ｦ謠冗判
    D3D12_VERTEX_BUFFER_VIEW vbView = skinnedModel_->GetVertexBufferView();
    commandList->IASetVertexBuffers(0, 1, &vbView);
    commandList->DrawInstanced(static_cast<UINT>(skinnedModel_->GetVertexCount()), 1, 0, 0);
    
    // restore pipeline
    if (object3d_->GetObject3dCommon()->GetPipelineState()) {
        commandList->SetPipelineState(object3d_->GetObject3dCommon()->GetPipelineState());
    }
}

void SkinnedObject::DrawShadow(const Matrix4x4& lightViewProjection) {
    if (!skinnedModel_ || !object3d_) return;

    auto commandList = object3d_->GetObject3dCommon()->GetDxCommon()->GetCommandList();
    
    // Set Skinned Shadow Pipeline (We don't have one right now, we can use a custom one later, but for now we won't draw shadows for skinned objects if it's too complex, or we can just fall back to normal DrawShadow if we had a SkinnedShadow_VS.hlsl).
    // For now, skip shadow drawing or it will crash.
    // object3d_->DrawShadow(lightViewProjection);
}
void SkinnedObject::DrawSkeleton(Object3dCommon* object3dCommon, Model* cubeModel, const Matrix4x4& view, const Matrix4x4& projection) {
    if (!showSkeleton_ || !cubeModel) return;

    const auto& joints = skinnedModel_->GetJoints();
    size_t numJoints = joints.size();

    if (jointVisuals_.size() < numJoints) {
        jointVisuals_.resize(numJoints);
        for (size_t i = 0; i < numJoints; ++i) {
            jointVisuals_[i] = std::make_unique<Object3d>();
            jointVisuals_[i]->Initialize(object3dCommon);
            jointVisuals_[i]->SetModel(cubeModel);
            jointVisuals_[i]->SetScale({ 0.04f, 0.04f, 0.04f });
        }
    }

    Matrix4x4 objWorld = Math::MakeAffineMatrix(scale_, rotation_, position_);

    object3dCommon->PreDrawPlayerHighlight();

    for (size_t i = 0; i < numJoints; ++i) {
        Matrix4x4 jointWorld = Math::Multiply(joints[i].globalMatrix, objWorld);

        jointVisuals_[i]->SetCamera(view, projection);
        
        Vector3 globalPos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };
        jointVisuals_[i]->SetPosition(globalPos);
        jointVisuals_[i]->SetRotation({ 0, 0, 0 });
        jointVisuals_[i]->SetScale({ 0.04f, 0.04f, 0.04f });

        if (static_cast<int>(i) == selectedJointIndex_) {
            jointVisuals_[i]->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
        } else {
            jointVisuals_[i]->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
        }

        jointVisuals_[i]->SetEnableLighting(false);
        jointVisuals_[i]->Update(Math::MakeIdentity4x4());
        jointVisuals_[i]->Draw();
    }

    size_t boneVisualCount = 0;
    for (size_t i = 0; i < numJoints; ++i) {
        int parentIdx = joints[i].parentIndex;
        if (parentIdx == -1) continue;

        if (boneVisuals_.size() <= boneVisualCount) {
            auto boneObj = std::make_unique<Object3d>();
            boneObj->Initialize(object3dCommon);
            boneObj->SetModel(cubeModel);
            boneVisuals_.push_back(std::move(boneObj));
        }

        auto& boneObj = boneVisuals_[boneVisualCount];
        boneVisualCount++;

        Matrix4x4 pJointWorld = Math::Multiply(joints[parentIdx].globalMatrix, objWorld);
        Matrix4x4 cJointWorld = Math::Multiply(joints[i].globalMatrix, objWorld);

        Vector3 pPos = { pJointWorld.m[3][0], pJointWorld.m[3][1], pJointWorld.m[3][2] };
        Vector3 cPos = { cJointWorld.m[3][0], cJointWorld.m[3][1], cJointWorld.m[3][2] };

        Vector3 v = Math::Subtract(cPos, pPos);
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len < 0.001f) continue;

        Vector3 dir = { v.x / len, v.y / len, v.z / len };

        Vector3 centerPos = {
            (pPos.x + cPos.x) * 0.5f,
            (pPos.y + cPos.y) * 0.5f,
            (pPos.z + cPos.z) * 0.5f
        };

        float phi_y = std::atan2(dir.x, dir.z);
        float phi_x = std::atan2(std::sqrt(dir.x * dir.x + dir.z * dir.z), dir.y);

        boneObj->SetCamera(view, projection);
        boneObj->SetPosition(centerPos);
        boneObj->SetRotation({ phi_x, phi_y, 0.0f });
        boneObj->SetScale({ 0.015f, len * 0.5f, 0.015f });

        boneObj->SetColor({ 0.9f, 0.9f, 0.5f, 1.0f });
        boneObj->SetEnableLighting(false);

        boneObj->Update(Math::MakeIdentity4x4());
        boneObj->Draw();
    }

    object3dCommon->PreDraw();
}


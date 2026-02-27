#include "ModelManager.h"

// 静的メンバの定義
Object3dCommon* ModelManager::common_ = nullptr;
std::unordered_map<std::string, Model*> ModelManager::models_;
Object3d* ModelManager::internalObject_ = nullptr;
Matrix4x4 ModelManager::viewMatrix_ = Math::MakeIdentity4x4();
Matrix4x4 ModelManager::projectionMatrix_ = Math::MakeIdentity4x4();

void ModelManager::Initialize(Object3dCommon* common) {
    common_ = common;
    // 描画用の実体を一つだけ作っておく
    internalObject_ = new Object3d();
    internalObject_->Initialize(common_);
}

void ModelManager::Finalize() {
    for (auto& pair : models_) { delete pair.second; }
    models_.clear();
    delete internalObject_;
}

void ModelManager::SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
    viewMatrix_ = view;
    projectionMatrix_ = projection;
}

void ModelManager::Draw(const std::string& modelName, const Vector3& pos, const Vector3& rot, const Vector3& scale, const Vector4& color) {
    // 1. モデルがなければ読み込む（キャッシュ機能）
    if (models_.find(modelName) == models_.end()) {
        models_[modelName] = Model::CreateFromOBJ(common_->GetDxCommon(), "Resources", modelName, common_->GetTextureManager());
    }

    // 2. 使い回し用オブジェクトに設定を流し込む
    internalObject_->SetModel(models_[modelName]);
    internalObject_->SetPosition(pos);
    internalObject_->SetRotation(rot);
    internalObject_->SetScale(scale);
    internalObject_->SetColor(color);
    internalObject_->SetCamera(viewMatrix_, projectionMatrix_);

    // 3. 更新と描画
    internalObject_->Update();
    internalObject_->Draw();
}
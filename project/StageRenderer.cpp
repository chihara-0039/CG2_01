#include "StageRenderer.h"
#include <cassert>

// 解放
StageRenderer::~StageRenderer() {
    Clear();

    delete groundModel_;
    groundModel_ = nullptr;

    delete wallModel_;
    wallModel_ = nullptr;

    delete bubbleModel_;
    bubbleModel_ = nullptr;

    delete goalModel_;
    goalModel_ = nullptr;
}

// ステージマップの内容に応じて、描画用オブジェクトを生成していくクラス
void StageRenderer::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;

	// 仮モデル設定
    groundModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources",
        "block.obj",
        object3dCommon_->GetTextureManager()
    );

	// 仮モデル設定
    wallModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources",
        "block.obj",
        object3dCommon_->GetTextureManager()
    );

	// 仮モデル設定
    bubbleModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources",
        "block.obj",
        object3dCommon_->GetTextureManager()
    );


	// 仮モデル設定
    goalModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources",
        "star.obj",
        object3dCommon_->GetTextureManager()
    );
}


void StageRenderer::BuildFromStageMap(const StageMap& stageMap) {
    Clear();

    for (int y = 0; y < stageMap.GetHeight(); y++) {
        for (int z = 0; z < stageMap.GetDepth(); z++) {
            for (int x = 0; x < stageMap.GetWidth(); x++) {
                const MapCell* cell = stageMap.GetCell(x, y, z);
                if (!cell) {
                    continue;
                }

                if (cell->type == BlockType::None) {
                    continue;
                }

                Vector3 position = {
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(z)
                };

                switch (cell->type) {
                case BlockType::Ground:
                CreateStageObject(
                    groundModel_,
                    position,
                    blockScale_,
                    { 0.0f, 0.0f, 0.0f }
                );
                break;

                case BlockType::Wall:
                CreateStageObject(
                    wallModel_,
                    position,
                    blockScale_,
                    { 0.0f, 0.0f, 0.0f }
                );
                break;

                case BlockType::BubblePickup:
                CreateStageObject(
                    bubbleModel_,
                    position,
                    { blockScale_.x * 0.7f, blockScale_.y * 0.7f, blockScale_.z * 0.7f },
                    { 0.0f, 0.0f, 0.0f }
                );
                break;

                case BlockType::Goal:
                CreateStageObject(
                    goalModel_,
                    position,
                    { blockScale_.x * 0.8f, blockScale_.y * 0.8f, blockScale_.z * 0.8f },
                    { 0.0f, 0.0f, 0.0f }
                );
                break;

                case BlockType::Star:
                CreateStageObject(
                    wallModel_,
                    position,
                    blockScale_,
                    { 0.0f, 0.0f, 0.0f }
                );
                break;

                case BlockType::PlayerStart:
                CreateStageObject(
                    goalModel_,
                    position,
                    { 0.6f, 0.6f, 0.6f },
                    { 0.0f, 0.0f, 0.0f }
                );
                break;

                default:
                break;
                }
            }
        }
    }
}

void StageRenderer::SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
    for (Object3d* obj : objects_) {
        obj->SetCamera(view, projection);
    }
}

void StageRenderer::Update() {
    for (Object3d* obj : objects_) {
        obj->Update();
    }
}

void StageRenderer::Draw() {
    for (Object3d* obj : objects_) {
        obj->Draw();
    }
}

void StageRenderer::Clear() {
    for (Object3d* obj : objects_) {
        delete obj;
    }
    objects_.clear();
}

Object3d* StageRenderer::CreateStageObject(
    Model* model,
    const Vector3& position,
    const Vector3& scale,
    const Vector3& rotation
) {
    Object3d* obj = new Object3d();
    obj->Initialize(object3dCommon_);
    obj->SetModel(model);
    obj->SetPosition(position);
    obj->SetScale(scale);
    obj->SetRotation(rotation);

    objects_.push_back(obj);
    return obj;
}
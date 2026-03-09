#include "StageRenderer.h"
#include <cassert>

void StageRenderer::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;
}

void StageRenderer::BuildFromStageMap(const StageMap& stageMap) {
    Clear();

    // 仮モデル設定
    // 今は全部 plane を流用してもOK
    // あとで block.obj とか bubble.obj に差し替えればいい
    groundModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources",
        "plane.obj",
        object3dCommon_->GetTextureManager()
    );

    wallModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources",
        "plane.obj",
        object3dCommon_->GetTextureManager()
    );

    bubbleModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources",
        "plane.obj",
        object3dCommon_->GetTextureManager()
    );

    goalModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources",
        "plane.obj",
        object3dCommon_->GetTextureManager()
    );

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
                // plane.obj を床っぽく寝かせる
                CreateStageObject(
                    groundModel_,
                    position,
                    { 1.0f, 1.0f, 1.0f },
                    { 1.57f, 0.0f, 0.0f }
                );
                break;

                case BlockType::Wall:
                // 仮で立てる
                CreateStageObject(
                    wallModel_,
                    position,
                    { 1.0f, 1.0f, 1.0f },
                    { 0.0f, 0.0f, 0.0f }
                );
                break;

                case BlockType::BubblePickup:
                CreateStageObject(
                    bubbleModel_,
                    position,
                    { 0.7f, 0.7f, 0.7f },
                    { 0.0f, 0.0f, 0.0f }
                );
                break;

                case BlockType::Goal:
                CreateStageObject(
                    goalModel_,
                    position,
                    { 0.8f, 0.8f, 0.8f },
                    { 0.0f, 0.0f, 0.0f }
                );
                break;

                case BlockType::Stair:
                CreateStageObject(
                    wallModel_,
                    position,
                    { 1.0f, 1.0f, 1.0f },
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

    delete groundModel_;
    groundModel_ = nullptr;

    delete wallModel_;
    wallModel_ = nullptr;

    delete bubbleModel_;
    bubbleModel_ = nullptr;

    delete goalModel_;
    goalModel_ = nullptr;
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
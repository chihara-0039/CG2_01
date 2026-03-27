#include "MapCursor.h"
#include <cassert>

MapCursor::~MapCursor() {
    delete cursorObject_;
    cursorObject_ = nullptr;

    delete cursorModel_;
    cursorModel_ = nullptr;
}

void MapCursor::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;

    // カーソルは見やすさ優先で axis を使う
    cursorModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources",
        "choice.obj",
        object3dCommon_->GetTextureManager()
    );

    cursorObject_ = new Object3d();
    cursorObject_->Initialize(object3dCommon_);
    cursorObject_->SetModel(cursorModel_);

    // 大きすぎないように小さめ
    cursorObject_->SetScale({ 0.5f, 0.5f, 0.5f });

    // axis は立体目印としてそのまま使う
    cursorObject_->SetRotation({ 0.0f, 0.0f, 0.0f });
    cursorObject_->SetPosition(IndexToWorldPosition());
}

void MapCursor::Update() {
    if (!cursorObject_) {
        return;
    }

    // 1. 現在のインデックスから基本のワールド座標を取得
    Vector3 pos = IndexToWorldPosition();

    // 2. ★ここで高さを調整！ (例: 0.2f だけ上に浮かせる)
    pos.y += -0.16f;

    cursorObject_->SetPosition(pos);
    // モデルに対してスケールをセットする
    cursorObject_->SetScale(scale_);

    cursorObject_->Update();
}

void MapCursor::Draw() {
    if (!cursorObject_) {
        return;
    }

    cursorObject_->Draw();
}

void MapCursor::SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
    if (!cursorObject_) {
        return;
    }

    cursorObject_->SetCamera(view, projection);
}

void MapCursor::Move(int dx, int dy, int dz, const StageMap& stageMap) {
    index_.x += dx;
    index_.y += dy;
    index_.z += dz;
    ClampToStage(stageMap);
}

void MapCursor::SetIndex(const Int3& index, const StageMap& stageMap) {
    index_ = index;
    ClampToStage(stageMap);
}

void MapCursor::ClampToStage(const StageMap& stageMap) {
    if (index_.x < 0) { index_.x = 0; }
    if (index_.y < 0) { index_.y = 0; }
    if (index_.z < 0) { index_.z = 0; }

    if (stageMap.GetWidth() > 0 && index_.x >= stageMap.GetWidth()) {
        index_.x = stageMap.GetWidth() - 1;
    }
    if (stageMap.GetHeight() > 0 && index_.y >= stageMap.GetHeight()) {
        index_.y = stageMap.GetHeight() - 1;
    }
    if (stageMap.GetDepth() > 0 && index_.z >= stageMap.GetDepth()) {
        index_.z = stageMap.GetDepth() - 1;
    }
}

Vector3 MapCursor::IndexToWorldPosition() const {
    return {
        static_cast<float>(index_.x),
        static_cast<float>(index_.y) + 0.2f,
        static_cast<float>(index_.z)
    };
}
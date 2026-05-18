#pragma once
#include "StageMap.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include <memory>

class MapCursor {
public:
    MapCursor() = default;
    ~MapCursor();

    void Initialize(Object3dCommon* object3dCommon);
    void Update(const Matrix4x4& lightVP);
    void Draw();

    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection);
    void SetScale(const Vector3& scale) { scale_ = scale; }

    void Move(int dx, int dy, int dz, const StageMap& stageMap);
    void SetIndex(const Int3& index, const StageMap& stageMap);

    const Int3& GetIndex() const { return index_; }

    // ImGui描画用
    void DrawImGui();

private:

    Object3dCommon* object3dCommon_ = nullptr;
    std::unique_ptr<Model> cursorModel_;
    std::unique_ptr<Object3d> cursorObject_;

    Int3 index_{ 0, 0, 0 };

	Vector3 scale_ = { 1.2f, 1.2f, 1.2f }; // デフォルトのスケール（少し大きめ）

private:
    void ClampToStage(const StageMap& stageMap);
    Vector3 IndexToWorldPosition() const;
};
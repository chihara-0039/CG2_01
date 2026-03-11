#pragma once
#include "StageMap.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"

class MapCursor {
public:
    MapCursor() = default;
    ~MapCursor();

    void Initialize(Object3dCommon* object3dCommon);
    void Update();
    void Draw();

    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection);

    void Move(int dx, int dy, int dz, const StageMap& stageMap);
    void SetIndex(const Int3& index, const StageMap& stageMap);

    const Int3& GetIndex() const { return index_; }

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Model* cursorModel_ = nullptr;
    Object3d* cursorObject_ = nullptr;

    Int3 index_{ 0, 0, 0 };

private:
    void ClampToStage(const StageMap& stageMap);
    Vector3 IndexToWorldPosition() const;
};
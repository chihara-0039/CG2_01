#pragma once
#include <vector>
#include "StageMap.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"

class StageRenderer {
public:
    StageRenderer() = default;
    ~StageRenderer() = default;

    void Initialize(Object3dCommon* object3dCommon);
    void BuildFromStageMap(const StageMap& stageMap);

    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection);
    void Update();
    void Draw();

    void Clear();

private:
    Object3dCommon* object3dCommon_ = nullptr;

    Model* groundModel_ = nullptr;
    Model* wallModel_ = nullptr;
    Model* bubbleModel_ = nullptr;
    Model* goalModel_ = nullptr;

    std::vector<Object3d*> objects_;

private:
    Object3d* CreateStageObject(Model* model, const Vector3& position, const Vector3& scale, const Vector3& rotation);
};
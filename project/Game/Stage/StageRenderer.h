#pragma once
#include <vector>
#include "StageMap.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"

class StageRenderer {
public:
    
    ~StageRenderer();

    void Initialize(Object3dCommon* object3dCommon);
    void BuildFromStageMap(const StageMap& stageMap);

	void SetBlockScale(const Vector3& scale) { blockScale_ = scale; }
    const Vector3& GetBlockScale() const { return blockScale_; }
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
    Model* ladderModel_ = nullptr;
    Model* doorModel_ = nullptr;
    Model* pSwichModel_ = nullptr;
    Model* pBlockOnModel_ = nullptr;
    //Model* PBlockOff_ = nullptr;

    std::vector<Object3d*> objects_;
    Vector3 blockScale_{ 1.0f, 1.0f, 1.0f };

private:
    Object3d* CreateStageObject(Model* model, const Vector3& position, const Vector3& scale, const Vector3& rotation);

};
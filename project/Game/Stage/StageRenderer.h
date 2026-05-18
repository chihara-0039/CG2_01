#pragma once
#include <vector>
#include <memory>
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

    void Update(const StageMap& stageMap, const Matrix4x4& lightVP);
    void DrawShadow(const Matrix4x4& lightVP);
    void Draw();

    void UpdateEffect(const StageMap& stageMap);

    // 🌟 配置プレビュー表示機能
    void SetPlacementPreview(
        const StageMap& stageMap,
        const Int3& cursorIndex,
        BlockType type,
        int customId
    );
    void ClearPlacementPreview();

    void Clear();

private:
    Object3dCommon* object3dCommon_ = nullptr;

    std::unique_ptr<Model> groundModel_;
    std::unique_ptr<Model> wallModel_;
    std::unique_ptr<Model> bubbleModel_;
    std::unique_ptr<Model> goalModel_;
    std::unique_ptr<Model> ladderModel_;
    std::unique_ptr<Model> doorModel_;
    std::unique_ptr<Model> pSwichModel_;
    std::unique_ptr<Model> pBlockOnModel_;
    //Model* PBlockOff_ = nullptr;
    std::unique_ptr<Model> crumbleModel_;

    std::vector<std::unique_ptr<Object3d>> objects_;
    std::vector<std::unique_ptr<Object3d>> previewObjects_; // 🌟 半透明プレビュー用オブジェクト
    Vector3 blockScale_{ 1.0f, 1.0f, 1.0f };

private:
    Object3d* CreateStageObject(Model* model, const Vector3& position, const Vector3& scale, const Vector3& rotation);

    // ▼ 追加：動く足場とマップ上のセル位置を紐付ける構造体
        struct MovingFloorInstance {
        Object3d* object = nullptr; // 3Dオブジェクトへのポインタ
        Int3 cellIndex;            // StageMap上での [x, y, z] の位置
    };

    // ▼ 追加：ステージ内のすべての動く足場を管理するリスト
    std::vector<MovingFloorInstance> movingFloorInstances_;

};
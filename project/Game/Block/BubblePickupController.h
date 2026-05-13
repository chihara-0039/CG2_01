#pragma once

#include "StageMap.h"
#include "StageRenderer.h"
#include "BlockInventory.h"
#include "MyMath.h"

// シャボン玉取得処理専用クラス
class BubblePickupController {
public:
    void Initialize(
        StageMap* stageMap,
        StageRenderer* stageRenderer,
        BlockInventory* inventory
    );

    // プレイヤー位置を渡して、近くのシャボン玉を取得する
    void Update(const Vector3& playerPosition);

private:
    // ワールド座標から近いマス座標を作る
    Int3 ToGridIndex(const Vector3& position) const;

    // 指定マスにシャボン玉があれば取得する
    bool TryCollectAt(const Int3& index);

private:
    StageMap* stageMap_ = nullptr;
    StageRenderer* stageRenderer_ = nullptr;
    BlockInventory* inventory_ = nullptr;
};

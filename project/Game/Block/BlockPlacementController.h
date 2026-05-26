#pragma once

#include "StageMap.h"
#include "StageRenderer.h"
#include "BlockInventory.h"

// 入手したブロックをステージ上に置く処理専用クラス
class BlockPlacementController {
public:
    void Initialize(
        StageMap* stageMap,
        StageRenderer* stageRenderer,
        BlockInventory* inventory
    );

    // 指定マスにブロックを置く
    bool TryPlace(const Int3& index, float rotationY = 0.0f);

    // 配置するブロック種類を変えたい時用
    void SetPlaceBlockType(BlockType type) { placeBlockType_ = type; }
    void SetPlaceCustomId(int id) { placeCustomId_ = id; }
    int GetPlaceCustomId() const { return placeCustomId_; }

private:
    bool CanPlaceAt(const Int3& index) const;

private:
    StageMap* stageMap_ = nullptr;
    StageRenderer* stageRenderer_ = nullptr;
    BlockInventory* inventory_ = nullptr;

    // とりあえず入手ブロックは Ground として置く
    BlockType placeBlockType_ = BlockType::Ground;
    int placeCustomId_ = 0; // 0: 通常, 1〜5: カスタム
};
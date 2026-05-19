#pragma once
#include "../Stage/StageMap.h" // BlockType を使用するため

// プレイヤーが所持している「配置可能ブロック」の種類別・カスタム別数を管理するクラス
class BlockInventory {
public:
    // 所持数を初期化
    void Initialize(int initialCount = 0);

    // 所持数を増やす
    void AddBlock(BlockType type, int count = 1, int customId = 0);
    void AddBlock(int count = 1); // 後方互換性用 (デフォルトで Wall を追加)

    // 所持数を消費する
    // 成功したら true、足りなければ false
    bool ConsumeBlock(BlockType type, int count = 1, int customId = 0);
    bool ConsumeBlock(int count = 1); // 後方互換性用 (デフォルトで Wall を消費)

    // 所持数取得
    int GetBlockCount(BlockType type, int customId = 0) const;
    int GetBlockCount() const; // 後方互換性用 (全ブロックの合計数)

    // 所持しているか
    bool HasBlock(BlockType type, int customId = 0) const;
    bool HasBlock() const; // 後方互換性用 (何かしら持っているか)

private:
    int wallCount_ = 0;
    int ladderCount_ = 0;
    int iceCount_ = 0;
    int movingCount_ = 0;
    int crumbleCount_ = 0;
    int customCounts_[5] = { 0, 0, 0, 0, 0 }; // カスタムパーツ (1〜5) の所持数
};
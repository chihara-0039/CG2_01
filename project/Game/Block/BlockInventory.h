#pragma once

// プレイヤーが所持している「配置可能ブロック」の数を管理するクラス
class BlockInventory {
public:
    // 所持数を初期化
    void Initialize(int initialCount = 0);

    // 所持数を増やす
    void AddBlock(int count = 1);

    // 所持数を消費する
    // 成功したら true、足りなければ false
    bool ConsumeBlock(int count = 1);

    // 所持数取得
    int GetBlockCount() const { return blockCount_; }

    // 所持しているか
    bool HasBlock() const { return blockCount_ > 0; }

private:
    int blockCount_ = 0;
};
#pragma once
#include <vector>
#include <cstdint>
#include <string>

// 3次元整数座標
struct Int3 {
    int x;
    int y;
    int z;
};

// ブロック種類
enum class BlockType : uint32_t {
    None = 0,
    Ground,
    Wall,
    Ladder,
    Star,
    BubblePickup,
    Goal,
    PlayerStart,
    Door
};

// 1マス分のデータ
struct MapCell {
    BlockType type = BlockType::None;
    int variant = 0;      // 見た目違い用。今は使わなくてOK
    bool isSolid = false; // 当たり判定用
	float rotationX = 0.0f; // X軸回転（オブジェクトの向きを変えたい）
	float rotationY = 0.0f; // Y軸回転（オブジェクトの向きを変えたい）
    Int3 doorTargetIndex = { 0,0,0 };
};

class StageMap {
public:
    StageMap() = default;
    ~StageMap() = default;

    // サイズ指定で初期化
    void Initialize(int width, int height, int depth);

    // ステージデータをファイルに保存する
    void SaveToFile(const std::string& filename);
    // ファイルからステージデータを読み込む
    void LoadFromFile(const std::string& filename);

    // 全消し
    void Clear();

    // 範囲内か
    bool IsInside(int x, int y, int z) const;
    bool IsInside(const Int3& index) const;

    // 取得
    const MapCell* GetCell(int x, int y, int z) const;
    const MapCell* GetCell(const Int3& index) const;

    MapCell* GetCell(int x, int y, int z);
    MapCell* GetCell(const Int3& index);

    // 設置
    bool SetBlock(int x, int y, int z, BlockType type, int variant = 0);
    bool SetBlock(const Int3& index, BlockType type, int variant = 0);

    // 削除
    bool RemoveBlock(int x, int y, int z);
    bool RemoveBlock(const Int3& index);

    // サイズ取得
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    int GetDepth() const { return depth_; }

private:
    int width_ = 0;
    int height_ = 0;
    int depth_ = 0;

    std::vector<MapCell> cells_;

private:
    int ToIndex(int x, int y, int z) const;
    static MapCell MakeCell(BlockType type, int variant);
};
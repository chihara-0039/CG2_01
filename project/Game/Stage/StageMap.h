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
    Door,
    PSwitch,
    PBlock,
    CrumblingFloor
};

inline const char* BlockTypeToString(BlockType type) {
    switch (type) {
    case BlockType::None:           return "None";
    case BlockType::Ground:         return "Ground";
    case BlockType::Wall:           return "Wall";
    case BlockType::Ladder:         return "Ladder";
    case BlockType::Star:           return "Star";
    case BlockType::BubblePickup:   return "BubblePickup";
    case BlockType::Goal:           return "Goal";
    case BlockType::PlayerStart:    return "PlayerStart";
    case BlockType::Door:           return "Door";
    case BlockType::PSwitch:        return "PSwitch";
    case BlockType::PBlock:         return "PBlock";
    case BlockType::CrumblingFloor: return "CrumblingFloor";
    default:                        return "Unknown";
    }
}

// 1マス分のデータ
struct MapCell {
    BlockType type = BlockType::None;
    int variant = 0;      // 見た目違い用。今は使わなくてOK
    bool isSolid = false; // 当たり判定用
	float rotationX = 0.0f; // X軸回転（オブジェクトの向きを変えたい）
	float rotationY = 0.0f; // Y軸回転（オブジェクトの向きを変えたい）
    Int3 doorTargetIndex = { 0,0,0 };

    // 崩れる足場用のタイマー管理
    float crumbleTimer = 0.0f;
    bool isCrumbling = false; // プレイヤーが乗っているフラグ
    // --- 復活ギミック用に追加 ---
    bool isHidden = false;      // 現在消えているかどうか
    float respawnTimer = 0.0f;  // 復活までのカウント
    // --- カラー演出用 ---
    float colorG = 1.0f; // 緑 (1.0で通常、0.0に近づくと赤くなる)
    float colorB = 1.0f; // 青
    float opacity = 1.0f; // 透明度 (1.0で表示、0.0で非表示)

};

class StageMap {
public:
    StageMap() = default;
    ~StageMap() = default;

    // サイズ指定で初期化
    void Initialize(int width, int height, int depth);

    // 追加
    void Update(float deltaTime, float totalTime);

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

    // スイッチ取得
    void SetPSwitchActive(bool active) {
        if (isPSwitchActive_ != active) {
            isPSwitchActive_ = active;
            needsRebuild_ = true; // ★状態が変わったらフラグを立てる
        }
    }
    bool NeedsRebuild() const { return needsRebuild_; }

    // ★ これを追加：フラグを「再構築の必要なし（false）」に戻す
    void ResetRebuildFlag() {
        needsRebuild_ = false;
    }

    void ClearRebuildFlag() { needsRebuild_ = false; }
    bool IsPSwitchActive() const { return isPSwitchActive_; }

    // ImGui描画用
    void DrawImGui();

private:

    int width_ = 0;
    int height_ = 0;
    int depth_ = 0;

    std::vector<MapCell> cells_;

private:
    int ToIndex(int x, int y, int z) const;
    static MapCell MakeCell(BlockType type, int variant);

    bool isPSwitchActive_ = false; // Pスイッチの状態
    bool needsRebuild_ = false; // ★追加

    
};
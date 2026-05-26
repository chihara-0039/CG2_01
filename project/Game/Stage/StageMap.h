#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "MyMath.h"

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
    CrumblingFloor,
    IceBlock,
    MovingFloor,
    Key,        // 拾える鍵
    KeyBlock,   // 鍵で開くブロック
    Spike,       // トゲ
    EnemyWalker, // 敵（歩行）
    EnemyFlyer,  // 敵（飛行）
    EnemyChaser  // 敵（追尾）
};

struct MovingFloorRef {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct EnemyRef {
    int x = 0;
    int y = 0;
    int z = 0;
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
    case BlockType::IceBlock:       return "IceBlock";
    case BlockType::MovingFloor:    return "MovingFloor";
    case BlockType::Key:            return "Key";
    case BlockType::KeyBlock:       return "KeyBlock";
    case BlockType::Spike:          return "Spike";
    case BlockType::EnemyWalker:    return "EnemyWalker";
    case BlockType::EnemyFlyer:     return "EnemyFlyer";
    case BlockType::EnemyChaser:    return "EnemyChaser";
    default:                        return "Unknown";
    }
}

// シャボン玉の中身のエンコード・デコード用ヘルパー関数
inline int PackBubbleContents(BlockType type, int customId) {
    return (static_cast<int>(type) & 0xFFFF) | ((customId & 0xFFFF) << 16);
}
inline BlockType UnpackBubbleType(int packed) {
    return static_cast<BlockType>(packed & 0xFFFF);
}
inline int UnpackBubbleCustomId(int packed) {
    return (packed >> 16) & 0xFFFF;
}

// 3次元整数座標
struct Int3 {
    int x;
    int y;
    int z;
};



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
    float colorR = 1.0f; // 赤
    float colorG = 1.0f; // 緑 (1.0で通常、0.0に近づくと赤くなる)
    float colorB = 1.0f; // 青
    float opacity = 1.0f; // 透明度 (1.0で表示、0.0で非表示)

    // ▼ 追加：動く足場用（どの方向に何マス動くか）
    Int3 moveOffset{ 0, 0, 0 };
    // 動く足場の計算用データ
    float moveTimer = 0.0f;                  // サイン波計算用のタイマー
    // 現在の滑らかな移動オフセット
    float currentOffsetX = 0.0f;
    float currentOffsetY = 0.0f;
    float currentOffsetZ = 0.0f;

    // 1フレームあたりの移動量（差分：プレイヤーを一緒に引っ張るために使用）
    float deltaOffsetX = 0.0f;
    float deltaOffsetY = 0.0f;
    float deltaOffsetZ = 0.0f;
};

struct CustomBlockCell {
    BlockType type = BlockType::None;
};

// カスタムブロックパーツのプロパティ定義 (3x3x3 の複合ブロックアセンブリ)
struct CustomBlockPart {
    int id = 0;              // 1〜5 がカスタムパーツスロット
    std::string name = "";   // パーツ名
    BlockType baseType = BlockType::Wall; // 互換性用のベース種類
    float colorR = 1.0f;     // カスタムカラー
    float colorG = 1.0f;
    float colorB = 1.0f;

    CustomBlockCell cells[3][3][3]; // [y][z][x] 3Dアセンブリ形状データ

    bool IsEmpty() const {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                for (int x = 0; x < 3; ++x) {
                    if (cells[y][z][x].type != BlockType::None) return false;
                }
            }
        }
        return true;
    }
};

class StageMap {
public:
    StageMap() = default;
    ~StageMap() = default;

    // サイズ指定で初期化
    void Initialize(int width, int height, int depth);

    // 追加
    void Update(float deltaTime, float totalTime, const Vector3& playerPos);

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

    //5/21佐倉変更

    void SetPSwitchActive(int switchId) {
        isPSwitchActive_ = true;
        needsRebuild_ = true;

        for (auto& cell : cells_) {

            // 同じIDのPスイッチも消す
            if (cell.type == BlockType::PSwitch && cell.variant == switchId) {
                cell.isSolid = false;
                cell.isHidden = true;
            }


            if (cell.type == BlockType::PBlock && cell.variant == switchId) {
                cell.isSolid = false;
                cell.isHidden = true;
            }
        }
    }

    void ResetPSwitchStateNoRebuild() {
        isPSwitchActive_ = false;

        for (auto& cell : cells_) {

            if (cell.type == BlockType::PSwitch) {
                cell.isSolid = false;
                cell.isHidden = false;
            }

            if (cell.type == BlockType::PBlock) {
                cell.isSolid = true;
                cell.isHidden = false;
            }
        }
        // ここでは true にしない
        needsRebuild_ = false;
    }



    bool NeedsRebuild() const { return needsRebuild_; }

    // ★ これを追加：フラグを「再構築の必要なし（false）」に戻す
    void ResetRebuildFlag() {
        needsRebuild_ = false;
    }

    void ClearRebuildFlag() { needsRebuild_ = false; }

    //5/18佐倉追加
    void RequestRebuild() { needsRebuild_ = true; }

    bool IsPSwitchActive() const { return isPSwitchActive_; }

    //5/19佐倉
    void ResetPSwitchState();

    // --- カスタムブロックパーツ関連 ---
    const std::vector<CustomBlockPart>& GetCustomParts() const { return customParts_; }
    std::vector<CustomBlockPart>& GetCustomParts() { return customParts_; }
    const CustomBlockPart* GetCustomPart(int id) const {
        if (id >= 1 && id <= (int)customParts_.size()) {
            return &customParts_[id - 1];
        }
        return nullptr;
    }
    CustomBlockPart* GetCustomPart(int id) {
        if (id >= 1 && id <= (int)customParts_.size()) {
            return &customParts_[id - 1];
        }
        return nullptr;
    }

    // ImGui描画用
    void DrawImGui();

    // 動く足場用のワールド座標当たり判定
    const MapCell* GetIntersectingMovingFloor(float pX, float pY, float pZ, float rX, float rY, float rZ) const;

    // ▼ 追加：指定座標から繋がっている鍵ブロックをすべて消去する関数
    void RemoveConnectedKeyBlocks(int x, int y, int z);

	// ★ 追加：動く足場のリストを再構築する関数（ロード後やサイズ変更後に呼ぶ）
    void RebuildMovingFloorList();
    // ★ 追加：敵キャラクターのリストを再構築する関数
    void RebuildEnemyList();
    const std::vector<EnemyRef>& GetEnemies() const { return enemies_; }

    /// <summary>
    /// 指定した座標のドアと同じID（variant）を持つ、相方のドアの座標を検索する
    /// </summary>
    Int3 FindPairedDoor(int srcX, int srcY, int srcZ) const;

private:

    int width_ = 0;
    int height_ = 0;
    int depth_ = 0;

    std::vector<MapCell> cells_;
    std::vector<CustomBlockPart> customParts_; // カスタムブロックパーツ定義リスト (スロット1〜5)
    std::vector<MovingFloorRef> movingFloors_;
    std::vector<EnemyRef> enemies_;


private:
    int ToIndex(int x, int y, int z) const;
    static MapCell MakeCell(BlockType type, int variant);

    bool isPSwitchActive_ = false; // Pスイッチの状態
    bool needsRebuild_ = false; // ★追加
};
#pragma once
#include <memory>
#include <vector>
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "Input.h"
#include "../Block/BlockInventory.h"
#include "StageMap.h"

class BlockInventoryUI {
public:
    enum class State {
        Closed,
        Opening,
        Opened,
        Closing
    };

    struct BlockButton {
        BlockType type;
        std::unique_ptr<Sprite> sprite;
        uint32_t textureHandle;
        Vector2 localPos; // パネルの左上からの相対座標
        Vector2 size;
        bool isAvailable = false;
        int customId = 0; // 0: 通常, 1〜5: カスタム
        std::vector<std::unique_ptr<Sprite>> silhouetteSprites; // 3x3上面図シルエット用スプライト群
        std::vector<std::unique_ptr<Sprite>> countSprites; // 所持数ドットインジケータ用スプライト群
    };

public:
    void Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon, TextureManager* textureManager, BlockInventory* inventory);
    void Update(Input* input, WinApp* winApp, bool isGamePlayMode, const StageMap* stageMap = nullptr);
    void Draw();
    void Finalize();

    // インベントリが開いているか（またはアニメーション中か）
    bool IsActive() const { return state_ != State::Closed; }
    bool IsOpened() const { return state_ == State::Opened; }

    // 現在選択されているブロック種類
    BlockType GetSelectedBlockType() const { return selectedBlockType_; }
    void SetSelectedBlockType(BlockType type) { selectedBlockType_ = type; }

    // 現在選択されているブロックのカスタムID (1〜5, 0は通常)
    int GetSelectedCustomId() const { return selectedCustomId_; }
    void SetSelectedCustomId(int id) { selectedCustomId_ = id; }

    // パネルの位置を取得（マウスのパネル外判定に使用）
    float GetPanelLeftX() const { return currentPos_.x; }

    // インベントリの状態を取得
    State GetState() const { return state_; }

    // インベントリを強制的に閉じる/開くトグル
    void ToggleOpen();

private:
    // マウスクリック判定
    bool CheckClick(const Vector2& pos, const Vector2& size, float mouseX, float mouseY);

private:
    SpriteCommon* spriteCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr;
    BlockInventory* inventory_ = nullptr;
    const StageMap* stageMap_ = nullptr; // 追加：カスタム名取得用ポインタ

    // パネル背景スプライト
    std::unique_ptr<Sprite> panelSprite_;
    uint32_t panelTextureHandle_ = 0;

    // 選択中のフォーカス枠スプライト
    std::unique_ptr<Sprite> focusSprite_;
    uint32_t focusTextureHandle_ = 0;

    // ブロックボタン一覧
    std::vector<BlockButton> buttons_;
    BlockType selectedBlockType_ = BlockType::Ground;
    int selectedCustomId_ = 0; // 0: 通常, 1〜5: カスタム

    // 状態管理
    State state_ = State::Closed;
    Vector2 currentPos_;
    Vector2 closedPos_;
    Vector2 openedPos_;

    float panelWidth_ = 320.0f;
    float panelHeight_ = 720.0f;
    float tabWidth_ = 64.0f; // 取っ手（タブ）の幅
    float arrowWidth_ = 45.0f; // 実際に画面に残す矢印部分の幅

    bool prevMouseLeft_ = false;

    // ダブルクリック検出用 (GetTickCountを使用)
    uint32_t lastClickTick_ = 0;
    BlockType lastClickedType_ = BlockType::None;
    bool useRequested_ = false;

public:
    // ダブルクリックによる即時使用要求の取得と消費
    bool ConsumeUseRequest() {
        bool req = useRequested_;
        useRequested_ = false;
        return req;
    }
};

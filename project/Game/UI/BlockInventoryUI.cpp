#include "BlockInventoryUI.h"
#include <cassert>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void BlockInventoryUI::Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon, TextureManager* textureManager, BlockInventory* inventory) {
    assert(spriteCommon);
    assert(textureManager);
    assert(inventory);

    spriteCommon_ = spriteCommon;
    textureManager_ = textureManager;
    inventory_ = inventory;

    // 1. インベントリ背景画像のロード
    panelTextureHandle_ = textureManager_->LoadTexture("Resources/UI/inventory/inventory.png");
    
    const D3D12_RESOURCE_DESC& desc = textureManager_->GetResourceDesc(panelTextureHandle_);
    float imgW = static_cast<float>(desc.Width);
    float imgH = static_cast<float>(desc.Height);

    float scale = 720.0f / imgH;
    panelWidth_ = imgW * scale;
    panelHeight_ = 720.0f;

    tabWidth_ = panelWidth_ * (149.0f / 420.0f);

    panelSprite_ = std::make_unique<Sprite>();
    panelSprite_->Initialize(spriteCommon_, panelTextureHandle_);
    panelSprite_->SetSize({ panelWidth_, panelHeight_ });

    // 2. フォーカス枠の初期化 (uvCheckerを黄色カラーフィルタして使用)
    focusTextureHandle_ = textureManager_->LoadTexture("Resources/Models/axis/uvChecker.png");
    focusSprite_ = std::make_unique<Sprite>();
    focusSprite_->Initialize(spriteCommon_, focusTextureHandle_);
    focusSprite_->SetColor({ 1.0f, 0.9f, 0.0f, 0.4f });

    // 3. ブロックボタンの設定 (2列の美しいグリッドレイアウト)
    float btnSize = 54.0f;
    Vector2 sizeVec = { btnSize, btnSize };

    auto addBtn = [&](BlockType type, int customId, float lx, float ly, const std::string& texPath) {
        BlockButton btn;
        btn.type = type;
        btn.customId = customId;
        btn.textureHandle = textureManager_->LoadTexture(texPath);
        btn.localPos = { lx, ly };
        btn.size = sizeVec;
        btn.sprite = std::make_unique<Sprite>();
        btn.sprite->Initialize(spriteCommon_, btn.textureHandle);
        btn.sprite->SetSize(sizeVec);
        buttons_.push_back(std::move(btn));
    };

    // 通常ブロック (左列)
    addBtn(BlockType::Ground, 0, 174.0f, 130.0f, "Resources/Models/block/block.png");
    addBtn(BlockType::Wall,   0, 174.0f, 220.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Ladder, 0, 174.0f, 310.0f, "Resources/Models/ladder/ladder.png");

    // カスタムブロックスロット 1〜5 (空き位置に並べる)
    addBtn(BlockType::Wall,   1, 249.0f, 220.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall,   2, 249.0f, 310.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall,   3, 174.0f, 400.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall,   4, 249.0f, 400.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall,   5, 174.0f, 490.0f, "Resources/Models/wall/wall.png");

    // 4. アニメーション座標の初期化
    closedPos_ = { 1280.0f - tabWidth_, 0.0f };
    openedPos_ = { 1280.0f - panelWidth_, 0.0f };
    currentPos_ = closedPos_;
    state_ = State::Closed;

    selectedBlockType_ = BlockType::Ground;
    selectedCustomId_ = 0;

    panelSprite_->SetPosition(currentPos_);
    panelSprite_->Update();
}

void BlockInventoryUI::Update(Input* input, WinApp* winApp, bool isGamePlayMode, const StageMap* stageMap) {
    stageMap_ = stageMap; // 毎フレーム受け取ったポインタを記録
    if (!isGamePlayMode) {
        state_ = State::Closed;
        currentPos_ = closedPos_;
        panelSprite_->SetPosition(currentPos_);
        panelSprite_->Update();
        return;
    }

    const auto& mouse = input->GetMouseState();
    RECT rect;
    GetClientRect(winApp->GetHwnd(), &rect);
    float currentClientW = static_cast<float>(rect.right - rect.left);
    float currentClientH = static_cast<float>(rect.bottom - rect.top);

    float scaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientW;
    float scaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientH;

    float swapMouseX = static_cast<float>(mouse.posX) * scaleX;
    float swapMouseY = static_cast<float>(mouse.posY) * scaleY;

    float offsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
    float mouseX = swapMouseX - offsetX;
    float mouseY = swapMouseY;

    bool clickTrigger = mouse.buttons[0] && !prevMouseLeft_;
    prevMouseLeft_ = mouse.buttons[0];

    // --- 1. アニメーション制御とイージング ---
    if (state_ == State::Opening) {
        currentPos_.x += (openedPos_.x - currentPos_.x) * 0.15f;
        if (std::abs(currentPos_.x - openedPos_.x) < 1.0f) {
            currentPos_ = openedPos_;
            state_ = State::Opened;
        }
    } else if (state_ == State::Closing) {
        currentPos_.x += (closedPos_.x - currentPos_.x) * 0.15f;
        if (std::abs(currentPos_.x - closedPos_.x) < 1.0f) {
            currentPos_ = closedPos_;
            state_ = State::Closed;
        }
    }

    panelSprite_->SetPosition(currentPos_);
    panelSprite_->Update();

    // --- 2. 取っ手クリックによる開閉制御 ---
    if (clickTrigger) {
        bool hoverTab = (mouseX >= currentPos_.x && mouseX <= currentPos_.x + tabWidth_ &&
                         mouseY >= 0.0f && mouseY <= panelHeight_);
        if (hoverTab) {
            ToggleOpen();
            return;
        }
    }

    // --- 3. ブロックボタンの更新とクリック判定 ---
    for (auto& btn : buttons_) {
        // カスタムパーツプロパティの動的同期 (ベース挙動やカラーをリアルタイム適用)
        float colorR = 1.0f, colorG = 1.0f, colorB = 1.0f;
        if (btn.customId >= 1 && btn.customId <= 5 && stageMap) {
            const auto* part = stageMap->GetCustomPart(btn.customId);
            if (part) {
                btn.type = part->baseType;
                colorR = part->colorR;
                colorG = part->colorG;
                colorB = part->colorB;

                // ベース挙動に応じてテクスチャ変更
                if (btn.type == BlockType::Ladder) {
                    btn.textureHandle = textureManager_->LoadTexture("Resources/Models/ladder/ladder.png");
                } else {
                    btn.textureHandle = textureManager_->LoadTexture("Resources/Models/wall/wall.png");
                }
                btn.sprite->Initialize(spriteCommon_, btn.textureHandle);
                btn.sprite->SetSize(btn.size);
            }
        }

        if (btn.type == BlockType::Ground) {
            btn.isAvailable = true;
        } else {
            btn.isAvailable = (inventory_->GetBlockCount(btn.type, btn.customId) > 0);
        }

        // 所持状態とカスタムカラーの乗算
        if (btn.isAvailable) {
            btn.sprite->SetColor({ colorR, colorG, colorB, 1.0f });
        } else {
            // 所持数0なら暗く表示
            btn.sprite->SetColor({ colorR * 0.3f, colorG * 0.3f, colorB * 0.3f, 1.0f });
        }

        Vector2 screenPos = { currentPos_.x + btn.localPos.x, btn.localPos.y };
        btn.sprite->SetPosition(screenPos);
        btn.sprite->Update();

        // 開いているときのみクリック選択を受け付ける
        if (state_ == State::Opened && clickTrigger && btn.isAvailable) {
            if (mouseX >= screenPos.x && mouseX <= screenPos.x + btn.size.x &&
                mouseY >= screenPos.y && mouseY <= screenPos.y + btn.size.y) {
                
                uint32_t currentTick = GetTickCount();
                // 300ミリ秒以内の同一ボタンクリックをダブルクリックと判定
                if (currentTick - lastClickTick_ < 300 && lastClickedType_ == btn.type) {
                    selectedBlockType_ = btn.type;
                    selectedCustomId_ = btn.customId;
                    useRequested_ = true;
                    ToggleOpen();
                } else {
                    selectedBlockType_ = btn.type;
                    selectedCustomId_ = btn.customId;
                }

                lastClickTick_ = currentTick;
                lastClickedType_ = btn.type;
            }
        }
    }

    // フォーカス枠の位置更新
    for (const auto& btn : buttons_) {
        if (btn.type == selectedBlockType_ && btn.customId == selectedCustomId_) {
            Vector2 screenPos = { currentPos_.x + btn.localPos.x - 4.0f, btn.localPos.y - 4.0f };
            focusSprite_->SetPosition(screenPos);
            focusSprite_->SetSize({ btn.size.x + 8.0f, btn.size.y + 8.0f });
            focusSprite_->Update();
            break;
        }
    }
}

void BlockInventoryUI::Draw() {
    spriteCommon_->PreDraw();

    panelSprite_->Draw();

    if (state_ != State::Closed) {
        focusSprite_->Draw();

        for (const auto& btn : buttons_) {
            btn.sprite->Draw();
        }
    }

#ifdef USE_IMGUI
    if (state_ != State::Closed) {
        // パネルに完全に被せる透明な ImGui ウィンドウを作成
        ImGui::SetNextWindowPos(ImVec2(currentPos_.x, currentPos_.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth_, panelHeight_), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        
        ImGui::Begin("InventoryOverlay", nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar | 
            ImGuiWindowFlags_NoBackground | 
            ImGuiWindowFlags_NoSavedSettings | 
            ImGuiWindowFlags_NoInputs);

        for (const auto& btn : buttons_) {
            // Ground (普通のブロック) は所持数無限
            if (btn.type == BlockType::Ground && btn.customId == 0) {
                ImGui::SetCursorPos(ImVec2(btn.localPos.x, btn.localPos.y + btn.size.y + 2.0f));
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.8f), "INF");
                
                ImGui::SetCursorPos(ImVec2(btn.localPos.x, btn.localPos.y - 15.0f));
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 0.9f), "GROUND");
            } else {
                int count = inventory_->GetBlockCount(btn.type, btn.customId);
                
                // 所持数描画
                ImGui::SetCursorPos(ImVec2(btn.localPos.x, btn.localPos.y + btn.size.y + 2.0f));
                if (count > 0) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "x%d", count);
                } else {
                    ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "x0");
                }

                // ラベル描画 (カスタムパーツならカスタム名に完全同期！)
                ImGui::SetCursorPos(ImVec2(btn.localPos.x, btn.localPos.y - 15.0f));
                if (btn.customId >= 1 && btn.customId <= 5) {
                    std::string labelStr = "PART " + std::to_string(btn.customId);
                    if (stageMap_) {
                        const auto* part = stageMap_->GetCustomPart(btn.customId);
                        if (part && !part->name.empty()) {
                            labelStr = part->name;
                        }
                    }
                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 0.9f), labelStr.c_str());

                    // アセンブリであることを象徴する [ASSY] マークを描画
                    ImGui::SetCursorPos(ImVec2(btn.localPos.x + 3.0f, btn.localPos.y + 3.0f));
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.6f), "[ASSY]");
                } else {
                    if (btn.type == BlockType::Wall) {
                        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 0.9f), "WALL");
                    } else if (btn.type == BlockType::Ladder) {
                        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 0.9f), "LADDER");
                    }
                }
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }
#endif
}

void BlockInventoryUI::ToggleOpen() {
    if (state_ == State::Closed || state_ == State::Closing) {
        state_ = State::Opening;
    } else if (state_ == State::Opened || state_ == State::Opening) {
        state_ = State::Closing;
    }
}

void BlockInventoryUI::Finalize() {
    panelSprite_.reset();
    focusSprite_.reset();
    for (auto& btn : buttons_) {
        btn.sprite.reset();
    }
    buttons_.clear();
}

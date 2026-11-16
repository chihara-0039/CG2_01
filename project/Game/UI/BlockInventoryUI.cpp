#include "BlockInventoryUI.h"
#include "InventoryNavigation.h"
#include "DirectXCommon.h"
#include <cassert>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

// 道具袋の表示と選択を準備し、初期値と必要なリソースを設定する。
void BlockInventoryUI::Initialize(DirectXCommon *dxCommon, SpriteCommon *spriteCommon, TextureManager *textureManager,
                                  BlockInventory *inventory) {
    assert(spriteCommon);
    assert(textureManager);
    assert(inventory);

    spriteCommon_ = spriteCommon;
    textureManager_ = textureManager;
    inventory_ = inventory;
    char legacy[2]{};
    legacyClosedUpdates_ = dxCommon && dxCommon->IsPerformanceCaptureActive() &&
                           GetEnvironmentVariableA("CG2_PERF_LEGACY_INVENTORY", legacy, 2) == 1 && legacy[0] == '1';

    // 1. インベントリ背景画像のロード
    panelTextureHandle_ = textureManager_->LoadTexture("Resources/UI/adventure/inventory.png");

    const D3D12_RESOURCE_DESC &desc = textureManager_->GetResourceDesc(panelTextureHandle_);
    float imgW = static_cast<float>(desc.Width);
    float imgH = static_cast<float>(desc.Height);

    float scale = 720.0f / imgH;
    panelWidth_ = imgW * scale;
    panelHeight_ = 720.0f;

    tabWidth_ = panelWidth_ * (149.0f / 420.0f);

    panelSprite_ = std::make_unique<Sprite>();
    panelSprite_->Initialize(spriteCommon_, panelTextureHandle_);
    panelSprite_->SetSize({panelWidth_, panelHeight_});

    // 2. フォーカス枠の初期化 (white.pngを黄色カラーフィルタして使用)
    focusTextureHandle_ = textureManager_->LoadTexture("Resources/UI/inventory/white.png");
    focusSprite_ = std::make_unique<Sprite>();
    focusSprite_->Initialize(spriteCommon_, focusTextureHandle_);
    focusSprite_->SetColor({.14f, .43f, .44f, .22f});

    quickGuideSprite_ = std::make_unique<Sprite>();
    quickGuideSprite_->Initialize(spriteCommon_,
                                  textureManager_->LoadTexture("Resources/UI/adventure/inventory-guide.png"));
    quickGuideSprite_->SetSize({220, 70});

    // 3. ブロックボタンの設定 (2列の美しいグリッドレイアウト)
    float btnSize = 54.0f;
    Vector2 sizeVec = {btnSize, btnSize};

    auto addBtn = [&](BlockType type, int customId, float lx, float ly, const std::string &texPath) {
        BlockButton btn;
        btn.type = type;
        btn.customId = customId;
        btn.textureHandle = textureManager_->LoadTexture(texPath);
        btn.localPos = {lx, ly};
        btn.size = sizeVec;
        btn.sprite = std::make_unique<Sprite>();
        btn.sprite->Initialize(spriteCommon_, btn.textureHandle);
        btn.sprite->SetSize(sizeVec);
        const auto font = textureManager_->LoadTexture("Resources/UI/inventory/labels.png");
        for (int i = 0; i < 28; ++i) {
            auto glyph = std::make_unique<Sprite>();
            glyph->Initialize(spriteCommon_, font);
            glyph->SetSize({8.0f, 14.0f});
            glyph->SetColor({.14f, .24f, .27f, 1});
            btn.textSprites.push_back(std::move(glyph));
        }

        // カスタムパーツは3x3x3の各セルを立体表示する。
        if (customId >= 1 && customId <= 5) {
            for (int i = 0; i < 27; ++i) {
                auto s = std::make_unique<Sprite>();
                s->Initialize(spriteCommon_, textureManager_->LoadTexture("Resources/UI/inventory/part-voxel.png"));
                s->SetSize({14.0f, 14.0f}); // 1セルのサイズ: 10x10
                btn.silhouetteSprites.push_back(std::move(s));
            }
        }

        // 所持数ドット（最大8個）の生成
        {
            uint32_t whiteTex = textureManager_->LoadTexture("Resources/UI/inventory/white.png");
            for (int i = 0; i < 8; ++i) {
                auto s = std::make_unique<Sprite>();
                s->Initialize(spriteCommon_, whiteTex);
                s->SetSize({6.0f, 6.0f}); // ドットのサイズ: 6x6
                btn.countSprites.push_back(std::move(s));
            }
        }

        buttons_.push_back(std::move(btn));
    };

    // 通常ブロック (左列)
    addBtn(BlockType::Ground, 0, 174.0f, 130.0f, "Resources/Models/block/block.png");
    addBtn(BlockType::Wall, 0, 174.0f, 220.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Ladder, 0, 174.0f, 310.0f, "Resources/Models/ladder/ladder.png");

    // 新規特殊ブロック (空き座標を有効活用)
    addBtn(BlockType::IceBlock, 0, 249.0f, 130.0f, "Resources/Models/iceBlock/iceBlock.png");
    addBtn(BlockType::MovingFloor, 0, 249.0f, 490.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::CrumblingFloor, 0, 174.0f, 580.0f, "Resources/Models/CollapsedBlocks/CollapsedBlocks.png");

    // カスタムブロックスロット 1〜5 (空き位置に並べる)
    addBtn(BlockType::Wall, 1, 249.0f, 220.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall, 2, 249.0f, 310.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall, 3, 174.0f, 400.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall, 4, 249.0f, 400.0f, "Resources/Models/wall/wall.png");
    addBtn(BlockType::Wall, 5, 174.0f, 490.0f, "Resources/Models/wall/wall.png");

    // 4. アニメーション座標の初期化
    closedPos_ = {1280.0f - arrowWidth_, 0.0f};
    openedPos_ = {1280.0f - panelWidth_, 0.0f};
    currentPos_ = closedPos_;
    state_ = State::Closed;

    selectedBlockType_ = BlockType::Ground;
    selectedCustomId_ = 0;

    panelSprite_->SetPosition(currentPos_);
    panelSprite_->Update();
}

// 開閉・直接選択・カーソル・決定の入力を処理し、在庫表示へ反映する。
void BlockInventoryUI::Update(Input *input, WinApp *winApp, bool isGamePlayMode, const StageMap *stageMap,
                              bool canQuickPlace) {
    stageMap_ = stageMap; // 毎フレーム受け取ったポインタを記録
    if (!isGamePlayMode) {
        ResetForStage();
        return;
    }

    const auto &mouse = input->GetMouseState();
    RECT rect;
    GetClientRect(winApp->GetHwnd(), &rect);
    float currentClientW = static_cast<float>(rect.right - rect.left);
    float currentClientH = static_cast<float>(rect.bottom - rect.top);

    if (currentClientW <= 0.0f || currentClientH <= 0.0f) {
        return;
    }

#ifdef NDEBUG
    float mouseX = static_cast<float>(mouse.posX) * (static_cast<float>(WinApp::kClientWidth) / currentClientW);
    float mouseY = static_cast<float>(mouse.posY) * (static_cast<float>(WinApp::kClientHeight) / currentClientH);
#else
    float scaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientW;
    float scaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientH;

    float swapMouseX = static_cast<float>(mouse.posX) * scaleX;
    float swapMouseY = static_cast<float>(mouse.posY) * scaleY;

    float offsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
    float mouseX = swapMouseX - offsetX;
    float mouseY = swapMouseY;
#endif

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

    quickGuideSprite_->SetTexture(textureManager_->LoadTexture(input->IsUsingGamePad()
                                                                   ? "Resources/UI/adventure/inventory-guide-pad.png"
                                                                   : "Resources/UI/adventure/inventory-guide.png"));
    quickGuideSprite_->SetPosition({currentPos_.x + 174.0f, 85.0f});
    quickGuideSprite_->Update();

    // --- 2. 取っ手クリックによる開閉制御 ---
    if (clickTrigger) {
        float arrowMinY = 0.0f; // 右上の黒い矢印があるY範囲（Y: 0〜130px）
        float arrowMaxY = 130.0f;
        bool hoverTab = (mouseX >= currentPos_.x && mouseX <= currentPos_.x + arrowWidth_ && mouseY >= arrowMinY &&
                         mouseY <= arrowMaxY);
        if (hoverTab) {
            ToggleOpen();
            return;
        }
    }

    // Stable shortcuts work without opening the panel; no stock means no request.
    const BYTE quickKeys[] = {DIK_0, DIK_1, DIK_2, DIK_3, DIK_4, DIK_5, DIK_6, DIK_7, DIK_8, DIK_9};
    if (canQuickPlace && (state_ == State::Closed || state_ == State::Opened)) {
        for (int digit = 0; digit < 10; ++digit) {
            if (input->TriggerKey(quickKeys[digit]) && SelectShortcut(digit)) {
                return;
            }
        }
        if (state_ == State::Closed && input->TriggerKey(DIK_I) && selectedBlockType_ != BlockType::None &&
            inventory_->HasBlock(selectedBlockType_, selectedCustomId_)) {
            useRequested_ = true;
            return;
        }
    }

    // 閉じている間は、非表示の全アイコン・文字スプライトを再計算しない。
    // 開き始めたフレームには在庫と名前を同期する。
    if (state_ == State::Closed && !legacyClosedUpdates_) {
        return;
    }

    // --- 3. ブロックボタンの更新とクリック判定 ---
    float startX = 174.0f;
    float startY = 180.0f;
    float colWidth = 110.0f;
    float rowHeight = 96.0f;
    int activeIndex = 0;

    for (auto &btn : buttons_) {
        // カスタムパーツプロパティの動的同期 (ベース挙動やカラーをリアルタイム適用)
        float colorR = 1.0f, colorG = 1.0f, colorB = 1.0f;
        if (btn.customId >= 1 && btn.customId <= 5 && stageMap) {
            const auto *part = stageMap->GetCustomPart(btn.customId);
            if (part) {
                btn.type = part->baseType;
                colorR = part->colorR;
                colorG = part->colorG;
                colorB = part->colorB;

                const auto previousTexture = btn.textureHandle;
                // ベース挙動に応じてテクスチャ変更
                if (btn.type == BlockType::Ladder) {
                    btn.textureHandle = textureManager_->LoadTexture("Resources/Models/ladder/ladder.png");
                } else {
                    btn.textureHandle = textureManager_->LoadTexture("Resources/Models/wall/wall.png");
                }
                if (legacyClosedUpdates_) {
                    btn.sprite->Initialize(spriteCommon_, btn.textureHandle);
                    btn.sprite->SetSize(btn.size);
                } else if (previousTexture != btn.textureHandle) {
                    btn.sprite->SetTexture(btn.textureHandle);
                }
            }
        } else {
            // 通常の特殊ブロックタイプ用のカラー補正
            if (btn.type == BlockType::IceBlock) {
                colorR = 0.5f;
                colorG = 0.85f;
                colorB = 1.0f; // 美しいアイスブルー
            } else if (btn.type == BlockType::MovingFloor) {
                colorR = 0.9f;
                colorG = 0.65f;
                colorB = 0.4f; // インダストリアルオレンジ
            }
        }

        // 所持数が0個より多いもののみ有効（Groundブロックも含めて所持数依存）
        btn.isAvailable = (inventory_->GetBlockCount(btn.type, btn.customId) > 0);

        // 所持状態とカスタムカラーの乗算
        if (btn.isAvailable) {
            btn.sprite->SetColor({colorR, colorG, colorB, 1.0f});
        } else {
            // 非表示になりますが、念のためカラー設定
            btn.sprite->SetColor({colorR * 0.3f, colorG * 0.3f, colorB * 0.3f, 1.0f});
        }

        // 有効なブロックのみ動的レイアウトを適用
        if (btn.isAvailable) {
            int col = activeIndex % 2;
            int row = activeIndex / 2;
            btn.localPos = {startX + col * colWidth, startY + row * rowHeight};
            activeIndex++;
        }

        Vector2 screenPos = {currentPos_.x + btn.localPos.x, btn.localPos.y};
        btn.sprite->SetPosition(screenPos);
        btn.sprite->Update();
        btn.textLength = 0;
        if (btn.isAvailable) {
            const int shortcut = ShortcutFor(btn.type, btn.customId);
            std::string name = BlockTypeToString(btn.type);
            if (btn.customId == 0) {
                switch (btn.type) {
                case BlockType::Wall:
                    name = "BLOCK";
                    break;
                case BlockType::Ladder:
                    name = "LADDER";
                    break;
                case BlockType::IceBlock:
                    name = "ICE";
                    break;
                case BlockType::MovingFloor:
                    name = "MOVING";
                    break;
                case BlockType::CrumblingFloor:
                    name = "CRUMBLE";
                    break;
                default:
                    break;
                }
            }
            if (btn.customId > 0 && stageMap) {
                const auto *part = stageMap->GetCustomPart(btn.customId);
                if (part && !part->name.empty()) {
                    name = part->name;
                }
            }
            name = std::to_string(shortcut) + " " + name.substr(0, 8);
            const std::string count = "x" + std::to_string(inventory_->GetBlockCount(btn.type, btn.customId));
            auto textLine = [&](const std::string &text, float y) {
                for (size_t i = 0; i < text.size() && btn.textLength < btn.textSprites.size(); ++i) {
                    const unsigned char code = static_cast<unsigned char>(text[i]);
                    const int glyph = code >= 32 && code <= 126 ? code - 32 : '?' - 32;
                    auto &sprite = btn.textSprites[btn.textLength++];
                    sprite->SetTextureRect(
                        {static_cast<float>((glyph % 16) * 16), static_cast<float>((glyph / 16) * 24)}, {16, 24});
                    sprite->SetPosition({screenPos.x + static_cast<float>(i) * 8.0f, y});
                    sprite->Update();
                }
            };
            textLine(name, screenPos.y - 17.0f);
            textLine(count, screenPos.y + btn.size.y + 1.0f);
        }

        // Show every height layer as an isometric voxel, instead of flattening the part into a top view.
        if (btn.customId >= 1 && btn.customId <= 5 && btn.isAvailable && stageMap) {
            const auto *part = stageMap->GetCustomPart(btn.customId);
            if (part) {
                for (int y = 0; y < 3; ++y) {
                    for (int z = 0; z < 3; ++z) {
                        for (int x = 0; x < 3; ++x) {
                            auto &voxel = btn.silhouetteSprites[y * 9 + z * 3 + x];
                            const auto type = part->cells[y][z][x].type;
                            voxel->SetPosition({screenPos.x + 20.0f + (x - z) * 7.0f,
                                                screenPos.y + 23.0f + (x + z) * 3.5f - y * 7.0f});
                            Vector4 color{part->colorR, part->colorG, part->colorB,
                                          type == BlockType::None ? 0.0f : 1.0f};
                            if (type == BlockType::Ladder) {
                                color = {.35f, 1.0f, .85f, 1.0f};
                            }
                            voxel->SetColor(color);
                            voxel->Update();
                        }
                    }
                }
            }
        }

        // 所持数ドットスプライトの座標とカラーを更新
        if (btn.isAvailable) {
            int count = inventory_->GetBlockCount(btn.type, btn.customId);
            int drawCount = (count > 8) ? 8 : count;

            float spacing = 6.0f; // ドットの間隔
            // ボタンの右下付近から左へ並べる
            float startX = btn.localPos.x + btn.size.x - 7.0f;
            float dotY = btn.localPos.y + btn.size.y - 7.0f;

            for (int i = 0; i < 8; ++i) {
                if (i < drawCount) {
                    float dotX = startX - i * spacing;
                    btn.countSprites[i]->SetPosition({currentPos_.x + dotX, dotY});
                    btn.countSprites[i]->SetColor({0.2f, 1.0f, 0.2f, 1.0f}); // 明るい黄緑色
                    btn.countSprites[i]->Update();
                }
            }
        }

        // 開いているときのみクリック選択を受け付ける
        if (state_ == State::Opened && clickTrigger && btn.isAvailable) {
            if (mouseX >= screenPos.x && mouseX <= screenPos.x + btn.size.x && mouseY >= screenPos.y &&
                mouseY <= screenPos.y + btn.size.y) {

                selectedBlockType_ = btn.type;
                selectedCustomId_ = btn.customId;
                ConfirmSelection(); // 1クリックで持ち出す。種類が同じ別パーツも区別する。
            }
        }
    }

    // 現在選択されている項目が非表示になった場合のバリデーション
    bool selectedAvailable = false;
    for (const auto &btn : buttons_) {
        if (btn.type == selectedBlockType_ && btn.customId == selectedCustomId_ && btn.isAvailable) {
            selectedAvailable = true;
            break;
        }
    }
    if (!selectedAvailable) {
        selectedBlockType_ = BlockType::None;
        selectedCustomId_ = 0;
        for (const auto &btn : buttons_) {
            if (btn.isAvailable) {
                selectedBlockType_ = btn.type;
                selectedCustomId_ = btn.customId;
                break;
            }
        }
    }

    // 十字キー／矢印キーで選択し、A／Enterで決定、B／Escで閉じる。
    // 開閉アニメーション中の誤入力を避けるため、完全に開いた状態だけ受け付ける。
    if (state_ == State::Opened) {
        int selectedIndex = -1;
        for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
            const auto &btn = buttons_[i];
            if (btn.type == selectedBlockType_ && btn.customId == selectedCustomId_ && btn.isAvailable) {
                selectedIndex = i;
                break;
            }
        }

        std::vector<int> available;
        int selectedPosition = -1;
        for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
            if (!buttons_[i].isAvailable) {
                continue;
            }
            if (i == selectedIndex) {
                selectedPosition = static_cast<int>(available.size());
            }
            available.push_back(i);
        }
        const int dx = (input->TriggerKey(DIK_RIGHT) || input->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_RIGHT)) -
                       (input->TriggerKey(DIK_LEFT) || input->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_LEFT));
        const int dy = (input->TriggerKey(DIK_DOWN) || input->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_DOWN)) -
                       (input->TriggerKey(DIK_UP) || input->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_UP));
        const int position = InventoryNavigation::Move(selectedPosition, static_cast<int>(available.size()), dx, dy);
        if (position >= 0) {
            const auto &btn = buttons_[available[position]];
            selectedBlockType_ = btn.type;
            selectedCustomId_ = btn.customId;
        }

        if (state_ == State::Opened &&
            (input->TriggerControllerButton(XINPUT_GAMEPAD_A) || input->TriggerKey(DIK_RETURN)) &&
            selectedBlockType_ != BlockType::None) {
            ConfirmSelection();
        } else if (state_ == State::Opened && (input->TriggerControllerButton(XINPUT_GAMEPAD_B) ||
                                               input->TriggerControllerButton(XINPUT_GAMEPAD_LEFT_SHOULDER) ||
                                               input->TriggerKey(DIK_B) || input->TriggerKey(DIK_ESCAPE))) {
            ToggleOpen();
        }
    }

    // フォーカス枠の位置更新
    for (const auto &btn : buttons_) {
        if (btn.type == selectedBlockType_ && btn.customId == selectedCustomId_) {
            Vector2 screenPos = {currentPos_.x + btn.localPos.x - 4.0f, btn.localPos.y - 4.0f};
            focusSprite_->SetPosition(screenPos);
            focusSprite_->SetSize({btn.size.x + 8.0f, btn.size.y + 8.0f});
            focusSprite_->Update();
            break;
        }
    }
}

// ボタンの種類とパーツ番号から、直接選択用のキーを決める。
int BlockInventoryUI::ShortcutFor(BlockType type, int customId) {
    if (customId >= 1 && customId <= 5) {
        return customId;
    }
    switch (type) {
    case BlockType::Wall:
        return 6;
    case BlockType::Ladder:
        return 7;
    case BlockType::IceBlock:
        return 8;
    case BlockType::MovingFloor:
        return 9;
    default:
        return 0;
    }
}

// 数字キーに対応する在庫を探し、選択と持ち出しの状態へ反映する。
bool BlockInventoryUI::SelectShortcut(int digit) {
    for (auto &button : buttons_) {
        if (ShortcutFor(button.type, button.customId) != digit || !inventory_->HasBlock(button.type, button.customId)) {
            continue;
        }
        if (button.customId > 0 && stageMap_) {
            button.type = stageMap_->GetCustomPart(button.customId)->baseType;
        }
        selectedBlockType_ = button.type;
        selectedCustomId_ = button.customId;
        state_ = State::Closed;
        currentPos_ = closedPos_;
        panelSprite_->SetPosition(currentPos_);
        panelSprite_->Update();
        useRequested_ = true;
        return true;
    }
    return false;
}

// 選択パーツと在庫を確認し、配置モードへ渡す内容を確定する。
bool BlockInventoryUI::ConfirmSelection() {
    if (state_ != State::Opened || selectedBlockType_ == BlockType::None ||
        (selectedBlockType_ != BlockType::Ground && !inventory_->HasBlock(selectedBlockType_, selectedCustomId_))) {
        return false;
    }
    useRequested_ = true;
    ToggleOpen();
    return true;
}

// 道具袋の表示と選択の表示対象を選び、描画コマンドを記録する。
void BlockInventoryUI::Draw() {
    if (state_ == State::Closed) {
        return;
    }
    spriteCommon_->PreDraw();

    panelSprite_->Draw();

    if (state_ != State::Closed) {
        quickGuideSprite_->Draw();
        if (selectedBlockType_ != BlockType::None) {
            focusSprite_->Draw();
        }

        for (const auto &btn : buttons_) {
            if (btn.isAvailable) {
                // カスタムパーツは単なる壁のテクスチャ画像の代わりに
                // 高さを含む最大27セルの立体アイコンを描画する。
                if (btn.customId >= 1 && btn.customId <= 5) {
                    for (const auto &s : btn.silhouetteSprites) {
                        s->Draw();
                    }
                } else {
                    btn.sprite->Draw();
                }
                for (size_t i = 0; i < btn.textLength; ++i) {
                    btn.textSprites[i]->Draw();
                }
            }
        }
    }
}

// 新しいステージに合わせ、道具袋の選択・表示・要求状態を初期化する。
void BlockInventoryUI::ResetForStage() {
    state_ = State::Closed;
    currentPos_ = closedPos_;
    selectedBlockType_ = BlockType::None;
    selectedCustomId_ = 0;
    useRequested_ = false;
    prevMouseLeft_ = false;
    stageMap_ = nullptr;
    if (panelSprite_) {
        panelSprite_->SetPosition(currentPos_);
        panelSprite_->Update();
    }
    for (auto &button : buttons_) {
        button.isAvailable = false;
        button.textLength = 0;
    }
}

// 道具袋の開閉を切り替え、表示開始時の選択状態を整える。
void BlockInventoryUI::ToggleOpen() {
    if (state_ == State::Closed || state_ == State::Closing) {
        state_ = State::Opening;
    } else if (state_ == State::Opened || state_ == State::Opening) {
        state_ = State::Closing;
    }
}

// 道具袋の表示と選択が保持するオブジェクトとリソースを解放する。
void BlockInventoryUI::Finalize() {
    panelSprite_.reset();
    focusSprite_.reset();
    for (auto &btn : buttons_) {
        btn.sprite.reset();
        btn.silhouetteSprites.clear();
        btn.countSprites.clear();
    }
    buttons_.clear();
}

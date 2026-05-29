#include <filesystem>

#include "MyGame.h"
#include "Goal.h"
#include "ModelManager.h"
#include <memory>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

// --- MyGame繧ｯ繝ｩ繧ｹ縺ｮ螳溯｣・---
void MyGame::Initialize() {
    // 蝓ｺ逶､邉ｻ縺ｮ逕滓・・・ew 縺ｧ縺ｯ縺ｪ縺・std::make_unique 繧剃ｽｿ逕ｨ・・
    winApp = std::make_unique<WinApp>();
    winApp->Initialize();

	// DirectXCommon 縺ｮ逕滓・縺ｨ蛻晄悄蛹悶・nitialize 縺ｫ縺ｯ winApp 縺ｮ逕溘・繧､繝ｳ繧ｿ繧呈ｸ｡縺兔r
    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get()); // get() 縺ｧ荳ｭ霄ｫ縺ｮ逕溘・繧､繝ｳ繧ｿ繧定ｲｸ縺怜・縺・

	// Input 繧ｯ繝ｩ繧ｹ繧ょ酔讒倥↓ std::make_unique 縺ｧ逕滓・縺励！nitialize 縺ｫ縺ｯ winApp 縺ｮ逕溘・繧､繝ｳ繧ｿ繧呈ｸ｡縺兔r
    input = std::make_unique<Input>();
    input->Initialize(winApp.get()); // get() 繧剃ｽｿ逕ｨ

	// TextureManager 縺ｯ SpriteCommon 縺ｨ Object3dCommon 縺ｮ荳｡譁ｹ縺ｧ蠢・ｦ√↓縺ｪ繧九・縺ｧ縲∝・縺ｫ逕滓・縺励※縺翫￥
    textureManager = std::make_unique<TextureManager>();
    textureManager->Initialize(dxCommon.get());

	// SpriteCommon 縺ｨ Object3dCommon 縺ｯ繝・け繧ｹ繝√Ε邂｡逅・ｂ蠢・ｦ√↓縺ｪ繧九・縺ｧ縲ゝextureManager縺ｮ繧ｻ繝・ヨ繧貞ｿ倥ｌ縺壹↓
    spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->SetTextureManager(textureManager.get());
    spriteCommon->Initialize(dxCommon.get());

	// Object3dCommon 縺ｯ繝・け
    bool inventoryOpenForUpdate = blockInventoryUI_ && blockInventoryUI_->IsActive();

    if (stageSelect_)
    {
        std::string currentStage = stageSelect_->GetSelectedFileName();
        // 騾壼ｸｸ繝励Ξ繧､荳ｭ縺ｮ縺ｿ謫堺ｽ懊メ繝･繝ｼ繝医Μ繧｢繝ｫ繧旦pdate
        if (currentStage == "tutorial.txt" && tutorialSprite_ && !inventoryOpenForUpdate)
        {
            tutorialSprite_->Update();
        }
    }

    // 驟咲ｽｮ繝√Η繝ｼ繝医Μ繧｢繝ｫ縺ｯ繧､繝ｳ繝吶Φ繝医Μ縺碁幕縺・※縺・ｋ譎ゅ↓Update
    if ((currentMode_ == AppMode::GamePlay_BlockPlace || inventoryOpenForUpdate) && placementTutorialSprite_)
    {
        placementTutorialSprite_->Update();
    }

    float deltaTime = 1.0f / 60.0f;
    totalTime_ += deltaTime;
    stageMap_.Update(deltaTime, player_ ? player_->GetPosition() : Vector3{0.0f, 0.0f, 0.0f});

    stageRenderer_->UpdateEffect(stageMap_);

    // --- 繝励Ξ繧､繝､繝ｼ譖ｴ譁ｰ ---
    if (player_) {
        float cameraRotY = useFirstPersonCamera_ ? fpsCameraYaw_ : gameplayCameraController_.GetAngle();
        player_->Update(input.get(), stageMap_, cameraRotY, lightCamera_->GetViewProjectionMatrix(), dxCommon.get());
    }

    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->BuildFromStageMap(stageMap_);
        stageMap_.ClearRebuildFlag();
    }

    stageRespawnController_.Update(
        stageMap_,
        backupMap_,
        stageRenderer_.get(),
        player_.get(),
        &blockInventory_,
        &bubblePickupController_,
        &blockPlacementController_,
        &stageEditorController_
    );

   
    /*==================================================
    笆ｼ 繝励Ξ繧､繝､繝ｼ蠎ｧ讓吝叙蠕予r
    ==================================================*/
    Vector3 pPos{};
    if (player_) {
        pPos = player_->GetPosition();
    }

    /*==================================================
    笆ｼ 繧ｷ繝｣繝懊Φ邇牙叙蠕予r
    ==================================================*/
    if (player_) {
        bubblePickupController_.Update(pPos);
    }

    int gx = static_cast<int>(std::floor(pPos.x + 0.5f));
    int gy = static_cast<int>(std::floor(pPos.y));
    int gz = static_cast<int>(std::floor(pPos.z + 0.5f));

    /*==================================================
        笆ｼ 繧ｴ繝ｼ繝ｫ蛻､螳夲ｼ遺・霑ｽ蜉驛ｨ蛻・ｼ噂r
    ==================================================*/
    Vector3 radius = { 0.4f, 0.9f, 0.4f }; // 繝励Ξ繧､繝､繝ｼ繧ｵ繧､繧ｺ縺ｫ蜷医ｏ縺帙ｋ

    if (Goal::Check(pPos, radius, stageMap_)) {
        isGoalReached_ = true;
    }

    /*==================================================
        笆ｼ 繧､繝ｳ繝吶Φ繝医Μ繧帝幕縺十r
        B繧ｭ繝ｼ縺ｧ繧､繝ｳ繝吶Φ繝医Μ繧帝幕縺上る・鄂ｮ繝｢繝ｼ繝峨∈縺ｮ遘ｻ陦後・繝繝悶Ν繧ｯ繝ｪ繝・け譎ゅ・縺ｿ縲・r
    ==================================================*/
    if (input->TriggerKey(DIK_B) && blockInventory_.HasBlock()) {
        if (blockInventoryUI_) {
            blockInventoryUI_->ToggleOpen(); // B繧ｭ繝ｼ
            } else if (debugFlags_.showSkybox && skydomeObject_) {
                skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                skydomeObject_->Draw();
            }
            if (currentMode_ == AppMode::StageEditor ||
                currentMode_ == AppMode::GamePlay ||
                currentMode_ == AppMode::GamePlay_BlockPlace) {

                if (stageRenderer_) {
                    stageRenderer_->Draw(); 

                    // 蜊企乗・繝悶Ο繝・け繧呈怙蠕後↓謠冗判
                    stageRenderer_->DrawTransparent();

                    // 騾壼ｸｸ謠冗判縺ｫ謌ｻ縺兔r
                    object3dCommon->PreDraw();
                    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
                }
                if (currentMode_ == AppMode::GamePlay) {
                    if (player_ && !useFirstPersonCamera_) {
                        player_->Draw();
                        if (IsPlayerHiddenByWall()) {
                            object3dCommon->PreDrawPlayerHighlight();
                            player_->DrawHighlight();
                            object3dCommon->PreDraw();
                            commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
                        }
                    }
                    if (gameplayUIManager_) {
                        gameplayUIManager_->Draw3DPrompts(currentMode_ == AppMode::GamePlay, player_.get(), object3dCommon.get(), commandList, shadowMap_->GetSrvHandle());
                    }
                }
                if ((currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace) && mapCursor_) {
                    mapCursor_->Draw();
                }
            }
            if (currentMode_ == AppMode::DebugView) {
                if (terrainObject_ && debugFlags_.showTerrain) {
                    terrainObject_->Draw();
                }
                for (auto& obj : objectList) {
                    if (obj) obj->Draw();
                }
                if (player_) { player_->Draw(); }
            }
        }
    }

    // 繝代・繝・ぅ繧ｯ繝ｫ縺ｮ謠冗判
    if (debugFlags_.showParticles) {
        ID3D12DescriptorHeap* particleHeaps[] = { textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, particleHeaps);
        particleManager->Draw();
    }

    // 繧ｹ繝励Λ繧､繝医・謠冗判
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        spriteCommon->PreDraw();
        if (sprite) sprite->Draw();
    }

    // UI繧ｹ繝励Λ繧､繝医・謠冗判
    if (gameplayUIManager_) {
        gameplayUIManager_->DrawSprites(
            currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace,
            gameplayCameraController_.IsFollowPlayerMode()
        );
    }


                    std::string relPath = entry.path().string();
                    std::replace(relPath.begin(), relPath.end(), '\\', '/');
                    modelPaths_.push_back(relPath);
                    modelNames_.push_back(entry.path().filename().string());
                }
            }
        }
    }
}

void MyGame::ChangePreviewModel(int index) {
    if (index < 0 || index >= static_cast<int>(modelPaths_.size())) return;
    selectedModelIndex_ = index;

    if (index == 0) {
        skinnedObject_->Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    } else if (index == 1) {
        // 蠕捺擂縺ｮOBJ縺ｯ繧ｹ繧ｭ繝九Φ繧ｰ髱槫ｯｾ蠢懊・縺溘ａ縲√ム繝溘・縺ｧ繝・ヵ繧ｩ繝ｫ繝井ｺｺ蝙九ｒ陦ｨ遉ｺ
        skinnedObject_->Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    } else {
        skinnedObject_->InitializeFromGltf(object3dCommon.get(), dxCommon.get(), modelPaths_[index], textureManager.get());
    }
}

void MyGame::ApplyModelToPlayer() {
    activeGameModelIndex_ = selectedModelIndex_;
    if (!player_) return;

    if (activeGameModelIndex_ == 0) {
        player_->InitializeWithDefaultSkinned(object3dCommon.get(), dxCommon.get(), textureManager.get());
    } else if (activeGameModelIndex_ == 1) {
        player_->Initialize(object3dCommon.get(), models[2].get());
    } else {
        player_->InitializeWithSkinnedGltf(object3dCommon.get(), dxCommon.get(), modelPaths_[activeGameModelIndex_], textureManager.get());
    }
    
    player_->SetPosition({ 0.0f, 1.5f, 0.0f });
}

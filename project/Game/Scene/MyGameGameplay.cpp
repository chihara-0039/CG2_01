#include "MyGameGameplay.h"

#include <algorithm>

#include "Goal.h"
#include "MyGame.h"
#include "externals/imgui/imgui.h"

void MyGameGameplay::UpdateGamePlay(MyGame& game) {
    const Matrix4x4& lightVP = game.lightCamera_->GetViewProjectionMatrix();

    if (game.input->TriggerKey(DIK_C)) {
        game.useFirstPersonCamera_ = !game.useFirstPersonCamera_;
        if (game.useFirstPersonCamera_ && game.player_) {
            game.fpsCameraYaw_ = game.player_->GetRotation().y;
            game.fpsCameraPitch_ = 0.0f;
        }
    }

    if (!game.useFirstPersonCamera_) {
        if (game.input->TriggerKey(DIK_V)) {
            bool cur = game.gameplayCameraController_.IsFollowPlayerMode();
            game.gameplayCameraController_.SetFollowPlayerMode(!cur);
            if (!cur && game.player_) {
                Vector3 pp = game.player_->GetPosition();
                pp.y += 0.8f;
                game.gameplayCameraController_.SetCameraPivot(pp);
            } else if (cur && game.stageSelect_) {
                game.gameplayCameraController_.ResetCamera(
                    game.camera.get(),
                    game.player_.get(),
                    game.stageMap_,
                    game.stageSelect_->GetSelectedIndex());
            }
        }
        game.camera->SetFov(game.gameplayCameraController_.GetFov());
        game.gameplayCameraController_.Update(
            game.input.get(), game.camera.get(), game.winApp.get(), game.player_.get());

    } else {
        game.camera->SetFov(0.9f);

        bool isGuiCaptured = false;
#ifndef NDEBUG
        isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
#endif
        const auto& mouse = game.input->GetMouseState();
        if (mouse.buttons[0] && !isGuiCaptured) {
            RECT rect;
            GetClientRect(game.winApp->GetHwnd(), &rect);
            float cw = static_cast<float>(rect.right - rect.left);
            float ch = static_cast<float>(rect.bottom - rect.top);
            if (cw > 0.0f && ch > 0.0f) {
                float sx = static_cast<float>(WinApp::kWindowWidth) / cw;
                float sy = static_cast<float>(WinApp::kWindowHeight) / ch;
                float mx = static_cast<float>(mouse.posX) * sx;
                float my = static_cast<float>(mouse.posY) * sy;

                const float er = 0.15f;
                const float spd = 0.03f;
                float le = WinApp::kWindowWidth * er;
                float re = WinApp::kWindowWidth * (1.0f - er);
                float te = WinApp::kWindowHeight * er;
                float be = WinApp::kWindowHeight * (1.0f - er);

                if (mx < le) {
                    game.fpsCameraYaw_ += spd;
                } else if (mx > re) {
                    game.fpsCameraYaw_ -= spd;
                }
                if (my < te) {
                    game.fpsCameraPitch_ += spd;
                } else if (my > be) {
                    game.fpsCameraPitch_ -= spd;
                }
            }
        }

        const float ks = 0.03f;
        if (game.input->PushKey(DIK_LEFT)) { game.fpsCameraYaw_ += ks; }
        if (game.input->PushKey(DIK_RIGHT)) { game.fpsCameraYaw_ -= ks; }
        if (game.input->PushKey(DIK_UP)) { game.fpsCameraPitch_ += ks; }
        if (game.input->PushKey(DIK_DOWN)) { game.fpsCameraPitch_ -= ks; }
        game.fpsCameraPitch_ = std::clamp(game.fpsCameraPitch_, -1.4f, 1.4f);

        if (game.player_) {
            Vector3 pp = game.player_->GetPosition();
            game.camera->SetPosition({ pp.x, pp.y + 1.2f, pp.z });
            game.camera->SetRotation({ game.fpsCameraPitch_, game.fpsCameraYaw_, 0.0f });
        }
        game.camera->Update();
    }

    if (game.gameplayUIManager_) {
        game.gameplayUIManager_->UpdateCameraGuide(
            game.currentMode_ == MyGame::AppMode::GamePlay, game.input.get(), game.winApp.get());
    }

    bool invOpen = game.blockInventoryUI_ && game.blockInventoryUI_->IsActive();
    if (game.stageSelect_ && game.stageSelect_->GetSelectedFileName() == "tutorial.txt"
        && game.tutorialSprite_ && !invOpen) {
        game.tutorialSprite_->Update();
    }
    if ((game.currentMode_ == MyGame::AppMode::GamePlay_BlockPlace || invOpen)
        && game.placementTutorialSprite_) {
        game.placementTutorialSprite_->Update();
    }

    float dt = 1.0f / 60.0f;
    game.totalTime_ += dt;
    game.stageMap_.Update(dt, game.player_ ? game.player_->GetPosition() : Vector3{ 0, 0, 0 });
    game.stageRenderer_->UpdateEffect(game.stageMap_);

    if (game.player_) {
        float camRot =
            game.useFirstPersonCamera_ ? game.fpsCameraYaw_ : game.gameplayCameraController_.GetAngle();
        game.player_->Update(game.input.get(), game.stageMap_, camRot, lightVP, game.dxCommon.get());
    }

    if (game.stageMap_.NeedsRebuild()) {
        game.stageRenderer_->BuildFromStageMap(game.stageMap_);
        game.stageMap_.ClearRebuildFlag();
    }

    game.stageRespawnController_.Update(
        game.stageMap_,
        game.backupMap_,
        game.stageRenderer_.get(),
        game.player_.get(),
        &game.blockInventory_,
        &game.bubblePickupController_,
        &game.blockPlacementController_,
        &game.stageEditorController_);

    Vector3 pPos = game.player_ ? game.player_->GetPosition() : Vector3{};
    if (game.player_) {
        game.bubblePickupController_.Update(pPos);
    }

    if (Goal::Check(pPos, { 0.4f, 0.9f, 0.4f }, game.stageMap_)) {
        game.isGoalReached_ = true;
    }

    if (game.input->TriggerKey(DIK_B) && game.blockInventory_.HasBlock()) {
        if (game.blockInventoryUI_) {
            game.blockInventoryUI_->ToggleOpen();
        }
    }

    if (game.isGoalReached_) {
        // game.currentMode_ = MyGame::AppMode::GameClear;
    }
}

void MyGameGameplay::UpdateBlockPlace(MyGame& game) {
    const Int3& cursor = game.mapCursor_->GetIndex();

    if (game.input->TriggerKey(DIK_R)) {
        game.placeRotationY_ += 1.5707963f;
        if (game.placeRotationY_ >= 6.0f) {
            game.placeRotationY_ = 0.0f;
        }
    }

    game.stageEditorController_.HandleCursorInput(
        game.input.get(),
        game.stageMap_,
        game.mapCursor_.get(),
        game.lightCamera_.get(),
        game.camera.get());

    BlockType selectedType = BlockType::Ground;
    int selectedCustomId = 0;
    if (game.blockInventoryUI_) {
        selectedType = game.blockInventoryUI_->GetSelectedBlockType();
        selectedCustomId = game.blockInventoryUI_->GetSelectedCustomId();
        game.blockPlacementController_.SetPlaceBlockType(selectedType);
        game.blockPlacementController_.SetPlaceCustomId(selectedCustomId);
    }

    if (game.stageRenderer_) {
        game.stageRenderer_->SetPlacementPreview(
            game.stageMap_, cursor, selectedType, selectedCustomId, game.placeRotationY_);
    }

    static bool prevMouse0 = false;
    bool mouseJustPressed = game.input->GetMouseState().buttons[0] && !prevMouse0;
    prevMouse0 = game.input->GetMouseState().buttons[0];
    bool mouseTrigger = false;
    if (mouseJustPressed && (!game.blockInventoryUI_ || !game.blockInventoryUI_->IsActive())) {
        mouseTrigger = true;
    }

    if (game.input->TriggerKey(DIK_RETURN) || mouseTrigger) {
        if (game.blockPlacementController_.TryPlace(cursor, game.placeRotationY_)) {
            bool hasRest = (selectedType == BlockType::Ground)
                || game.blockInventory_.HasBlock(selectedType, selectedCustomId);
            if (!hasRest) {
                game.currentMode_ = MyGame::AppMode::GamePlay;
                game.placeRotationY_ = 0.0f;
                if (game.stageRenderer_) {
                    game.stageRenderer_->ClearPlacementPreview();
                }
            }
        }
    }

    if (game.input->TriggerKey(DIK_ESCAPE) || game.input->TriggerKey(DIK_B)) {
        game.currentMode_ = MyGame::AppMode::GamePlay;
        game.placeRotationY_ = 0.0f;
        if (game.stageRenderer_) {
            game.stageRenderer_->ClearPlacementPreview();
        }
    }

    game.stageEditorController_.HandleCameraInput(game.input.get(), game.camera.get());
}

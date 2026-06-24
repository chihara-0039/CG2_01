#include "MyGameRenderer.h"

#include <cmath>

#include "MyGame.h"

void MyGameRenderer::Draw(MyGame& game) {
    auto commandList = game.dxCommon->GetCommandList();

    if (game.skydomeObject_) {
        if (game.postProcess_.GetSkyboxLinkMode() == 1) {
            game.skydomeObject_->SetColor(game.postProcess_.GetClearColor());
        } else {
            game.skydomeObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    const Matrix4x4& lightVP = game.lightCamera_->GetViewProjectionMatrix();

    game.shadowMap_->PreDraw(commandList);
    commandList->SetGraphicsRootSignature(game.object3dCommon->GetRootSignature());
    commandList->SetPipelineState(game.object3dCommon->GetShadowPipelineState());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& obj : game.objectList) {
        if (obj) { obj->DrawShadow(lightVP); }
    }
    if (game.player_) { game.player_->DrawShadow(lightVP); }
    if (game.currentMode_ == MyGame::AppMode::SkinningEditor) { game.skinningEditor_.DrawShadow(lightVP); }
    if (game.stageRenderer_) { game.stageRenderer_->DrawShadow(lightVP); }

    game.shadowMap_->PostDraw(commandList);

    if (game.postProcess_.IsEnabled()) {
        game.postProcess_.BeginRender(commandList, game.dxCommon.get());
        RenderScene(game);
        game.postProcess_.EndRender(commandList);
        game.dxCommon->PreDraw(false);
        game.postProcess_.DrawToBackBuffer(commandList, game.camera->GetProjectionMatrix());
    } else {
#ifdef NDEBUG
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)WinApp::kWindowWidth, (float)WinApp::kWindowHeight, 0.0f, 1.0f };
        D3D12_RECT scissor = { 0, 0, WinApp::kWindowWidth, WinApp::kWindowHeight };
#else
        D3D12_VIEWPORT viewport = { 320.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
        D3D12_RECT scissor = { 320, 0, 1600, 720 };
#endif
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
        game.dxCommon->PreDraw();
        RenderScene(game);
    }

    game.dxCommon->EndImGui();
    game.dxCommon->PostDraw();
}

void MyGameRenderer::RenderScene(MyGame& game) {
    auto commandList = game.dxCommon->GetCommandList();
    const Matrix4x4& lightVP = game.lightCamera_->GetViewProjectionMatrix();

    if (!game.debugFlags_.show3DObjects &&
        game.currentMode_ != MyGame::AppMode::EffectPreview &&
        game.currentMode_ != MyGame::AppMode::EffectShowcase) {
        return;
    }

    ID3D12DescriptorHeap* heaps[] = { game.textureManager->GetSrvHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
    game.object3dCommon->PreDraw();
    commandList->SetGraphicsRootDescriptorTable(4, game.shadowMap_->GetSrvHandle());

    if (game.currentMode_ == MyGame::AppMode::EffectPreview ||
        game.currentMode_ == MyGame::AppMode::EffectShowcase) {
        if (game.terrainObject_) {
            game.terrainObject_->Draw();
        }
        if (game.debugFlags_.showParticles) {
            ID3D12DescriptorHeap* ph[] = { game.textureManager->GetSrvHeap() };
            commandList->SetDescriptorHeaps(1, ph);
            game.particleManager->Draw();
        }
        return;
    }

    DrawSkybox(game);

    if (game.currentMode_ == MyGame::AppMode::StageSelect) {
        if (game.stageSelect_) { game.stageSelect_->Draw(); }
    } else if (game.currentMode_ == MyGame::AppMode::SkinningEditor) {
        game.skinningEditor_.Draw(game.object3dCommon.get(), game.camera.get());
    } else {
        const bool isGameMode =
            game.currentMode_ == MyGame::AppMode::StageEditor ||
            game.currentMode_ == MyGame::AppMode::GamePlay ||
            game.currentMode_ == MyGame::AppMode::GamePlay_BlockPlace ||
            game.currentMode_ == MyGame::AppMode::EffectPreview;

        if (isGameMode && game.stageRenderer_) {
            game.stageRenderer_->Draw();
            game.stageRenderer_->DrawTransparent();
            game.object3dCommon->PreDraw();
            commandList->SetGraphicsRootDescriptorTable(4, game.shadowMap_->GetSrvHandle());
        }

        if (game.currentMode_ == MyGame::AppMode::GamePlay ||
            game.currentMode_ == MyGame::AppMode::EffectPreview) {
            if (game.player_ && !game.useFirstPersonCamera_) {
                game.player_->Draw();
                if (IsPlayerHiddenByWall(game)) {
                    game.object3dCommon->PreDrawPlayerHighlight();
                    game.player_->DrawHighlight();
                    game.object3dCommon->PreDraw();
                    commandList->SetGraphicsRootDescriptorTable(4, game.shadowMap_->GetSrvHandle());
                }
            }

            if (game.currentMode_ == MyGame::AppMode::GamePlay && game.gameplayUIManager_) {
                game.gameplayUIManager_->Draw3DPrompts(
                    true, game.player_.get(), game.object3dCommon.get(), commandList, game.shadowMap_->GetSrvHandle());
            }
        }

        if ((game.currentMode_ == MyGame::AppMode::StageEditor ||
             game.currentMode_ == MyGame::AppMode::GamePlay_BlockPlace) &&
            game.mapCursor_) {
            game.mapCursor_->Draw();
        }

        if (game.currentMode_ == MyGame::AppMode::DebugView) {
            if (game.terrainObject_ && game.debugFlags_.showTerrain) { game.terrainObject_->Draw(); }
            for (auto& obj : game.objectList) {
                if (obj) { obj->Draw(); }
            }
            if (game.player_) { game.player_->Draw(); }
        }
    }

    if (game.debugFlags_.showParticles) {
        ID3D12DescriptorHeap* ph[] = { game.textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, ph);
        game.particleManager->Draw();
    }

    if (game.debugFlags_.showSprite && game.currentMode_ == MyGame::AppMode::DebugView) {
        game.spriteCommon->PreDraw();
        if (game.sprite) { game.sprite->Draw(); }
    }

    if (game.gameplayUIManager_) {
        game.gameplayUIManager_->DrawSprites(
            game.currentMode_ == MyGame::AppMode::GamePlay ||
            game.currentMode_ == MyGame::AppMode::GamePlay_BlockPlace,
            game.gameplayCameraController_.IsFollowPlayerMode());
    }

    if (game.blockInventoryUI_ &&
        (game.currentMode_ == MyGame::AppMode::GamePlay ||
         game.currentMode_ == MyGame::AppMode::GamePlay_BlockPlace)) {
        game.blockInventoryUI_->Draw();
    }

    const bool invOpen = game.blockInventoryUI_ && game.blockInventoryUI_->IsActive();
    if (game.currentMode_ == MyGame::AppMode::GamePlay && !invOpen && game.stageSelect_) {
        if (game.stageSelect_->GetSelectedFileName() == "tutorial.txt" && game.tutorialSprite_) {
            game.spriteCommon->PreDraw();
            game.tutorialSprite_->Draw();
        }
    }
    if ((game.currentMode_ == MyGame::AppMode::GamePlay_BlockPlace || invOpen) &&
        game.placementTutorialSprite_) {
        game.spriteCommon->PreDraw();
        game.placementTutorialSprite_->Draw();
    }
}

void MyGameRenderer::DrawSkybox(MyGame& game) {
    auto commandList = game.dxCommon->GetCommandList();
    if (game.debugFlags_.showSkybox && game.showSkyboxCubemap_ && game.skybox_) {
        game.skybox_->Draw();
        game.object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, game.shadowMap_->GetSrvHandle());
    } else if (game.debugFlags_.showSkybox && game.skydomeObject_) {
        game.skydomeObject_->SetCamera(game.camera->GetViewMatrix(), game.camera->GetProjectionMatrix());
        game.skydomeObject_->Draw();
    }
}

bool MyGameRenderer::IsPlayerHiddenByWall(const MyGame& game) const {
    if (!game.player_ || !game.camera) { return false; }

    Vector3 camPos = game.camera->GetPosition();
    Vector3 playerPos = game.player_->GetPosition();
    playerPos.y += 0.8f;

    Vector3 diff = {
        playerPos.x - camPos.x,
        playerPos.y - camPos.y,
        playerPos.z - camPos.z
    };
    const float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    if (len <= 0.001f) { return false; }

    Vector3 dir = { diff.x / len, diff.y / len, diff.z / len };
    for (float t = 0.8f; t < len - 1.0f; t += 0.8f) {
        Vector3 cp = { camPos.x + dir.x * t, camPos.y + dir.y * t, camPos.z + dir.z * t };
        const MapCell* cell = game.stageMap_.GetCell(
            static_cast<int>(std::floor(cp.x + 0.5f)),
            static_cast<int>(std::floor(cp.y)),
            static_cast<int>(std::floor(cp.z + 0.5f)));
        if (cell && cell->isSolid) { return true; }
    }
    return false;
}

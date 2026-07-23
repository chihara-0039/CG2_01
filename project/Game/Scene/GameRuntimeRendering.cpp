// 描画パスと可視化処理を、更新・ゲーム進行から分離する。
#include "GameRuntime.h"
#include <algorithm>
#include <cmath>
#include "externals/imgui/imgui.h"

void GameRuntime::DrawCollisionDebugBoxes() {
    if (!debugFlags_.showCollisionBoxes || !camera || !player_ ||
        (currentMode_ != AppMode::GamePlay &&
         currentMode_ != AppMode::GamePlay_BlockPlace &&
         currentMode_ != AppMode::StageEditor &&
         currentMode_ != AppMode::SkinningEditor)) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList) {
        return;
    }

    const Matrix4x4 viewProjection = Math::Multiply(
        camera->GetViewMatrix(), camera->GetProjectionMatrix());

#ifdef NDEBUG
    const float viewportLeft = 0.0f;
    const float viewportTop = 0.0f;
    const float viewportWidth = ImGui::GetIO().DisplaySize.x;
    const float viewportHeight = ImGui::GetIO().DisplaySize.y;
#else
    const float viewportLeft = 320.0f;
    const float viewportTop = 0.0f;
    const float viewportWidth = static_cast<float>(WinApp::kClientWidth);
    const float viewportHeight = static_cast<float>(WinApp::kClientHeight);
#endif

    drawList->PushClipRect(
        ImVec2(viewportLeft, viewportTop),
        ImVec2(viewportLeft + viewportWidth, viewportTop + viewportHeight),
        true);

    auto project = [&](const Vector3& point, ImVec2& screen) {
        const float clipX = point.x * viewProjection.m[0][0] + point.y * viewProjection.m[1][0] +
            point.z * viewProjection.m[2][0] + viewProjection.m[3][0];
        const float clipY = point.x * viewProjection.m[0][1] + point.y * viewProjection.m[1][1] +
            point.z * viewProjection.m[2][1] + viewProjection.m[3][1];
        const float clipW = point.x * viewProjection.m[0][3] + point.y * viewProjection.m[1][3] +
            point.z * viewProjection.m[2][3] + viewProjection.m[3][3];
        if (clipW <= 0.01f) {
            return false;
        }

        const float ndcX = clipX / clipW;
        const float ndcY = clipY / clipW;
        screen.x = viewportLeft + (ndcX * 0.5f + 0.5f) * viewportWidth;
        screen.y = viewportTop + (-ndcY * 0.5f + 0.5f) * viewportHeight;
        return true;
    };

    auto drawBox = [&](const Vector3& minimum, const Vector3& maximum, ImU32 color) {
        const Vector3 corners[8] = {
            { minimum.x, minimum.y, minimum.z }, { maximum.x, minimum.y, minimum.z },
            { maximum.x, maximum.y, minimum.z }, { minimum.x, maximum.y, minimum.z },
            { minimum.x, minimum.y, maximum.z }, { maximum.x, minimum.y, maximum.z },
            { maximum.x, maximum.y, maximum.z }, { minimum.x, maximum.y, maximum.z }
        };
        ImVec2 projected[8];
        for (int i = 0; i < 8; ++i) {
            if (!project(corners[i], projected[i])) {
                return;
            }
        }

        constexpr int edges[12][2] = {
            {0,1}, {1,2}, {2,3}, {3,0},
            {4,5}, {5,6}, {6,7}, {7,4},
            {0,4}, {1,5}, {2,6}, {3,7}
        };
        for (const auto& edge : edges) {
            drawList->AddLine(projected[edge[0]], projected[edge[1]], color, 2.0f);
        }
    };

    if (currentMode_ == AppMode::SkinningEditor) {
        for (const WorldCollisionBox& collider : skinningEditor_.BuildWorldCollisionBoxes()) {
            drawBox(collider.minimum, collider.maximum, IM_COL32(60, 210, 255, 240));
        }
        drawList->PopClipRect();
        return;
    }

    const Vector3 playerPosition = player_->GetPosition();
    const Vector3 playerRadius = player_->GetRadius();
    drawBox(
        { playerPosition.x - playerRadius.x, playerPosition.y, playerPosition.z - playerRadius.z },
        { playerPosition.x + playerRadius.x, playerPosition.y + playerRadius.y * 2.0f,
          playerPosition.z + playerRadius.z },
        IM_COL32(80, 255, 120, 255));

    for (const WorldCollisionBox& collider : blenderRuntimeLevel_.GetCollisionBoxes()) {
        drawBox(collider.minimum, collider.maximum, IM_COL32(60, 210, 255, 240));
    }
    if (blenderRuntimeLevel_.HasPlayerSpawn()) {
        const Vector3& runtimePlayerSpawn = blenderRuntimeLevel_.GetPlayerSpawn();
        const Vector3 halfSize{ 0.2f, 0.2f, 0.2f };
        drawBox(
            { runtimePlayerSpawn.x - halfSize.x, runtimePlayerSpawn.y - halfSize.y,
              runtimePlayerSpawn.z - halfSize.z },
            { runtimePlayerSpawn.x + halfSize.x, runtimePlayerSpawn.y + halfSize.y,
              runtimePlayerSpawn.z + halfSize.z },
            IM_COL32(255, 80, 220, 255));
    }

    constexpr int debugRange = 12;
    const int centerX = static_cast<int>(std::floor(playerPosition.x + 0.5f));
    const int centerY = static_cast<int>(std::floor(playerPosition.y));
    const int centerZ = static_cast<int>(std::floor(playerPosition.z + 0.5f));
    const int minX = (std::max)(0, centerX - debugRange);
    const int maxX = (std::min)(stageMap_.GetWidth() - 1, centerX + debugRange);
    const int minY = (std::max)(0, centerY - debugRange);
    const int maxY = (std::min)(stageMap_.GetHeight() - 1, centerY + debugRange);
    const int minZ = (std::max)(0, centerZ - debugRange);
    const int maxZ = (std::min)(stageMap_.GetDepth() - 1, centerZ + debugRange);

    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                const MapCell* cell = stageMap_.GetCell(x, y, z);
                if (!cell || cell->isHidden) {
                    continue;
                }
                const bool pBlockCollision =
                    cell->type == BlockType::PBlock && !stageMap_.IsPSwitchActive();
                if (!cell->isSolid && !pBlockCollision) {
                    continue;
                }

                Vector3 center = {
                    static_cast<float>(x),
                    static_cast<float>(y) + 0.5f,
                    static_cast<float>(z)
                };
                if (cell->type == BlockType::MovingFloor) {
                    center.x += cell->currentOffsetX;
                    center.y += cell->currentOffsetY;
                    center.z += cell->currentOffsetZ;
                }
                drawBox(
                    { center.x - 0.5f, center.y - 0.5f, center.z - 0.5f },
                    { center.x + 0.5f, center.y + 0.5f, center.z + 0.5f },
                    IM_COL32(255, 170, 40, 230));
            }
        }
    }
    drawList->PopClipRect();
}

void GameRuntime::Draw() {
    auto commandList = dxCommon->GetCommandList();

    if (skydomeObject_) {
        if (postProcessInitialized_ && postProcess_.GetSkyboxLinkMode() == 1) {
            skydomeObject_->SetColor(postProcess_.GetClearColor());
        } else {
            skydomeObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    shadowMap_->PreDraw(commandList);
    commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    commandList->SetPipelineState(object3dCommon->GetShadowPipelineState());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& obj : objectList) {
        if (obj) {
            obj->DrawShadow(lightVP);
        }
    }
    if (player_) {
        player_->DrawShadow(lightVP); 
    }
    if (currentMode_ == AppMode::SkinningEditor && skinningEditorInitialized_) {
        skinningEditor_.DrawShadow(lightVP); 
    }
    DrawRuntimeLevelShadows(lightVP);
    if (stageRenderer_) { 
        stageRenderer_->DrawShadow(lightVP);
    }

    shadowMap_->PostDraw(commandList);

    if (postProcessInitialized_ && postProcess_.IsEnabled()) {
        postProcess_.BeginRender(commandList, dxCommon.get());
        RenderScene();
        postProcess_.EndRender(commandList);
        dxCommon->PreDraw(false);
        postProcess_.DrawToBackBuffer(commandList, camera->GetProjectionMatrix());
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
        dxCommon->PreDraw();
        RenderScene();
    }

    DrawCollisionDebugBoxes();
    dxCommon->EndImGui();
    dxCommon->PostDraw();
}

void GameRuntime::RenderScene() {
    auto commandList = dxCommon->GetCommandList();

    if (!debugFlags_.show3DObjects &&
        currentMode_ != AppMode::EffectPreview &&
        currentMode_ != AppMode::EffectShowcase &&
        currentMode_ != AppMode::PostEffectShowcase) {
        return;
    }

    ID3D12DescriptorHeap* heaps[] = {
        textureManager->GetSrvHeap()
    };
    commandList->SetDescriptorHeaps(1, heaps);
    object3dCommon->PreDraw();
    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());

    if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        if (terrainObject_) {
            terrainObject_->Draw();
        }
        if (debugFlags_.showParticles && !IsWindowInactive()) {
            ID3D12DescriptorHeap* particleHeaps[] = { textureManager->GetSrvHeap() };
            commandList->SetDescriptorHeaps(1, particleHeaps);
            particleManager->Draw();
        }
        return;
    }

    if (currentMode_ == AppMode::PostEffectShowcase) {
        if (terrainObject_) {
            terrainObject_->Draw();
        }
        if (player_) {
            player_->Draw();
        }
        return;
    }

    DrawSkyboxForFrame();

    if (currentMode_ == AppMode::StageSelect) {
        if (stageSelect_) {
            stageSelect_->Draw();
        }
    } else if (currentMode_ == AppMode::SkinningEditor && skinningEditorInitialized_) {
        skinningEditor_.Draw(object3dCommon.get(), camera.get());
    } else {
        const bool isGameMode =
            currentMode_ == AppMode::StageEditor ||
            currentMode_ == AppMode::GamePlay ||
            currentMode_ == AppMode::GamePlay_BlockPlace ||
            currentMode_ == AppMode::EffectPreview;

        if (isGameMode && stageRenderer_) {
            stageRenderer_->Draw();
            stageRenderer_->DrawTransparent();
            object3dCommon->PreDraw();
            commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
        }

        DrawRuntimeLevelObjects();

        if (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::EffectPreview) {
            if (player_ && !useFirstPersonCamera_) {
                player_->Draw();
                if (IsPlayerHiddenByWall()) {
                    object3dCommon->PreDrawPlayerHighlight();
                    player_->DrawHighlight();
                    object3dCommon->PreDraw();
                    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
                }
            }

            if (currentMode_ == AppMode::GamePlay && gameplayUIManager_) {
                gameplayUIManager_->Draw3DPrompts(
                    true, player_.get(), object3dCommon.get(), commandList, shadowMap_->GetSrvHandle());
            }
        }

        if ((currentMode_ == AppMode::StageEditor ||
             currentMode_ == AppMode::GamePlay_BlockPlace) &&
            mapCursor_) {
            mapCursor_->Draw();
        }

        if (currentMode_ == AppMode::DebugView) {
            if (terrainObject_ && debugFlags_.showTerrain) {
                terrainObject_->Draw();
            }
            for (auto& obj : objectList) {
                if (obj) {
                    obj->Draw();
                }
            }
            if (player_) { 
                player_->Draw();
            }
        }
    }

    if (debugFlags_.showParticles && !IsWindowInactive()) {
        ID3D12DescriptorHeap* particleHeaps[] = { textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, particleHeaps);
        particleManager->Draw();
    }

    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        spriteCommon->PreDraw();
        if (sprite) { sprite->Draw(); }
    }

    if (gameplayUIManager_) {
        gameplayUIManager_->DrawSprites(
            currentMode_ == AppMode::GamePlay ||
            currentMode_ == AppMode::GamePlay_BlockPlace,
            gameplayCameraController_.IsFollowPlayerMode());
    }

    if (blockInventoryUI_ &&
        (currentMode_ == AppMode::GamePlay ||
         currentMode_ == AppMode::GamePlay_BlockPlace)) {
        blockInventoryUI_->Draw();
    }

    const bool isInventoryOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();
    if (currentMode_ == AppMode::GamePlay && !isInventoryOpen && stageSelect_) {
        if (stageSelect_->GetSelectedFileName() == "tutorial.txt" && tutorialSprite_) {
            spriteCommon->PreDraw();
            tutorialSprite_->Draw();
        }
    }
    if ((currentMode_ == AppMode::GamePlay_BlockPlace || isInventoryOpen) && placementTutorialSprite_) {
        spriteCommon->PreDraw();
        placementTutorialSprite_->Draw();
    }
}

void GameRuntime::DrawSkyboxForFrame() {
    auto commandList = dxCommon->GetCommandList();
    if (debugFlags_.showSkybox && showSkyboxCubemap_ && skybox_) {
        skybox_->Draw();
        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
    } else if (debugFlags_.showSkybox && skydomeObject_) {
        skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skydomeObject_->Draw();
    }
}

bool GameRuntime::IsPlayerHiddenByWall() const {
    if (!player_ || !camera) { 
        return false;
    }

    Vector3 cameraPosition = camera->GetPosition();
    Vector3 playerPos = player_->GetPosition();
    playerPos.y += 0.8f;

    Vector3 cameraToPlayer = {
        playerPos.x - cameraPosition.x,
        playerPos.y - cameraPosition.y,
        playerPos.z - cameraPosition.z
    };
    const float cameraToPlayerLength = std::sqrt(
        cameraToPlayer.x * cameraToPlayer.x +
        cameraToPlayer.y * cameraToPlayer.y +
        cameraToPlayer.z * cameraToPlayer.z);
    if (cameraToPlayerLength <= 0.001f) {
        return false;
    }

    Vector3 cameraToPlayerDirection = {
        cameraToPlayer.x / cameraToPlayerLength,
        cameraToPlayer.y / cameraToPlayerLength,
        cameraToPlayer.z / cameraToPlayerLength
    };
    for (float rayDistance = 0.8f; rayDistance < cameraToPlayerLength - 1.0f; rayDistance += 0.8f) {
        Vector3 samplePosition = {
            cameraPosition.x + cameraToPlayerDirection.x * rayDistance,
            cameraPosition.y + cameraToPlayerDirection.y * rayDistance,
            cameraPosition.z + cameraToPlayerDirection.z * rayDistance
        };
        const MapCell* cell = stageMap_.GetCell(
            static_cast<int>(std::floor(samplePosition.x + 0.5f)),
            static_cast<int>(std::floor(samplePosition.y)),
            static_cast<int>(std::floor(samplePosition.z + 0.5f)));
        if (cell && cell->isSolid) { 
            return true;
        }
    }
    return false;
}



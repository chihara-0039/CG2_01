// 録画ツールと探索ステージの接続。ゲームの通常ステージは変更しない。
#include "GameRuntime.h"
#include "../../Tests/ExpeditionPhysicsChecks.h"
#include "Goal.h"
#include "../Stage/RotationIsland.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "../../Tests/MouseLookChecks.h"

namespace {
// 自動検証や録画に使う環境変数を、安全な文字列として取得する。
std::string CaptureEnvironment(const char *name) {
    char value[32768]{};
    const DWORD length = GetEnvironmentVariableA(name, value, sizeof(value));
    return length && length < sizeof(value) ? std::string(value) : std::string{};
}
}

// 指定された検証・録画モードの準備を行い、必要なら自動チェックを開始する。
void GameRuntime::InitializeCaptureTools() {
    if (CaptureEnvironment("CG2_MOUSE_VERIFY") == "1") {
        std::filesystem::create_directories("../generated/mouse-look-review");
        const auto name = CaptureEnvironment("CG2_MOUSE_VERIFY_NAME");
        const bool ok = VerifyMouseLook(*winApp, *player_,
                                        "../generated/mouse-look-review/" + (name.empty() ? "native" : name) + ".json");
        PostQuitMessage(ok ? 0 : 3);
        return;
    }

    const char *captions[] = {"baseline", "bubble", "scan", "trial_hint", "recording"};
    for (int i = 0; i < 5; ++i) {
        fogLabels_[i] = std::make_unique<Sprite>();
        fogLabels_[i]->Initialize(spriteCommon.get(), textureManager->LoadTexture(std::string("Resources/UI/PR1_01/") +
                                                                                  captions[i] + ".png"));
        fogLabels_[i]->SetPosition({36, 28});
        fogLabels_[i]->SetSize({1200, 130});
        fogLabels_[i]->Update();
    }
    fogLabels_[4]->SetPosition({1550, 30});
    fogLabels_[4]->SetSize({320, 65});
    fogLabels_[4]->Update();
    scanMeter_ = std::make_unique<Sprite>();
    scanMeter_->Initialize(spriteCommon.get(), 0);
    scanMeter_->SetPosition({38, 164});
    scanMeter_->SetColor({.12f, .85f, 1, 1});
    const auto comparison = CaptureEnvironment("CG2_COMPARE");
    for (int i = 0; i < 12; ++i) {
        buildLabels_[i] = std::make_unique<Sprite>();
        buildLabels_[i]->Initialize(
            spriteCommon.get(), textureManager->LoadTexture("Resources/UI/PR1_01/build-" + std::to_string(i) + ".png"));
        buildLabels_[i]->SetPosition({20, 20});
        buildLabels_[i]->SetSize({950, 110});
        buildLabels_[i]->Update();
    }
    for (int i = 0; i < 2; ++i) {
        buildCounts_[i] = std::make_unique<Sprite>();
        buildCounts_[i]->Initialize(spriteCommon.get(), textureManager->LoadTexture("Resources/UI/PR1_01/build-count-" +
                                                                                    std::to_string(i) + ".png"));
        buildCounts_[i]->SetPosition({990, 25});
        buildCounts_[i]->SetSize({270, 65});
        buildCounts_[i]->Update();
    }
    if (!CaptureEnvironment("CG2_BUILD_SHOT").empty() || CaptureEnvironment("CG2_BUILD_TRIAL") == "1" ||
        CaptureEnvironment("CG2_BUILD_VERIFY") == "1") {
        BeginBuildTrial();
        buildCapture_ = !CaptureEnvironment("CG2_BUILD_SHOT").empty();
        comparisonActive_ = buildCapture_;
        comparisonStarted_ = std::chrono::steady_clock::now();
    }
    if (CaptureEnvironment("CG2_TRIAL") == "1" || CaptureEnvironment("CG2_VERIFY_TRIAL") == "1") {
        BeginFogTrial();
    }
    if (CaptureEnvironment("CG2_VERIFY_TRIAL") == "1") {
        trialVerification_ = true;
    }
    if (comparison == "baseline" || comparison == "bubble" || comparison == "scan") {
        comparisonMode_ = comparison == "baseline" ? 0 : comparison == "bubble" ? 1 : 2;
        BeginFogTrial();
        fogExplorer_.SetMode(comparisonMode_);
        comparisonActive_ = true;
        comparisonStarted_ = std::chrono::steady_clock::now();
    }
    if (CaptureEnvironment("CG2_DIORAMA") == "1") {
        BeginDiorama();
    }
    if (!CaptureEnvironment("CG2_CINEMATIC_SHOT").empty()) {
        if (CaptureEnvironment("CG2_CINEMATIC_SHOT") != "title") {
            BeginFogTrial();
            fogTrial_ = false;
            dioramaCapture_ = true;
            stageMap_.LoadFromFile("Resources/Stages/tutorial.txt");
            backupMap_ = stageMap_;
            stageRenderer_->BuildFromStageMap(stageMap_);
            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
            playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
            bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
            blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
            gameplayCameraController_.ResetCamera(camera.get(), player_.get(), stageMap_, 0);
            if (CaptureEnvironment("CG2_CINEMATIC_SHOT") == "arrival_transition") {
                sceneManager_->ChangeScene(SceneType::StageSelect, *this);
                // Regression fixture: the outgoing selection camera must never
                // become the arrival's gameplay destination.
                camera->SetPosition({70, 45, -65});
                camera->SetRotation({.2f, 1.4f, 0});
                camera->SetFov(.45f);
                camera->Update();
                startIntroPending_ = true;
                RequestSceneChange(SceneType::GamePlay);
            } else {
                startIntroPending_ = CaptureEnvironment("CG2_CINEMATIC_SHOT") == "arrival";
                OnSceneEntered(SceneType::GamePlay);
            }
        }
        if (CaptureEnvironment("CG2_CINEMATIC_SHOT") == "headlamp") {
            player_->SetPosition({3.0f, 0.5f, 1.0f});
            player_->SetRotation({0, 3.14159265f, 0});
            stageMap_.SetLightIntensity(0.12f);
            gameplayCameraController_.ResetCamera(camera.get(), player_.get(), stageMap_, 0);
        }
        if (CaptureEnvironment("CG2_CINEMATIC_SHOT") == "occlusion") {
            stageMap_.SetBlock(5, 0, 5, BlockType::Ground);
            stageRenderer_->BuildFromStageMap(stageMap_);
            player_->SetPosition({5.0f, 0.5f, 5.0f});
            gameplayCameraController_.ResetCamera(camera.get(), player_.get(), stageMap_, 0);
        }
        comparisonStarted_ = std::chrono::steady_clock::now();
    }
    const bool expeditionReview = CaptureEnvironment("CG2_CINEMATIC_SHOT") == "expedition";
    const bool placementReview = CaptureEnvironment("CG2_CINEMATIC_SHOT") == "placement";
    if (CaptureEnvironment("CG2_EXPEDITION") == "1" || expeditionReview) {
        blenderStageActive_ = false;
        player_->SetExternalCollisionBoxes(nullptr);
        fogTrial_ = false;
        buildTrial_ = false;
        stageMap_.LoadFromFile("Resources/Stages/expeditionGrandTour.txt");
        backupMap_ = stageMap_;
        stageRenderer_->BuildFromStageMap(stageMap_);
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
        blockInventory_.Initialize(0);
        bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        stageSelect_->Initialize(object3dCommon.get(), input.get(), 3);
        gameplayCameraController_.ResetCamera(camera.get(), player_.get(), stageMap_, 7);
        RefreshGoalGuideTarget();
        startIntroPending_ = !expeditionReview && !placementReview && !dxCommon->IsPerformanceCaptureActive();
        sceneManager_->ChangeScene(SceneType::GamePlay, *this);
        if (CaptureEnvironment("CG2_EXPEDITION_PHYSICS") == "1") {
            const bool ok = VerifyExpeditionPlayerPhysics("../generated/expedition-review/player-physics.json");
            PostQuitMessage(ok ? 0 : 3);
        }
        if (CaptureEnvironment("CG2_PICKUP_VERIFY") == "1") {
            StageMap fixture;
            fixture.Initialize(8, 6, 8);
            fixture.SetBlock(3, 0, 3, BlockType::BubblePickup, PackBubbleContents(BlockType::Wall, 0));
            fixture.SetBlock(3, 1, 3, BlockType::Ground);
            BlockInventory stock;
            stock.Initialize(0);
            BubblePickupController pickups;
            pickups.Initialize(&fixture, stageRenderer_.get(), &stock);
            pickups.Update({3, 1.5f, 3});
            const bool floorProtected =
                fixture.GetCell(3, 0, 3)->type == BlockType::BubblePickup && stock.GetBlockCount() == 0;
            fixture.SetBlock(3, 2, 3, BlockType::BubblePickup, PackBubbleContents(BlockType::Wall, 0));
            pickups.Update({3, 1.5f, 3});
            const bool collected = fixture.GetCell(3, 2, 3)->type == BlockType::None && stock.GetBlockCount() == 1;
            fixture.SetBlock(4, 2, 3, BlockType::BubblePickup, PackBubbleContents(BlockType::Star, 0));
            pickups.Update({4, 1.5f, 3});
            const bool invalidProtected = fixture.GetCell(4, 2, 3)->type == BlockType::BubblePickup &&
                                          stock.GetBlockCount() == 1 && pickups.GetCollectedCount() == 1;
            const bool ok = floorProtected && collected && invalidProtected;
            std::ofstream("../generated/expedition-review/pickup-verification.json")
                << "{\"success\":" << (ok ? "true" : "false")
                << ",\"floorProtected\":" << (floorProtected ? "true" : "false")
                << ",\"reachableCollected\":" << (collected ? "true" : "false")
                << ",\"invalidContentsPreserved\":" << (invalidProtected ? "true" : "false") << "}";
            PostQuitMessage(ok ? 0 : 3);
        }
        if (CaptureEnvironment("CG2_SESSION_VERIFY") == "1") {
            stageSelect_->Initialize(object3dCommon.get(), input.get(), 3);
            blockInventory_.AddBlock(BlockType::Wall, 1, 1);
            blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
            blockPlacementController_.SetPlaceCustomId(1);
            blockPlacementController_.SetPlayerPosition({6, 1.5f, 6});
            const bool placed = blockPlacementController_.TryPlace({37, 0, 5});
            blockInventory_.AddBlock(BlockType::Wall, 2, 1);
            const bool pending = blockInventoryUI_->SelectShortcut(1);
            blockInventoryUI_->ToggleOpen();
            placeRotationY_ = 1.57f;
            useFirstPersonCamera_ = true;
            isGamePaused_ = true;
            ReturnToStageSelect();
            const bool stockReset = blockInventory_.GetBlockCount() == 0;
            const bool undoReset = !blockPlacementController_.HasUndo();
            const bool uiReset = !blockInventoryUI_->IsActive() && !blockInventoryUI_->ConsumeUseRequest() &&
                                 blockInventoryUI_->GetSelectedBlockType() == BlockType::None;
            const bool modesReset = placeRotationY_ == 0 && !useFirstPersonCamera_ && !isGamePaused_;
            const bool mapRestored = stageMap_.GetCell(37, 0, 5)->type == BlockType::None;
            const bool selectionKept = stageSelect_->GetSelectedIndex() == 3;
            InputTestPlayback::Advance(*input, 0, 0, {DIK_SPACE});
            stageSelect_->Update();
            const bool confirmed = stageSelect_->IsFnished();
            stageSelect_->ReportLoadFailure();
            const bool failureStaysSelectable =
                confirmed && !stageSelect_->IsFnished() && stageSelect_->GetSelectedIndex() == 3;
            const bool ok = placed && pending && stockReset && undoReset && uiReset && modesReset && mapRestored &&
                            selectionKept && failureStaysSelectable;
            std::ofstream("../generated/expedition-review/session-verification.json")
                << "{\"success\":" << (ok ? "true" : "false")
                << ",\"inventoryReset\":" << (stockReset ? "true" : "false")
                << ",\"undoReset\":" << (undoReset ? "true" : "false")
                << ",\"pendingUiRequestCleared\":" << (uiReset ? "true" : "false")
                << ",\"modesReset\":" << (modesReset ? "true" : "false")
                << ",\"mapRestored\":" << (mapRestored ? "true" : "false")
                << ",\"selectionPreserved\":" << (selectionKept ? "true" : "false")
                << ",\"failedSelectionCanRetry\":" << (failureStaysSelectable ? "true" : "false") << "}";
            PostQuitMessage(ok ? 0 : 3);
        }
        if (CaptureEnvironment("CG2_PLACEMENT_VERIFY") == "1") {
            Input script;
            Camera testCamera;
            GameplayCameraController controller;
            controller.Initialize();
            controller.ResetCamera(&testCamera, player_.get(), stageMap_, 7);
            const float fov = controller.GetFov();
            const auto initial = testCamera.GetPosition();
            StageEditorController editor;
            editor.Initialize();
            mapCursor_->SetIndex({17, 1, 9}, stageMap_);
            InputTestPlayback::Advance(script, 0, 0, {DIK_Q});
            editor.HandleCursorInput(&script, stageMap_, mapCursor_.get(), lightCamera_.get(), &testCamera);
            controller.Update(&script, &testCamera, winApp.get(), player_.get(), true);
            const auto afterQ = testCamera.GetPosition();
            const bool qOnlyCursor = mapCursor_->GetIndex().y == 2 && controller.GetFov() == fov &&
                                     initial.x == afterQ.x && initial.y == afterQ.y && initial.z == afterQ.z;
            InputTestPlayback::Advance(script, 0, 0, {DIK_E});
            editor.HandleCursorInput(&script, stageMap_, mapCursor_.get(), lightCamera_.get(), &testCamera);
            controller.Update(&script, &testCamera, winApp.get(), player_.get(), true);
            const bool eOnlyCursor = mapCursor_->GetIndex().y == 1 && controller.GetFov() == fov;
            InputTestPlayback::Advance(script, 0, 0, {});
            InputTestPlayback::Mouse(script, 30, -20, 1);
            const float angle = controller.GetAngle();
            controller.Update(&script, &testCamera, winApp.get(), player_.get(), true);
            const bool rightOrbit = controller.GetAngle() != angle && controller.GetFov() == fov;
            InputTestPlayback::Mouse(script, 30, 0, 0);
            const float afterOrbit = controller.GetAngle();
            controller.Update(&script, &testCamera, winApp.get(), player_.get(), true);
            const bool leftNoOrbit = controller.GetAngle() == afterOrbit;
            InputTestPlayback::Mouse(script, 0, 0);
            InputTestPlayback::Advance(script, 0, 0, {DIK_Q});
            controller.Update(&script, &testCamera, winApp.get(), player_.get(), false);
            const bool playZoom = controller.GetFov() > fov;
            controller.TogglePlayerFollow(player_.get());
            const bool followDisabled = !controller.IsFollowPlayerMode();
            controller.TogglePlayerFollow(player_.get());
            const bool followRestored = controller.IsFollowPlayerMode();
            const bool ok =
                qOnlyCursor && eOnlyCursor && rightOrbit && leftNoOrbit && playZoom && followDisabled && followRestored;
            std::ofstream("../generated/expedition-review/placement-controls.json")
                << "{\"success\":" << (ok ? "true" : "false") << ",\"qOnlyCursor\":" << (qOnlyCursor ? "true" : "false")
                << ",\"eOnlyCursor\":" << (eOnlyCursor ? "true" : "false")
                << ",\"rightDragOrbits\":" << (rightOrbit ? "true" : "false")
                << ",\"leftClickDoesNotOrbit\":" << (leftNoOrbit ? "true" : "false")
                << ",\"normalPlayKeyboardZoom\":" << (playZoom ? "true" : "false")
                << ",\"followCanDisable\":" << (followDisabled ? "true" : "false")
                << ",\"followCanRestore\":" << (followRestored ? "true" : "false") << "}";
            PostQuitMessage(ok ? 0 : 3);
        }
        if (CaptureEnvironment("CG2_EXPEDITION_VERIFY") == "1") {
            int placedFeedback = 0, undoFeedback = 0, collectedFeedback = 0;
            blockPlacementController_.SetFeedbackCallback([&](const Vector3 &, bool undo) {
                if (undo) {
                    ++undoFeedback;
                } else {
                    ++placedFeedback;
                }
            });
            bubblePickupController_.SetCollectCallback([&](const Vector3 &) { ++collectedFeedback; });
            bool collected = true, selected = true, placed = true, undo = true, roundtrip = true;
            stageMap_.SaveToFile("../generated/expedition-review/roundtrip.txt");
            StageMap restored;
            restored.LoadFromFile("../generated/expedition-review/roundtrip.txt");
            roundtrip =
                restored.GetWidth() == 40 && restored.GetHeight() == 12 && restored.GetVisualStyle() == "Miniature" &&
                restored.GetCell(5, 4, 33)->doorTargetIndex.x == 6 && restored.GetCell(25, 3, 17)->moveOffset.z == 6;
            for (int slot = 1; slot <= 5; ++slot) {
                roundtrip = roundtrip && restored.GetCustomPart(slot)->name == stageMap_.GetCustomPart(slot)->name;
                for (int y = 0; y < 3; ++y) {
                    for (int z = 0; z < 3; ++z) {
                        for (int x = 0; x < 3; ++x) {
                            roundtrip = roundtrip && restored.GetCustomPart(slot)->cells[y][z][x].type ==
                                                         stageMap_.GetCustomPart(slot)->cells[y][z][x].type;
                        }
                    }
                }
            }
            for (int y = 0; y < stageMap_.GetHeight(); ++y) {
                for (int z = 0; z < stageMap_.GetDepth(); ++z) {
                    for (int x = 0; x < stageMap_.GetWidth(); ++x) {
                        if (stageMap_.GetCell(x, y, z)->type == BlockType::BubblePickup) {
                            bubblePickupController_.Update({static_cast<float>(x), y - 0.5f, static_cast<float>(z)});
                        }
                    }
                }
            }
            collected =
                bubblePickupController_.AreAllPickupsCollected() && bubblePickupController_.GetTotalPickupCount() == 10;
            for (int slot = 1; slot <= 5; ++slot) {
                selected = blockInventoryUI_->SelectShortcut(slot) &&
                           blockInventoryUI_->GetSelectedCustomId() == slot && blockInventoryUI_->ConsumeUseRequest() &&
                           selected;
                blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
                blockPlacementController_.SetPlaceCustomId(slot);
                blockPlacementController_.SetPlayerPosition({6, 1.5f, 6});
                placed = blockPlacementController_.TryPlace({37, 0, slot * 5}, 1.5707963f) &&
                         blockInventory_.GetBlockCount(BlockType::Wall, slot) == 0 && placed;
                undo = blockPlacementController_.TryUndo() &&
                       blockInventory_.GetBlockCount(BlockType::Wall, slot) == 1 &&
                       !blockPlacementController_.TryUndo() && undo;
            }
            bool normalSlots = true, zeroStockRejected = true;
            const BlockType kinds[] = {BlockType::CrumblingFloor, BlockType::Wall, BlockType::Ladder,
                                       BlockType::IceBlock, BlockType::MovingFloor};
            const int digits[] = {0, 6, 7, 8, 9};
            for (int i = 0; i < 5; ++i) {
                normalSlots = blockInventoryUI_->SelectShortcut(digits[i]) &&
                              blockInventoryUI_->GetSelectedBlockType() == kinds[i] &&
                              blockInventoryUI_->ConsumeUseRequest() && normalSlots;
                blockInventory_.ConsumeBlock(kinds[i], 1);
                zeroStockRejected = !blockInventoryUI_->SelectShortcut(digits[i]) && zeroStockRejected;
                blockInventory_.AddBlock(kinds[i], 1);
            }
            const bool emptyRejected = !blockInventoryUI_->SelectShortcut(42);
            const bool invalidPlace = !blockPlacementController_.TryPlace({-1, -1, -1});
            const bool feedbackOk = invalidPlace && placedFeedback == 5 && undoFeedback == 5 && collectedFeedback == 10;
            blockPlacementController_.SetFeedbackCallback({});
            bubblePickupController_.SetCollectCallback({});
            std::ofstream("../generated/expedition-review/particle-events.json")
                << "{\"success\":" << (feedbackOk ? "true" : "false") << ",\"placed\":" << placedFeedback
                << ",\"undone\":" << undoFeedback << ",\"collected\":" << collectedFeedback << "}";
            const bool ok = feedbackOk && collected && selected && placed && undo && roundtrip && emptyRejected &&
                            normalSlots && zeroStockRejected;
            std::ofstream("../generated/expedition-review/native-verification.json")
                << "{\"success\":" << (ok ? "true" : "false") << ",\"saveLoad\":" << (roundtrip ? "true" : "false")
                << ",\"collectTenBubbles\":" << (collected ? "true" : "false")
                << ",\"quickSelectFiveParts\":" << (selected ? "true" : "false")
                << ",\"rotateAndPlaceFiveParts\":" << (placed ? "true" : "false")
                << ",\"undoRestoresStockOnce\":" << (undo ? "true" : "false")
                << ",\"normalQuickSlots\":" << (normalSlots ? "true" : "false")
                << ",\"zeroStockRejected\":" << (zeroStockRejected ? "true" : "false") << "}";
            PostQuitMessage(ok ? 0 : 3);
        }
        if (expeditionReview) {
            for (int slot = 1; slot <= 5; ++slot) {
                blockInventory_.AddBlock(BlockType::Wall, 1, slot);
            }
            blockInventoryUI_->ToggleOpen();
        }
    }
    if (CaptureEnvironment("CG2_CAMPAIGN_VERIFY") == "1") {
        std::filesystem::create_directories("../generated/campaign-review");
        bool all = true;
        std::ofstream report("../generated/campaign-review/verification.json");
        report << "{\"courses\":[";
        stageSelect_->Initialize(object3dCommon.get(), input.get(), 0);
        bool navigation = stageSelect_->GetStageCount() == 4;
        const float initialAngle = stageSelect_->GetPreviewAngle();
        for (int index = 1; index <= 4; ++index) {
            InputTestPlayback::Advance(*input, 0, 0, {});
            stageSelect_->Update();
            InputTestPlayback::Advance(*input, 0, 0, {DIK_DOWN});
            stageSelect_->Update();
            navigation = navigation && stageSelect_->GetSelectedIndex() == index % 4;
        }
        const bool rotating = stageSelect_->GetPreviewAngle() != initialAngle;
        navigation = navigation && rotating;
        const float beforeDrag = stageSelect_->GetPreviewAngle();
        InputTestPlayback::Advance(*input, 0, 0, {});
        InputTestPlayback::Mouse(*input, 24, 8, 1);
        stageSelect_->Update();
        const bool manualOrbit = stageSelect_->GetPreviewAngle() - beforeDrag > .1f;
        InputTestPlayback::Mouse(*input, 0, 0);
        navigation = navigation && manualOrbit;
        for (int index = 0; index < 4; ++index) {
            sceneFadePhase_ = SceneFadePhase::None;
            sceneManager_->ChangeScene(SceneType::StageSelect, *this);
            stageSelect_->Initialize(object3dCommon.get(), input.get(), index);
            InputTestPlayback::Advance(*input, 0, 0, {});
            InputTestPlayback::Advance(*input, 0, 0, {DIK_RETURN});
            UpdateStageSelect();
            const bool selected =
                pendingSceneType_ == SceneType::GamePlay && sceneFadePhase_ == SceneFadePhase::FadingOut;
            sceneFadePhase_ = SceneFadePhase::None;
            sceneManager_->ChangeScene(SceneType::GamePlay, *this);
            startIntro_->Cancel(camera.get());
            startIntroPending_ = false;
            player_->Respawn();
            isGamePaused_ = false;
            dioramaCapture_ = false;
            comparisonActive_ = false;
            InputTestPlayback::Advance(*input, 0, 0, {});
            Vector3 goal{};
            std::vector<Vector3> bubbles, keys;
            for (int y = 0; y < stageMap_.GetHeight(); ++y) {
                for (int z = 0; z < stageMap_.GetDepth(); ++z) {
                    for (int x = 0; x < stageMap_.GetWidth(); ++x) {
                        const auto *cell = stageMap_.GetCell(x, y, z);
                        if (cell->type == BlockType::Goal) {
                            goal = {static_cast<float>(x), y - .5f, static_cast<float>(z)};
                        }
                        if (cell->type == BlockType::BubblePickup) {
                            bubbles.push_back({static_cast<float>(x), y - .5f, static_cast<float>(z)});
                        }
                        if (cell->type == BlockType::Key) {
                            keys.push_back({static_cast<float>(x), y - .5f, static_cast<float>(z)});
                        }
                    }
                }
            }
            player_->SetPosition(goal);
            UpdateGamePlay();
            const bool locked = isGoalBlocked_ && !isGoalReached_;
            const bool normalOcclusion = ShouldShowPlayerOcclusion();
            InputTestPlayback::Advance(*input, 0, 0, {DIK_F1});
            UpdateGameplayUserInterface();
            const bool helpOpened = showControlHelp_;
            InputTestPlayback::Advance(*input, 0, 0, {});
            InputTestPlayback::Advance(*input, 0, 0, {DIK_F1});
            UpdateGameplayUserInterface();
            const bool helpToggled = helpOpened && !showControlHelp_;
            InputTestPlayback::Advance(*input, 0, 0, {});
            for (const auto &position : bubbles) {
                bubblePickupController_.Update(position);
            }
            player_->SetPosition(goal);
            UpdateGamePlay();
            const bool bubblesDoNotUnlock = !isGoalReached_ && isGoalBlocked_;
            for (const auto &p : keys) {
                player_->SetPosition(p);
                InputTestPlayback::Advance(*input, 0, 0, {});
                InputTestPlayback::Advance(*input, 0, 0, {DIK_F});
                player_->KeyUpdate(stageMap_);
            }
            InputTestPlayback::Advance(*input, 0, 0, {});
            const bool collected = player_->GetCollectedLevelKeys() == objectiveKeyTotal_ &&
                                   objectiveKeyTotal_ == static_cast<int>(keys.size());
            player_->SetPosition(goal);
            UpdateGamePlay();
            const bool goalReached = isGoalReached_;
            stageMap_ = backupMap_;
            stageRenderer_->BuildFromStageMap(stageMap_);
            RefreshGoalGuideTarget();
            player_->Respawn();
            blockInventory_.Initialize(0);
            bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
            isGoalReached_ = false;
            for (const auto &p : keys) {
                player_->SetPosition(p);
                InputTestPlayback::Advance(*input, 0, 0, {});
                InputTestPlayback::Advance(*input, 0, 0, {DIK_F});
                player_->KeyUpdate(stageMap_);
            }
            InputTestPlayback::Advance(*input, 0, 0, {});
            player_->SetPosition(goal);
            UpdateGamePlay();
            const bool keysAloneUnlock = isGoalReached_ && bubblePickupController_.GetCollectedCount() == 0;
            const bool goalHighlightSuppressed = !ShouldShowPlayerOcclusion();
            UpdatePlayerCameraAndTransform(camera->GetViewMatrix(), camera->GetProjectionMatrix(),
                                           lightCamera_->GetViewProjectionMatrix());
            bool finalCameraMatches = true;
            if (player_->GetSkinnedObject() && player_->GetSkinnedObject()->GetObject3d()) {
                const auto &uploaded = player_->GetSkinnedObject()->GetObject3d()->GetUploadedTransform();
                const auto expected = Math::Multiply(
                    uploaded.World, Math::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix()));
                for (int row = 0; row < 4; ++row) {
                    for (int col = 0; col < 4; ++col) {
                        finalCameraMatches =
                            finalCameraMatches && std::abs(uploaded.WVP.m[row][col] - expected.m[row][col]) < .0001f;
                    }
                }
            }
            sceneFadePhase_ = SceneFadePhase::None;
            goalCelebrationTimer_ = StarGet::kDuration;
            UpdateGoalCelebration();
            const bool clearRequested =
                sceneFadePhase_ == SceneFadePhase::FadingOut && pendingSceneType_ == SceneType::GameClear;
            sceneFadePhase_ = SceneFadePhase::None;
            sceneManager_->ChangeScene(SceneType::GameClear, *this);
            InputTestPlayback::Advance(*input, 0, 0, {DIK_RETURN});
            UpdateGameClear(true);
            const bool returned = sceneFadePhase_ == SceneFadePhase::FadingOut &&
                                  pendingSceneType_ == SceneType::StageSelect &&
                                  stageSelect_->GetSelectedIndex() == index && blockInventory_.GetBlockCount() == 0;
            const bool ok = selected && locked && normalOcclusion && helpToggled && bubblesDoNotUnlock &&
                            keysAloneUnlock && finalCameraMatches && collected && goalReached &&
                            goalHighlightSuppressed && clearRequested && returned;
            all = all && ok;
            if (index > 0) {
                report << ",";
            }
            report << "{\"index\":" << index << ",\"selected\":" << (selected ? "true" : "false")
                   << ",\"lockedBeforeCollection\":" << (locked ? "true" : "false")
                   << ",\"keysAloneUnlock\":" << (keysAloneUnlock ? "true" : "false")
                   << ",\"bubblesDoNotUnlock\":" << (bubblesDoNotUnlock ? "true" : "false")
                   << ",\"finalCameraMatches\":" << (finalCameraMatches ? "true" : "false")
                   << ",\"allKeysCollected\":" << (collected ? "true" : "false")
                   << ",\"goalReached\":" << (goalReached ? "true" : "false")
                   << ",\"normalOcclusionEnabled\":" << (normalOcclusion ? "true" : "false")
                   << ",\"helpToggle\":" << (helpToggled ? "true" : "false")
                   << ",\"goalHighlightSuppressed\":" << (goalHighlightSuppressed ? "true" : "false")
                   << ",\"clearRequested\":" << (clearRequested ? "true" : "false")
                   << ",\"returnedToSelection\":" << (returned ? "true" : "false") << "}";
        }
        sceneFadePhase_ = SceneFadePhase::None;
        sceneManager_->ChangeScene(SceneType::GameClear, *this);
        InputTestPlayback::Advance(*input, 0, 0, {});
        InputTestPlayback::Advance(*input, 0, 0, {DIK_ESCAPE});
        UpdateGameClear(true);
        const bool titleReturn = pendingSceneType_ == SceneType::Title && sceneFadePhase_ == SceneFadePhase::FadingOut;
        report << "],\"allFourSelectable\":" << (navigation ? "true" : "false")
               << ",\"clearEscapeReturnsTitle\":" << (titleReturn ? "true" : "false")
               << ",\"success\":" << (all && navigation && titleReturn ? "true" : "false") << "}";
        report.close();
        PostQuitMessage(all && navigation && titleReturn ? 0 : 3);
    }
    if (CaptureEnvironment("CG2_ROTATION") == "1") {
        rotationTrial_ = true;
        blenderStageActive_ = false;
        fogTrial_ = false;
        buildTrial_ = false;
        const auto rotationCourse = CaptureEnvironment("CG2_ROTATION_COURSE");
        rotationResultPage_ = rotationCourse == "bridge" ? 5 : rotationCourse == "relay" ? 6 : 4;
        const std::string rotationFile = rotationCourse == "relay"    ? "rotationRelay.txt"
                                         : rotationCourse == "bridge" ? "rotationBridge.txt"
                                                                      : "rotationTrial.txt";
        stageMap_.LoadFromFile("Resources/Stages/" + rotationFile);
        if (stageMap_.GetRotationRegions().empty()) {
            stageMap_.AddRotationRegion(RotationIsland::legacy);
        }
        backupMap_ = stageMap_;
        stageRenderer_->BuildFromStageMap(stageMap_);
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
        blockInventory_.Initialize(0);
        blockInventoryUI_->ResetForStage();
        blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        blockPlacementController_.SetRotationIsland(true);
        bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        stageSelect_->Initialize(object3dCommon.get(), input.get(), 0);
        RefreshGoalGuideTarget();
        startIntroPending_ = !dxCommon->IsPerformanceCaptureActive();
        sceneManager_->ChangeScene(SceneType::GamePlay, *this);
        objectiveGuideSprite_->SetTexture(textureManager->LoadTexture(
            "Resources/UI/campaign/rotation-" +
            std::string(rotationCourse == "bridge" || rotationCourse == "relay" ? rotationCourse : "trial") + ".png"));
        objectiveGuideSprite_->SetPosition({24, 24});
        objectiveGuideSprite_->SetSize({1000, 110});
        tutorialSprite_->SetTexture(textureManager->LoadTexture("Resources/UI/campaign/rotation-minimal.png"));
        controllerTutorialSprite_->SetTexture(
            textureManager->LoadTexture("Resources/UI/campaign/rotation-minimal.png"));
        objectiveGuideActive_ = true;
        objectiveGuideStartedAt_ = std::chrono::steady_clock::now();
        if (CaptureEnvironment("CG2_ROTATION_COURSE_VERIFY") == "1") {
            startIntro_->Cancel(camera.get());
            startIntroPending_ = false;
            bool ok = true;
            for (const auto &r : stageMap_.GetRotationRegions()) {
                player_->SetRespawnPosition(
                    {static_cast<float>(r.control.x), 1.5f, static_cast<float>(r.control.z + 1)});
                player_->Respawn();
                InputTestPlayback::Advance(*input, 0, 0, {});
                for (int f = 0; f < 5; ++f) {
                    UpdateGamePlay();
                }
                const Int3 placed{r.x + r.size / 2, 3, r.z + r.size / 2 + 1};
                blockInventory_.AddBlock(BlockType::Wall, 1);
                blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
                blockPlacementController_.SetPlayerPosition(player_->GetPosition());
                ok = blockPlacementController_.TryPlace(placed) && ok;
                InputTestPlayback::Advance(*input, 0, 0, {DIK_F});
                UpdateGamePlay();
                const Int3 moved = RotationIsland::Turn(placed, false, r);
                ok = stageMap_.GetCell(moved)->type == BlockType::Wall && ok;
                ok = blockPlacementController_.TryUndo() && ok;
                InputTestPlayback::Advance(*input, 0, 0, {});
                UpdateGamePlay();
                InputTestPlayback::Advance(*input, 0, 0, {DIK_F, DIK_LSHIFT});
                UpdateGamePlay();
                ok = stageMap_.GetCell(placed)->type == BlockType::None && ok;
            }
            std::filesystem::create_directories("../generated/rotation-course-review");
            std::ofstream("../generated/rotation-course-review/" + rotationCourse + "-verification.json")
                << "{\"success\":" << (ok ? "true" : "false")
                << ",\"regions\":" << stageMap_.GetRotationRegions().size() << ",\"actualFInputAndRotatedUndo\":true}";
            PostQuitMessage(ok ? 0 : 3);
        }
        if (CaptureEnvironment("CG2_ROTATION_VERIFY") == "1") {
            std::filesystem::create_directories("../generated/cloud-flight-review");
            startIntro_->Cancel(camera.get());
            startIntroPending_ = false;
            player_->SetRespawnPosition({8, 1.5f, 12});
            player_->Respawn();
            blockInventory_.AddBlock(BlockType::Wall, 1);
            blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
            blockPlacementController_.SetPlayerPosition({8, 1.5f, 12});
            const bool placedInside = blockPlacementController_.TryPlace({15, 2, 14});
            const bool turned = RotationIsland::Apply(stageMap_, player_->GetPosition(), false);
            blockPlacementController_.RotateUndo(false);
            const auto moved = RotationIsland::Turn({15, 2, 14}, false);
            const bool partMoved = stageMap_.GetCell(moved)->type == BlockType::Wall;
            const bool undo = blockPlacementController_.TryUndo() && blockInventory_.GetBlockCount() == 1 &&
                              !blockPlacementController_.TryUndo();
            const bool fixed = blockPlacementController_.TryPlace({10, 1, 12});
            RotationIsland::Apply(stageMap_, player_->GetPosition(), true);
            blockPlacementController_.RotateUndo(true);
            const bool fixedKept = stageMap_.GetCell(10, 1, 12)->type == BlockType::Wall;
            const bool fixedUndo = blockPlacementController_.TryUndo() && blockInventory_.GetBlockCount() == 1;
            blockInventory_.AddBlock(BlockType::Wall, 1, 1);
            blockPlacementController_.SetPlaceCustomId(1);
            const bool boundaryRejected = !blockPlacementController_.Evaluate({10, 3, 15}).Ready();
            bubblePickupController_.Update({15, 1.5f, 12});
            RotationIsland::Apply(stageMap_, player_->GetPosition(), false);
            blockPlacementController_.RotateUndo(false);
            const bool collectedKept = bubblePickupController_.GetCollectedCount() == 1 &&
                                       stageMap_.GetCell(15, 2, 12)->type == BlockType::None;
            RotationIsland::Apply(stageMap_, player_->GetPosition(), true);
            blockPlacementController_.RotateUndo(true);
            InputTestPlayback::Advance(*input, 0, 0, {});
            for (int frame = 0; frame < 4; ++frame) {
                UpdateGamePlay();
            }
            InputTestPlayback::Advance(*input, 0, 0, {DIK_F});
            UpdateGamePlay();
            const bool keyTurns = stageMap_.GetCell(15, 1, 8)->type != BlockType::None;
            InputTestPlayback::Advance(*input, 0, 0, {});
            UpdateGamePlay();
            InputTestPlayback::Advance(*input, 0, 0, {DIK_F, DIK_LSHIFT});
            UpdateGamePlay();
            const bool keyRestores = stageMap_.GetCell(15, 1, 8)->type == BlockType::None;
            const bool ok = placedInside && turned && partMoved && undo && fixed && fixedKept && fixedUndo &&
                            boundaryRejected && collectedKept && keyTurns && keyRestores;
            std::ofstream("../generated/cloud-flight-review/rotation-verification.json")
                << "{\"success\":" << (ok ? "true" : "false")
                << ",\"placedInside\":" << (placedInside ? "true" : "false")
                << ",\"partMoved\":" << (partMoved ? "true" : "false") << ",\"undoOnce\":" << (undo ? "true" : "false")
                << ",\"fixedKept\":" << (fixedKept && fixedUndo ? "true" : "false")
                << ",\"boundaryRejected\":" << (boundaryRejected ? "true" : "false")
                << ",\"collectedKept\":" << (collectedKept ? "true" : "false")
                << ",\"keyTurns\":" << (keyTurns ? "true" : "false")
                << ",\"keyRestores\":" << (keyRestores ? "true" : "false") << "}";
            PostQuitMessage(ok ? 0 : 3);
        }
    }
    const auto campaignShot = CaptureEnvironment("CG2_CAMPAIGN_SHOT");
    if (!campaignShot.empty()) {
        std::filesystem::create_directories("../generated/campaign-review");
        comparisonActive_ = true;
        comparisonStarted_ = std::chrono::steady_clock::now();
        if (campaignShot == "select") {
            stageSelect_->Initialize(object3dCommon.get(), input.get(), 0);
            sceneManager_->ChangeScene(SceneType::StageSelect, *this);
        } else if (campaignShot == "clear") {
            sceneManager_->ChangeScene(SceneType::GameClear, *this);
        } else if (campaignShot == "rotation-editor" && rotationTrial_) {
            startIntro_->Cancel(camera.get());
            startIntroPending_ = false;
            sceneManager_->ChangeScene(SceneType::StageEditor, *this);
            camera->SetPosition({stageMap_.GetWidth() * .5f, 32, -22});
            camera->SetRotation({.65f, 0, 0});
            camera->Update();
        } else if (campaignShot == "rotation" && rotationTrial_) {
            startIntro_->Cancel(camera.get());
            startIntroPending_ = false;
            player_->Respawn();
            camera->SetPosition({36, 27, -14});
            camera->SetRotation({.60f, -.66f, 0});
            camera->SetFov(.94f);
            camera->Update();
        } else {
            const int index = std::clamp(std::atoi(campaignShot.c_str()), 0, 3);
            stageSelect_->Initialize(object3dCommon.get(), input.get(), index);
            InputTestPlayback::Advance(*input, 0, 0, {DIK_RETURN});
            UpdateStageSelect();
            sceneFadePhase_ = SceneFadePhase::None;
            sceneManager_->ChangeScene(SceneType::GamePlay, *this);
            startIntro_->Cancel(camera.get());
            startIntroPending_ = false;
            player_->Respawn();
            const float cx = stageMap_.GetWidth() * .5f, cz = stageMap_.GetDepth() * .5f;
            camera->SetPosition({cx + 30, 36, cz - 35});
            camera->SetRotation({.62f, -.71f, 0});
            camera->SetFov(.95f);
            camera->Update();
            if (campaignShot == "play" || campaignShot == "inventory") {
                gameplayCameraController_.ResetCamera(camera.get(), player_.get(), stageMap_, 7);
                if (campaignShot == "inventory") {
                    for (int slot = 1; slot <= 5; ++slot) {
                        blockInventory_.AddBlock(BlockType::Wall, 1, slot);
                    }
                    for (const auto type : {BlockType::Wall, BlockType::Ladder, BlockType::IceBlock,
                                            BlockType::MovingFloor, BlockType::CrumblingFloor}) {
                        blockInventory_.AddBlock(type, 2);
                    }
                    blockInventoryUI_->ToggleOpen();
                }
            }
            if (campaignShot == "particles") {
                const Vector3 p{goalGuideTarget_.x, goalGuideTarget_.y - .5f, goalGuideTarget_.z};
                player_->SetPosition(p);
                camera->SetPosition({p.x + 5, p.y + 3, p.z - 6});
                camera->SetRotation({.30f, -.6947f, 0});
                camera->SetFov(.75f);
                camera->Update();
            }
            if (campaignShot == "star") {
                for (int y = 0; y < stageMap_.GetHeight(); ++y) {
                    for (int z = 0; z < stageMap_.GetDepth(); ++z) {
                        for (int x = 0; x < stageMap_.GetWidth(); ++x) {
                            if (stageMap_.GetCell(x, y, z)->type == BlockType::Key) {
                                player_->SetPosition({static_cast<float>(x), y - .5f, static_cast<float>(z)});
                                player_->KeyUpdate(stageMap_);
                            }
                        }
                    }
                }
                stageRenderer_->BuildFromStageMap(stageMap_);
                stageMap_.ClearRebuildFlag();
                player_->SetPosition({goalGuideTarget_.x, goalGuideTarget_.y - .5f, goalGuideTarget_.z});
                isGoalReached_ = true;
                isGoalBlocked_ = false;
                const auto p = player_->GetPosition();
                camera->SetPosition({p.x + 5, p.y + 3, p.z - 6});
                camera->SetRotation({.30f, -.6947f, 0});
                camera->SetFov(.75f);
                camera->Update();
            }
        }
        sceneFadePhase_ = SceneFadePhase::None;
    }
    if ((dxCommon->IsPerformanceCaptureActive() && CaptureEnvironment("CG2_PERF_PLACEMENT") == "1") ||
        placementReview) {
        if (placementReview) {
            player_->SetPosition({15, 1.5f, 9});
            gameplayCameraController_.ResetCamera(camera.get(), player_.get(), stageMap_, 7);
        }
        blockInventory_.AddBlock(BlockType::Wall, 1, 3);
        blockInventoryUI_->SelectShortcut(3);
        blockInventoryUI_->ConsumeUseRequest();
        blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
        blockPlacementController_.SetPlaceCustomId(3);
        mapCursor_->SetIndex({17, 1, 9}, stageMap_);
        RequestSceneChange(SceneType::GamePlayBlockPlace);
    }
    // FPS計測では録画・位置スクリプトを使わず、通常のチュートリアルを起動する。
    if (dxCommon->IsPerformanceCaptureActive() && CaptureEnvironment("CG2_PERF_SCENE") == "tutorial") {
        blenderStageActive_ = false;
        player_->SetExternalCollisionBoxes(nullptr);
        fogTrial_ = false;
        buildTrial_ = false;
        stageMap_.LoadFromFile("Resources/Stages/tutorial.txt");
        backupMap_ = stageMap_;
        stageRenderer_->BuildFromStageMap(stageMap_);
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
        gameplayCameraController_.ResetCamera(camera.get(), player_.get(), stageMap_, 0);
        blockInventory_.Initialize(0);
        bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        RefreshGoalGuideTarget();
        startIntroPending_ = false;
        sceneManager_->ChangeScene(SceneType::GamePlay, *this);
    }
}

// 探索霧の比較に使うステージ・カメラ・記録状態を準備する。
void GameRuntime::BeginFogTrial() {
    if (startIntro_) {
        startIntro_->Cancel(camera.get());
    }
    sceneFadePhase_ = SceneFadePhase::None;
    sceneFadeAlpha_ = 0;
    startIntroPending_ = false;
    fogTrial_ = true;
    fogExplorer_.Reset();
    stageMap_.Initialize(48, 10, 64);
    stageMap_.SetClearColor({.48f, .64f, .76f, 1});
    stageMap_.SetLightColor({1, .96f, .90f});
    stageMap_.SetLightIntensity(.85f);
    stageMap_.SetWeatherPresetName("");
    // 広い出発点から細い道へ。左の分岐でシャボン玉を回収して奥の星へ進む。
    for (int z = 5; z <= 16; ++z) {
        for (int x = 14; x <= 30; ++x) {
            stageMap_.SetBlock(x, 0, z, BlockType::Ground);
        }
    }
    for (int z = 16; z <= 56; ++z) {
        for (int x = 19; x <= 25; ++x) {
            stageMap_.SetBlock(x, 0, z, BlockType::Ground);
        }
    }
    for (int z = 25; z <= 29; ++z) {
        for (int x = 8; x <= 19; ++x) {
            stageMap_.SetBlock(x, 0, z, BlockType::Ground);
        }
    }
    for (int z = 22; z <= 32; ++z) {
        for (int x = 5; x <= 11; ++x) {
            stageMap_.SetBlock(x, 0, z, BlockType::Ground);
        }
    }
    // 柱で距離の基準を作る。通路上は塞がず、スキャンで先の道が見える。
    for (int z = 18; z <= 54; z += 9) {
        for (int y = 1; y <= 3; ++y) {
            stageMap_.SetBlock(18, y, z, BlockType::Wall);
            stageMap_.SetBlock(26, y, z, BlockType::Wall);
        }
    }
    stageMap_.SetBlock(22, 1, 10, BlockType::PlayerStart);
    stageMap_.SetBlock(8, 1, 27, BlockType::BubblePickup);
    stageMap_.SetBlock(22, 1, 54, BlockType::Goal);
    backupMap_ = stageMap_;
    stageRenderer_->BuildFromStageMap(stageMap_);
    blenderStageActive_ = false;
    player_->SetExternalCollisionBoxes(nullptr);
    stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    player_->SetStageCollisionEnabled(true);
    blockInventory_.Initialize(0);
    bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    isGoalReached_ = isGoalBlocked_ = isGamePaused_ = false;
    useFirstPersonCamera_ = false;
    objectiveGuideActive_ = false;
    RefreshGoalGuideTarget();
    gameplayCameraController_.ConfigureExploration();
    sceneManager_->ChangeScene(SceneType::GamePlay, *this);
    weatherRuntimeController_.StopStorm(*particleManager);
    particleManager->SetWeatherEmitterActive(false);
    particleManager->SetAmbientCloudEmitterActive(false);
    particleManager->ClearParticles();
    fogUpdatedAt_ = std::chrono::steady_clock::now();
}

// 自動撮影の入力・時間・録画終了条件を進める。
void GameRuntime::UpdateCaptureTools() {
    const auto campaignShot = CaptureEnvironment("CG2_CAMPAIGN_SHOT");
    if (!campaignShot.empty()) {
        if (!comparisonRecordingStarted_) {
            FrameRecorder::Options options;
            options.name = "campaign-" + campaignShot;
            options.gifSeconds = (campaignShot == "star" || campaignShot == "particles") ? 12 : 8;
            const auto output = CaptureEnvironment("CG2_CAPTURE_DIR");
            options.directory = output.empty() ? "../generated/campaign-review" : output;
            if (!dxCommon->Recorder().Start(options)) {
                PostQuitMessage(2);
                return;
            }
            comparisonRecordingStarted_ = true;
            // シーンや録画の準備時間を含めず、演出を撮る時間を確保する。
            comparisonStarted_ = std::chrono::steady_clock::now();
        }
        const float elapsed =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - comparisonStarted_).count();
        // 同じ画角で8種類を順に再生し、形と軌跡を比較するための確認用撮影。
        if (campaignShot == "particles" && particleManager && player_) {
            static int previous = -1;
            const int phase = static_cast<int>(elapsed / 1.3f);
            if (phase != previous && phase < 8) {
                const auto p = player_->GetPosition();
                const Vector3 source{p.x + (phase >= 3 && phase <= 5 ? 1.3f : 0), p.y + (phase >= 6 ? 1.3f : .03f),
                                     p.z};
                particleManager->EmitAdventureEffect(static_cast<ParticleManager::AdventureEffect>(phase + 1), source,
                                                     {p.x, p.y + .8f, p.z});
                previous = phase;
            }
        }
        // 表紙の確認用撮影では、打刻後にTabで閉じ、再び開く実際の入力を送る。
        if (campaignShot == "clear" && CaptureEnvironment("CG2_JOURNAL_INSPECT") == "1") {
            static int lastPhase = 0;
            const int phase = elapsed < 4 ? 0 : elapsed < 6 ? 1 : 2;
            InputTestPlayback::Advance(*input, 0, 0,
                                       phase != lastPhase ? std::initializer_list<BYTE>{DIK_TAB}
                                                          : std::initializer_list<BYTE>{});
            lastPhase = phase;
        }
        if (campaignShot == "rotation" && rotationTrial_) {
            static int quarter = 0;
            const int target = elapsed < 3 ? 0 : elapsed < 6 ? 1 : 2;
            if (target != quarter) {
                const bool reverse = target == 2;
                if (RotationIsland::Apply(stageMap_, player_->GetPosition(), reverse)) {
                    blockPlacementController_.RotateUndo(reverse);
                    stageRenderer_->BuildFromStageMap(stageMap_);
                    stageMap_.ClearRebuildFlag();
                    quarter = target;
                }
            }
        }
        if (elapsed > ((campaignShot == "star" || campaignShot == "particles") ? 12.0f : 8.0f)) {
            if (dxCommon->Recorder().IsRecording()) {
                dxCommon->Recorder().Stop();
            }
            if (!dxCommon->Recorder().IsBusy()) {
                PostQuitMessage(0);
            }
        }
        return;
    }

    if (dxCommon->IsPerformanceCaptureActive() && CaptureEnvironment("CG2_PERF_PLACEMENT") == "1") {
        InputTestPlayback::Advance(*input, 0, 0, {});
        InputTestPlayback::Mouse(*input, 0, 0);
        if (CaptureEnvironment("CG2_PERF_PLACEMENT_MOVE") == "1") {
            static int frame = 0;
            mapCursor_->SetIndex({17 + (frame++ / 5) % 3, 1, 9}, stageMap_);
        }
    }
    const auto cinematic = CaptureEnvironment("CG2_CINEMATIC_SHOT");
    if (!cinematic.empty()) {
        static bool triggered = false;
        static bool framingOk = true;
        static int trackedFrames = 0;
        static float maxNdcX = 0, maxNdcY = 0;
        if (cinematic == "arrival_transition" && startIntro_ && startIntro_->BlocksInput() &&
            startIntro_->IsPlayerVisible()) {
            const auto p = player_->GetPosition(), c = camera->GetPosition(), r = camera->GetRotation();
            const float x = p.x - c.x, y = p.y + .8f - c.y, z = p.z - c.z;
            const float depth =
                x * std::sin(r.y) * std::cos(r.x) - y * std::sin(r.x) + z * std::cos(r.y) * std::cos(r.x);
            const float nx =
                (x * std::cos(r.y) - z * std::sin(r.y)) / (depth * std::tan(camera->GetFov() / 2) * (1280.0f / 720));
            const float ny =
                (x * std::sin(r.y) * std::sin(r.x) + y * std::cos(r.x) + z * std::cos(r.y) * std::sin(r.x)) /
                (depth * std::tan(camera->GetFov() / 2));
            maxNdcX = (std::max)(maxNdcX, std::abs(nx));
            maxNdcY = (std::max)(maxNdcY, std::abs(ny));
            framingOk = framingOk && depth > 0 && std::abs(nx) < .56f && std::abs(ny) < .56f;
            ++trackedFrames;
        }
        static std::ofstream trace("../generated/cinematic-review/trace-" + cinematic + ".csv");
        const auto cp = camera->GetPosition(), pp = player_->GetPosition();
        const auto cr = camera->GetRotation();
        trace << pp.x << "," << pp.y << "," << pp.z << "," << cp.x << "," << cp.y << "," << cp.z << "," << cr.x << ","
              << cr.y << "," << camera->GetFov() << "\n";
        const float elapsed =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - comparisonStarted_).count();
        if (cinematic == "headlamp") {
            player_->SetRotation({0, 3.14159265f + std::sin(elapsed * 0.8f) * 0.65f, 0});
        }
        if (cinematic == "occlusion" && elapsed > 4.0f) {
            static bool movedToClear = false;
            if (!movedToClear) {
                player_->SetPosition({1.0f, 0.5f, 1.0f});
                gameplayCameraController_.ResetCamera(camera.get(), player_.get(), stageMap_, 0);
                movedToClear = true;
            }
        }
        if (cinematic == "placement") {
            InputTestPlayback::Advance(*input, 0, 0, {});
            InputTestPlayback::Mouse(*input, 0, 0);
            mapCursor_->SetIndex({elapsed < 4.0f ? 17 : 15, 1, 9}, stageMap_);
            gameplayCameraController_.SetAngle(5.55f + std::sin(elapsed * .7f) * .25f);
        }
        if (!comparisonRecordingStarted_) {
            FrameRecorder::Options options;
            options.name = "cinematic-" + cinematic;
            options.gifSeconds = 10;
            const auto output = CaptureEnvironment("CG2_CAPTURE_DIR");
            if (!output.empty()) {
                options.directory = output;
            }
            if (!dxCommon->Recorder().Start(options)) {
                PostQuitMessage(2);
                return;
            }
            comparisonRecordingStarted_ = true;
        }
        if (cinematic != "arrival" && cinematic != "arrival_transition" && cinematic != "title" &&
            cinematic != "headlamp" && cinematic != "occlusion" && cinematic != "expedition" &&
            cinematic != "placement" && elapsed >= 1 && !triggered) {
            BeginGameOver(cinematic == "fall" || cinematic == "retry");
            triggered = true;
        }
        // 再挑戦撮影は通常の決定入力を送り、暗転・リスポーン・開幕までを通す。
        if (cinematic == "retry") {
            InputTestPlayback::Advance(
                *input, 0, 0, elapsed > 5.0f ? std::initializer_list<BYTE>{DIK_RETURN} : std::initializer_list<BYTE>{});
        }
        if (elapsed >= (cinematic == "arrival_transition" ? 10.0f : 8.0f)) {
            if (dxCommon->Recorder().IsRecording()) {
                dxCommon->Recorder().Stop();
            }
            if (!dxCommon->Recorder().IsBusy()) {
                bool ok = true;
                if (cinematic == "retry") {
                    ok = gameOver_ && !gameOver_->IsActive() && player_ && player_->IsGrounded();
                    std::ofstream("../generated/cinematic-review/retry.json")
                        << "{\"success\":" << (ok ? "true" : "false") << "}";
                }
                if (cinematic == "arrival_transition") {
                    const auto p = player_->GetPosition(), spawn = startIntro_->GetSpawnPosition();
                    const bool landed = std::abs(p.x - spawn.x) < .01f && std::abs(p.y - spawn.y) < .01f &&
                                        std::abs(p.z - spawn.z) < .01f;
                    ok = framingOk && trackedFrames > 60 && landed;
                    std::ofstream("../generated/cinematic-review/arrival-transition-check.json")
                        << "{\"success\":" << (ok ? "true" : "false")
                        << ",\"outgoingCameraPerturbed\":true,\"trackedFrames\":" << trackedFrames
                        << ",\"maxNdcX\":" << maxNdcX << ",\"maxNdcY\":" << maxNdcY
                        << ",\"landedAtSpawn\":" << (landed ? "true" : "false") << ",\"spawnY\":" << spawn.y << "}";
                }
                PostQuitMessage(ok ? 0 : 3);
            }
        }
        return;
    }
    UpdateDioramaCapture();
    auto &recorder = dxCommon->Recorder();
    const bool canUndo = (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) &&
                         !(gameOver_ && gameOver_->IsActive()) && !(startIntro_ && startIntro_->BlocksInput()) &&
                         !isGamePaused_ && !isGoalReached_ && (!blockInventoryUI_ || !blockInventoryUI_->IsActive()) &&
                         !comparisonActive_;
    if (canUndo && player_) {
        blockPlacementController_.SetPlayerPosition(player_->GetPosition());
        if (input->TriggerKey(DIK_Z) || input->TriggerControllerButton(XINPUT_GAMEPAD_BACK)) {
            const bool restored = blockPlacementController_.TryUndo();
            buildStatus_ = restored ? 4 : PlacementStatus(blockPlacementController_.LastReason());
            placementFeedbackUntil_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        }
    }
    if (input->TriggerKey(DIK_F8)) {
        if (recorder.IsRecording()) {
            recorder.Stop();
        } else if (!recorder.IsBusy()) {
            recorder.Start(FrameRecorder::Options{});
        }
    }
    if (input->TriggerKey(DIK_F9)) {
        showRecorder_ = !showRecorder_;
    }
    if (input->TriggerKey(DIK_F7) && !comparisonActive_) {
        BeginBuildTrial();
    }
    if (input->TriggerKey(DIK_F6) && !comparisonActive_) {
        buildTrial_ = false;
        BeginFogTrial();
    }
    if (fogTrial_ && !comparisonActive_ && input->TriggerKey(DIK_F4)) {
        fogExplorer_.SetMode((fogExplorer_.Mode() + 1) % 3);
    }
    if (showRecorder_) {
        recorder.DrawImGui();
    }
    // 実際のプレイヤー物理・回収・ゴール制御を通す統合確認。比較動画とは分離する。
    if (trialVerification_) {
        ++verifyFrame_;
        if (verifyFrame_ == 60) {
            verifyGround_ = player_->IsGrounded() && std::abs(player_->GetPosition().y - .5f) < .15f;
            fogExplorer_.TryScan();
        }
        if (verifyFrame_ == 90) {
            verifyScan_ = fogExplorer_.Strength() > .8f;
            isGamePaused_ = true;
            verifyCooldown_ = fogExplorer_.Cooldown();
        }
        if (verifyFrame_ == 110) {
            verifyPause_ = std::abs(verifyCooldown_ - fogExplorer_.Cooldown()) < .001f;
            isGamePaused_ = false;
        }
        if (verifyFrame_ == 150) {
            player_->SetPosition({22, .5f, 54});
        }
        if (verifyFrame_ == 152) {
            verifyLock_ = isGoalBlocked_ && !isGoalReached_;
            player_->SetPosition({22, .5f, 10});
        }
        if (verifyFrame_ == 180) {
            player_->SetPosition({8, .5f, 27});
        }
        if (verifyFrame_ == 182) {
            verifyPickup_ = bubblePickupController_.AreAllPickupsCollected();
        }
        if (verifyFrame_ == 210) {
            player_->SetPosition({22, .5f, 54});
        }
        if (verifyFrame_ == 212) {
            std::filesystem::create_directories("../generated/PR1_01");
            std::ofstream report("../generated/PR1_01/gameplay-verification.json");
            report << std::boolalpha << "{\"grounded\":" << verifyGround_ << ",\"scan\":" << verifyScan_
                   << ",\"pauseFreezesCooldown\":" << verifyPause_ << ",\"goalLockedBeforePickup\":" << verifyLock_
                   << ",\"pickupCollected\":" << verifyPickup_ << ",\"goalReachedAfterPickup\":" << isGoalReached_
                   << "}";
            PostQuitMessage(
                verifyGround_ && verifyScan_ && verifyPause_ && verifyLock_ && verifyPickup_ && isGoalReached_ ? 0 : 3);
        }
    }
#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (fogTrial_ && showRecorder_) {
        ImGui::Begin("Exploration Fog");
        ImGui::Text("F4: method %d / G: scan / F6: reset trial", fogExplorer_.Mode());
        ImGui::Text("Scan cooldown %.1fs / strength %.2f", fogExplorer_.Cooldown(), fogExplorer_.Strength());
        ImGui::End();
    }
#endif
}

// プレイヤーの移動に合わせて、探索済みの範囲を霧へ反映する。
void GameRuntime::UpdateExplorationFog() {
    if (buildTrial_) {
        UpdateBuildTrial();
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - fogUpdatedAt_).count();
    fogUpdatedAt_ = now;
    const float captureDuration = CaptureEnvironment("CG2_SHOT") == "route" ? 12.0f : 10.0f;
    // クリア画面へ遷移しても撮影の終了処理を止めない。
    if (comparisonActive_ && comparisonRecordingStarted_ &&
        std::chrono::duration<float>(now - comparisonStarted_).count() >= captureDuration + 1.5f) {
        if (dxCommon->Recorder().IsRecording()) {
            dxCommon->Recorder().Stop();
        }
        if (!dxCommon->Recorder().IsBusy()) {
            PostQuitMessage(0);
        }
    }
    const bool active =
        fogTrial_ && (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);
    if (!active) {
        object3dCommon->SetExplorationFog({0, 0, 0, 0}, {}, {}, {});
        return;
    }
    if (comparisonActive_) {
        // 実入力の録画とは別の再現撮影。カメラ・位置・時刻は全方式で同一。
        const float elapsed = std::chrono::duration<float>(now - comparisonStarted_).count();
        const float t = (std::max)(0.0f, elapsed - 1.5f);
        const auto shot = CaptureEnvironment("CG2_SHOT");
        float yaw = -.8f * std::sin(t * .7f), distance = 20, height = 14, pitch = .55f;
        Vector3 p{22, .5f, 10 + std::clamp(t, 0.0f, 8.0f) * 1.4f};
        if (shot == "footpath") {
            p = {22, .5f, 14 + std::clamp(t, 0.0f, 8.0f) * 1.5f};
            yaw = .25f;
            distance = 9;
            height = 6;
            pitch = .56f;
        }
        if (shot == "branch") {
            p = {22, .5f, 27};
            yaw = -.8f * std::sin(t * .65f);
            distance = 15;
            height = 10;
            pitch = .56f;
        }
        if (shot == "character") {
            p = {22, .5f, 10};
            yaw = t * .63f;
            distance = 3.8f;
            height = 1.65f;
            pitch = .22f;
            player_->SetRotation({0, 3.14159265f, 0});
        }
        if (shot == "route") {
            // 未回収の星、分岐の回収アイテム、回収後の星を順に確認する再現配置。
            p = t < 3 ? Vector3{22, .5f, 54} : t < 6 ? Vector3{8, .5f, 27} : Vector3{22, .5f, 54};
            yaw = .25f;
            distance = 12;
            height = 8;
            pitch = .54f;
        }
        player_->SetPosition(p);
        camera->SetPosition({p.x - std::sin(yaw) * distance, p.y + height, p.z - std::cos(yaw) * distance});
        camera->SetRotation({pitch, yaw, 0});
        camera->SetFov(.9f);
        camera->Update();
        if (elapsed >= 1.5f && !comparisonRecordingStarted_) {
            FrameRecorder::Options options;
            options.name = comparisonMode_ == 0 ? "baseline" : comparisonMode_ == 1 ? "bubble" : "scan";
            if (!shot.empty()) {
                options.name = shot + "-" + options.name;
            }
            const auto output = CaptureEnvironment("CG2_CAPTURE_DIR");
            if (!output.empty()) {
                options.directory = output;
            }
            options.gifSeconds = static_cast<int>(captureDuration);
            if (!dxCommon->Recorder().Start(options)) {
                PostQuitMessage(2);
                return;
            }
            comparisonRecordingStarted_ = true;
        }
        if (t >= 1.2f && !comparisonScanned_ && shot != "character") {
            fogExplorer_.TryScan();
            comparisonScanned_ = true;
        }
        if (elapsed >= captureDuration + 1.5f && comparisonRecordingStarted_ && dxCommon->Recorder().IsRecording()) {
            dxCommon->Recorder().Stop();
        }
        if (elapsed >= captureDuration + 1.5f && comparisonRecordingStarted_ && !dxCommon->Recorder().IsBusy()) {
            PostQuitMessage(0);
        }
    }
    const bool paused = (gameOver_ && gameOver_->IsActive()) || (startIntro_ && startIntro_->BlocksInput()) ||
                        isGamePaused_ || (blockInventoryUI_ && blockInventoryUI_->IsActive()) ||
                        sceneFadePhase_ != SceneFadePhase::None || isGoalReached_ ||
                        (!comparisonActive_ && IsWindowInactive());
    if (!paused && input->TriggerKey(DIK_G)) {
        fogExplorer_.TryScan();
    }
    fogExplorer_.Update(dt, player_->GetPosition(), camera->GetRotation().y, paused);
    const auto p = fogExplorer_.Origin(), d = fogExplorer_.Direction();
    object3dCommon->SetExplorationFog({1, 4, 28, .94f}, {p.x, p.y, p.z, static_cast<float>(fogExplorer_.Mode())},
                                      {d.x, 0, d.z, 28}, {fogExplorer_.Strength(), fogExplorer_.Radius(), 0, 0});
    scanMeter_->SetSize({280 * (1 - fogExplorer_.Cooldown() / 6), 7});
    scanMeter_->Update();
}

// 録画や比較に必要な案内・計測表示を画面へ重ねる。
void GameRuntime::DrawCaptureOverlay() {
    if (dioramaCapture_) {
        return;
    }
    spriteCommon->PreDraw();
    if (buildTrial_ && (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        buildLabels_[buildStatus_]->Draw();
        buildCounts_[blockInventory_.GetBlockCount(BlockType::Wall, 1) > 0 ? 1 : 0]->Draw();
        return;
    }
    const auto shot = CaptureEnvironment("CG2_SHOT");
    if (fogTrial_ && currentMode_ == AppMode::GamePlay && shot != "character" && shot != "route") {
        fogLabels_[fogExplorer_.Mode()]->Draw();
        scanMeter_->Draw();
    }
    if (dxCommon->Recorder().IsRecording()) {
        fogLabels_[4]->Draw();
    }
}

// 箱庭表現の検証ステージとカメラを準備する。
void GameRuntime::BeginDiorama() {
    BeginFogTrial();
    dioramaCapture_ = CaptureEnvironment("CG2_DIORAMA_CAPTURE") == "1";
    fogTrial_ = false;
    buildTrial_ = false;
    stageMap_.LoadFromFile(CaptureEnvironment("CG2_DIORAMA_SHOT") == "complexroute"
                               ? "Resources/Stages/complexRoute.txt"
                           : CaptureEnvironment("CG2_DIORAMA_SHOT") == "complex" ? "Resources/Stages/complexParts.txt"
                                                                                 : "Resources/Stages/dioramaRuins.txt");
    if (CaptureEnvironment("CG2_DIORAMA_SHOT") == "snow") {
        stageMap_.SetWeatherPresetName("Snowy");
    }
    backupMap_ = stageMap_;
    stageRenderer_->BuildFromStageMap(stageMap_);
    stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    blockInventory_.Initialize(0);
    bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    blockInventoryUI_->SetSelectedBlockType(BlockType::Wall);
    blockInventoryUI_->SetSelectedCustomId(1);
    gameplayCameraController_.ConfigureDiorama();
    RefreshGoalGuideTarget();
    object3dCommon->SetExplorationFog({0, 0, 0, 0}, {}, {}, {});
    if (CaptureEnvironment("CG2_DIORAMA_SHOT") == "complex" && dioramaCapture_) {
        complexChecksPassed_ = VerifyComplexParts();
    }
    comparisonStarted_ = std::chrono::steady_clock::now();
}

// 箱庭表現の撮影を進め、所定時間で録画を終了する。
void GameRuntime::UpdateDioramaCapture() {
    if (CaptureEnvironment("CG2_DIORAMA") != "1" || CaptureEnvironment("CG2_DIORAMA_CAPTURE") != "1") {
        return;
    }
    const auto detail = CaptureEnvironment("CG2_DIORAMA_SHOT");
    if (detail == "complexroute") {
        UpdateComplexRouteCapture();
        return;
    }
    if (detail == "complex") {
        UpdateComplexCapture();
        return;
    }
    if (detail == "overview" || detail == "rotation" || detail == "undo" || detail == "snow") {
        UpdateDioramaDetail(detail);
        return;
    }
    // Reproducible integration footage: positions are scripted, game rules are real.
    static int step = 0;
    static bool first = false, undo = false, second = false, lock = false, earlyClear = false;
    const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - comparisonStarted_).count();
    const float t = (std::max)(0.0f, elapsed - 1.5f);
    if (elapsed >= 1.5f && !comparisonRecordingStarted_) {
        FrameRecorder::Options o;
        o.name = "diorama-ruins";
        o.gifSeconds = 15;
        const auto output = CaptureEnvironment("CG2_CAPTURE_DIR");
        if (!output.empty()) {
            o.directory = output;
        }
        if (!dxCommon->Recorder().Start(o)) {
            PostQuitMessage(2);
            return;
        }
        comparisonRecordingStarted_ = true;
        lock = !bubblePickupController_.AreAllPickupsCollected();
    }
    if (!isGoalReached_ && (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        Vector3 p{4, 2.5f, 3};
        if (t >= 1 && t < 2) {
            p.z = 3 + (t - 1) * 2;
        } else if (t >= 2 && t < 5) {
            p.z = 5;
        } else if (t >= 5 && t < 7) {
            p.z = 5 + (t - 5) * 3.5f;
        } else if (t >= 7 && t < 8) {
            p = {4 + (t - 7), 2.5f, 12};
        } else if (t >= 8 && t < 9) {
            p = {5, 2.5f, 12 - (std::min)(1.0f, (t - 8) * 2)};
        } else if (t >= 9 && t < 10) {
            p = {5 + (t - 9) * 6, 2.5f, 11};
        } else if (t >= 10 && t < 10.5f) {
            p = {11, 2.5f, 11 + (t - 10) * 2};
        } else if (t >= 10.5f && t < 12) {
            p = {11, 2.5f + (t - 10.5f) * 2, 12};
        } else if (t >= 12 && t < 13) {
            p = {11 + (t - 12), 5.5f, 12};
        } else if (t >= 13) {
            p = {12 + (std::min)(1.0f, t - 13), 5.5f, 12 + (std::min)(1.0f, t - 13)};
        }
        player_->SetPosition(p);
        bubblePickupController_.Update(p);
        blockPlacementController_.SetPlayerPosition(p);
    }
    blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
    blockPlacementController_.SetPlaceCustomId(1);
    if (t >= 2 && t < 3) {
        const auto plan = blockPlacementController_.Evaluate({3, 2, 7});
        stageRenderer_->SetPlacementPreview(stageMap_, {3, 2, 7}, BlockType::Wall, 1, 0, plan.Ready() ? 1 : 2);
    }
    if (t >= 3 && step == 0) {
        first = blockPlacementController_.TryPlace({3, 2, 7});
        stageRenderer_->ClearPlacementPreview();
        ++step;
    }
    if (t >= 3.5f && step == 1) {
        undo = blockPlacementController_.TryUndo();
        ++step;
    }
    if (t >= 4 && step == 2) {
        first = blockPlacementController_.TryPlace({3, 2, 7}) && first;
        ++step;
    }
    if (t >= 8 && t < 9) {
        const auto plan = blockPlacementController_.Evaluate({7, 2, 10}, 1.5707963f);
        stageRenderer_->SetPlacementPreview(stageMap_, {7, 2, 10}, BlockType::Wall, 1, 1.5707963f,
                                            plan.Ready() ? 1 : 2);
    }
    if (t >= 9 && step == 3) {
        second = blockPlacementController_.TryPlace({7, 2, 10}, 1.5707963f);
        stageRenderer_->ClearPlacementPreview();
        ++step;
    }
    if (isGoalReached_ && (t < 13 || player_->IsOnLadder() || player_->GetPosition().y < 5.45f)) {
        earlyClear = true;
    }
    if (t >= 15 && comparisonRecordingStarted_) {
        if (dxCommon->Recorder().IsRecording()) {
            dxCommon->Recorder().Stop();
        }
        if (!dxCommon->Recorder().IsBusy()) {
            std::filesystem::create_directories("../generated/PR1_01/diorama");
            std::ofstream("../generated/PR1_01/diorama/verification.json")
                << "{\"goalInitiallyLocked\":" << (lock ? "true" : "false")
                << ",\"firstBridge\":" << (first ? "true" : "false") << ",\"undo\":" << (undo ? "true" : "false")
                << ",\"rotatedSecondBridge\":" << (second ? "true" : "false")
                << ",\"allPickups\":" << (bubblePickupController_.AreAllPickupsCollected() ? "true" : "false")
                << ",\"goalReached\":" << (isGoalReached_ ? "true" : "false")
                << ",\"noPrematureClear\":" << (!earlyClear ? "true" : "false") << "}";
            PostQuitMessage(first && undo && second && lock && isGoalReached_ && !earlyClear ? 0 : 3);
        }
    }
}

// 箱庭撮影で確認するパーツ・カメラ・演出を更新する。
void GameRuntime::UpdateDioramaDetail(const std::string &shot) {
    static bool setup = false, placed = false, undone = false, replaced = false;
    static int step = 0;
    const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - comparisonStarted_).count();
    const float t = (std::max)(0.0f, elapsed - 1.5f);
    const bool rotation = shot == "rotation", overview = shot == "overview" || shot == "snow";
    if (!setup) {
        bubblePickupController_.Update({4, 2.5f, 5});
        if (rotation || overview) {
            bubblePickupController_.Update({5, 2.5f, 12});
        }
        blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
        blockPlacementController_.SetPlaceCustomId(1);
        blockPlacementController_.SetPlayerPosition({4, 2.5f, 5});
        if (overview) {
            placed = blockPlacementController_.TryPlace({3, 2, 7});
            replaced = blockPlacementController_.TryPlace({7, 2, 10}, 1.5707963f);
        }
        setup = true;
    }
    Vector3 p = rotation ? Vector3{5, 2.5f, 11} : Vector3{4, 2.5f, 5};
    if (overview) {
        p = {4, 2.5f, 3};
    } else if (rotation && t >= 7) {
        p.x = 5 + std::clamp((t - 7) / 2, 0.0f, 1.0f) * 6;
    } else if (!rotation && t >= 8) {
        p.z = 5 + std::clamp((t - 8) / 2, 0.0f, 1.0f) * 6;
    }
    player_->SetPosition(p);
    blockPlacementController_.SetPlayerPosition(p);
    if (overview) {
        gameplayCameraController_.ConfigureDioramaShot({7.5f, 2.5f, 8}, -.85f + t * .13f, .72f, 24);
    } else if (rotation) {
        gameplayCameraController_.ConfigureDioramaShot({8, 2.5f, 11}, .7f, .8f, 14);
    } else {
        gameplayCameraController_.ConfigureDioramaShot({4, 2.0f, 8}, -.12f, .95f, 15);
    }
    if (elapsed >= 1.5f && !comparisonRecordingStarted_) {
        FrameRecorder::Options o;
        o.name = "diorama-" + shot;
        o.gifSeconds = 10;
        const auto output = CaptureEnvironment("CG2_CAPTURE_DIR");
        if (!output.empty()) {
            o.directory = output;
        }
        if (!dxCommon->Recorder().Start(o)) {
            PostQuitMessage(2);
            return;
        }
        comparisonRecordingStarted_ = true;
    }
    if (!overview && rotation) {
        if (t < 6) {
            const Int3 origin = t < 2 ? Int3{4, 2, 10} : Int3{7, 2, 10};
            const float angle = t < 4 ? 0 : 1.5707963f;
            const auto plan = blockPlacementController_.Evaluate(origin, angle);
            stageRenderer_->SetPlacementPreview(stageMap_, origin, BlockType::Wall, 1, angle, plan.Ready() ? 1 : 2);
        }
        if (t >= 6 && step == 0) {
            placed = blockPlacementController_.TryPlace({7, 2, 10}, 1.5707963f);
            stageRenderer_->ClearPlacementPreview();
            ++step;
        }
    } else if (!overview) {
        if (t < 2 || (t >= 4 && t < 7)) {
            const float angle = t >= 5 && t < 6 ? 1.5707963f : 0;
            const auto plan = blockPlacementController_.Evaluate({3, 2, 7}, angle);
            stageRenderer_->SetPlacementPreview(stageMap_, {3, 2, 7}, BlockType::Wall, 1, angle, plan.Ready() ? 1 : 2);
        }
        if (t >= 2 && step == 0) {
            placed = blockPlacementController_.TryPlace({3, 2, 7});
            stageRenderer_->ClearPlacementPreview();
            ++step;
        }
        if (t >= 4 && step == 1) {
            undone = blockPlacementController_.TryUndo();
            ++step;
        }
        if (t >= 7 && step == 2) {
            replaced = blockPlacementController_.TryPlace({3, 2, 7});
            stageRenderer_->ClearPlacementPreview();
            ++step;
        }
    }
    if (t >= 10 && comparisonRecordingStarted_) {
        if (dxCommon->Recorder().IsRecording()) {
            dxCommon->Recorder().Stop();
        }
        if (!dxCommon->Recorder().IsBusy()) {
            std::filesystem::create_directories("../generated/PR1_01/diorama");
            const bool ok = placed && (rotation || (overview ? replaced : undone && replaced));
            std::ofstream("../generated/PR1_01/diorama/" + shot + ".json")
                << "{\"success\":" << (ok ? "true" : "false")
                << ",\"stock\":" << blockInventory_.GetBlockCount(BlockType::Wall, 1) << ",\"scriptedPositions\":true}";
            PostQuitMessage(ok ? 0 : 3);
        }
    }
}

// 複雑なパーツの回収・配置・保存などの検証結果を集計する。
bool GameRuntime::VerifyComplexParts() {
    std::filesystem::create_directories("../generated/PR1_01/diorama");
    std::ofstream report("../generated/PR1_01/diorama/complex-checks.json");
    report << "{\"cases\":[";
    bool all = true;
    int cases = 0, checks = 0;
    for (int slot = 2; slot <= 4; ++slot) {
        for (int turn = 0; turn < 4; ++turn) {
            StageMap map;
            map.LoadFromFile("Resources/Stages/complexParts.txt");
            BlockInventory stock;
            stock.Initialize(0);
            stock.AddBlock(BlockType::Wall, 1, slot);
            BlockPlacementController controller;
            controller.Initialize(&map, stageRenderer_.get(), &stock);
            controller.SetPlaceBlockType(BlockType::Wall);
            controller.SetPlaceCustomId(slot);
            controller.SetPlayerPosition({0, 0, 0});
            auto snapshot = [&]() {
                std::vector<std::pair<int, int>> state;
                for (int y = 0; y < map.GetHeight(); ++y) {
                    for (int z = 0; z < map.GetDepth(); ++z) {
                        for (int x = 0; x < map.GetWidth(); ++x) {
                            const auto *c = map.GetCell(x, y, z);
                            state.push_back({static_cast<int>(c->type), c->variant});
                        }
                    }
                }
                return state;
            };
            const auto baseline = snapshot();
            auto expected = baseline;
            const auto *part = map.GetCustomPart(slot);
            int count = 0;
            Int3 first{}, last{};
            // Independent fixture walk: preserve height and block kind while rotating
            // each local coordinate one quarter turn at a time.
            for (int y = 0; y < 3; ++y) {
                for (int z = 0; z < 3; ++z) {
                    for (int x = 0; x < 3; ++x) {
                        const auto type = part->cells[y][z][x].type;
                        if (type == BlockType::None) {
                            continue;
                        }
                        int rx = x, rz = z;
                        for (int k = 0; k < turn; ++k) {
                            const int next = 2 - rz;
                            rz = rx;
                            rx = next;
                        }
                        const Int3 at{6 + rx, 3 + y, 6 + rz};
                        if (count == 0) {
                            first = at;
                        }
                        last = at;
                        ++count;
                        expected[(at.y * map.GetDepth() + at.z) * map.GetWidth() + at.x] = {static_cast<int>(type),
                                                                                            slot};
                    }
                }
            }
            const float angle = turn * 1.5707963f;
            const bool exact = controller.TryPlace({6, 3, 6}, angle) && snapshot() == expected;
            const bool one = stock.GetBlockCount(BlockType::Wall, slot) == 0;
            const bool undo =
                controller.TryUndo() && snapshot() == baseline && stock.GetBlockCount(BlockType::Wall, slot) == 1;
            const bool duplicate = !controller.TryUndo() && stock.GetBlockCount(BlockType::Wall, slot) == 1;
            map.SetBlock(last, BlockType::Ground);
            const auto blockedBefore = snapshot();
            const bool blocked = !controller.TryPlace({6, 3, 6}, angle) && snapshot() == blockedBefore &&
                                 stock.GetBlockCount(BlockType::Wall, slot) == 1;
            map.RemoveBlock(last);
            const bool outside = !controller.TryPlace({17, 9, 14}, angle) && snapshot() == baseline &&
                                 stock.GetBlockCount(BlockType::Wall, slot) == 1;
            controller.SetPlayerPosition({static_cast<float>(first.x), first.y - .5f, static_cast<float>(first.z)});
            const bool body = !controller.TryPlace({6, 3, 6}, angle) && snapshot() == baseline &&
                              stock.GetBlockCount(BlockType::Wall, slot) == 1;
            const bool passed = exact && one && undo && duplicate && blocked && outside && body;
            all = all && passed;
            checks += 7;
            if (cases++) {
                report << ",";
            }
            report << "{\"part\":" << slot << ",\"turn\":" << turn << ",\"cells\":" << count
                   << ",\"exactShapeAndEmptySpaces\":" << (exact ? "true" : "false")
                   << ",\"oneStockConsumed\":" << (one ? "true" : "false")
                   << ",\"undoAllAndRefundOne\":" << (undo ? "true" : "false")
                   << ",\"noDoubleRefund\":" << (duplicate ? "true" : "false")
                   << ",\"overlapKeepsWorldAndStock\":" << (blocked ? "true" : "false")
                   << ",\"outsideKeepsWorldAndStock\":" << (outside ? "true" : "false")
                   << ",\"playerOverlapKeepsWorldAndStock\":" << (body ? "true" : "false") << "}";
        }
    }
    report << "],\"caseCount\":" << cases << ",\"checkCount\":" << checks << ",\"success\":" << (all ? "true" : "false")
           << "}";
    stageRenderer_->BuildFromStageMap(stageMap_);
    return all;
}

// 複雑なパーツを見せる自動操作と撮影を進める。
void GameRuntime::UpdateComplexCapture() {
    static bool setup = false, actionsOk = true;
    static int stage = -1, step = 0;
    const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - comparisonStarted_).count();
    const float t = (std::max)(0.0f, elapsed - 1.5f);
    if (!setup) {
        for (int z : {5, 7, 9}) {
            bubblePickupController_.Update({2, 2.5f, static_cast<float>(z)});
        }
        setup = true;
    }
    player_->SetPosition({2, 2.5f, 4});
    blockPlacementController_.SetPlayerPosition({2, 2.5f, 4});
    gameplayCameraController_.ConfigureDioramaShot({8, 3, 7}, -.45f + t * .02f, .7f, 23);
    if (elapsed >= 1.5f && !comparisonRecordingStarted_) {
        FrameRecorder::Options o;
        o.name = "diorama-complex";
        o.gifSeconds = 15;
        const auto output = CaptureEnvironment("CG2_CAPTURE_DIR");
        if (!output.empty()) {
            o.directory = output;
        }
        if (!dxCommon->Recorder().Start(o)) {
            PostQuitMessage(2);
            return;
        }
        comparisonRecordingStarted_ = true;
    }
    if (t < 12) {
        const int group = static_cast<int>(t / 4), slot = group + 2;
        const float phase = t - group * 4;
        const Int3 at{3 + group * 4, 3, 6};
        if (stage != group) {
            stage = group;
            step = 0;
            blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
            blockPlacementController_.SetPlaceCustomId(slot);
        }
        if (phase < 1 || (phase >= 2 && phase < 3)) {
            const float angle = phase < .5f ? 0 : phase < 2 ? 1.5707963f : 3.1415926f;
            const auto plan = blockPlacementController_.Evaluate(at, angle);
            stageRenderer_->SetPlacementPreview(stageMap_, at, BlockType::Wall, slot, angle, plan.Ready() ? 1 : 2);
        }
        if (phase >= 1 && step == 0) {
            actionsOk = blockPlacementController_.TryPlace(at, 1.5707963f) && actionsOk;
            stageRenderer_->ClearPlacementPreview();
            step = 1;
        }
        if (phase >= 2 && step == 1) {
            actionsOk = blockPlacementController_.TryUndo() && actionsOk;
            step = 2;
        }
        if (phase >= 3 && step == 2) {
            actionsOk = blockPlacementController_.TryPlace(at, 3.1415926f) && actionsOk;
            stageRenderer_->ClearPlacementPreview();
            step = 3;
        }
    }
    if (t >= 15 && comparisonRecordingStarted_) {
        if (dxCommon->Recorder().IsRecording()) {
            dxCommon->Recorder().Stop();
        }
        if (!dxCommon->Recorder().IsBusy()) {
            int cells = 0;
            for (int y = 3; y < 6; ++y) {
                for (int z = 6; z < 9; ++z) {
                    for (int x = 3; x < 14; ++x) {
                        const auto *c = stageMap_.GetCell(x, y, z);
                        if (c->variant >= 2 && c->variant <= 4) {
                            ++cells;
                        }
                    }
                }
            }
            const bool ok = complexChecksPassed_ && actionsOk && cells == 40;
            std::ofstream("../generated/PR1_01/diorama/complex.json")
                << "{\"success\":" << (ok ? "true" : "false") << ",\"placedCells\":" << cells
                << ",\"scriptedPositions\":true}";
            PostQuitMessage(ok ? 0 : 3);
        }
    }
}

// 足場づくりの検証用に、ステージと所持パーツを準備する。
void GameRuntime::BeginBuildTrial() {
    BeginFogTrial();
    buildTrial_ = true;
    buildAction_ = false;
    buildStatus_ = 0;
    buildStep_ = 0;
    buildReached_ = false;
    stageMap_.Initialize(14, 7, 17);
    stageMap_.SetClearColor({.48f, .64f, .76f, 1});
    stageMap_.SetLightIntensity(.8f);
    for (int x = 3; x <= 10; ++x) {
        for (int z = 2; z <= 5; ++z) {
            stageMap_.SetBlock(x, 0, z, BlockType::Ground);
        }
    }
    for (int x = 3; x <= 10; ++x) {
        for (int z = 9; z <= 14; ++z) {
            stageMap_.SetBlock(x, 0, z, BlockType::Ground);
        }
    }
    stageMap_.SetBlock(5, 0, 6, BlockType::IceBlock);
    stageMap_.SetBlock(7, 1, 3, BlockType::PlayerStart);
    stageMap_.SetBlock(7, 1, 4, BlockType::BubblePickup, PackBubbleContents(BlockType::Wall, 1));
    stageMap_.SetBlock(7, 1, 12, BlockType::Goal);
    auto *part = stageMap_.GetCustomPart(1);
    *part = CustomBlockPart{};
    part->id = 1;
    part->name = "Bridge";
    part->baseType = BlockType::Wall;
    part->colorR = 1;
    part->colorG = .65f;
    part->colorB = .2f;
    for (int z = 0; z < 3; ++z) {
        part->cells[0][z][1].type = BlockType::Wall;
    }
    backupMap_ = stageMap_;
    stageRenderer_->BuildFromStageMap(stageMap_);
    blockInventory_.Initialize(0);
    bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    player_->SetPosition({7, .5f, 3});
    // 箱庭全体を見回す視点。配置モードへ入っても注視点を維持する。
    gameplayCameraController_.SetFollowPlayerMode(false);
    gameplayCameraController_.SetCameraPivot({6.5f, 0, 8});
    gameplayCameraController_.SetAngle(-.64f);
    gameplayCameraController_.SetPitch(.60f);
    blockInventoryUI_->SetSelectedBlockType(BlockType::Wall);
    blockInventoryUI_->SetSelectedCustomId(1);
    object3dCommon->SetExplorationFog({0, 0, 0, 0}, {}, {}, {});
}

// 足場づくりの案内・検証操作・撮影時間を更新する。
void GameRuntime::UpdateBuildTrial() {
    object3dCommon->SetExplorationFog({0, 0, 0, 0}, {}, {}, {});
    if (CaptureEnvironment("CG2_BUILD_VERIFY") == "1") {
        VerifyBuilding();
        return;
    }
    if (!buildCapture_) {
        return;
    }
    const auto shot = CaptureEnvironment("CG2_BUILD_SHOT");
    const float duration = shot == "route" ? 15.0f : 10.0f;
    const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - comparisonStarted_).count();
    const float t = (std::max)(0.0f, elapsed - 1.5f);
    const bool playing = currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace;
    if (playing) {
        camera->SetPosition({17, 12, -5});
        camera->SetRotation({.60f, -.64f, 0});
        camera->SetFov(.8f);
        camera->Update();
        Vector3 p{7, .5f, 4};
        if (shot == "route") {
            p.z = t < 1 ? 3 + t : t < 6 ? 4 : 5 + std::clamp((t - 6) / 3.5f, 0.0f, 1.0f) * 7;
        }
        player_->SetPosition(p);
        if (shot != "route") {
            bubblePickupController_.Update(p);
        }
        blockPlacementController_.SetPlayerPosition(p);
    }
    blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
    blockPlacementController_.SetPlaceCustomId(1);
    if (elapsed >= 1.5f && !comparisonRecordingStarted_) {
        FrameRecorder::Options o;
        o.name = CaptureEnvironment("CG2_BUILD_NAME");
        if (o.name.empty()) {
            o.name = "building";
        }
        const auto output = CaptureEnvironment("CG2_CAPTURE_DIR");
        if (!output.empty()) {
            o.directory = output;
        }
        o.gifSeconds = static_cast<int>(duration);
        if (!dxCommon->Recorder().Start(o)) {
            PostQuitMessage(2);
            return;
        }
        comparisonRecordingStarted_ = true;
    }
    if (shot == "collision") {
        const Int3 cursor{4, 0, 6};
        const auto plan = blockPlacementController_.Evaluate(cursor);
        buildStatus_ = PlacementStatus(plan.reason);
        if (t >= 3 && !buildAction_) {
            blockPlacementController_.TryPlace(cursor);
            buildAction_ = true;
        }
        // 拒否後も予定形状を残し、別の場所へ動かせることを示す。
        if (blockInventory_.HasBlock(BlockType::Wall, 1)) {
            stageRenderer_->SetPlacementPreview(stageMap_, cursor, BlockType::Wall, 1, 0, plan.Ready() ? 1 : 2);
        }
    } else if (shot == "undo") {
        const Int3 cursor{6, 0, 6};
        if (t < 3 || (t >= 5 && t < 8)) {
            const float rotation = t >= 6 ? 1.5707963f : 0;
            const auto plan = blockPlacementController_.Evaluate(cursor, rotation);
            stageRenderer_->SetPlacementPreview(stageMap_, cursor, BlockType::Wall, 1, rotation, plan.Ready() ? 1 : 2);
            buildStatus_ = t >= 5 ? 4 : 6;
        }
        if (t >= 3 && buildStep_ == 0) {
            blockPlacementController_.TryPlace(cursor);
            buildStep_ = 1;
            buildStatus_ = 3;
        }
        if (t >= 5 && buildStep_ == 1) {
            blockPlacementController_.TryUndo();
            buildStep_ = 2;
            buildStatus_ = 4;
        }
        if (t >= 8 && buildStep_ == 2) {
            blockPlacementController_.TryPlace(cursor, 1.5707963f);
            buildStep_ = 3;
            buildStatus_ = 3;
        }
    } else if (shot == "route") {
        buildStatus_ = 5;
        if (t >= 1 && buildStep_ == 0) {
            blockInventoryUI_->ToggleOpen();
            buildStep_ = 1;
        }
        if (t >= 2.5f && buildStep_ == 1) {
            blockInventoryUI_->ConfirmSelection();
            buildStep_ = 2;
        }
        if (t >= 3 && t < 5 && currentMode_ == AppMode::GamePlay_BlockPlace) {
            const Int3 cursor = t < 4 ? Int3{4, 0, 6} : Int3{6, 0, 6};
            mapCursor_->SetIndex(cursor, stageMap_);
            const auto plan = blockPlacementController_.Evaluate(cursor);
            buildStatus_ = PlacementStatus(plan.reason);
            stageRenderer_->SetPlacementPreview(stageMap_, cursor, BlockType::Wall, 1, 0, plan.Ready() ? 1 : 2);
        }
        if (t >= 5 && buildStep_ == 2) {
            if (blockPlacementController_.TryPlace({6, 0, 6})) {
                stageRenderer_->ClearPlacementPreview();
                RequestSceneChange(SceneType::GamePlay);
                buildStep_ = 3;
            }
        }
        buildReached_ = buildReached_ || isGoalReached_;
    }
    if (t >= duration && comparisonRecordingStarted_) {
        stageRenderer_->ClearPlacementPreview();
        if (dxCommon->Recorder().IsRecording()) {
            dxCommon->Recorder().Stop();
        }
        if (!dxCommon->Recorder().IsBusy()) {
            std::filesystem::create_directories("../generated/PR1_01/build");
            int cells = 0;
            for (int z = 6; z <= 8; ++z) {
                if (stageMap_.GetCell(5, 0, z)->variant == 1) {
                    ++cells;
                }
            }
            std::ofstream("../generated/PR1_01/build/" + CaptureEnvironment("CG2_BUILD_NAME") + ".json")
                << "{\"placedCells\":" << cells
                << ",\"inventory\":" << blockInventory_.GetBlockCount(BlockType::Wall, 1)
                << ",\"goalReached\":" << (buildReached_ ? "true" : "false") << "}";
            PostQuitMessage(shot == "route" && !buildReached_ ? 3 : 0);
        }
    }
}

// 配置と回収の検証を実行し、成功条件を記録する。
void GameRuntime::VerifyBuilding() {
    std::vector<std::pair<std::string, bool>> results;
    auto reset = [&]() {
        BeginBuildTrial();
        blockInventory_.AddBlock(BlockType::Wall, 1, 1);
        blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
        blockPlacementController_.SetPlaceCustomId(1);
        blockPlacementController_.SetPlayerPosition({7, .5f, 4});
    };
    reset();
    const bool blocked = !blockPlacementController_.TryPlace({4, 0, 6});
    results.push_back({"overlapRejectsEntireShape", blocked && stageMap_.GetCell(5, 0, 7)->type == BlockType::None &&
                                                        stageMap_.GetCell(5, 0, 8)->type == BlockType::None});
    results.push_back({"failedPlacementKeepsInventory", blockInventory_.GetBlockCount(BlockType::Wall, 1) == 1});
    results.push_back({"outsideRejected", !blockPlacementController_.TryPlace({12, 0, 15}) &&
                                              blockInventory_.GetBlockCount(BlockType::Wall, 1) == 1});
    blockPlacementController_.SetPlayerPosition({7, .5f, 7});
    results.push_back({"playerOverlapRejected", !blockPlacementController_.TryPlace({6, 1, 6})});
    blockPlacementController_.SetPlayerPosition({7, .5f, 4});
    results.push_back({"validWholeShapePlaced",
                       blockPlacementController_.TryPlace({6, 0, 6}) && stageMap_.GetCell(7, 0, 6)->variant == 1 &&
                           stageMap_.GetCell(7, 0, 7)->variant == 1 && stageMap_.GetCell(7, 0, 8)->variant == 1});
    results.push_back({"consumeOnce", blockInventory_.GetBlockCount(BlockType::Wall, 1) == 0});
    results.push_back({"noStockRejected", !blockPlacementController_.TryPlace({8, 0, 6})});
    blockPlacementController_.SetPlayerPosition({7, .5f, 7});
    results.push_back({"undoUnderPlayerRejected", !blockPlacementController_.TryUndo()});
    blockPlacementController_.SetPlayerPosition({7, .5f, 4});
    results.push_back({"undoRestoresShapeAndInventory", blockPlacementController_.TryUndo() &&
                                                            stageMap_.GetCell(7, 0, 7)->type == BlockType::None &&
                                                            blockInventory_.GetBlockCount(BlockType::Wall, 1) == 1});
    results.push_back({"secondUndoCannotDuplicate",
                       !blockPlacementController_.TryUndo() && blockInventory_.GetBlockCount(BlockType::Wall, 1) == 1});
    bool rotations = true;
    for (int i = 0; i < 4; ++i) {
        reset();
        rotations = rotations && blockPlacementController_.TryPlace({6, 0, 6}, i * 1.5707963f);
        for (int j = 0; j < 3; ++j) {
            rotations = rotations && stageMap_.GetCell(i % 2 ? 6 + j : 7, 0, i % 2 ? 7 : 6 + j)->variant == 1;
        }
    }
    results.push_back({"fourRotationsMatchCells", rotations});
    reset();
    blockPlacementController_.TryPlace({6, 0, 6});
    stageMap_.SetBlock(7, 0, 7, BlockType::IceBlock);
    results.push_back({"changedWorldUndoRejected", !blockPlacementController_.TryUndo() &&
                                                       blockInventory_.GetBlockCount(BlockType::Wall, 1) == 0 &&
                                                       stageMap_.GetCell(7, 0, 7)->type == BlockType::IceBlock});
    reset();
    *stageMap_.GetCustomPart(1) = CustomBlockPart{};
    results.push_back({"emptyPartRejected", !blockPlacementController_.TryPlace({6, 0, 6}) &&
                                                blockInventory_.GetBlockCount(BlockType::Wall, 1) == 1});
    reset();
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    results.push_back({"resetClearsUndo", !blockPlacementController_.HasUndo()});
    reset();
    blockInventory_.AddBlock(BlockType::MovingFloor, 1);
    blockPlacementController_.SetPlaceCustomId(0);
    blockPlacementController_.SetPlaceBlockType(BlockType::MovingFloor);
    const bool floorPlaced = blockPlacementController_.TryPlace({9, 0, 7});
    stageMap_.Update(.7f, {7, .5f, 4});
    const auto *moving = stageMap_.GetCell(9, 0, 7);
    blockPlacementController_.SetPlayerPosition(
        {9 + moving->currentOffsetX, .5f + moving->currentOffsetY, 7 + moving->currentOffsetZ});
    results.push_back({"movingUndoUnderRiderRejected", floorPlaced && !blockPlacementController_.TryUndo() &&
                                                           blockInventory_.GetBlockCount(BlockType::MovingFloor) == 0});
    blockPlacementController_.SetPlayerPosition({2, .5f, 2});
    results.push_back(
        {"movingUndoAfterAnimationRestoresOnce", blockPlacementController_.TryUndo() &&
                                                     blockInventory_.GetBlockCount(BlockType::MovingFloor) == 1 &&
                                                     !blockPlacementController_.TryUndo()});
    blockPlacementController_.TryPlace({9, 0, 7});
    stageMap_.GetCell(9, 0, 7)->moveOffset.z = 8;
    results.push_back(
        {"editedMovingFloorUndoRejected",
         !blockPlacementController_.TryUndo() && blockInventory_.GetBlockCount(BlockType::MovingFloor) == 0});
    bool passed = true;
    std::filesystem::create_directories("../generated/PR1_01/build");
    std::ofstream f("../generated/PR1_01/build/verification.json");
    f << "{";
    for (size_t i = 0; i < results.size(); ++i) {
        if (i) {
            f << ",";
        }
        f << "\"" << results[i].first << "\":" << (results[i].second ? "true" : "false");
        passed = passed && results[i].second;
    }
    f << "}";
    f.close();
    PostQuitMessage(passed ? 0 : 3);
}

// 現在の配置可否を、表示用の状態番号へ変換する。
int GameRuntime::PlacementStatus(Placement::Reason reason) const {
    switch (reason) {
    case Placement::Reason::Ready:
        return 6;
    case Placement::Reason::Occupied:
        return 2;
    case Placement::Reason::Outside:
        return 8;
    case Placement::Reason::PlayerOverlap:
        return 9;
    case Placement::Reason::NoInventory:
        return 10;
    default:
        return 11;
    }
}

// Actual stage integration: pickup inventory, placement, undo, ladder and goal
// run through the normal gameplay systems; only movement/camera timing is scripted.
void GameRuntime::UpdateComplexRouteCapture() {
    static int step = 0;
    static bool ok = true, ladderSeen = false, early = false;
    const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - comparisonStarted_).count();
    const float t = (std::max)(0.0f, elapsed - 1.5f);
    if (elapsed >= 1.5f && !comparisonRecordingStarted_) {
        FrameRecorder::Options o;
        o.name = "diorama-complexroute";
        o.gifSeconds = 15;
        const auto output = CaptureEnvironment("CG2_CAPTURE_DIR");
        if (!output.empty()) {
            o.directory = output;
        }
        if (!dxCommon->Recorder().Start(o)) {
            PostQuitMessage(2);
            return;
        }
        comparisonRecordingStarted_ = true;
    }
    // Reframe smoothly before the high part enters the shot. No post-capture crop.
    const float cameraPhase = std::clamp((t - 8.5f) / 3.0f, 0.0f, 1.0f);
    const float ease = cameraPhase * cameraPhase * (3 - 2 * cameraPhase);
    gameplayCameraController_.ConfigureDioramaShot({7.5f + ease, 2.5f + 1.2f * ease, 7.5f + 1.5f * ease},
                                                   .25f - .85f * ease, .9f - .2f * ease, 21 - ease);
    if (t >= 11 && t < 13 && player_->IsOnLadder()) {
        ladderSeen = true;
    }
    if (!isGoalReached_) {
        Vector3 p{4, 2.5f, 3};
        if (t >= 1 && t < 2) {
            p.z = 3 + (t - 1) * 2;
        } else if (t >= 2 && t < 4) {
            p.z = 5;
        } else if (t >= 4 && t < 6) {
            p.z = 5 + (t - 4) * 3.5f;
        } else if (t >= 6 && t < 7) {
            p = {4 + (t - 6), 2.5f, 12};
        } else if (t >= 7 && t < 8) {
            p = {5, 2.5f, 12 - (t - 7)};
        } else if (t >= 8 && t < 9) {
            p = {5 + (t - 8) * 6, 2.5f, 11};
        } else if (t >= 9 && t < 9.5f) {
            p = {11, 2.5f, 11 - (t - 9) * 4};
        } else if (t >= 9.5f && t < 10) {
            p = {11, 2.5f, 9};
        } else if (t >= 10 && t < 10.3f) {
            p = {11 + (t - 10) / .3f, 2.5f, 9};
        } else if (t >= 10.3f && t < 11) {
            p = {12, 2.5f, 9 + (t - 10.3f) / .7f * 3};
        } else if (t >= 11 && t < 13) {
            p = {12, 2.5f + (t - 11) * 1.5f, 12};
        } else if (t >= 13) {
            p = {12 - (std::min)(1.0f, (t - 13) * 2), 5.5f, 12};
        }
        player_->SetPosition(p);
        bubblePickupController_.Update(p);
        blockPlacementController_.SetPlayerPosition(p);
    }
    blockPlacementController_.SetPlaceBlockType(BlockType::Wall);
    const int slot = t < 7 ? 2 : t < 9.5f ? 1 : 3;
    const Int3 origin = slot == 2 ? Int3{4, 2, 7} : slot == 1 ? Int3{7, 2, 10} : Int3{10, 3, 10};
    const float angle = slot == 1 ? 1.5707963f : 0;
    blockPlacementController_.SetPlaceCustomId(slot);
    if ((t >= 2 && t < 3) || (t >= 7 && t < 8) || (t >= 9.5f && t < 10)) {
        const auto plan = blockPlacementController_.Evaluate(origin, angle);
        stageRenderer_->SetPlacementPreview(stageMap_, origin, BlockType::Wall, slot, angle, plan.Ready() ? 1 : 2);
    }
    auto place = [&]() {
        ok = blockPlacementController_.TryPlace(origin, angle) && ok;
        stageRenderer_->ClearPlacementPreview();
        ++step;
    };
    if (t >= 3 && step == 0) {
        place();
    }
    if (t >= 3.4f && step == 1) {
        ok = blockPlacementController_.TryUndo() && ok;
        ++step;
    }
    if (t >= 3.8f && step == 2) {
        place();
    }
    if (t >= 8 && step == 3) {
        place();
    }
    if (t >= 10 && step == 4) {
        place();
    }
    if (isGoalReached_ && (t < 13 || player_->IsOnLadder() || player_->GetPosition().y < 5.45f)) {
        early = true;
    }
    if (t >= 15 && comparisonRecordingStarted_) {
        if (dxCommon->Recorder().IsRecording()) {
            dxCommon->Recorder().Stop();
        }
        if (!dxCommon->Recorder().IsBusy()) {
            int l = 0, stairs = 0, ladders = 0;
            for (int y = 0; y < stageMap_.GetHeight(); ++y) {
                for (int z = 0; z < stageMap_.GetDepth(); ++z) {
                    for (int x = 0; x < stageMap_.GetWidth(); ++x) {
                        const auto *c = stageMap_.GetCell(x, y, z);
                        if (c->variant == 2 && c->type == BlockType::Wall) {
                            ++l;
                        }
                        if (c->variant == 3) {
                            if (c->type == BlockType::Wall) {
                                ++stairs;
                            }
                            if (c->type == BlockType::Ladder) {
                                ++ladders;
                            }
                        }
                    }
                }
            }
            bool support = true;
            for (int z = 7; z <= 9; ++z) {
                support = support && stageMap_.GetCell(4, 2, z)->type == BlockType::Wall;
            }
            for (int x = 7; x <= 9; ++x) {
                support = support && stageMap_.GetCell(x, 2, 11)->type == BlockType::Wall;
            }
            const bool stock = blockInventory_.GetBlockCount(BlockType::Wall, 1) == 0 &&
                               blockInventory_.GetBlockCount(BlockType::Wall, 2) == 0 &&
                               blockInventory_.GetBlockCount(BlockType::Wall, 3) == 0;
            const bool success = ok && step == 5 && l == 5 && stairs == 12 && ladders == 3 && support && stock &&
                                 ladderSeen && bubblePickupController_.AreAllPickupsCollected() && isGoalReached_ &&
                                 !early;
            std::ofstream("../generated/PR1_01/diorama/complexroute.json")
                << "{\"success\":" << (success ? "true" : "false") << ",\"scriptedPositions\":true,\"lCells\":" << l
                << ",\"stairCells\":" << stairs << ",\"ladderCells\":" << ladders
                << ",\"placementAndUndo\":" << (ok ? "true" : "false")
                << ",\"bridgeSupport\":" << (support ? "true" : "false")
                << ",\"inventoryConsumed\":" << (stock ? "true" : "false")
                << ",\"ladderDetected\":" << (ladderSeen ? "true" : "false")
                << ",\"goalReached\":" << (isGoalReached_ ? "true" : "false")
                << ",\"noPrematureClear\":" << (!early ? "true" : "false") << "}";
            PostQuitMessage(success ? 0 : 3);
        }
    }
}

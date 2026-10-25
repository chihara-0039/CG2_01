// 録画ツールと探索ステージの接続。ゲームの通常ステージは変更しない。
#include "GameRuntime.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
std::string CaptureEnvironment(const char* name) {
    char value[32768]{};
    const DWORD length=GetEnvironmentVariableA(name,value,sizeof(value));
    return length && length<sizeof(value) ? std::string(value) : std::string{};
}
}

void GameRuntime::InitializeCaptureTools() {
    const char* captions[] = {"baseline", "bubble", "scan", "trial_hint", "recording"};
    for (int i=0;i<5;++i) {
        fogLabels_[i]=std::make_unique<Sprite>();
        fogLabels_[i]->Initialize(spriteCommon.get(), textureManager->LoadTexture(std::string("Resources/UI/PR1_01/")+captions[i]+".png"));
        fogLabels_[i]->SetPosition({36,28});
        fogLabels_[i]->SetSize({1200,130});
        fogLabels_[i]->Update();
    }
    fogLabels_[4]->SetPosition({1550,30}); fogLabels_[4]->SetSize({320,65}); fogLabels_[4]->Update();
    scanMeter_=std::make_unique<Sprite>(); scanMeter_->Initialize(spriteCommon.get(),0);
    scanMeter_->SetPosition({38,164}); scanMeter_->SetColor({.12f,.85f,1,1});
    const auto comparison=CaptureEnvironment("CG2_COMPARE");
    for(int i=0;i<12;++i) {
        buildLabels_[i]=std::make_unique<Sprite>();
        buildLabels_[i]->Initialize(spriteCommon.get(),textureManager->LoadTexture("Resources/UI/PR1_01/build-"+std::to_string(i)+".png"));
        buildLabels_[i]->SetPosition({20,20}); buildLabels_[i]->SetSize({950,110}); buildLabels_[i]->Update();
    }
    for(int i=0;i<2;++i) {
        buildCounts_[i]=std::make_unique<Sprite>();
        buildCounts_[i]->Initialize(spriteCommon.get(),textureManager->LoadTexture("Resources/UI/PR1_01/build-count-"+std::to_string(i)+".png"));
        buildCounts_[i]->SetPosition({990,25}); buildCounts_[i]->SetSize({270,65}); buildCounts_[i]->Update();
    }
    if (!CaptureEnvironment("CG2_BUILD_SHOT").empty() || CaptureEnvironment("CG2_BUILD_TRIAL")=="1" || CaptureEnvironment("CG2_BUILD_VERIFY")=="1") {
        BeginBuildTrial();
        buildCapture_=!CaptureEnvironment("CG2_BUILD_SHOT").empty();
        comparisonActive_=buildCapture_; comparisonStarted_=std::chrono::steady_clock::now();
    }
    if (CaptureEnvironment("CG2_TRIAL")=="1" || CaptureEnvironment("CG2_VERIFY_TRIAL")=="1") BeginFogTrial();
    if (CaptureEnvironment("CG2_VERIFY_TRIAL")=="1") trialVerification_=true;
    if (comparison=="baseline" || comparison=="bubble" || comparison=="scan") {
        comparisonMode_=comparison=="baseline" ? 0 : comparison=="bubble" ? 1 : 2;
        BeginFogTrial();
        fogExplorer_.SetMode(comparisonMode_);
        comparisonActive_=true;
        comparisonStarted_=std::chrono::steady_clock::now();
    }
}

void GameRuntime::BeginFogTrial() {
    if (startIntro_) startIntro_->Cancel(camera.get());
    sceneFadePhase_=SceneFadePhase::None; sceneFadeAlpha_=0; startIntroPending_=false;
    fogTrial_=true; fogExplorer_.Reset();
    stageMap_.Initialize(48,10,64);
    stageMap_.SetClearColor({.48f,.64f,.76f,1});
    stageMap_.SetLightColor({1,.96f,.90f}); stageMap_.SetLightIntensity(.85f);
    stageMap_.SetWeatherPresetName("");
    // 広い出発点から細い道へ。左の分岐でシャボン玉を回収して奥の星へ進む。
    for (int z=5;z<=16;++z) for(int x=14;x<=30;++x) stageMap_.SetBlock(x,0,z,BlockType::Ground);
    for (int z=16;z<=56;++z) for(int x=19;x<=25;++x) stageMap_.SetBlock(x,0,z,BlockType::Ground);
    for (int z=25;z<=29;++z) for(int x=8;x<=19;++x) stageMap_.SetBlock(x,0,z,BlockType::Ground);
    for (int z=22;z<=32;++z) for(int x=5;x<=11;++x) stageMap_.SetBlock(x,0,z,BlockType::Ground);
    // 柱で距離の基準を作る。通路上は塞がず、スキャンで先の道が見える。
    for(int z=18;z<=54;z+=9) for(int y=1;y<=3;++y) {
        stageMap_.SetBlock(18,y,z,BlockType::Wall); stageMap_.SetBlock(26,y,z,BlockType::Wall);
    }
    stageMap_.SetBlock(22,1,10,BlockType::PlayerStart);
    stageMap_.SetBlock(8,1,27,BlockType::BubblePickup);
    stageMap_.SetBlock(22,1,54,BlockType::Goal);
    backupMap_=stageMap_;
    stageRenderer_->BuildFromStageMap(stageMap_);
    blenderStageActive_=false; player_->SetExternalCollisionBoxes(nullptr);
    stageEditorController_.ResetPlayerToStartCell(stageMap_,player_.get());
    playerBasePosition_.ApplyFromStageMap(stageMap_,player_.get());
    player_->SetStageCollisionEnabled(true);
    blockInventory_.Initialize(0);
    bubblePickupController_.Initialize(&stageMap_,stageRenderer_.get(),&blockInventory_);
    blockPlacementController_.Initialize(&stageMap_,stageRenderer_.get(),&blockInventory_);
    isGoalReached_=isGoalBlocked_=isGamePaused_=false; useFirstPersonCamera_=false;
    objectiveGuideActive_=false; RefreshGoalGuideTarget();
    gameplayCameraController_.ConfigureExploration();
    sceneManager_->ChangeScene(SceneType::GamePlay,*this);
    weatherRuntimeController_.StopStorm(*particleManager);
    particleManager->SetWeatherEmitterActive(false); particleManager->SetAmbientCloudEmitterActive(false); particleManager->ClearParticles();
    fogUpdatedAt_=std::chrono::steady_clock::now();
}

void GameRuntime::UpdateCaptureTools() {
    auto& recorder=dxCommon->Recorder();
    const bool canUndo=(currentMode_==AppMode::GamePlay || currentMode_==AppMode::GamePlay_BlockPlace) &&
        !isGamePaused_ && !isGoalReached_ && (!blockInventoryUI_ || !blockInventoryUI_->IsActive()) && !comparisonActive_;
    if(canUndo && player_) {
        blockPlacementController_.SetPlayerPosition(player_->GetPosition());
        if(input->TriggerKey(DIK_Z) || input->TriggerControllerButton(XINPUT_GAMEPAD_BACK)) {
            const bool restored=blockPlacementController_.TryUndo();
            buildStatus_=restored?4:PlacementStatus(blockPlacementController_.LastReason());
            placementFeedbackUntil_=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        }
    }
    if (input->TriggerKey(DIK_F8)) {
        if (recorder.IsRecording()) recorder.Stop();
        else if (!recorder.IsBusy()) recorder.Start(FrameRecorder::Options{});
    }
    if (input->TriggerKey(DIK_F9)) showRecorder_=!showRecorder_;
    if (input->TriggerKey(DIK_F7) && !comparisonActive_) BeginBuildTrial();
    if (input->TriggerKey(DIK_F6) && !comparisonActive_) {buildTrial_=false;BeginFogTrial();}
    if (fogTrial_ && !comparisonActive_ && input->TriggerKey(DIK_F4)) fogExplorer_.SetMode((fogExplorer_.Mode()+1)%3);
    if (showRecorder_) recorder.DrawImGui();
    // 実際のプレイヤー物理・回収・ゴール制御を通す統合確認。比較動画とは分離する。
    if (trialVerification_) {
        ++verifyFrame_;
        if (verifyFrame_==60) { verifyGround_=player_->IsGrounded() && std::abs(player_->GetPosition().y-.5f)<.15f; fogExplorer_.TryScan(); }
        if (verifyFrame_==90) { verifyScan_=fogExplorer_.Strength()>.8f; isGamePaused_=true; verifyCooldown_=fogExplorer_.Cooldown(); }
        if (verifyFrame_==110) { verifyPause_=std::abs(verifyCooldown_-fogExplorer_.Cooldown())<.001f; isGamePaused_=false; }
        if (verifyFrame_==150) player_->SetPosition({22,.5f,54});
        if (verifyFrame_==152) { verifyLock_=isGoalBlocked_ && !isGoalReached_; player_->SetPosition({22,.5f,10}); }
        if (verifyFrame_==180) player_->SetPosition({8,.5f,27});
        if (verifyFrame_==182) verifyPickup_=bubblePickupController_.AreAllPickupsCollected();
        if (verifyFrame_==210) player_->SetPosition({22,.5f,54});
        if (verifyFrame_==212) {
            std::filesystem::create_directories("../generated/PR1_01");
            std::ofstream report("../generated/PR1_01/gameplay-verification.json");
            report<<std::boolalpha<<"{\"grounded\":"<<verifyGround_<<",\"scan\":"<<verifyScan_<<",\"pauseFreezesCooldown\":"<<verifyPause_
                <<",\"goalLockedBeforePickup\":"<<verifyLock_<<",\"pickupCollected\":"<<verifyPickup_<<",\"goalReachedAfterPickup\":"<<isGoalReached_<<"}";
            PostQuitMessage(verifyGround_ && verifyScan_ && verifyPause_ && verifyLock_ && verifyPickup_ && isGoalReached_ ? 0 : 3);
        }
    }
#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (fogTrial_ && showRecorder_) {
        ImGui::Begin("Exploration Fog");
        ImGui::Text("F4: method %d / G: scan / F6: reset trial",fogExplorer_.Mode());
        ImGui::Text("Scan cooldown %.1fs / strength %.2f",fogExplorer_.Cooldown(),fogExplorer_.Strength());
        ImGui::End();
    }
#endif
}

void GameRuntime::UpdateExplorationFog() {
    if(buildTrial_) { UpdateBuildTrial(); return; }
    const auto now=std::chrono::steady_clock::now();
    const float dt=std::chrono::duration<float>(now-fogUpdatedAt_).count(); fogUpdatedAt_=now;
    const float captureDuration=CaptureEnvironment("CG2_SHOT")=="route" ? 12.0f : 10.0f;
    // クリア画面へ遷移しても撮影の終了処理を止めない。
    if (comparisonActive_ && comparisonRecordingStarted_ && std::chrono::duration<float>(now-comparisonStarted_).count()>=captureDuration+1.5f) {
        if (dxCommon->Recorder().IsRecording()) dxCommon->Recorder().Stop();
        if (!dxCommon->Recorder().IsBusy()) PostQuitMessage(0);
    }
    const bool active=fogTrial_ && (currentMode_==AppMode::GamePlay || currentMode_==AppMode::GamePlay_BlockPlace);
    if (!active) { object3dCommon->SetExplorationFog({0,0,0,0},{},{},{}); return; }
    if (comparisonActive_) {
        // 実入力の録画とは別の再現撮影。カメラ・位置・時刻は全方式で同一。
        const float elapsed=std::chrono::duration<float>(now-comparisonStarted_).count();
        const float t=(std::max)(0.0f,elapsed-1.5f);
        const auto shot=CaptureEnvironment("CG2_SHOT");
        float yaw=-.8f*std::sin(t*.7f),distance=20,height=14,pitch=.55f;
        Vector3 p{22,.5f,10+std::clamp(t,0.0f,8.0f)*1.4f};
        if(shot=="footpath") { p={22,.5f,14+std::clamp(t,0.0f,8.0f)*1.5f}; yaw=.25f; distance=9; height=6; pitch=.56f; }
        if(shot=="branch") { p={22,.5f,27}; yaw=-.8f*std::sin(t*.65f); distance=15; height=10; pitch=.56f; }
        if(shot=="character") { p={22,.5f,10}; yaw=t*.63f; distance=3.8f; height=1.65f; pitch=.22f; player_->SetRotation({0,3.14159265f,0}); }
        if(shot=="route") {
            // 未回収の星、分岐の回収アイテム、回収後の星を順に確認する再現配置。
            p=t<3 ? Vector3{22,.5f,54} : t<6 ? Vector3{8,.5f,27} : Vector3{22,.5f,54};
            yaw=.25f; distance=12; height=8; pitch=.54f;
        }
        player_->SetPosition(p);
        camera->SetPosition({p.x-std::sin(yaw)*distance,p.y+height,p.z-std::cos(yaw)*distance});
        camera->SetRotation({pitch,yaw,0}); camera->SetFov(.9f); camera->Update();
        if (elapsed>=1.5f && !comparisonRecordingStarted_) {
            FrameRecorder::Options options; options.name=comparisonMode_==0 ? "baseline" : comparisonMode_==1 ? "bubble" : "scan";
            if(!shot.empty()) options.name=shot+"-"+options.name;
            const auto output=CaptureEnvironment("CG2_CAPTURE_DIR"); if(!output.empty()) options.directory=output;
            options.gifSeconds=static_cast<int>(captureDuration);
            if (!dxCommon->Recorder().Start(options)) { PostQuitMessage(2); return; }
            comparisonRecordingStarted_=true;
        }
        if (t>=1.2f && !comparisonScanned_ && shot!="character") { fogExplorer_.TryScan(); comparisonScanned_=true; }
        if (elapsed>=captureDuration+1.5f && comparisonRecordingStarted_ && dxCommon->Recorder().IsRecording()) dxCommon->Recorder().Stop();
        if (elapsed>=captureDuration+1.5f && comparisonRecordingStarted_ && !dxCommon->Recorder().IsBusy()) PostQuitMessage(0);
    }
    const bool paused=isGamePaused_ || (blockInventoryUI_ && blockInventoryUI_->IsActive()) || sceneFadePhase_!=SceneFadePhase::None || isGoalReached_ || (!comparisonActive_ && IsWindowInactive());
    if (!paused && input->TriggerKey(DIK_G)) fogExplorer_.TryScan();
    fogExplorer_.Update(dt,player_->GetPosition(),camera->GetRotation().y,paused);
    const auto p=fogExplorer_.Origin(), d=fogExplorer_.Direction();
    object3dCommon->SetExplorationFog({1,4,28,.94f}, {p.x,p.y,p.z,static_cast<float>(fogExplorer_.Mode())},
        {d.x,0,d.z,28}, {fogExplorer_.Strength(),fogExplorer_.Radius(),0,0});
    scanMeter_->SetSize({280*(1-fogExplorer_.Cooldown()/6),7}); scanMeter_->Update();
}

void GameRuntime::DrawCaptureOverlay() {
    spriteCommon->PreDraw();
    if(buildTrial_ && (currentMode_==AppMode::GamePlay || currentMode_==AppMode::GamePlay_BlockPlace)) {
        buildLabels_[buildStatus_]->Draw();
        buildCounts_[blockInventory_.GetBlockCount(BlockType::Wall,1)>0 ? 1:0]->Draw();
        return;
    }
    if(currentMode_==AppMode::GamePlay_BlockPlace ||
        (currentMode_==AppMode::GamePlay && std::chrono::steady_clock::now()<placementFeedbackUntil_)) buildLabels_[buildStatus_]->Draw();
    if (currentMode_==AppMode::Title && fogLabels_[3]) fogLabels_[3]->Draw();
    const auto shot=CaptureEnvironment("CG2_SHOT");
    if (fogTrial_ && currentMode_==AppMode::GamePlay && shot!="character" && shot!="route") {
        fogLabels_[fogExplorer_.Mode()]->Draw(); scanMeter_->Draw();
    }
    if (dxCommon->Recorder().IsRecording()) fogLabels_[4]->Draw();
}

void GameRuntime::BeginBuildTrial() {
    BeginFogTrial(); buildTrial_=true; buildAction_=false; buildStatus_=0;buildStep_=0;buildReached_=false;
    stageMap_.Initialize(14,7,17);
    stageMap_.SetClearColor({.48f,.64f,.76f,1}); stageMap_.SetLightIntensity(.8f);
    for(int x=3;x<=10;++x) for(int z=2;z<=5;++z) stageMap_.SetBlock(x,0,z,BlockType::Ground);
    for(int x=3;x<=10;++x) for(int z=9;z<=14;++z) stageMap_.SetBlock(x,0,z,BlockType::Ground);
    stageMap_.SetBlock(5,0,6,BlockType::IceBlock);
    stageMap_.SetBlock(7,1,3,BlockType::PlayerStart);
    stageMap_.SetBlock(7,1,4,BlockType::BubblePickup,PackBubbleContents(BlockType::Wall,1));
    stageMap_.SetBlock(7,1,12,BlockType::Goal);
    auto* part=stageMap_.GetCustomPart(1);
    *part=CustomBlockPart{}; part->id=1;part->name="Bridge";part->baseType=BlockType::Wall;
    part->colorR=1; part->colorG=.65f; part->colorB=.2f;
    for(int z=0;z<3;++z) part->cells[0][z][1].type=BlockType::Wall;
    backupMap_=stageMap_; stageRenderer_->BuildFromStageMap(stageMap_);
    blockInventory_.Initialize(0); bubblePickupController_.Initialize(&stageMap_,stageRenderer_.get(),&blockInventory_);
    blockPlacementController_.Initialize(&stageMap_,stageRenderer_.get(),&blockInventory_);
    player_->SetPosition({7,.5f,3});
    // 箱庭全体を見回す視点。配置モードへ入っても注視点を維持する。
    gameplayCameraController_.SetFollowPlayerMode(false);
    gameplayCameraController_.SetCameraPivot({6.5f,0,8});
    gameplayCameraController_.SetAngle(-.64f);gameplayCameraController_.SetPitch(.60f);
    blockInventoryUI_->SetSelectedBlockType(BlockType::Wall); blockInventoryUI_->SetSelectedCustomId(1);
    object3dCommon->SetExplorationFog({0,0,0,0},{},{},{});
}

void GameRuntime::UpdateBuildTrial() {
    object3dCommon->SetExplorationFog({0,0,0,0},{},{},{});
    if(CaptureEnvironment("CG2_BUILD_VERIFY")=="1") {VerifyBuilding();return;}
    if(!buildCapture_) return;
    const auto shot=CaptureEnvironment("CG2_BUILD_SHOT");
    const float duration=shot=="route"?15.0f:10.0f;
    const float elapsed=std::chrono::duration<float>(std::chrono::steady_clock::now()-comparisonStarted_).count();
    const float t=(std::max)(0.0f,elapsed-1.5f);
    const bool playing=currentMode_==AppMode::GamePlay || currentMode_==AppMode::GamePlay_BlockPlace;
    if(playing) {
        camera->SetPosition({17,12,-5}); camera->SetRotation({.60f,-.64f,0}); camera->SetFov(.8f); camera->Update();
        Vector3 p{7,.5f,4};
        if(shot=="route") {
            p.z=t<1?3+t:t<6?4:5+std::clamp((t-6)/3.5f,0.0f,1.0f)*7;
        }
        player_->SetPosition(p);
        if(shot!="route")bubblePickupController_.Update(p);
        blockPlacementController_.SetPlayerPosition(p);
    }
    blockPlacementController_.SetPlaceBlockType(BlockType::Wall); blockPlacementController_.SetPlaceCustomId(1);
    if(elapsed>=1.5f && !comparisonRecordingStarted_) {
        FrameRecorder::Options o; o.name=CaptureEnvironment("CG2_BUILD_NAME"); if(o.name.empty())o.name="building";
        const auto output=CaptureEnvironment("CG2_CAPTURE_DIR"); if(!output.empty())o.directory=output;
        o.gifSeconds=static_cast<int>(duration);
        if(!dxCommon->Recorder().Start(o)){PostQuitMessage(2);return;}
        comparisonRecordingStarted_=true;
    }
    if(shot=="collision") {
        const Int3 cursor{4,0,6};const auto plan=blockPlacementController_.Evaluate(cursor);
        buildStatus_=PlacementStatus(plan.reason);
        if(t>=3 && !buildAction_) {blockPlacementController_.TryPlace(cursor);buildAction_=true;}
        // 拒否後も予定形状を残し、別の場所へ動かせることを示す。
        if(blockInventory_.HasBlock(BlockType::Wall,1))stageRenderer_->SetPlacementPreview(stageMap_,cursor,BlockType::Wall,1,0,plan.Ready()?1:2);
    }else if(shot=="undo") {
        const Int3 cursor{6,0,6};
        if(t<3 || (t>=5 && t<8)) {
            const float rotation=t>=6?1.5707963f:0;
            const auto plan=blockPlacementController_.Evaluate(cursor,rotation);
            stageRenderer_->SetPlacementPreview(stageMap_,cursor,BlockType::Wall,1,rotation,plan.Ready()?1:2);
            buildStatus_=t>=5?4:6;
        }
        if(t>=3 && buildStep_==0){blockPlacementController_.TryPlace(cursor);buildStep_=1;buildStatus_=3;}
        if(t>=5 && buildStep_==1){blockPlacementController_.TryUndo();buildStep_=2;buildStatus_=4;}
        if(t>=8 && buildStep_==2){blockPlacementController_.TryPlace(cursor,1.5707963f);buildStep_=3;buildStatus_=3;}
    }else if(shot=="route") {
        buildStatus_=5;
        if(t>=1 && buildStep_==0) {blockInventoryUI_->ToggleOpen();buildStep_=1;}
        if(t>=2.5f && buildStep_==1) {blockInventoryUI_->ConfirmSelection();buildStep_=2;}
        if(t>=3 && t<5 && currentMode_==AppMode::GamePlay_BlockPlace) {
            const Int3 cursor=t<4?Int3{4,0,6}:Int3{6,0,6};mapCursor_->SetIndex(cursor,stageMap_);
            const auto plan=blockPlacementController_.Evaluate(cursor);buildStatus_=PlacementStatus(plan.reason);
            stageRenderer_->SetPlacementPreview(stageMap_,cursor,BlockType::Wall,1,0,plan.Ready()?1:2);
        }
        if(t>=5 && buildStep_==2) {
            if(blockPlacementController_.TryPlace({6,0,6})) {stageRenderer_->ClearPlacementPreview();RequestSceneChange(SceneType::GamePlay);buildStep_=3;}
        }
        buildReached_=buildReached_ || isGoalReached_;
    }
    if(t>=duration && comparisonRecordingStarted_) {
        stageRenderer_->ClearPlacementPreview();
        if(dxCommon->Recorder().IsRecording())dxCommon->Recorder().Stop();
        if(!dxCommon->Recorder().IsBusy()) {
            std::filesystem::create_directories("../generated/PR1_01/build");
            int cells=0;for(int z=6;z<=8;++z)if(stageMap_.GetCell(5,0,z)->variant==1)++cells;
            std::ofstream("../generated/PR1_01/build/"+CaptureEnvironment("CG2_BUILD_NAME")+".json")
                <<"{\"placedCells\":"<<cells<<",\"inventory\":"<<blockInventory_.GetBlockCount(BlockType::Wall,1)
                <<",\"goalReached\":"<<(buildReached_?"true":"false")<<"}";
            PostQuitMessage(shot=="route"&&!buildReached_?3:0);
        }
    }
}

void GameRuntime::VerifyBuilding() {
    std::vector<std::pair<std::string,bool>> results;
    auto reset=[&](){BeginBuildTrial();blockInventory_.AddBlock(BlockType::Wall,1,1);blockPlacementController_.SetPlaceBlockType(BlockType::Wall);blockPlacementController_.SetPlaceCustomId(1);blockPlacementController_.SetPlayerPosition({7,.5f,4});};
    reset();
    const bool blocked=!blockPlacementController_.TryPlace({4,0,6});
    results.push_back({"overlapRejectsEntireShape",blocked && stageMap_.GetCell(5,0,7)->type==BlockType::None && stageMap_.GetCell(5,0,8)->type==BlockType::None});
    results.push_back({"failedPlacementKeepsInventory",blockInventory_.GetBlockCount(BlockType::Wall,1)==1});
    results.push_back({"outsideRejected",!blockPlacementController_.TryPlace({12,0,15}) && blockInventory_.GetBlockCount(BlockType::Wall,1)==1});
    blockPlacementController_.SetPlayerPosition({7,.5f,7});
    results.push_back({"playerOverlapRejected",!blockPlacementController_.TryPlace({6,1,6})});
    blockPlacementController_.SetPlayerPosition({7,.5f,4});
    results.push_back({"validWholeShapePlaced",blockPlacementController_.TryPlace({6,0,6}) && stageMap_.GetCell(7,0,6)->variant==1 && stageMap_.GetCell(7,0,7)->variant==1 && stageMap_.GetCell(7,0,8)->variant==1});
    results.push_back({"consumeOnce",blockInventory_.GetBlockCount(BlockType::Wall,1)==0});
    results.push_back({"noStockRejected",!blockPlacementController_.TryPlace({8,0,6})});
    blockPlacementController_.SetPlayerPosition({7,.5f,7});
    results.push_back({"undoUnderPlayerRejected",!blockPlacementController_.TryUndo()});
    blockPlacementController_.SetPlayerPosition({7,.5f,4});
    results.push_back({"undoRestoresShapeAndInventory",blockPlacementController_.TryUndo() && stageMap_.GetCell(7,0,7)->type==BlockType::None && blockInventory_.GetBlockCount(BlockType::Wall,1)==1});
    results.push_back({"secondUndoCannotDuplicate",!blockPlacementController_.TryUndo() && blockInventory_.GetBlockCount(BlockType::Wall,1)==1});
    bool rotations=true;
    for(int i=0;i<4;++i) {
        reset();rotations=rotations&&blockPlacementController_.TryPlace({6,0,6},i*1.5707963f);
        for(int j=0;j<3;++j)rotations=rotations && stageMap_.GetCell(i%2?6+j:7,0,i%2?7:6+j)->variant==1;
    }
    results.push_back({"fourRotationsMatchCells",rotations});
    reset();blockPlacementController_.TryPlace({6,0,6});stageMap_.SetBlock(7,0,7,BlockType::IceBlock);
    results.push_back({"changedWorldUndoRejected",!blockPlacementController_.TryUndo() && blockInventory_.GetBlockCount(BlockType::Wall,1)==0 && stageMap_.GetCell(7,0,7)->type==BlockType::IceBlock});
    reset();*stageMap_.GetCustomPart(1)=CustomBlockPart{};
    results.push_back({"emptyPartRejected",!blockPlacementController_.TryPlace({6,0,6}) && blockInventory_.GetBlockCount(BlockType::Wall,1)==1});
    reset();blockPlacementController_.Initialize(&stageMap_,stageRenderer_.get(),&blockInventory_);
    results.push_back({"resetClearsUndo",!blockPlacementController_.HasUndo()});
    bool passed=true;std::filesystem::create_directories("../generated/PR1_01/build");std::ofstream f("../generated/PR1_01/build/verification.json");f<<"{";
    for(size_t i=0;i<results.size();++i){if(i)f<<",";f<<"\""<<results[i].first<<"\":"<<(results[i].second?"true":"false");passed=passed&&results[i].second;}f<<"}";f.close();
    PostQuitMessage(passed?0:3);
}

int GameRuntime::PlacementStatus(Placement::Reason reason) const {
    switch(reason) {
    case Placement::Reason::Ready:return 6;
    case Placement::Reason::Occupied:return 2;
    case Placement::Reason::Outside:return 8;
    case Placement::Reason::PlayerOverlap:return 9;
    case Placement::Reason::NoInventory:return 10;
    default:return 11;
    }
}

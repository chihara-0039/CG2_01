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
    if (input->TriggerKey(DIK_F8)) {
        if (recorder.IsRecording()) recorder.Stop();
        else if (!recorder.IsBusy()) recorder.Start(FrameRecorder::Options{});
    }
    if (input->TriggerKey(DIK_F9)) showRecorder_=!showRecorder_;
    if (input->TriggerKey(DIK_F6) && !comparisonActive_) BeginFogTrial();
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
    const auto now=std::chrono::steady_clock::now();
    const float dt=std::chrono::duration<float>(now-fogUpdatedAt_).count(); fogUpdatedAt_=now;
    const bool active=fogTrial_ && (currentMode_==AppMode::GamePlay || currentMode_==AppMode::GamePlay_BlockPlace);
    if (!active) { object3dCommon->SetExplorationFog({0,0,0,0},{},{},{}); return; }
    if (comparisonActive_) {
        // 実入力の録画とは別の再現撮影。カメラ・位置・時刻は全方式で同一。
        const float elapsed=std::chrono::duration<float>(now-comparisonStarted_).count();
        const float t=(std::max)(0.0f,elapsed-1.5f);
        const float yaw=-.8f*std::sin(t*.7f);
        const Vector3 p{22,.5f,10+std::clamp(t,0.0f,8.0f)*1.4f};
        player_->SetPosition(p);
        camera->SetPosition({p.x-std::sin(yaw)*20,p.y+14,p.z-std::cos(yaw)*20});
        camera->SetRotation({.55f,yaw,0}); camera->SetFov(.9f); camera->Update();
        if (elapsed>=1.5f && !comparisonRecordingStarted_) {
            FrameRecorder::Options options; options.name=comparisonMode_==0 ? "baseline" : comparisonMode_==1 ? "bubble" : "scan";
            const auto output=CaptureEnvironment("CG2_CAPTURE_DIR"); if(!output.empty()) options.directory=output;
            options.gifSeconds=10;
            if (!dxCommon->Recorder().Start(options)) { PostQuitMessage(2); return; }
            comparisonRecordingStarted_=true;
        }
        if (t>=1.2f && !comparisonScanned_) { fogExplorer_.TryScan(); comparisonScanned_=true; }
        if (elapsed>=11.5f && comparisonRecordingStarted_ && dxCommon->Recorder().IsRecording()) dxCommon->Recorder().Stop();
        if (elapsed>=11.5f && comparisonRecordingStarted_ && !dxCommon->Recorder().IsBusy()) PostQuitMessage(0);
    }
    const bool paused=isGamePaused_ || (blockInventoryUI_ && blockInventoryUI_->IsActive()) || sceneFadePhase_!=SceneFadePhase::None || isGoalReached_ || (!comparisonActive_ && IsWindowInactive());
    if (!paused && input->TriggerKey(DIK_G)) fogExplorer_.TryScan();
    fogExplorer_.Update(dt,player_->GetPosition(),camera->GetRotation().y,paused);
    const auto p=fogExplorer_.Origin(), d=fogExplorer_.Direction();
    object3dCommon->SetExplorationFog({1,8,55,.94f}, {p.x,p.y,p.z,static_cast<float>(fogExplorer_.Mode())},
        {d.x,0,d.z,28}, {fogExplorer_.Strength(),fogExplorer_.Radius(),0,0});
    scanMeter_->SetSize({280*(1-fogExplorer_.Cooldown()/6),7}); scanMeter_->Update();
}

void GameRuntime::DrawCaptureOverlay() {
    spriteCommon->PreDraw();
    if (currentMode_==AppMode::Title && fogLabels_[3]) fogLabels_[3]->Draw();
    if (fogTrial_ && currentMode_==AppMode::GamePlay) {
        fogLabels_[fogExplorer_.Mode()]->Draw(); scanMeter_->Draw();
    }
    if (dxCommon->Recorder().IsRecording()) fogLabels_[4]->Draw();
}

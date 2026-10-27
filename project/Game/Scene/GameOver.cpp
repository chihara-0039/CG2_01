#include "GameOver.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void GameOver::Initialize(SpriteCommon* common,const std::array<uint32_t,8>& textures) {
    const auto make=[common](uint32_t texture) {
        auto sprite=std::make_unique<Sprite>(); sprite->Initialize(common,texture); return sprite;
    };
    for (auto& strip:iris_) strip=make(0);
    for (size_t i=0;i<labels_.size();++i) {
        labels_[i]=make(textures[i]); labels_[i]->SetAnchorPoint({.5f,.5f});
    }
    for (size_t i=0;i<letters_.size();++i) {
        letters_[i]=make(textures[0]); letters_[i]->SetAnchorPoint({.5f,.5f});
        letters_[i]->SetTextureRect({static_cast<float>(i)*144,0},{144,192});
    }
    for (auto& spark:sparks_) { spark=make(0); spark->SetAnchorPoint({.5f,.5f}); }
    for (auto& border:borders_) { border=make(0); border->SetAnchorPoint({.5f,.5f}); }
    backdrop_=make(textures[5]); sigil_=make(textures[6]); sigil_->SetAnchorPoint({.5f,.5f});
    flash_=make(0); selection_=make(textures[7]); selection_->SetAnchorPoint({.5f,.5f});
    sound_.Initialize(); failureSound_=MakeSound(0); titleSound_=MakeSound(1);
    cursorSound_=MakeSound(2); confirmSound_=MakeSound(3);
}
Sound::SoundData GameOver::MakeSound(int kind) {
    Sound::SoundData data{};
    data.wfex.wFormatTag=WAVE_FORMAT_PCM; data.wfex.nChannels=1;
    data.wfex.nSamplesPerSec=44100; data.wfex.wBitsPerSample=16;
    data.wfex.nBlockAlign=2; data.wfex.nAvgBytesPerSec=88200;
    const float duration=kind==0 ? 1.3f : kind==1 ? .6f : kind==2 ? .085f : .3f;
    const int samples=static_cast<int>(44100*duration);
    data.buffer.resize(samples*sizeof(int16_t));
    double phase=0;
    for (int i=0;i<samples;++i) {
        const float time=static_cast<float>(i)/44100;
        const float t=time/duration;
        const float note=kind==0 ? (t<.22f ? 392.0f : t<.45f ? 311.13f : 196.0f)
            : kind==1 ? 261.63f : kind==2 ? 740.0f : (t<.33f ? 523.25f : t<.66f ? 659.25f : 783.99f);
        phase+=6.28318530718*note/44100;
        const float sine=static_cast<float>(std::sin(phase));
        const float harmonic=static_cast<float>(std::sin(phase*2.002));
        const float thud=std::sin(time*95.0f*6.28318530718f)*std::exp(-time*24);
        const float envelope=(std::min)(time/.009f,1.0f)*(1-t)*(1-t);
        const float wave=kind==0 ? sine*.26f+thud*.6f : sine*.3f+harmonic*.09f;
        const int16_t sample=static_cast<int16_t>(wave*envelope*16000);
        std::memcpy(data.buffer.data()+i*sizeof(sample),&sample,sizeof(sample));
    }
    return data;
}
void GameOver::Begin(Camera* camera,const Vector3& position,const Vector3& rotation,bool fall) {
    initialPosition_=position_=position; initialRotation_=rotation_=rotation;
    cameraPosition_=camera->GetPosition(); cameraRotation_=camera->GetRotation(); cameraFov_=camera->GetFov();
    flow_.Begin(); fall_=fall; titleSoundPlayed_=false; selectionY_=425;
    frame_=GameOverTimeline::Evaluate(0);
    lastUpdate_=std::chrono::steady_clock::now();
    sound_.StopAllSoundEffects(); sound_.SoundPlay(failureSound_,.65f);
    UpdateSprites();
}
GameOver::Action GameOver::Update(Camera* camera,const Input& input,bool visible) {
    if (!flow_.Active()) return Action::None;
    const auto now=std::chrono::steady_clock::now();
    const float delta=(std::max)(std::chrono::duration<float>(now-lastUpdate_).count(),0.0f);
    lastUpdate_=now;
    if (!visible || flow_.Leaving()) return Action::None;
    sound_.CleanupFinishedSoundEffects();
    const bool wasRevealing=flow_.Revealing();
    flow_.Advance(delta);
    if (wasRevealing) { UpdateSprites(); return Action::None; }
    frame_=GameOverTimeline::Evaluate(flow_.Elapsed());
    const auto pose=GameOverTimeline::EvaluatePose(flow_.Elapsed(),fall_,initialPosition_,initialRotation_,
        cameraPosition_,cameraRotation_,cameraFov_);
    position_=pose.position; rotation_=pose.rotation;
    camera->SetPosition(pose.cameraPosition); camera->SetRotation(pose.cameraRotation);
    camera->SetFov(pose.fov); camera->Update();
    selectionY_+=(flow_.Selected()==0 ? 425-selectionY_ : 505-selectionY_)*(1-std::exp(-delta*18));
    if (!titleSoundPlayed_ && flow_.Elapsed()>=2.1f) { sound_.SoundPlay(titleSound_,.4f); titleSoundPlayed_=true; }
    UpdateSprites();
    if (!flow_.MenuReady()) return Action::None;
    if (input.TriggerKey(DIK_W) || input.TriggerKey(DIK_S) || input.TriggerKey(DIK_UP) || input.TriggerKey(DIK_DOWN) ||
        input.TriggerControllerButton(XINPUT_GAMEPAD_DPAD_UP) || input.TriggerControllerButton(XINPUT_GAMEPAD_DPAD_DOWN)) {
        flow_.MoveSelection(); sound_.SoundPlay(cursorSound_,.5f); UpdateSprites();
    }
    const bool back=input.TriggerKey(DIK_ESCAPE) || input.TriggerControllerButton(XINPUT_GAMEPAD_B);
    const bool confirm=input.TriggerKey(DIK_SPACE) || input.TriggerKey(DIK_RETURN) || input.TriggerControllerButton(XINPUT_GAMEPAD_A);
    if (!back && !confirm) return Action::None;
    const auto action=flow_.Confirm(back);
    if (action==Action::StageSelect) { sound_.SoundPlay(confirmSound_,.5f); UpdateSprites(); return action; }
    // 真っ黒な状態で呼出元がリスポーンし、そのあと幕だけを開く。
    camera->SetPosition(cameraPosition_); camera->SetRotation(cameraRotation_); camera->SetFov(cameraFov_); camera->Update();
    sound_.SoundPlay(confirmSound_,.5f); UpdateSprites();
    return Action::Retry;
}
void GameOver::UpdateSprites() {
    const float radius=flow_.Revealing() ? GameOverTimeline::RetryRadius(flow_.RevealElapsed()) : frame_.radius;
    for (int row=0;row<90;++row) {
        const float y=static_cast<float>(row)*8;
        // 帯の上下で狭い方の穴を採り、円が閉じた時の隙間を防ぐ。
        const float dy=(std::max)(std::abs(y-360),std::abs(y+8-360));
        const float half=std::sqrt((std::max)(0.0f,radius*radius-dy*dy));
        const float width=std::clamp(640-half,0.0f,640.0f);
        for (int side=0;side<2;++side) {
            auto& strip=iris_[row*2+side];
            strip->SetPosition({side==0 ? 0.0f : 1280-width,y}); strip->SetSize({width,8});
            strip->SetColor({.008f,.012f,.025f,1}); strip->Update();
        }
    }
    flash_->SetPosition({0,0}); flash_->SetSize({1280,720});
    flash_->SetColor({1,.22f,.12f,flow_.Revealing() ? 0.0f : frame_.flash}); flash_->Update();
    const bool visible=flow_.Active() && !flow_.Revealing() && !flow_.Leaving();
    const float time=flow_.Elapsed();
    const float alpha=visible ? frame_.menuAlpha : 0;
    const float backgroundAlpha=visible ? std::clamp((time-1.95f)/.4f,0.0f,1.0f) : 0;
    backdrop_->SetPosition({0,0}); backdrop_->SetSize({1280,720});
    backdrop_->SetColor({1,1,1,backgroundAlpha}); backdrop_->Update();
    sigil_->SetPosition({640,250}); sigil_->SetSize({470,470}); sigil_->SetRotation(time*.055f);
    sigil_->SetColor({.65f,.78f,1,.2f*backgroundAlpha}); sigil_->Update();
    // 金属調の文字を一字ずつ落とし、わずかな跳ねで静止させる。
    for (size_t i=0;i<letters_.size();++i) {
        const auto letter=GameOverTimeline::EvaluateLetter(time,static_cast<int>(i));
        const float spacing=static_cast<float>(i)*92+(i>=4 ? 30.0f : 0.0f);
        letters_[i]->SetPosition({303+spacing,220+letter.offsetY});
        letters_[i]->SetSize({110*letter.scale,146*letter.scale}); letters_[i]->SetRotation(letter.rotation);
        letters_[i]->SetColor({1,1,1,visible ? letter.alpha : 0}); letters_[i]->Update();
    }
    const Vector2 positions[]={{640,220},{640,320},{640,425},{640,505},{640,628}};
    const Vector2 sizes[]={{900,150},{800,65},{600,70},{600,70},{900,56}};
    for (size_t i=1;i<labels_.size();++i) {
        const float slide=i==2 ? 18*(1-alpha) : i==3 ? 30*(1-alpha) : 0;
        labels_[i]->SetPosition({positions[i].x,positions[i].y+slide}); labels_[i]->SetSize(sizes[i]);
        const float dim=(i==2 || i==3) && static_cast<int>(i)-2!=flow_.Selected() ? .48f : 1;
        labels_[i]->SetColor({1,1,1,alpha*dim}); labels_[i]->Update();
    }
    selection_->SetPosition({640,selectionY_}); selection_->SetSize({670,82});
    selection_->SetColor({1,1,1,alpha*(.88f+.08f*std::sin(time*3))}); selection_->Update();
    for (size_t i=0;i<borders_.size();++i) {
        borders_[i]->SetPosition({640,i==0 ? 347.0f : 574.0f});
        borders_[i]->SetSize({520*alpha,1}); borders_[i]->SetColor({.65f,.56f,.33f,.35f*alpha}); borders_[i]->Update();
    }
    // 暗転に吸い込まれる金色の欠片。接触点のフラッシュと幕を視覚的につなぐ。
    const float burst=std::clamp((time-.18f)/1.15f,0.0f,1.0f);
    for (size_t i=0;i<sparks_.size();++i) {
        const float angle=static_cast<float>(i)*2.39996323f;
        const float distance=(45+static_cast<float>(i%5)*22)*std::sin(burst*3.14159265359f);
        sparks_[i]->SetPosition({640+std::cos(angle+burst*.8f)*distance,
            350+std::sin(angle+burst*.8f)*distance*.7f});
        sparks_[i]->SetSize({2+5*(1-burst),2+5*(1-burst)}); sparks_[i]->SetRotation(angle+time);
        sparks_[i]->SetColor({1,.78f,.35f,visible && time>.18f ? (1-burst)*.65f : 0}); sparks_[i]->Update();
    }

}
void GameOver::Draw() {
    if (!flow_.Active()) return;
    flash_->Draw();
    for (auto& strip:iris_) strip->Draw();
    backdrop_->Draw(); sigil_->Draw();
    for (auto& spark:sparks_) spark->Draw();
    for (auto& letter:letters_) letter->Draw();
    for (auto& border:borders_) border->Draw();
    selection_->Draw();
    for (size_t i=1;i<labels_.size();++i) labels_[i]->Draw();
}
void GameOver::Cancel(Camera* camera) {
    if (flow_.Active() && !flow_.Revealing() && camera) {
        camera->SetPosition(cameraPosition_); camera->SetRotation(cameraRotation_); camera->SetFov(cameraFov_); camera->Update();
    }
    flow_.Cancel(); sound_.StopAllSoundEffects();
}
void GameOver::Finalize() { sound_.StopAllSoundEffects(); sound_.Finalize(); }

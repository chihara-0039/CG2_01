#include "GameOver.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
float Mix(float a,float b,float t) { return a+(b-a)*t; }
}
void GameOver::Initialize(SpriteCommon* common,const std::array<uint32_t,5>& textures) {
    const auto make=[common](uint32_t texture) {
        auto sprite=std::make_unique<Sprite>(); sprite->Initialize(common,texture); return sprite;
    };
    for (auto& strip:iris_) strip=make(0);
    for (size_t i=0;i<labels_.size();++i) {
        labels_[i]=make(textures[i]); labels_[i]->SetAnchorPoint({.5f,.5f});
    }
    flash_=make(0); selection_=make(0);
    sound_.Initialize(); failureSound_=MakeSound();
}
Sound::SoundData GameOver::MakeSound() {
    Sound::SoundData data{};
    data.wfex.wFormatTag=WAVE_FORMAT_PCM; data.wfex.nChannels=1;
    data.wfex.nSamplesPerSec=44100; data.wfex.wBitsPerSample=16;
    data.wfex.nBlockAlign=2; data.wfex.nAvgBytesPerSec=88200;
    const int samples=44100;
    data.buffer.resize(samples*sizeof(int16_t));
    double phase=0;
    for (int i=0;i<samples;++i) {
        const float time=static_cast<float>(i)/44100;
        // 短い低音の衝撃から、音程が落ちる余韻へつなぐ。
        phase+=6.28318530718*(440.0f*std::exp(-time*3.0f))/44100;
        const float wave=static_cast<float>(std::sin(phase));
        const float thud=std::sin(time*95.0f*6.28318530718f)*std::exp(-time*20);
        const float envelope=(std::min)(time/.008f,1.0f)*(1-time)*(1-time);
        const int16_t sample=static_cast<int16_t>((wave*.35f+thud*.6f)*envelope*16000);
        std::memcpy(data.buffer.data()+i*sizeof(sample),&sample,sizeof(sample));
    }
    return data;
}
void GameOver::Begin(Camera* camera,const Vector3& position,const Vector3& rotation,bool fall) {
    initialPosition_=position_=position; initialRotation_=rotation_=rotation;
    cameraPosition_=camera->GetPosition(); cameraRotation_=camera->GetRotation(); cameraFov_=camera->GetFov();
    flow_.Begin(); fall_=fall;
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
    // 接触は小さく弾かれて倒れる。落下は身体を回しながら下へ消える。
    const float motion=frame_.motion;
    position_=initialPosition_;
    position_.y+=fall_ ? -4*motion : .65f*std::sin(motion*3.14159265359f);
    rotation_=initialRotation_; rotation_.z+=motion*(fall_ ? 2.4f : 1.4f);
    const float yaw=cameraRotation_.y;
    const Vector3 focus{position_.x-std::sin(yaw)*8,position_.y+3.5f,position_.z-std::cos(yaw)*8};
    const float blend=frame_.cameraBlend;
    const Vector3 cam{Mix(cameraPosition_.x,focus.x,blend),Mix(cameraPosition_.y,focus.y,blend),Mix(cameraPosition_.z,focus.z,blend)};
    const Vector3 direction{position_.x-cam.x,position_.y+.4f-cam.y,position_.z-cam.z};
    const float pitch=-std::atan2(direction.y,std::sqrt(direction.x*direction.x+direction.z*direction.z));
    const float aimYaw=std::atan2(direction.x,direction.z);
    camera->SetPosition(cam);
    camera->SetRotation({Mix(cameraRotation_.x,pitch,blend),
        cameraRotation_.y+std::remainder(aimYaw-cameraRotation_.y,6.28318530718f)*blend,
        cameraRotation_.z+.16f*motion});
    camera->SetFov(Mix(cameraFov_,.65f,blend)); camera->Update();
    UpdateSprites();
    if (!flow_.MenuReady()) return Action::None;
    if (input.TriggerKey(DIK_W) || input.TriggerKey(DIK_S) || input.TriggerKey(DIK_UP) || input.TriggerKey(DIK_DOWN) ||
        input.TriggerControllerButton(XINPUT_GAMEPAD_DPAD_UP) || input.TriggerControllerButton(XINPUT_GAMEPAD_DPAD_DOWN)) {
        flow_.MoveSelection(); UpdateSprites();
    }
    const bool back=input.TriggerKey(DIK_ESCAPE) || input.TriggerControllerButton(XINPUT_GAMEPAD_B);
    const bool confirm=input.TriggerKey(DIK_SPACE) || input.TriggerKey(DIK_RETURN) || input.TriggerControllerButton(XINPUT_GAMEPAD_A);
    if (!back && !confirm) return Action::None;
    const auto action=flow_.Confirm(back);
    if (action==Action::StageSelect) { UpdateSprites(); return action; }
    // 真っ黒な状態で呼出元がリスポーンし、そのあと幕だけを開く。
    camera->SetPosition(cameraPosition_); camera->SetRotation(cameraRotation_); camera->SetFov(cameraFov_); camera->Update();
    sound_.StopAllSoundEffects(); UpdateSprites();
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
    const float alpha=(flow_.Revealing() || flow_.Leaving() || !flow_.Active()) ? 0.0f : frame_.menuAlpha;
    const float pop=1+.08f*(1-frame_.menuAlpha);
    const Vector2 positions[]={{640,225},{640,320},{640,425},{640,505},{640,620}};
    const Vector2 sizes[]={{900*pop,150*pop},{800,65},{600,70},{600,70},{900,56}};
    for (size_t i=0;i<labels_.size();++i) {
        labels_[i]->SetPosition(positions[i]); labels_[i]->SetSize(sizes[i]);
        const float dim=(i==2 || i==3) && static_cast<int>(i)-2!=flow_.Selected() ? .5f : 1;
        labels_[i]->SetColor({1,1,1,alpha*dim}); labels_[i]->Update();
    }
    selection_->SetPosition({325,flow_.Selected()==0 ? 389.0f : 469.0f}); selection_->SetSize({630,72});
    selection_->SetColor({.2f,.38f,.5f,.45f*alpha}); selection_->Update();
}
void GameOver::Draw() {
    if (!flow_.Active()) return;
    flash_->Draw();
    for (auto& strip:iris_) strip->Draw();
    selection_->Draw();
    for (auto& label:labels_) label->Draw();
}
void GameOver::Cancel(Camera* camera) {
    if (flow_.Active() && !flow_.Revealing() && camera) {
        camera->SetPosition(cameraPosition_); camera->SetRotation(cameraRotation_); camera->SetFov(cameraFov_); camera->Update();
    }
    flow_.Cancel(); sound_.StopAllSoundEffects();
}
void GameOver::Finalize() { sound_.StopAllSoundEffects(); sound_.Finalize(); }

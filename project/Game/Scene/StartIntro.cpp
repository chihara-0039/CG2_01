#include "StartIntro.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void StartIntro::Initialize(SpriteCommon* common, uint32_t cloudTexture) {
    for (auto& bar : bars_) {
        bar = std::make_unique<Sprite>(); bar->Initialize(common,0);
    }
    for (auto& cloud : clouds_) {
        cloud = std::make_unique<Sprite>(); cloud->Initialize(common,cloudTexture);
        cloud->SetAnchorPoint({.5f,.5f});
    }
    sound_.Initialize();
    wind_ = MakeTone(false);
    landing_ = MakeTone(true);
}

Sound::SoundData StartIntro::MakeTone(bool landing) {
    Sound::SoundData data{};
    data.wfex.wFormatTag = WAVE_FORMAT_PCM;
    data.wfex.nChannels = 1;
    data.wfex.nSamplesPerSec = 44100;
    data.wfex.wBitsPerSample = 16;
    data.wfex.nBlockAlign = 2;
    data.wfex.nAvgBytesPerSec = 88200;
    const float duration = landing ? .42f : 1.6f;
    const int samples = static_cast<int>(44100*duration);
    data.buffer.resize(samples*sizeof(int16_t));
    uint32_t noise = 1234567;
    float filtered = 0;
    double phase = 0;
    for (int i=0;i<samples;++i) {
        const float time = static_cast<float>(i)/44100;
        noise = noise*1664525u+1013904223u;
        const float white = static_cast<float>((noise>>8)&65535)/32767.5f-1;
        filtered += .045f*(white-filtered);
        phase += 6.28318530718*(landing ? 95*std::exp(-time*9) : 180)/44100;
        const float envelope = landing ? (std::min)(time/.008f,1.0f)*std::exp(-time*12)
            : std::sin(time/duration*3.14159265359f);
        const float value = landing ? .65f*std::sin(phase)+filtered : filtered;
        const int16_t sample = static_cast<int16_t>(value*envelope*14000);
        std::memcpy(data.buffer.data()+i*sizeof(sample),&sample,sizeof(sample));
    }
    return data;
}

void StartIntro::Begin(Camera* camera, const Vector3& center, float radius, const Vector3& spawn) {
    elapsed_ = 0; active_ = true; goPending_ = false; windStarted_ = false;
    stageCenter_ = center; stageRadius_ = radius; spawn_ = spawn;
    gameCamera_ = {camera->GetPosition(),camera->GetRotation(),camera->GetFov()};
    cameraFarClip_ = camera->GetFarClip();
    frame_ = StageArrival::Evaluate(0,center,radius,spawn,gameCamera_);
    camera->SetPosition(frame_.camera.position); camera->SetRotation(frame_.camera.rotation);
    camera->SetFov(frame_.camera.fov);
    camera->SetFarClip((std::max)(cameraFarClip_,radius*6+80)); camera->Update();
    lastUpdate_ = std::chrono::steady_clock::now();
    UpdateSprites();
}

void StartIntro::Update(Camera* camera, bool visible) {
    if (!active_) return;
    const auto now = std::chrono::steady_clock::now();
    const float delta = (std::max)(std::chrono::duration<float>(now-lastUpdate_).count(),0.0f);
    lastUpdate_ = now;
    // 黒幕の裏と非アクティブ中は進めない。復帰時に降下を飛ばさない。
    if (!visible) return;
    if (!windStarted_) { sound_.SoundPlay(wind_,.4f); windStarted_ = true; }
    const bool wasLanded = frame_.landed;
    elapsed_ += delta;
    frame_ = StageArrival::Evaluate(elapsed_,stageCenter_,stageRadius_,spawn_,gameCamera_);
    if (!wasLanded) {
        camera->SetPosition(frame_.camera.position); camera->SetRotation(frame_.camera.rotation);
        camera->SetFov(frame_.camera.fov); camera->Update();
    }
    if (!wasLanded && frame_.landed) {
        sound_.SoundPlay(landing_,.7f); goPending_ = true;
        camera->SetFarClip(cameraFarClip_); camera->Update();
    }
    sound_.CleanupFinishedSoundEffects();
    UpdateSprites();
    if (frame_.finished) active_ = false;
}

void StartIntro::Cancel(Camera* camera) {
    if (active_ && !frame_.landed && camera) {
        camera->SetPosition(gameCamera_.position); camera->SetRotation(gameCamera_.rotation);
        camera->SetFov(gameCamera_.fov); camera->SetFarClip(cameraFarClip_); camera->Update();
    }
    active_ = false; goPending_ = false; sound_.StopAllSoundEffects();
}
bool StartIntro::ConsumeGo() { const bool result=goPending_; goPending_=false; return result; }
void StartIntro::Finalize() { sound_.StopAllSoundEffects(); sound_.Finalize(); }

void StartIntro::UpdateSprites() {
    for (size_t i=0;i<bars_.size();++i) {
        bars_[i]->SetPosition({0,i==0 ? 0.0f : 720-44*frame_.letterbox});
        bars_[i]->SetSize({1280,44*frame_.letterbox});
        bars_[i]->SetColor({0,0,0,.88f}); bars_[i]->Update();
    }
    // 半透明の雲が左右へ流れ、奥のステージと手前のプレイヤーを開示する。
    const float spread = std::clamp(elapsed_/1.25f,0.0f,1.0f);
    for (size_t i=0;i<clouds_.size();++i) {
        const float angle = static_cast<float>(i)*.78539816339f;
        clouds_[i]->SetPosition({640+std::cos(angle)*(170+750*spread),
            360+std::sin(angle)*(100+440*spread)});
        clouds_[i]->SetSize({1100,720});
        clouds_[i]->SetColor({.92f,.97f,1,frame_.cloudAlpha*.8f});
        clouds_[i]->Update();
    }
}
void StartIntro::Draw() {
    if (!active_) return;
    for (auto& cloud : clouds_) cloud->Draw();
    for (auto& bar : bars_) bar->Draw();
}

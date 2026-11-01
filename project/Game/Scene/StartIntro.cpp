#include "StartIntro.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void StartIntro::Initialize(SpriteCommon* common, uint32_t cloudTexture, uint32_t starTexture, Object3dCommon* objectCommon) {
    for (auto& bar : bars_) {
        bar = std::make_unique<Sprite>(); bar->Initialize(common,0);
    }
    for (auto& cloud : clouds_) {
        cloud = std::make_unique<Sprite>(); cloud->Initialize(common,cloudTexture);
        cloud->SetAnchorPoint({.5f,.5f});
    }
    for(auto& star:trailStars_){star=std::make_unique<Sprite>();star->Initialize(common,starTexture);star->SetAnchorPoint({.5f,.5f});}
    launchCloudModel_=Model::CreateFromOBJ(objectCommon->GetDxCommon(),"Resources/Models/Diorama","cloud.obj",objectCommon->GetTextureManager());
    for(auto& cloud:launchCloudObjects_){cloud=std::make_unique<Object3d>();cloud->Initialize(objectCommon);cloud->SetModel(launchCloudModel_.get());cloud->SetColor({1,1,1,1});cloud->SetShininess(.04f);cloud->SetMetallic(0);}
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
        // 位相はdoubleで蓄積し、波形を音量計算へ渡す時点でfloatへ明示変換する。
        const float wave = static_cast<float>(std::sin(phase));
        const float value = landing ? .65f*wave+filtered : filtered;
        const int16_t sample = static_cast<int16_t>(value*envelope*14000);
        std::memcpy(data.buffer.data()+i*sizeof(sample),&sample,sizeof(sample));
    }
    return data;
}

void StartIntro::Begin(Camera* camera, const Vector3& center, float radius, const Vector3& spawn, const Vector3& spawnRotation) {
    elapsed_ = 0; active_ = true; goPending_ = false; windStarted_ = false;
    stageCenter_ = center; stageRadius_ = radius; spawn_ = spawn;
    spawnRotation_ = spawnRotation; lastCloudAt_ = -1;
    gameCamera_ = {camera->GetPosition(),camera->GetRotation(),camera->GetFov()};
    cameraFarClip_ = camera->GetFarClip();
    frame_ = StageArrival::Evaluate(0,center,radius,spawn,gameCamera_,spawnRotation_);
    launchCloudPosition_=frame_.playerPosition;
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
    frame_ = StageArrival::Evaluate(elapsed_,stageCenter_,stageRadius_,spawn_,gameCamera_,spawnRotation_);
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
    // Star streaks open outward during acceleration and disappear before landing.
    const float cruise=std::clamp(elapsed_/StageArrival::kOrbitDuration,0.0f,1.0f);
    const float speed=std::sin(cruise*3.14159265359f);
    for(size_t i=0;i<trailStars_.size();++i){
        const float index=static_cast<float>(i),angle=index*2.39996323f;
        const float phase=std::fmod(elapsed_*.7f+index*.137f,1.0f);
        trailStars_[i]->SetPosition({640+std::cos(angle)*(200+phase*570),360+std::sin(angle)*(120+phase*350)});
        const float size=9+phase*18;
        trailStars_[i]->SetSize({size,size*(1+2*speed)});trailStars_[i]->SetRotation(angle-1.5707963f);
        trailStars_[i]->SetColor({.65f+.35f*(i%2),.85f,1,speed*(1-phase)});trailStars_[i]->Update();
    }
    // 半透明の雲が左右へ流れ、奥のステージと手前のプレイヤーを開示する。
    const float spread = std::clamp(elapsed_/.65f,0.0f,1.0f);
    for (size_t i=0;i<clouds_.size();++i) {
        const float angle = static_cast<float>(i)*.78539816339f;
        clouds_[i]->SetPosition({640+std::cos(angle)*(550+750*spread),
            360+std::sin(angle)*(310+440*spread)});
        clouds_[i]->SetSize({750,420});
        clouds_[i]->SetColor({.92f,.97f,1,frame_.cloudAlpha*.25f});
        clouds_[i]->Update();
    }
}
void StartIntro::Draw() {
    if (!active_) return;
    for (auto& star : trailStars_) star->Draw();
    for (auto& cloud : clouds_) cloud->Draw();
    for (auto& bar : bars_) bar->Draw();
}

int StartIntro::ConsumeFlightCloud() {
    if (!BlocksInput() || elapsed_<=0 || elapsed_>StageArrival::kOrbitDuration) return 0;
    if (lastCloudAt_>=0 && elapsed_-lastCloudAt_<.12f) return 0;
    const int kind=1; // The dense launch cloud is a world-space stage mesh.
    // 低FPSや復帰時に過去の発生分を一度に積まない。
    lastCloudAt_=elapsed_;
    return kind;
}

void StartIntro::DrawLaunchCloud(const Matrix4x4& view,const Matrix4x4& projection,const Matrix4x4& lightVP) {
    if(!active_ || elapsed_>=2.0f)return;
    const float fade=std::clamp((elapsed_-1.1f)/.9f,0.0f,1.0f);
    const float scale=1-fade*fade*(3-2*fade);
    const Vector3 offsets[]={{0,.6f,0},{-1.8f,.1f,0},{1.8f,.25f,0},{0,.3f,-1.6f},{0,.4f,1.6f}};
    for(size_t i=0;i<launchCloudObjects_.size();++i){
        auto& cloud=launchCloudObjects_[i];const auto o=offsets[i];
        cloud->SetPosition({launchCloudPosition_.x+o.x,launchCloudPosition_.y+o.y,launchCloudPosition_.z+o.z});
        const float size=(i==0?3.2f:2.5f)*scale;
        cloud->SetScale({size,size*.8f,size});cloud->SetCamera(view,projection);cloud->Update(lightVP);cloud->Draw();
    }
}

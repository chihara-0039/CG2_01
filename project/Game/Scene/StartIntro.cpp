#include "StartIntro.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

namespace {
// 小さなブロック体を矩形で描く。追加画像やフォント環境に依存しない。
std::array<int,7> Glyph(char c) {
    switch (c) {
    case 'R': return {30,17,17,30,20,18,17};
    case 'E': return {31,16,16,30,16,16,31};
    case 'A': return {14,17,17,31,17,17,17};
    case 'D': return {30,17,17,17,17,17,30};
    case 'Y': return {17,17,10,4,4,4,4};
    case '3': return {30,1,1,14,1,1,30};
    case '2': return {14,17,1,2,4,8,31};
    case '1': return {4,12,4,4,4,4,14};
    case 'G': return {14,17,16,23,17,17,14};
    case 'O': return {14,17,17,17,17,17,14};
    case '!': return {4,4,4,4,4,0,4};
    default: return {};
    }
}
}

void StartIntro::Initialize(SpriteCommon* common) {
    const auto make = [common]() {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(common, 0);
        return sprite;
    };
    for (auto& pixel : pixels_) pixel = make();
    for (auto& bar : bars_) bar = make();
    for (auto& line : frameLines_) { line = make(); line->SetAnchorPoint({.5f,.5f}); }
    backdrop_ = make();
    sound_.Initialize();
    tick_ = MakeTone(false);
    go_ = MakeTone(true);
}

Sound::SoundData StartIntro::MakeTone(bool go) {
    Sound::SoundData data{};
    data.wfex.wFormatTag = WAVE_FORMAT_PCM;
    data.wfex.nChannels = 1;
    data.wfex.nSamplesPerSec = 44100;
    data.wfex.wBitsPerSample = 16;
    data.wfex.nBlockAlign = 2;
    data.wfex.nAvgBytesPerSec = 88200;
    const float duration = go ? .38f : .11f;
    const int samples = static_cast<int>(44100 * duration);
    data.buffer.resize(samples * sizeof(int16_t));
    double phase = 0;
    for (int i = 0; i < samples; ++i) {
        const float time = static_cast<float>(i)/44100;
        const float frequency = go ? (time < .10f ? 660.0f : time < .20f ? 880.0f : 1320.0f) : 880.0f;
        const float envelope = (std::min)(time/.008f, 1.0f) * (std::min)((duration-time)/.035f, 1.0f);
        phase += 6.28318530718 * frequency / 44100;
        const int16_t value = static_cast<int16_t>(std::sin(phase)*envelope*9000);
        std::memcpy(data.buffer.data()+i*sizeof(value), &value, sizeof(value));
    }
    return data;
}

void StartIntro::Begin(Camera* camera) {
    elapsed_ = 0;
    lastCue_ = -1;
    active_ = true;
    goPending_ = false;
    cameraFov_ = camera->GetFov();
    frame_ = StartIntroTimeline::Evaluate(0);
    lastUpdate_ = std::chrono::steady_clock::now();
    UpdateSprites();
}

void StartIntro::Update(Camera* camera, bool visible) {
    if (!active_) return;
    const auto now = std::chrono::steady_clock::now();
    // 演出時計は実経過時間を使う。0.1秒で打ち切ると低FPS時だけ拍が伸びる。
    const float delta = (std::max)(std::chrono::duration<float>(now-lastUpdate_).count(), 0.0f);
    lastUpdate_ = now;
    // 黒い幕の裏と非アクティブ中は進めず、カウントを見逃させない。
    if (!visible) return;
    elapsed_ += delta;
    frame_ = StartIntroTimeline::Evaluate(elapsed_);
    if (frame_.cue != lastCue_) {
        if (frame_.cue >= 1) sound_.SoundPlay(frame_.cue == 4 ? go_ : tick_, .55f);
        if (frame_.cue == 4) goPending_ = true;
        lastCue_ = frame_.cue;
    }
    sound_.CleanupFinishedSoundEffects();
    if (frame_.blocksInput) {
        camera->SetFov(cameraFov_+.12f*(1-frame_.cameraBlend));
        camera->Update();
    } else if (goPending_) {
        camera->SetFov(cameraFov_);
        camera->Update();
    }
    UpdateSprites();
    if (frame_.finished) active_ = false;
}

void StartIntro::Cancel(Camera* camera) {
    if (active_ && frame_.blocksInput && camera) { camera->SetFov(cameraFov_); camera->Update(); }
    active_ = false;
    goPending_ = false;
    sound_.StopAllSoundEffects();
}

bool StartIntro::ConsumeGo() {
    const bool result = goPending_;
    goPending_ = false;
    return result;
}

void StartIntro::Finalize() {
    sound_.StopAllSoundEffects();
    sound_.Finalize();
}

void StartIntro::UpdateSprites() {
    constexpr std::string_view words[] = {"READY", "3", "2", "1", "GO!"};
    const auto word = words[frame_.cue];
    const float pixel = (frame_.cue == 0 ? 10.0f : 17.0f)*frame_.scale;
    const float width = (static_cast<float>(word.size())*6-1)*pixel;
    const float left = 640-width*.5f;
    const Vector4 color = frame_.cue == 4 ? Vector4{1.0f,.80f,.28f,frame_.alpha} : Vector4{.56f,.90f,1.0f,frame_.alpha};
    pixelCount_ = 0;
    for (size_t c = 0; c < word.size(); ++c) {
        const auto glyph = Glyph(word[c]);
        for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col) {
            if (!(glyph[row] & (1<<(4-col))) || pixelCount_ >= pixels_.size()) continue;
            auto& sprite = pixels_[pixelCount_++];
            sprite->SetPosition({left+(static_cast<float>(c)*6+col)*pixel, 360+(row-3.5f)*pixel});
            sprite->SetSize({pixel*.94f,pixel*.94f});
            sprite->SetColor(color);
            sprite->Update();
        }
    }
    for (size_t i = 0; i < bars_.size(); ++i) {
        bars_[i]->SetPosition({0, i == 0 ? 0.0f : 720-44*frame_.letterbox});
        bars_[i]->SetSize({1280,44*frame_.letterbox});
        bars_[i]->SetColor({0,0,0,.88f});
        bars_[i]->Update();
    }
    backdrop_->SetPosition({0,250}); backdrop_->SetSize({1280,220});
    backdrop_->SetColor({.015f,.025f,.05f,.40f*frame_.alpha}); backdrop_->Update();
    const float angle = elapsed_*.7f;
    const float radius = (frame_.cue == 0 || frame_.cue == 4 ? 195.0f : 120.0f)
        -20.0f*frame_.cameraBlend;
    for (size_t i = 0; i < frameLines_.size(); ++i) {
        const float rotation = angle+static_cast<float>(i)*1.57079632679f;
        frameLines_[i]->SetPosition({640+std::cos(rotation)*radius,360+std::sin(rotation)*radius});
        frameLines_[i]->SetSize({frame_.cue == 0 ? 20.0f : 44.0f,3});
        frameLines_[i]->SetRotation(rotation+1.57079632679f);
        frameLines_[i]->SetColor(color); frameLines_[i]->Update();
    }
}

void StartIntro::Draw() {
    if (!active_) return;
    for (auto& bar : bars_) bar->Draw();
    backdrop_->Draw();
    for (auto& line : frameLines_) line->Draw();
    for (size_t i = 0; i < pixelCount_; ++i) pixels_[i]->Draw();
}

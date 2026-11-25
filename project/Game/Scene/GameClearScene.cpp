#include "GameClearScene.h"
#include "GameRuntime.h"
#include <cmath>
#include <cstring>

// 演出の用途に合わせたPCM波形を生成し、再生用の音声データへまとめる。
Sound::SoundData GameClearScene::MakeSound(bool stamp) {
    Sound::SoundData data{};
    data.wfex.wFormatTag = WAVE_FORMAT_PCM;
    data.wfex.nChannels = 1;
    data.wfex.nSamplesPerSec = 44100;
    data.wfex.wBitsPerSample = 16;
    data.wfex.nBlockAlign = 2;
    data.wfex.nAvgBytesPerSec = 88200;
    const int count = static_cast<int>(44100 * (stamp ? .30f : .40f));
    data.buffer.resize(count * 2);
    uint32_t seed = 39;
    float filtered = 0;
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / 44100;
        seed = seed * 1664525u + 1013904223u;
        const float noise = static_cast<float>((seed >> 8) & 65535) / 32767.5f - 1;
        filtered += .19f * (noise - filtered);
        const float sample = stamp ? (.65f * std::sin(t * 590) * std::exp(-t * 35) + filtered * .7f * std::exp(-t * 55))
                                   : (noise - filtered) * .18f * std::sin(t / .40f * 3.14159265f);
        const int16_t value = static_cast<int16_t>(std::clamp(sample, -1.0f, 1.0f) * 23000);
        std::memcpy(data.buffer.data() + i * 2, &value, 2);
    }
    return data;
}
// クリア手帳を準備し、初期値と必要なリソースを設定する。
void GameClearScene::Initialize(GameRuntime &game) {
    game.OnSceneEntered(SceneType::GameClear);
    elapsed_ = 0;
    inspection_ = {};
    lastUpdate_ = std::chrono::steady_clock::now();
    auto *common = game.GetObject3dCommon();
    const int index = std::clamp(game.GetResultPageIndex(), 0, 6);
    const std::string names[Count] = {"back",
                                      "pages",
                                      "report-" + std::to_string(index),
                                      "cover",
                                      "cover-art-" + std::to_string(index),
                                      "map-" + std::to_string(index),
                                      "ink",
                                      "shadow",
                                      "stamp"};
    for (int i = 0; i < Count; ++i) {
        models_[i] = Model::CreateFromOBJ(common->GetDxCommon(), "Resources/Models/Journal", names[i] + ".obj",
                                          common->GetTextureManager());
        objects_[i] = std::make_unique<Object3d>();
        objects_[i]->Initialize(common);
        objects_[i]->SetModel(models_[i].get());
        objects_[i]->SetEnableLighting(false);
    }
    camera_.SetPosition({0, 1.8f, -18});
    camera_.SetRotation({.10f, 0, 0});
    camera_.SetFov(.56f);
    camera_.Update();
    stampParticles_ = std::make_unique<ParticleManager>();
    stampParticles_->Initialize(common->GetDxCommon(), common->GetTextureManager());
    stampParticles_->SetAmbientCloudEmitterActive(false);
    stampParticles_->SetWeatherEmitterActive(false);
    stampParticles_->SetDrawGPUParticleSphere(false);
    sound_.Initialize();
    pageSound_ = MakeSound(false);
    stampSound_ = MakeSound(true);
    frame_ = JournalResult::Evaluate(0);
}
// 手帳を開いて打刻する時刻・姿勢・効果音を進め、完了後の入力を受け付ける。
void GameClearScene::Update(GameRuntime &game, const SceneUpdateContext &context) {
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::clamp(std::chrono::duration<float>(now - lastUpdate_).count(), 0.0f, .05f);
    lastUpdate_ = now;
    const float previous = elapsed_;
    if (game.IsResultAnimationVisible()) {
        elapsed_ += dt;
    }
    if (JournalResult::Crossed(previous, elapsed_, .45f)) {
        sound_.SoundPlay(pageSound_, .65f);
    }
    if (JournalResult::Crossed(previous, elapsed_, JournalResult::kImpact)) {
        sound_.SoundPlay(stampSound_, .8f);
        stampParticles_->EmitSceneEffect(ParticleManager::SceneEffect::Stamp, {3.65f, -1.65f, -.12f});
    }
    frame_ = JournalResult::Evaluate(elapsed_);
    if (frame_.ready && game.IsResultAnimationVisible()) {
        const auto *input = game.GetInput();
        if ((input->TriggerKey(DIK_TAB) || input->TriggerControllerButton(XINPUT_GAMEPAD_X)) &&
            inspection_.Toggle(frame_.ready)) {
            sound_.SoundPlay(pageSound_, .65f);
        }
        inspection_.Advance(dt);
        frame_.open = inspection_.Open();
    } else if (frame_.ready) {
        // ウィンドウが非アクティブでも、見返し中の表紙の角度を保持する。
        frame_.open = inspection_.Open();
    }
    sound_.CleanupFinishedSoundEffects();
    const Vector3 root{-3.2f * (1 - frame_.open), -9.5f * (1 - frame_.rise) + frame_.shake * .055f, 0};
    for (auto &obj : objects_) {
        obj->SetPosition(root);
        obj->SetRotation({0, 0, frame_.shake * .006f});
        obj->SetScale({1, 1, 1});
        obj->SetColor({1, 1, 1, 1});
    }
    for (const int part : {Cover, CoverArt, Inside}) {
        objects_[part]->SetRotation({0, frame_.open * 3.14159265359f, frame_.shake * .006f});
    }
    objects_[Ink]->SetPosition({root.x + 3.65f, root.y - 1.65f, -.055f});
    objects_[Ink]->SetRotation({0, 0, -.13f});
    const float distance = std::abs(frame_.stampZ + .36f);
    const float spread = 1 + distance * .045f;
    objects_[Shadow]->SetPosition({root.x + 3.65f, root.y - 1.65f, -.042f});
    objects_[Shadow]->SetScale({spread, spread, 1});
    objects_[Shadow]->SetColor({1, 1, 1, .7f / (1 + distance * .4f)});
    objects_[Stamp]->SetPosition({root.x + 3.65f, root.y - 1.65f, frame_.stampZ + .04f * frame_.press});
    objects_[Stamp]->SetRotation({0, 0, -.13f});
    objects_[Stamp]->SetScale({1, 1, 1 - .12f * frame_.press});
    for (auto &obj : objects_) {
        obj->SetCamera(camera_.GetViewMatrix(), camera_.GetProjectionMatrix());
        obj->Update(context.lightViewProjection);
    }
    // 閉じた表紙へ打刻の粒を残さない。打刻の時刻自体は巻き戻さない。
    if (frame_.ready && frame_.open < .99f) {
        stampParticles_->ClearParticles();
    }
    // 非アクティブ・暗転中は紙面の演出も停止し、復帰時に一気に進ませない。
    stampParticles_->Update(game.IsResultAnimationVisible() ? dt : 0, camera_.GetViewMatrix(),
                            camera_.GetProjectionMatrix(), {});
    game.RunGameClearScene(frame_.ready);
}
// クリア手帳の表示対象を選び、描画コマンドを記録する。
void GameClearScene::Draw(GameRuntime &game) {
    (void)game;
    for (const int part : {Back, Pages, Paper, Cover}) {
        objects_[part]->Draw();
    }
    // 外側と内側の印刷を分け、裏面の文字が表紙へ映り込むことを防ぐ。
    if (frame_.open < .5f) {
        objects_[CoverArt]->Draw();
    } else {
        objects_[Inside]->Draw();
    }
    if (frame_.hand) {
        objects_[Shadow]->Draw();
    }
    if (frame_.ink && frame_.open > .02f) {
        objects_[Ink]->Draw();
    }
    if (frame_.hand) {
        objects_[Stamp]->Draw();
    }
    stampParticles_->Draw();
}
// クリア手帳が保持するオブジェクトとリソースを解放する。
void GameClearScene::Finalize(GameRuntime &game) {
    stampParticles_.reset();
    sound_.StopAllSoundEffects();
    sound_.Finalize();
    for (auto &obj : objects_) {
        obj.reset();
    }
    for (auto &model : models_) {
        model.reset();
    }
    game.OnSceneExited(SceneType::GameClear);
}

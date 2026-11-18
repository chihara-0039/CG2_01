#include "StageSelect.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>

namespace {
constexpr std::array<const char *, 4> approvedCourses = {"beginnerTrail.txt", "mechanismGardens.txt",
                                                         "skylineRelay.txt", "expeditionGrandTour.txt"};
}
// ステージ選択を準備し、初期値と必要なリソースを設定する。
void StageSelect::Initialize(Object3dCommon *common, Input *input, int startIndex, bool loadPreview) {
    object3dCommon_ = common;
    input_ = input;
    const auto previousFiles = stageFiles_;
    stageFiles_.clear();
    std::ifstream sequence("Resources/Stages/sequence.txt");
    std::string line;
    while (std::getline(sequence, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (std::find(approvedCourses.begin(), approvedCourses.end(), line) != approvedCourses.end()) {
            stageFiles_.push_back(line);
        }
    }
    if (stageFiles_.empty()) {
        stageFiles_.push_back("beginnerTrail.txt");
    }
    selectedStageIndex_ = std::clamp(startIndex, 0, static_cast<int>(stageFiles_.size()) - 1);
    isFinished_ = false;
    loadFailed_ = false;
    previewAngle_ = 0;
    previewPitch_ = 0;
    previewCamera_.SetPosition({0, 9, -24});
    previewCamera_.SetRotation({.36f, 0, 0});
    previewCamera_.SetFov(.65f);
    previewCamera_.Update();
    // タイトルでは一覧だけを用意し、選択へ入る時にプレビューを読み込む。
    // 同じ一覧で戻ってきた場合はGPUモデルを作り直さない。
    if (!loadPreview ||
        (previousFiles == stageFiles_ && previewModels_.size() == stageFiles_.size() && previewObject_)) {
        return;
    }
    previewModels_.clear();
    for (const auto &name : stageFiles_) {
        const auto index =
            std::distance(approvedCourses.begin(), std::find(approvedCourses.begin(), approvedCourses.end(), name));
        previewModels_.push_back(Model::CreateFromOBJ(common->GetDxCommon(), "Resources/Models/StagePreviews",
                                                      "course-" + std::to_string(index) + ".obj",
                                                      common->GetTextureManager()));
    }
    previewObject_ = std::make_unique<Object3d>();
    previewObject_->Initialize(common);
    previewObject_->SetEnableLighting(false);
    previewObject_->SetPosition({3.2f, 0, 0});
    skyCloudModel_ = Model::CreateFromOBJ(common->GetDxCommon(), "Resources/Models/DioramaGarden", "cloud.obj",
                                          common->GetTextureManager());
    skyClouds_.clear();
    std::mt19937 rng(3911);
    std::uniform_real_distribution<float> scatter(-14, 14);
    for (int i = 0; i < 8; ++i) {
        auto cloud = std::make_unique<Object3d>();
        cloud->Initialize(common);
        cloud->SetModel(skyCloudModel_.get());
        cloud->SetEnableLighting(false);
        cloud->SetScale({2.0f + i * .12f, .7f, 2.0f});
        cloud->SetColor({.86f, .91f, .95f, 1});
        cloud->SetPosition({scatter(rng), -5.0f - std::abs(scatter(rng)) * .25f, scatter(rng)});
        skyClouds_.push_back(std::move(cloud));
    }
}
// ステージとプレビューの操作を受け付け、選択状態と表示を更新する。
void StageSelect::Update() {
    if (stageFiles_.empty()) {
        return;
    }
    const bool previous = input_->TriggerKey(DIK_W) || input_->TriggerKey(DIK_A) || input_->TriggerKey(DIK_UP) ||
                          input_->TriggerKey(DIK_LEFT) || input_->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_UP) ||
                          input_->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_LEFT);
    const bool next = input_->TriggerKey(DIK_S) || input_->TriggerKey(DIK_D) || input_->TriggerKey(DIK_DOWN) ||
                      input_->TriggerKey(DIK_RIGHT) || input_->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_DOWN) ||
                      input_->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_RIGHT);
    if (previous || next) {
        selectedStageIndex_ = (selectedStageIndex_ + (next ? 1 : -1) + static_cast<int>(stageFiles_.size())) %
                              static_cast<int>(stageFiles_.size());
        loadFailed_ = false;
    }
    if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN) ||
        input_->TriggerControllerButton(XINPUT_GAMEPAD_A)) {
        isFinished_ = true;
    }
    const auto mouse = input_->GetMouseState();
    const auto pad = input_->GetGamePadState();
    previewAngle_ += .0025f + pad.rightStickX * .035f;
    previewPitch_ = std::clamp(previewPitch_ + pad.rightStickY * .015f, -.30f, .45f);
    if (mouse.buttons[1]) {
        previewAngle_ += mouse.x * .006f;
        previewPitch_ = std::clamp(previewPitch_ + mouse.y * .004f, -.30f, .45f);
    }
    for (size_t i = 0; i < skyClouds_.size(); ++i) {
        auto p = skyClouds_[i]->GetPosition();
        p.x += .002f + static_cast<float>(i) * .0004f;
        p.z += .0007f;
        if (p.x > 17) {
            p.x = -17;
        }
        if (p.z > 17) {
            p.z = -17;
        }
        skyClouds_[i]->SetPosition(p);
        skyClouds_[i]->SetCamera(previewCamera_.GetViewMatrix(), previewCamera_.GetProjectionMatrix());
        skyClouds_[i]->Update(Math::MakeIdentity4x4());
    }
    previewObject_->SetModel(previewModels_[selectedStageIndex_].get());
    previewObject_->SetRotation({previewPitch_, previewAngle_, 0});
    previewObject_->SetCamera(previewCamera_.GetViewMatrix(), previewCamera_.GetProjectionMatrix());
    previewObject_->Update(Math::MakeIdentity4x4());
}
// ステージ選択の表示対象を選び、描画コマンドを記録する。
void StageSelect::Draw() {
    for (const auto &cloud : skyClouds_) {
        cloud->Draw();
    }
    if (previewObject_) {
        previewObject_->Draw();
    }
}

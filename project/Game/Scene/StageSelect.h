#pragma once
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "Camera.h"
#include "Input.h"
#include <vector>
#include <string>
#include <memory>
#include <array>

// ステージ選択画面の入力、選択中ステージ表示、決定状態を管理する。
class StageSelect {
  public:
    // ステージ一覧とプレビュー表示用オブジェクトを初期化する。
    void Initialize(Object3dCommon *objCommon, Input *input, int startIndex = 0);

    // 入力に応じて選択ステージとプレビュー回転を更新する。
    void Update();

    // 選択中ステージのプレビューを描画する。
    void Draw();

    bool IsFnished() const { return isFinished_; }
    void ReportLoadFailure() {
        isFinished_ = false;
        loadFailed_ = true;
    }
    int GetSelectedIndex() const { return selectedStageIndex_; }
    // Presentation presets follow the level, even when the selection order changes.
    int GetSelectedCameraPresetIndex() const {
        const auto file = GetSelectedFileName();
        if (file == "expeditionGrandTour.txt" || file == "beginnerTrail.txt" || file == "mechanismGardens.txt" ||
            file == "skylineRelay.txt") {
            return 7;
        }
        if (file == "tutorial.txt") {
            return 0;
        }
        if (file == "gameStage01.txt") {
            return 1;
        }
        if (file == "gameStage02.txt") {
            return 2;
        }
        if (file == "stage04.txt") {
            return 3;
        }
        if (file == "new_stage_03.txt") {
            return 4;
        }
        if (file == "stage_03 .txt") {
            return 5;
        }
        if (file == "dioramaRuins.txt") {
            return 6;
        }
        return 0;
    }

    // 決定済みステージのファイル名を返す。呼び出し側はIsFnished後の使用を想定する。
    std::string GetSelectedFileName() const { return stageFiles_[selectedStageIndex_]; }
    bool HasLoadFailed() const { return loadFailed_; }
    float GetPreviewAngle() const { return previewAngle_; }
    int GetStageCount() const { return static_cast<int>(stageFiles_.size()); }

  private:
    Object3dCommon *object3dCommon_ = nullptr;
    Input *input_ = nullptr;
    Camera camera_;

    // 選択可能なステージファイル名の一覧。
    std::vector<std::string> stageFiles_;

    int selectedStageIndex_ = 0;
    bool isFinished_ = false;
    bool loadFailed_ = false;

    std::vector<std::unique_ptr<Model>> previewModels_;
    std::unique_ptr<Object3d> previewObject_;
    std::unique_ptr<Model> skyCloudModel_;
    std::vector<std::unique_ptr<Object3d>> skyClouds_;
    Camera previewCamera_;
    float previewAngle_ = 0.0f, previewPitch_ = 0.0f;
};

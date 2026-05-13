#pragma once
#include "IScene.h"
#include "StageMap.h"
#include "StageRenderer.h"
#include "Input.h"
#include "Camera.h"
#include "Object3dCommon.h"
#include "TextureManager.h"
#include "MapCursor.h" // 追加
#include <memory>
#include <vector>
#include <string>

class EditorScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawUI() override;
    bool IsFinished() const override { return isFinished_; }

    void SetEnginePointers(Object3dCommon* obj, Input* in, TextureManager* tex);

private:
    // ヘルパー関数
    void ApplyPlacement(); // 設置ロジック
    void RefreshStageList(); // ファイルリスト更新

    // エディタ専用の変数群（あなたのヘッダをベースに不足分を追加）
    BlockType selectedBlockType_ = BlockType::Ground;
    Int3 cursorIndex_ = { 0, 0, 0 };
    bool isFinished_ = false;
    bool isWaitingForSecondDoor_ = false;
    Int3 firstDoorIndex_ = { -1, -1, -1 };

    // エンジンから借りる道具
    Object3dCommon* objCommon_ = nullptr;
    Input* input_ = nullptr;
    TextureManager* texManager_ = nullptr;

    // 表示用
    StageMap stageMap_;
    StageRenderer stageRenderer_;
    Camera editorCamera_;
    std::unique_ptr<MapCursor> mapCursor_; // これがないとエラーになります

    // 保存・読み込み用
    std::vector<std::string> stageFiles_;
    int selectedStageIndex_ = -1;
    char newStageName_[64] = "new_stage";
};
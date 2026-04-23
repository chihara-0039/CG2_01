#pragma once
#include "IScene.h"
#include "StageMap.h"
#include "StageRenderer.h"
#include "Input.h"
#include "Camera.h"
#include "Object3dCommon.h"
#include "TextureManager.h"
#include <memory>
// ... 必要なインクルード ...

class EditorScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawUI() override;
    bool IsFinished() const override { return isFinished_; }

    // ポインタ受け取り用
    void SetEnginePointers(Object3dCommon* obj, Input* in, TextureManager* tex);

private:
    // エディタ専用の変数群（以前 MyGame にあったもの）
    BlockType selectedBlockType_ = BlockType::Ground;
    Int3 cursorIndex_ = { 0, 0, 0 };
    bool isFinished_ = false;

    // エンジンから借りる道具
    Object3dCommon* objCommon_ = nullptr;
    Input* input_ = nullptr;
    TextureManager* texManager_ = nullptr;

    // 表示用
    StageMap stageMap_;
    StageRenderer stageRenderer_;
    Camera editorCamera_;
};
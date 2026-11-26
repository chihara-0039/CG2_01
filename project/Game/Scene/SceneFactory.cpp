#include "SceneFactory.h"

#include "BaseScene.h"
#include "GameRuntime.h"
#include "GameClearScene.h"
#include "TitleScene.h"

#include <cassert>

#include "StageSelectScene.h"
#include "DebugViewScene.h"
#include "StageEditorScene.h"
#include "GamePlayScene.h"
#include "GamePlayBlockPlaceScene.h"
#include "SkinningEditorScene.h"
#include "EffectPreviewScene.h"
#include "EffectShowcaseScene.h"
#include "PostEffectShowcaseScene.h"

// 指定されたシーン種類に対応するインスタンスを作成する。
std::unique_ptr<BaseScene> SceneFactory::CreateScene(SceneType type) const {
    switch (type) {
    case SceneType::Title:
        return std::make_unique<TitleScene>();
    case SceneType::StageSelect:
        return std::make_unique<StageSelectScene>();
    case SceneType::GameClear:
        return std::make_unique<GameClearScene>();
    case SceneType::DebugView:
        return std::make_unique<DebugViewScene>();
    case SceneType::StageEditor:
        return std::make_unique<StageEditorScene>();
    case SceneType::GamePlay:
        return std::make_unique<GamePlayScene>();
    case SceneType::GamePlayBlockPlace:
        return std::make_unique<GamePlayBlockPlaceScene>();
    case SceneType::SkinningEditor:
        return std::make_unique<SkinningEditorScene>();
    case SceneType::EffectPreview:
        return std::make_unique<EffectPreviewScene>();
    case SceneType::EffectShowcase:
        return std::make_unique<EffectShowcaseScene>();
    case SceneType::PostEffectShowcase:
        return std::make_unique<PostEffectShowcaseScene>();
    }

    assert(false && "Unknown SceneType.");
    return std::make_unique<DebugViewScene>();
}

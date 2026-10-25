#include "StagePlacementPreviewBuilder.h"

#include <cmath>
#include "../Block/PlacementPlan.h"
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"

namespace {
// プレビュー用のスタイル情報をまとめる構造体
struct PreviewStyle {
    Vector4 color{ 1.0f, 1.0f, 1.0f, 0.4f };
    Model* model = nullptr;
    bool canPreview = true;
};

// カーソル位置のブロック種別とカスタムパーツIDに応じて、プレビュー用の色とモデルを決定する。
PreviewStyle ResolvePreviewStyle(
    const StageMap& stageMap,
    const StagePlacementPreviewBuilder::Models& models,
    BlockType type,
    int customId) {

    PreviewStyle style;
    style.model = models.wall;

    // カスタムパーツは登録済みスロットの色とベースモデルを優先して使う。
    if (customId >= 1 && customId <= 5) {
        const auto* part = stageMap.GetCustomPart(customId);
        if (part) {
            style.color = { part->colorR, part->colorG, part->colorB, 0.4f };
            style.model = (part->baseType == BlockType::Ladder) ? models.ladder : models.wall;
        }
        return style;
    }

    // 通常ブロックは配置予定の種類ごとに、実体と同じモデルか識別しやすい半透明色を選ぶ。
    switch (type) {
    case BlockType::Wall:
        style.color = { 1.0f, 0.4f, 0.4f, 0.4f };
        style.model = models.wall;
        break;
    case BlockType::TransparentBlock:
        style.color = { 1.0f, 1.0f, 1.0f, 0.4f };
        style.model = models.wall;
        break;
    case BlockType::Ladder:
        style.color = { 0.4f, 1.0f, 0.4f, 0.4f };
        style.model = models.ladder;
        break;
    case BlockType::Ground:
        style.color = { 0.7f, 0.7f, 0.7f, 0.4f };
        style.model = models.ground;
        break;
    case BlockType::IceBlock:
        style.color = { 0.5f, 0.85f, 1.0f, 0.4f };
        style.model = models.iceBlock;
        break;
    case BlockType::MovingFloor:
        style.color = { 0.9f, 0.65f, 0.4f, 0.4f };
        style.model = models.movingFloor;
        break;
    case BlockType::CrumblingFloor:
        style.color = { 0.8f, 0.6f, 0.4f, 0.4f };
        style.model = models.crumble;
        break;
    default:
        style.canPreview = false;
        break;
    }

    return style;
}

// プレビュー用の一時オブジェクトを作成するヘルパー関数
std::unique_ptr<Object3d> CreatePreviewObject(
    Object3dCommon* object3dCommon,
    Model* model,
    const Vector3& position,
    const Vector3& rotation,
    const Vector3& scale,
    const Vector4& color) {

    // プレビュー専用の一時オブジェクトを作成し、呼び出し側の previewObjects に所有権を渡す。
    auto previewObject = std::make_unique<Object3d>();
    previewObject->Initialize(object3dCommon);
    previewObject->SetModel(model);
    previewObject->SetPosition(position);
    previewObject->SetRotation(rotation);
    previewObject->SetScale(scale);
    previewObject->SetColor(color);
    return previewObject;
}
}

// カーソル位置周辺の配置可否とブロック種別に基づき、previewObjectsを作り直す。
void StagePlacementPreviewBuilder::Build(
    std::vector<std::unique_ptr<Object3d>>& previewObjects,
    Object3dCommon* object3dCommon,
    const Models& models,
    const Vector3& blockScale,
    const StageMap& stageMap,
    const Int3& cursorIndex,
    BlockType type,
    int customId,
    float rotationY,int validity) {

    // プレビューはカーソル移動や選択変更のたびに作り直すため、前回分を先に消す。
    previewObjects.clear();

    PreviewStyle style = ResolvePreviewStyle(stageMap, models, type, customId);
    if (!style.canPreview || !style.model) {
        // 未対応ブロックやモデル未設定の場合は、古いプレビューを消した状態で終了する。
        return;
    }

    const auto plan=Placement::Make(stageMap,cursorIndex,type,customId,rotationY);
    if(validity==1)style.color={.15f,1.0f,.55f,.60f};
    if(validity==2)style.color={1.0f,.12f,.18f,.68f};
    for(const auto& cell:plan.cells) {
        Model* model=style.model;
        if(customId>=1 && customId<=5)model=cell.type==BlockType::Ladder?models.ladder:models.wall;
        if(!model)continue;
        previewObjects.push_back(CreatePreviewObject(object3dCommon,model,
            {static_cast<float>(cell.index.x),static_cast<float>(cell.index.y),static_cast<float>(cell.index.z)},
            {1.57f,cell.rotationY,0},blockScale,style.color));
        if(validity!=0)previewObjects.back()->SetEmissive(1.15f);
    }
}

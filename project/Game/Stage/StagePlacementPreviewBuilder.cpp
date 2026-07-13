#include "StagePlacementPreviewBuilder.h"

#include <cmath>
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"

namespace {
struct PreviewStyle {
    Vector4 color{ 1.0f, 1.0f, 1.0f, 0.4f };
    Model* model = nullptr;
    bool canPreview = true;
};

PreviewStyle ResolvePreviewStyle(
    const StageMap& stageMap,
    const StagePlacementPreviewBuilder::Models& models,
    BlockType type,
    int customId) {

    PreviewStyle style;
    style.model = models.wall;

    // カスタムパーツは個別セルの種類より、登録されたパーツ定義を優先して見た目を決める。
    if (customId >= 1 && customId <= 5) {
        const auto* part = stageMap.GetCustomPart(customId);
        if (part) {
            style.color = { part->colorR, part->colorG, part->colorB, 0.4f };
            style.model = (part->baseType == BlockType::Ladder) ? models.ladder : models.wall;
        }
        return style;
    }

    // 通常ブロックは配置予定の種類ごとに、実体と同じモデル・識別しやすい半透明色を使う。
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

std::unique_ptr<Object3d> CreatePreviewObject(
    Object3dCommon* object3dCommon,
    Model* model,
    const Vector3& position,
    const Vector3& rotation,
    const Vector3& scale,
    const Vector4& color) {

    auto obj = std::make_unique<Object3d>();
    // プレビュー専用の一時オブジェクトなので、生成後はpreviewObjects側に所有権を渡す。
    obj->Initialize(object3dCommon);
    obj->SetModel(model);
    obj->SetPosition(position);
    obj->SetRotation(rotation);
    obj->SetScale(scale);
    obj->SetColor(color);
    return obj;
}
}

void StagePlacementPreviewBuilder::Build(
    std::vector<std::unique_ptr<Object3d>>& previewObjects,
    Object3dCommon* object3dCommon,
    const Models& models,
    const Vector3& blockScale,
    const StageMap& stageMap,
    const Int3& cursorIndex,
    BlockType type,
    int customId,
    float rotationY) {

    previewObjects.clear();

    PreviewStyle style = ResolvePreviewStyle(stageMap, models, type, customId);
    if (!style.canPreview || !style.model) {
        // 未対応ブロックやモデル未設定の場合は、古いプレビューを消した状態で終了する。
        return;
    }

    if (customId >= 1 && customId <= 5) {
        const auto* part = stageMap.GetCustomPart(customId);
        if (part && !part->IsEmpty()) {
            // 90度単位の回転に丸め、3x3パーツ内のローカル座標を回転後の座標へ変換する。
            int rotIndex = static_cast<int>(std::round(rotationY / 1.5707963f)) % 4;
            if (rotIndex < 0) {
                rotIndex += 4;
            }

            for (int ly = 0; ly < 3; ++ly) {
                for (int lz = 0; lz < 3; ++lz) {
                    for (int lx = 0; lx < 3; ++lx) {
                        const auto& cell = part->cells[ly][lz][lx];
                        if (cell.type == BlockType::None) {
                            continue;
                        }

                        // パーツ内セルごとに回転後の表示位置と向きを決定する。
                        int rx = lx;
                        int rz = lz;
                        float cellRotY = 0.0f;
                        if (rotIndex == 1) {
                            rx = 2 - lz;
                            rz = lx;
                            cellRotY = 1.5707963f;
                        } else if (rotIndex == 2) {
                            rx = 2 - lx;
                            rz = 2 - lz;
                            cellRotY = 3.1415927f;
                        } else if (rotIndex == 3) {
                            rx = lz;
                            rz = 2 - lx;
                            cellRotY = 4.712389f;
                        }

                        Vector3 pos = {
                            static_cast<float>(cursorIndex.x + rx),
                            static_cast<float>(cursorIndex.y + ly),
                            static_cast<float>(cursorIndex.z + rz)
                        };
                        Model* cellModel = (cell.type == BlockType::Ladder) ? models.ladder : models.wall;
                        if (!cellModel) {
                            continue;
                        }

                        previewObjects.push_back(CreatePreviewObject(
                            object3dCommon,
                            cellModel,
                            pos,
                            { 1.57f, cellRotY, 0.0f },
                            blockScale,
                            style.color));
                    }
                }
            }
            return;
        }
    }

    Vector3 pos = {
        static_cast<float>(cursorIndex.x),
        static_cast<float>(cursorIndex.y),
        static_cast<float>(cursorIndex.z)
    };

    previewObjects.push_back(CreatePreviewObject(
        object3dCommon,
        style.model,
        pos,
        { 1.57f, rotationY, 0.0f },
        blockScale,
        style.color));
}

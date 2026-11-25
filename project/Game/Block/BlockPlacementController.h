#pragma once

#include "StageMap.h"
#include "StageRenderer.h"
#include "BlockInventory.h"
#include "PlacementPlan.h"
#include "PlaceBlocksCommand.h"
#include <memory>
#include "../../Engine/Utility/EventSignal.h"
#include <functional>
#include <utility>

// 所持ブロックをステージ上へ配置するための制御クラス。
class BlockPlacementController {
  public:
    struct PlacementEvent {
        Vector3 position;
        bool undo;
    };
    using PlacementSignal = EventSignal<PlacementEvent>;
    // 複数の表示系が、配置処理に依存を追加せず購読できる。
    PlacementSignal::Connection ObservePlacement(std::function<void(const PlacementEvent &)> callback) {
        return placementSignal_.Subscribe(std::move(callback));
    }

    // 操作対象のマップ、描画、所持数管理を外部から注入する。
    void Initialize(StageMap *stageMap, StageRenderer *stageRenderer, BlockInventory *inventory);

    // 指定セルに現在選択中のブロックを配置する。成功時はインベントリも消費する。
    bool TryPlace(const Int3 &index, float rotationY = 0.0f);
    Placement::Plan Evaluate(const Int3 &index, float rotationY = 0) const;
    bool TryUndo();
    // 成功した配置・取消だけを通知し、描画側への依存を持たせない。
    void SetFeedbackCallback(std::function<void(const Vector3 &, bool)> callback) {
        legacyFeedback_ = ObservePlacement([callback = std::move(callback)](const PlacementEvent &event) {
            if (callback) {
                callback(event.position, event.undo);
            }
        });
    }
    void SetRotationIsland(bool enabled) { rotationIsland_ = enabled; }
    void RotateUndo(bool reverse, const RotationRegion &region = {11, 8, 9, {8, 2, 12}});
    bool HasUndo() const { return lastCommand_ && lastCommand_->CanUndo(); }
    Placement::Reason LastReason() const { return lastReason_; }
    void SetPlayerPosition(const Vector3 &p) {
        playerPosition_ = p;
        checkPlayer_ = true;
    }

    // UI側で選択された配置ブロック種別とカスタムIDを保持する。
    void SetPlaceBlockType(BlockType type) { placeBlockType_ = type; }
    void SetPlaceCustomId(int id) { placeCustomId_ = id; }
    int GetPlaceCustomId() const { return placeCustomId_; }

  private:
    std::unique_ptr<PlaceBlocksCommand> lastCommand_;
    PlacementSignal placementSignal_;
    PlacementSignal::Connection legacyFeedback_;
    bool rotationIsland_ = false;
    bool checkPlayer_ = false;
    Vector3 playerPosition_{};
    Placement::Reason lastReason_ = Placement::Reason::Ready;

  private:
    StageMap *stageMap_ = nullptr;
    StageRenderer *stageRenderer_ = nullptr;
    BlockInventory *inventory_ = nullptr;

    // 初期状態では通常地面ブロックを配置対象にする。
    BlockType placeBlockType_ = BlockType::Ground;
    int placeCustomId_ = 0; // 0: 通常ブロック, 1～5: カスタムパーツ
};

#pragma once
#include "StageMap.h"

class Player;

class PlayerBasePosition {
public:
    bool ApplyFromStageMap(const StageMap& stageMap, Player* player);

    const Vector3& GetPosition() const { return position_; }
    const Int3& GetIndex() const { return index_; }

private:
    Vector3 position_{ 0.0f, 1.5f, 0.0f };
    Int3 index_{ 0, 0, 0 };
};
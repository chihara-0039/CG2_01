#pragma once

// ステージ番号とセル座標から、壁透過の適用対象エリアかを判定する。
class StageTransparencyPolicy {
public:
    static bool IsTransparencyArea(int stageIndex, int x, int y, int z);
};

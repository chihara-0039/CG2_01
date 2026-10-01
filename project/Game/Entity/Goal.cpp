#include "Goal.h"
#include<cmath>

bool Goal::Check(const Vector3& pos, const Vector3& radius, const StageMap& map) {
    // StageMap の整数座標はブロック中心。プレイヤーは足元基準のAABBとして、
    // 実際に重なっている全セルを調べる。角だけの点判定では、見た目では星に
    // 触れていても中心を取りこぼすことがあった。
    constexpr float kBoundaryEpsilon = 0.0001f;
    const int minX = static_cast<int>(std::floor(pos.x - radius.x + 0.5f));
    const int maxX = static_cast<int>(std::floor(pos.x + radius.x + 0.5f - kBoundaryEpsilon));
    const int minY = static_cast<int>(std::floor(pos.y + 0.5f));
    const int maxY = static_cast<int>(std::floor(pos.y + radius.y * 2.0f + 0.5f - kBoundaryEpsilon));
    const int minZ = static_cast<int>(std::floor(pos.z - radius.z + 0.5f));
    const int maxZ = static_cast<int>(std::floor(pos.z + radius.z + 0.5f - kBoundaryEpsilon));

    for (int gy = minY; gy <= maxY; ++gy) {
        for (int gz = minZ; gz <= maxZ; ++gz) {
            for (int gx = minX; gx <= maxX; ++gx) {
                const MapCell* cell = map.GetCell(gx, gy, gz);
                if (cell && cell->type == BlockType::Goal) {
                    return true;
                }
            }
        }
    }
    return false;
}

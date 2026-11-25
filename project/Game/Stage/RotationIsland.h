#pragma once
#include "StageMap.h"
#include <vector>
#include <cmath>

namespace RotationIsland {
constexpr int x0 = 11, z0 = 8, size = 9;
inline constexpr RotationRegion legacy{x0, z0, size, {8, 2, 12}};
inline bool Contains(const Int3 &p, const RotationRegion &r = legacy) {
    return p.x >= r.x && p.x < r.x + r.size && p.z >= r.z && p.z < r.z + r.size;
}
// 指定セルを含む回転領域を探し、固定地形の場合は対象なしを返す。
inline int RegionAt(const StageMap &map, const Int3 &p) {
    const auto &regions = map.GetRotationRegions();
    for (int i = 0; i < static_cast<int>(regions.size()); ++i) {
        if (Contains(p, regions[i])) {
            return i;
        }
    }
    return -1;
}
// 回転領域の原点と大きさを使い、セルを90度回転した座標へ移す。
inline Int3 Turn(const Int3 &p, bool reverse, const RotationRegion &r = legacy) {
    const int x0 = r.x, z0 = r.z, size = r.size;
    const int x = p.x - x0, z = p.z - z0;
    return reverse ? Int3{x0 + z, p.y, z0 + size - 1 - x} : Int3{x0 + size - 1 - z, p.y, z0 + x};
}
// セルの向き・移動方向・扉の接続先を、島の回転に合わせる。
inline void TurnCell(MapCell &c, bool reverse, const RotationRegion &r = legacy) {
    if (c.type == BlockType::None) {
        return;
    }
    constexpr float tau = 6.28318530718f;
    c.rotationY = std::fmod(c.rotationY + (reverse ? 1.57079632679f : -1.57079632679f) + tau, tau);
    const int dx = c.moveOffset.x, dz = c.moveOffset.z;
    c.moveOffset.x = reverse ? dz : -dz;
    c.moveOffset.z = reverse ? -dx : dx;
    const float ox = c.currentOffsetX, oz = c.currentOffsetZ;
    c.currentOffsetX = reverse ? oz : -oz;
    c.currentOffsetZ = reverse ? -ox : ox;
    const float vx = c.deltaOffsetX, vz = c.deltaOffsetZ;
    c.deltaOffsetX = reverse ? vz : -vz;
    c.deltaOffsetZ = reverse ? -vx : vx;
    if (c.type == BlockType::Door && Contains(c.doorTargetIndex, r)) {
        c.doorTargetIndex = Turn(c.doorTargetIndex, reverse, r);
    }
}
// プレイヤーの巻き込みを防ぎ、島内セルと関連する接続先をまとめて回転する。
inline bool Apply(StageMap &map, const Vector3 &player, bool reverse, const RotationRegion &r = legacy) {
    const int x0 = r.x, z0 = r.z, size = r.size;
    // Rotation is operated from the fixed landing, never under the player.
    if (player.x + .4f >= x0 - .5f && player.x - .4f < x0 + size - .5f && player.z + .4f >= z0 - .5f &&
        player.z - .4f < z0 + size - .5f) {
        return false;
    }
    if (!map.GetCell(x0 + size - 1, map.GetHeight() - 1, z0 + size - 1)) {
        return false;
    }
    struct Cell {
        Int3 at;
        MapCell value;
    };
    std::vector<Cell> saved;
    saved.reserve(size * size * map.GetHeight());
    for (int y = 0; y < map.GetHeight(); ++y) {
        for (int z = z0; z < z0 + size; ++z) {
            for (int x = x0; x < x0 + size; ++x) {
                const Int3 at{x, y, z};
                auto cell = *map.GetCell(at);
                TurnCell(cell, reverse, r);
                saved.push_back({Turn(at, reverse, r), cell});
            }
        }
    }
    for (const auto &c : saved) {
        map.SetCell(c.at, c.value);
    }
    // Doors on fixed ground must continue to target the same rotating doorway.
    for (int y = 0; y < map.GetHeight(); ++y) {
        for (int z = 0; z < map.GetDepth(); ++z) {
            for (int x = 0; x < map.GetWidth(); ++x) {
                auto *c = map.GetCell(x, y, z);
                if (!Contains({x, y, z}, r) && c->type == BlockType::Door && Contains(c->doorTargetIndex, r)) {
                    map.EditCell({x, y, z},
                                 [&](MapCell &cell) { cell.doorTargetIndex = Turn(cell.doorTargetIndex, reverse, r); });
                }
            }
        }
    }
    map.RebuildMovingFloorList();
    map.RebuildEnemyList();
    map.RequestRebuild();
    return true;
}
}

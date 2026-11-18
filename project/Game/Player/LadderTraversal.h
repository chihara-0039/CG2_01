#pragma once
#include "StageMap.h"
#include "MyMath.h"
#include <cmath>
#include <algorithm>

namespace LadderTraversal {
// 到着点だけでなく経路全体を検査し、梯子の出入り中に壁をすり抜けることを防ぐ。
template <class Collision> bool ClearPath(const Vector3 &from, const Vector3 &to, Collision collision) {
    const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
    const int steps = static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy + dz * dz) / 0.04f));
    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / steps;
        if (collision(Vector3{from.x + dx * t, from.y + dy * t, from.z + dz * t})) {
            return false;
        }
    }
    return true;
}
inline bool IsLadder(const StageMap &map, int x, int y, int z) {
    const auto *cell = map.GetCell(x, y, z);
    return cell && cell->type == BlockType::Ladder;
}
inline Vector3 SideOffset(float input, float cameraYaw, float speed) {
    return {input * speed * std::cos(cameraYaw), 0.0f, -input * speed * std::sin(cameraYaw)};
}
struct Movement {
    float vertical;
    Vector3 side;
    Vector3 world;
};
// 上端の足場へ向かう水平軸を登る方向にし、カメラ基準の入力を昇降と横移動へ分ける。
inline Movement ResolveMovement(const StageMap &map, int x, int y, int z, float cameraYaw, float inputX, float inputZ) {
    while (IsLadder(map, x, y + 1, z)) {
        ++y;
    }
    const auto *ladder = map.GetCell(x, y, z);
    if (!ladder) {
        return {};
    }
    float nx = std::round(std::sin(ladder->rotationY));
    float nz = std::round(std::cos(ladder->rotationY));
    const auto *front = map.GetCell(x + static_cast<int>(nx), y, z + static_cast<int>(nz));
    const auto *back = map.GetCell(x - static_cast<int>(nx), y, z - static_cast<int>(nz));
    if ((!front || !front->isSolid) && back && back->isSolid) {
        nx = -nx;
        nz = -nz;
    }
    const float length = (std::max)(1.0f, std::hypot(inputX, inputZ));
    const Vector3 world{(inputX * std::cos(cameraYaw) + inputZ * std::sin(cameraYaw)) / length, 0,
                        (-inputX * std::sin(cameraYaw) + inputZ * std::cos(cameraYaw)) / length};
    float vertical = world.x * nx + world.z * nz;
    if (std::abs(vertical) < .15f) {
        vertical = 0;
    }
    const float lateral = world.x * nz - world.z * nx;
    return {vertical, {lateral * nz, 0, -lateral * nx}, world};
}
template <class Collision>
// 梯子の下端から地面へ降りられるか、通路と接地先を確認する。
bool TryBottomExit(const StageMap &map, int x, int y, int z, const Vector3 &position, float cameraYaw,
                   Collision collision, Vector3 &result) {
    while (IsLadder(map, x, y - 1, z)) {
        --y;
    }
    if (!IsLadder(map, x, y, z)) {
        return false;
    }
    // S means down on the ladder, then backward relative to the camera on the ground.
    const float backX = -std::sin(cameraYaw), backZ = -std::cos(cameraYaw);
    const int dx = std::abs(backX) > std::abs(backZ) ? (backX > 0 ? 1 : -1) : 0;
    const int dz = dx == 0 ? (backZ > 0 ? 1 : -1) : 0;
    const auto *ladder = map.GetCell(x, y, z);
    const int normalX = static_cast<int>(std::round(std::sin(ladder->rotationY)));
    const int normalZ = static_cast<int>(std::round(std::cos(ladder->rotationY)));
    // The bottom rung can sit beside the ground cell or one cell above it.
    for (int floorCellY : {y, y - 1}) {
        const float floorY = floorCellY + 0.5f;
        if (position.y < floorY - 0.001f || position.y > floorY + 0.18f) {
            continue;
        }
        for (const auto &offset : {Vector3{static_cast<float>(dx), 0, static_cast<float>(dz)},
                                   Vector3{static_cast<float>(normalX), 0, static_cast<float>(normalZ)},
                                   Vector3{static_cast<float>(-normalX), 0, static_cast<float>(-normalZ)}}) {
            const int tx = x + static_cast<int>(offset.x), tz = z + static_cast<int>(offset.z);
            const auto *support = map.GetCell(tx, floorCellY, tz);
            if (!support || !support->isSolid || support->type == BlockType::MovingFloor) {
                continue;
            }
            const Vector3 landed{position.x, floorY, position.z};
            const Vector3 exit{static_cast<float>(tx), floorY, static_cast<float>(tz)};
            if (ClearPath(position, landed, collision) && ClearPath(landed, exit, collision)) {
                result = exit;
                return true;
            }
        }
    }
    return false;
}
template <class Collision>
// 梯子の上端から移れる足場を探し、出口と移動経路の安全を確認する。
bool TryExit(const StageMap &map, int x, int y, int z, const Vector3 &position, Collision collision, Vector3 &result) {
    while (IsLadder(map, x, y + 1, z)) {
        ++y;
    }
    const auto *ladder = map.GetCell(x, y, z);
    const float top = y + 0.5f;
    if (!ladder || position.y < top - 0.13f || position.y > top + 0.18f) {
        return false;
    }
    const int dx = static_cast<int>(std::round(std::sin(ladder->rotationY)));
    const int dz = static_cast<int>(std::round(std::cos(ladder->rotationY)));
    for (int sign : {1, -1}) {
        const auto *support = map.GetCell(x + sign * dx, y, z + sign * dz);
        if (!support || !support->isSolid || support->type == BlockType::MovingFloor) {
            continue;
        }
        const Vector3 raised{position.x, top, position.z};
        const Vector3 exit{static_cast<float>(x + sign * dx), top, static_cast<float>(z + sign * dz)};
        if (ClearPath(position, raised, collision) && ClearPath(raised, exit, collision)) {
            result = exit;
            return true;
        }
    }
    return false;
}
template <class Collision>
// 足場の縁から降りられる梯子を探し、掴み直す位置を求める。
bool TryEnterFromTop(const StageMap &map, const Vector3 &position, Collision collision, Vector3 &result,
                     float cameraYaw = 0.0f, float inputX = 0.0f, float inputZ = -1.0f) {
    const int x = static_cast<int>(std::floor(position.x + 0.5f));
    const int z = static_cast<int>(std::floor(position.z + 0.5f));
    const int y = static_cast<int>(std::round(position.y - 0.5f));
    if (std::abs(position.y - (y + 0.5f)) > 0.18f) {
        return false;
    }
    const float length = std::hypot(inputX, inputZ);
    if (length < .2f) {
        return false;
    }
    const float worldX = (inputX * std::cos(cameraYaw) + inputZ * std::sin(cameraYaw)) / length;
    const float worldZ = (-inputX * std::sin(cameraYaw) + inputZ * std::cos(cameraYaw)) / length;
    for (const auto &offset : {Vector3{1, 0, 0}, Vector3{-1, 0, 0}, Vector3{0, 0, 1}, Vector3{0, 0, -1}}) {
        // 通常歩行と同じ方向にある梯子だけを掴み、横や背後への吸い付きを防ぐ。
        const float toward = offset.x * worldX + offset.z * worldZ;
        if (toward < 0.65f) {
            continue;
        }
        const int lx = x + static_cast<int>(offset.x), lz = z + static_cast<int>(offset.z);
        if (!IsLadder(map, lx, y, lz) || IsLadder(map, lx, y + 1, lz)) {
            continue;
        }
        const auto *ladder = map.GetCell(lx, y, lz);
        const int dx = static_cast<int>(std::round(std::sin(ladder->rotationY)));
        const int dz = static_cast<int>(std::round(std::cos(ladder->rotationY)));
        if (std::abs(lx - x) != std::abs(dx) || std::abs(lz - z) != std::abs(dz)) {
            continue;
        }
        const Vector3 edge{static_cast<float>(lx), position.y, static_cast<float>(lz)};
        const Vector3 down{edge.x, y + 0.38f, edge.z};
        if (ClearPath(position, edge, collision) && ClearPath(edge, down, collision)) {
            result = down;
            return true;
        }
    }
    return false;
}
}

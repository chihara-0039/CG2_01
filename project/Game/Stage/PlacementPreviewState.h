#pragma once
#include "StageMap.h"
#include <array>

struct PlacementPreviewState {
    std::array<int, 34> cells{};
    std::array<float, 7> material{};
    bool operator==(const PlacementPreviewState &) const = default;
    // プレビューの再構築要否を比較できるよう、カーソル・パーツ・地形状態をまとめる。
    static PlacementPreviewState Make(const StageMap &map, Int3 cursor, BlockType type, int customId, float rotation,
                                      int validity, bool miniature, Vector3 scale) {
        PlacementPreviewState key;
        key.cells[0] = cursor.x;
        key.cells[1] = cursor.y;
        key.cells[2] = cursor.z;
        key.cells[3] = static_cast<int>(type);
        key.cells[4] = customId;
        key.cells[5] = validity;
        key.cells[6] = miniature;
        key.material = {rotation, 1, 1, 1, scale.x, scale.y, scale.z};
        if (customId >= 1 && customId <= 5) {
            const auto *part = map.GetCustomPart(customId);
            key.material[1] = part->colorR;
            key.material[2] = part->colorG;
            key.material[3] = part->colorB;
            for (int y = 0; y < 3; ++y) {
                for (int z = 0; z < 3; ++z) {
                    for (int x = 0; x < 3; ++x) {
                        key.cells[7 + y * 9 + z * 3 + x] = static_cast<int>(part->cells[y][z][x].type);
                    }
                }
            }
        }
        return key;
    }
};

#pragma once

namespace InventoryNavigation {
// 所持中の項目だけを2列に並べた位置。端で別の行へ飛ばさない。
inline int Move(int current, int count, int dx, int dy) {
    if (count <= 0) {
        return -1;
    }
    if (current < 0 || current >= count) {
        return 0;
    }
    const int column = current % 2 + dx;
    const int row = current / 2 + dy;
    if (column < 0 || column >= 2 || row < 0) {
        return current;
    }
    const int next = row * 2 + column;
    return next < count ? next : current;
}
}

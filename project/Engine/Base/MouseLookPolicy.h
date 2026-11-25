#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace MouseLookPolicy {
struct Rect {
    long left, top, right, bottom;
};
// 開発用のカーソルは常に表示し、配布版だけAltと前面状態で切り替える。
inline bool Visible(bool release, bool foreground, bool alt) {
    return !release || !foreground || alt;
}
// 描画に使うエディタの寸法に合わせ、パネルを除いた領域を求める。
inline Rect Region(long width, long height, bool editor) {
    if (!editor) {
        return {0, 0, width, height};
    }
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float left = std::clamp(w * .15f, 240.0f, 300.0f);
    const float right = std::clamp(w * .20f, 340.0f, 400.0f);
    const float bottom = h < 820 ? std::clamp(h * .28f, 220.0f, 320.0f) : std::clamp(h * .32f, 280.0f, 420.0f);
    // 狭いウィンドウでは描画の最小サイズより、実際にパネルで隠れていない範囲を優先する。
    return {static_cast<long>(left), 38, static_cast<long>((std::max)(left, w - right)),
            static_cast<long>((std::max)(38.0f, h - bottom))};
}
inline bool Contains(const Rect &r, long x, long y) {
    return x >= r.left && y >= r.top && x < r.right && y < r.bottom;
}
inline bool NearEdge(const Rect &r, long x, long y) {
    const long margin = (std::min)(32L, (std::min)(r.right - r.left, r.bottom - r.top) / 4);
    return x < r.left + margin || x >= r.right - margin || y < r.top + margin || y >= r.bottom - margin;
}
struct Delta {
    long x = 0, y = 0, wheel = 0;
    bool received = false;
};
class RelativeMotion {
  public:
    // 物理的な移動量を蓄積する。カーソルの絶対位置や中央への移動は加算しない。
    void Add(long x, long y) {
        const auto sum = [](long a, long b) {
            return static_cast<long>(std::clamp<int64_t>(
                static_cast<int64_t>(a) + b, (std::numeric_limits<long>::min)(), (std::numeric_limits<long>::max)()));
        };
        pending_.x = sum(pending_.x, x);
        pending_.y = sum(pending_.y, y);
        pending_.received = true;
    }
    void AddWheel(long value) { pending_.wheel += value; }
    // 非アクティブ時の分も消費し、復帰直後に古い移動量を持ち越さない。
    Delta Consume(bool foreground) {
        const auto result = foreground ? pending_ : Delta{};
        pending_ = {};
        return result;
    }

  private:
    Delta pending_{};
};
}

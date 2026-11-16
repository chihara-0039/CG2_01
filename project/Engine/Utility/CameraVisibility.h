#pragma once
#include "MyMath.h"
#include <algorithm>
#include <cmath>

namespace CameraVisibility {
// Farthest AABB corner bounds every viewing direction. No per-frame cell scan.
inline float RequiredFarClip(const Vector3 &camera, const Vector3 &minimum, const Vector3 &maximum) {
    const float dx = (std::max)(std::abs(camera.x - minimum.x), std::abs(camera.x - maximum.x));
    const float dy = (std::max)(std::abs(camera.y - minimum.y), std::abs(camera.y - maximum.y));
    const float dz = (std::max)(std::abs(camera.z - minimum.z), std::abs(camera.z - maximum.z));
    return (std::max)(100.0f, std::sqrt(dx * dx + dy * dy + dz * dz) + 32.0f);
}
}

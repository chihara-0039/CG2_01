#pragma once
#include "MyMath.h"
#include <array>
#include <cstdint>

// Dirty Flagで姿勢と投影設定を別々に比較し、変更した行列だけ更新する。
class CameraMatrixCache {
  public:
    // setter経由以外のエディタ編集も、更新時の値比較で取りこぼさない。
    void Update(const Transform &pose, float fov, float aspect, float nearClip, float farClip) {
        const std::array<float, 9> poseKey{pose.scale.x,     pose.scale.y,     pose.scale.z,
                                           pose.rotate.x,    pose.rotate.y,    pose.rotate.z,
                                           pose.translate.x, pose.translate.y, pose.translate.z};
        const std::array<float, 4> lensKey{fov, aspect, nearClip, farClip};
        viewDirty_ = viewDirty_ || poseKey != poseKey_;
        projectionDirty_ = projectionDirty_ || lensKey != lensKey_;
        if (viewDirty_) {
            view_ = Math::Inverse(Math::MakeAffineMatrix(pose.scale, pose.rotate, pose.translate));
            poseKey_ = poseKey;
            viewDirty_ = false;
            ++viewRevision_;
        }
        if (projectionDirty_) {
            projection_ = Math::MakePerspectiveFovMatrix(fov, aspect, nearClip, farClip);
            lensKey_ = lensKey;
            projectionDirty_ = false;
            ++projectionRevision_;
        }
    }
    const Matrix4x4 &View() const { return view_; }
    const Matrix4x4 &Projection() const { return projection_; }
    uint64_t ViewRevision() const { return viewRevision_; }
    uint64_t ProjectionRevision() const { return projectionRevision_; }

  private:
    std::array<float, 9> poseKey_{};
    std::array<float, 4> lensKey_{};
    Matrix4x4 view_{}, projection_{};
    bool viewDirty_ = true, projectionDirty_ = true;
    uint64_t viewRevision_ = 0, projectionRevision_ = 0;
};

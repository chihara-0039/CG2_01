#include "MyMath.h"


namespace Math {

    // 単位行列
    Matrix4x4 MakeIdentity4x4() {
        Matrix4x4 result{};
        for (int i = 0; i < 4; ++i) {
            result.m[i][i] = 1.0f;
        }
        return result;
    }

    // 拡大縮小行列
    Matrix4x4 Matrix4x4MakeScaleMatrix(const Vector3& s) {
        Matrix4x4 result{};
        result.m[0][0] = s.x;
        result.m[1][1] = s.y;
        result.m[2][2] = s.z;
        result.m[3][3] = 1.0f;
        return result;
    }

    // X軸回転
    Matrix4x4 MakeRotateXMatrix(float radian) {
        Matrix4x4 result{};
        result.m[0][0] = 1.0f;
        result.m[1][1] = std::cos(radian);
        result.m[1][2] = std::sin(radian);
        result.m[2][1] = -std::sin(radian);
        result.m[2][2] = std::cos(radian);
        result.m[3][3] = 1.0f;
        return result;
    }

    // Y軸回転
    Matrix4x4 MakeRotateYMatrix(float radian) {
        Matrix4x4 result{};
        result.m[0][0] = std::cos(radian);
        result.m[0][2] = std::sin(radian);
        result.m[1][1] = 1.0f;
        result.m[2][0] = -std::sin(radian);
        result.m[2][2] = std::cos(radian);
        result.m[3][3] = 1.0f;
        return result;
    }

    // Z軸回転
    Matrix4x4 MakeRotateZMatrix(float radian) {
        Matrix4x4 result{};
        result.m[0][0] = std::cos(radian);
        result.m[0][1] = -std::sin(radian);
        result.m[1][0] = std::sin(radian);
        result.m[1][1] = std::cos(radian);
        result.m[2][2] = 1.0f;
        result.m[3][3] = 1.0f;
        return result;
    }

    // 平行移動行列
    Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
        Matrix4x4 result{};
        result.m[0][0] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][2] = 1.0f;
        result.m[3][3] = 1.0f;
        result.m[3][0] = translate.x;
        result.m[3][1] = translate.y;
        result.m[3][2] = translate.z;
        return result;
    }

    // 行列の積
    Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
        Matrix4x4 result{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                for (int k = 0; k < 4; ++k) {
                    result.m[i][j] += m1.m[i][k] * m2.m[k][j];
                }
            }
        }
        return result;
    }

    // アフィン
    Matrix4x4 MakeAffineMatrix(const Vector3& scale,
                               const Vector3& rotate,
                               const Vector3& translate) {
        Matrix4x4 scaleMatrix = Matrix4x4MakeScaleMatrix(scale);
        Matrix4x4 rotateX = MakeRotateXMatrix(rotate.x);
        Matrix4x4 rotateY = MakeRotateYMatrix(rotate.y);
        Matrix4x4 rotateZ = MakeRotateZMatrix(rotate.z);
        Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX, rotateY), rotateZ);
        Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);
        return Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
    }

    // 逆行列
    Matrix4x4 Inverse(Matrix4x4 m) {
        Matrix4x4 result{};
        float det;
        int i;

        result.m[0][0] = m.m[1][1] * m.m[2][2] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2]
            - m.m[2][1] * m.m[1][2] * m.m[3][3] + m.m[2][1] * m.m[1][3] * m.m[3][2]
            + m.m[3][1] * m.m[1][2] * m.m[2][3] - m.m[3][1] * m.m[1][3] * m.m[2][2];

        result.m[0][1] = -m.m[0][1] * m.m[2][2] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2]
            + m.m[2][1] * m.m[0][2] * m.m[3][3] - m.m[2][1] * m.m[0][3] * m.m[3][2]
            - m.m[3][1] * m.m[0][2] * m.m[2][3] + m.m[3][1] * m.m[0][3] * m.m[2][2];

        result.m[0][2] = m.m[0][1] * m.m[1][2] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2]
            - m.m[1][1] * m.m[0][2] * m.m[3][3] + m.m[1][1] * m.m[0][3] * m.m[3][2]
            + m.m[3][1] * m.m[0][2] * m.m[1][3] - m.m[3][1] * m.m[0][3] * m.m[1][2];

        result.m[0][3] = -m.m[0][1] * m.m[1][2] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2]
            + m.m[1][1] * m.m[0][2] * m.m[2][3] - m.m[1][1] * m.m[0][3] * m.m[2][2]
            - m.m[2][1] * m.m[0][2] * m.m[1][3] + m.m[2][1] * m.m[0][3] * m.m[1][2];

        result.m[1][0] = -m.m[1][0] * m.m[2][2] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2]
            + m.m[2][0] * m.m[1][2] * m.m[3][3] - m.m[2][0] * m.m[1][3] * m.m[3][2]
            - m.m[3][0] * m.m[1][2] * m.m[2][3] + m.m[3][0] * m.m[1][3] * m.m[2][2];

        result.m[1][1] = m.m[0][0] * m.m[2][2] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2]
            - m.m[2][0] * m.m[0][2] * m.m[3][3] + m.m[2][0] * m.m[0][3] * m.m[3][2]
            + m.m[3][0] * m.m[0][2] * m.m[2][3] - m.m[3][0] * m.m[0][3] * m.m[2][2];

        result.m[1][2] = -m.m[0][0] * m.m[1][2] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2]
            + m.m[1][0] * m.m[0][2] * m.m[3][3] - m.m[1][0] * m.m[0][3] * m.m[3][2]
            - m.m[3][0] * m.m[0][2] * m.m[1][3] + m.m[3][0] * m.m[0][3] * m.m[1][2];

        result.m[1][3] = m.m[0][0] * m.m[1][2] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2]
            - m.m[1][0] * m.m[0][2] * m.m[2][3] + m.m[1][0] * m.m[0][3] * m.m[2][2]
            + m.m[2][0] * m.m[0][2] * m.m[1][3] - m.m[2][0] * m.m[0][3] * m.m[1][2];

        result.m[2][0] = m.m[1][0] * m.m[2][1] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1]
            - m.m[2][0] * m.m[1][1] * m.m[3][3] + m.m[2][0] * m.m[1][3] * m.m[3][1]
            + m.m[3][0] * m.m[1][1] * m.m[2][3] - m.m[3][0] * m.m[1][3] * m.m[2][1];

        result.m[2][1] = -m.m[0][0] * m.m[2][1] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1]
            + m.m[2][0] * m.m[0][1] * m.m[3][3] - m.m[2][0] * m.m[0][3] * m.m[3][1]
            - m.m[3][0] * m.m[0][1] * m.m[2][3] + m.m[3][0] * m.m[0][3] * m.m[2][1];

        result.m[2][2] = m.m[0][0] * m.m[1][1] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1]
            - m.m[1][0] * m.m[0][1] * m.m[3][3] + m.m[1][0] * m.m[0][3] * m.m[3][1]
            + m.m[3][0] * m.m[0][1] * m.m[1][3] - m.m[3][0] * m.m[0][3] * m.m[1][1];

        result.m[2][3] = -m.m[0][0] * m.m[1][1] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1]
            + m.m[1][0] * m.m[0][1] * m.m[2][3] - m.m[1][0] * m.m[0][3] * m.m[2][1]
            - m.m[2][0] * m.m[0][1] * m.m[1][3] + m.m[2][0] * m.m[0][3] * m.m[1][1];

        result.m[3][0] = -m.m[1][0] * m.m[2][1] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1]
            + m.m[2][0] * m.m[1][1] * m.m[3][2] - m.m[2][0] * m.m[1][2] * m.m[3][1]
            - m.m[3][0] * m.m[1][1] * m.m[2][2] + m.m[3][0] * m.m[1][2] * m.m[2][1];

        result.m[3][1] = m.m[0][0] * m.m[2][1] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1]
            - m.m[2][0] * m.m[0][1] * m.m[3][2] + m.m[2][0] * m.m[0][2] * m.m[3][1]
            + m.m[3][0] * m.m[0][1] * m.m[2][2] - m.m[3][0] * m.m[0][2] * m.m[2][1];

        result.m[3][2] = -m.m[0][0] * m.m[1][1] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1]
            + m.m[1][0] * m.m[0][1] * m.m[3][2] - m.m[1][0] * m.m[0][2] * m.m[3][1]
            - m.m[3][0] * m.m[0][1] * m.m[1][2] + m.m[3][0] * m.m[0][2] * m.m[1][1];

        result.m[3][3] = m.m[0][0] * m.m[1][1] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1]
            - m.m[1][0] * m.m[0][1] * m.m[2][2] + m.m[1][0] * m.m[0][2] * m.m[2][1]
            + m.m[2][0] * m.m[0][1] * m.m[1][2] - m.m[2][0] * m.m[0][2] * m.m[1][1];

        det = m.m[0][0] * result.m[0][0] + m.m[0][1] * result.m[1][0]
            + m.m[0][2] * result.m[2][0] + m.m[0][3] * result.m[3][0];

        if (det == 0.0f) {
            return Matrix4x4{}; // 特異行列
        }

        det = 1.0f / det;

        for (i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result.m[i][j] *= det;
            }
        }
        return result;
    }

    // 透視投影
    Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio,
                                       float nearClip, float farClip) {
        Matrix4x4 result{};
        float f = 1.0f / std::tan(fovY / 2.0f);
        result.m[0][0] = f / aspectRatio;
        result.m[1][1] = f;
        result.m[2][2] = farClip / (farClip - nearClip);
        result.m[2][3] = 1.0f;
        result.m[3][2] = -(nearClip * farClip) / (farClip - nearClip);
        return result;
    }

    // 正射影
    Matrix4x4 MakeOrthographicMatrix(float left, float top, float right,
                                     float bottom, float nearClip, float farClip) {
        Matrix4x4 m{};
        m.m[0][0] = 2.0f / (right - left);
        m.m[1][1] = 2.0f / (top - bottom);
        m.m[2][2] = 1.0f / (farClip - nearClip);
        m.m[3][0] = -(right + left) / (right - left);
        m.m[3][1] = -(top + bottom) / (top - bottom);
        m.m[3][2] = -nearClip / (farClip - nearClip);
        m.m[3][3] = 1.0f;
        return m;
    }

    // ビューポート
    Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height,
                                 float minDepth, float maxDepth) {
        Matrix4x4 m{};
        m.m[0][0] = width / 2.0f;
        m.m[1][1] = -height / 2.0f;
        m.m[2][2] = maxDepth - minDepth;
        m.m[3][0] = left + width / 2.0f;
        m.m[3][1] = top + height / 2.0f;
        m.m[3][2] = minDepth;
        m.m[3][3] = 1.0f;
        return m;
    }

    // 正規化
    Vector3 Normalize(const Vector3& v) {
        float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (length == 0.0f) {
            return { 0.0f, 0.0f, 0.0f };
        }
        return { v.x / length, v.y / length, v.z / length };
    }

} // namespace Math

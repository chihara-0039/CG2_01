#include "MyMath.h"
#include <cmath>
#include <cassert>

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
        result.m[0][2] = -std::sin(radian);
        result.m[1][1] = 1.0f;
        result.m[2][0] = std::sin(radian);
        result.m[2][2] = std::cos(radian);
        result.m[3][3] = 1.0f;
        return result;
    }

    // Z軸回転
    Matrix4x4 MakeRotateZMatrix(float radian) {
        Matrix4x4 result{};
        result.m[0][0] = std::cos(radian);
        result.m[0][1] = std::sin(radian);
        result.m[1][0] = -std::sin(radian);
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

    // アフィン行列
    Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
        Matrix4x4 scaleMatrix = Matrix4x4MakeScaleMatrix(scale);
        Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
        Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
        Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);
        Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

        Matrix4x4 rotateMatrix = Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));
        return Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
    }

    // 逆行列
    Matrix4x4 Inverse(const Matrix4x4& m) {
        float a = m.m[0][0], b = m.m[0][1], c = m.m[0][2], d = m.m[0][3];
        float e = m.m[1][0], f = m.m[1][1], g = m.m[1][2], h = m.m[1][3];
        float i = m.m[2][0], j = m.m[2][1], k = m.m[2][2], l = m.m[2][3];
        float n = m.m[3][0], o = m.m[3][1], p = m.m[3][2], q = m.m[3][3];

        float det = a * f * k * q + a * g * l * o + a * h * j * p
            + b * e * l * p + b * g * i * q + b * h * k * n
            + c * e * j * q + c * f * l * n + c * h * i * o
            + d * e * k * o + d * f * i * p + d * g * j * n
            - a * f * l * p - a * g * j * q - a * h * k * o
            - b * e * k * q - b * g * l * n - b * h * i * p
            - c * e * l * o - c * f * i * q - c * h * j * n
            - d * e * j * p - d * f * k * n - d * g * i * o;

        if (det == 0.0f) {
            return MakeIdentity4x4();
        }

        float invDet = 1.0f / det;
        Matrix4x4 result{};

        result.m[0][0] = (f * k * q + g * l * o + h * j * p - f * l * p - g * j * q - h * k * o) * invDet;
        result.m[0][1] = (-b * k * q - c * l * o - d * j * p + b * l * p + c * j * q + d * k * o) * invDet;
        result.m[0][2] = (b * g * q + c * h * o + d * f * p - b * h * p - c * f * q - d * g * o) * invDet;
        result.m[0][3] = (-b * g * l - c * h * j - d * f * k + b * h * k + c * f * l + d * g * j) * invDet;

        result.m[1][0] = (-e * k * q - g * l * n - h * i * p + e * l * p + g * i * q + h * k * n) * invDet;
        result.m[1][1] = (a * k * q + c * l * n + d * i * p - a * l * p - c * i * q - d * k * n) * invDet;
        result.m[1][2] = (-a * g * q - c * h * n - d * e * p + a * h * p + c * e * q + d * g * n) * invDet;
        result.m[1][3] = (a * g * l + c * h * i + d * e * k - a * h * k - c * e * l - d * g * i) * invDet;

        result.m[2][0] = (e * j * q + f * l * n + h * i * o - e * l * o - f * i * q - h * j * n) * invDet;
        result.m[2][1] = (-a * j * q - b * l * n - d * i * o + a * l * o + b * i * q + d * j * n) * invDet;
        result.m[2][2] = (a * f * q + b * h * n + d * e * o - a * h * o - b * e * q - d * f * n) * invDet;
        result.m[2][3] = (-a * f * l - b * h * i - d * e * j + a * h * j + b * e * l + d * f * i) * invDet;

        result.m[3][0] = (-e * j * p - f * k * n - g * i * o + e * k * o + f * i * p + g * j * n) * invDet;
        result.m[3][1] = (a * j * p + b * k * n + c * i * o - a * k * o - b * i * p - c * j * n) * invDet;
        result.m[3][2] = (-a * f * p - b * g * n - c * e * o + a * g * o + b * e * p + c * f * n) * invDet;
        result.m[3][3] = (a * f * k + b * g * i + c * e * j - a * g * j - b * e * k - c * f * i) * invDet;

        return result;
    }

    // 透視投影行列
    Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
        Matrix4x4 result{};
        float h = 1.0f / std::tan(fovY / 2.0f);
        float w = h / aspectRatio;
        float f = farClip;
        float n = nearClip;

        result.m[0][0] = w;
        result.m[1][1] = h;
        result.m[2][2] = f / (f - n);
        result.m[2][3] = 1.0f;
        result.m[3][2] = -n * f / (f - n);
        return result;
    }

    // 正射影行列 (復活！)
    Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
        Matrix4x4 m = {};
        m.m[0][0] = 2.0f / (right - left);
        m.m[1][1] = 2.0f / (top - bottom);
        m.m[2][2] = 1.0f / (farClip - nearClip);
        m.m[3][3] = 1.0f;
        m.m[3][0] = (left + right) / (left - right);
        m.m[3][1] = (top + bottom) / (bottom - top);
        m.m[3][2] = nearClip / (nearClip - farClip);
        return m;
    }

    // ビューポート行列 (復活！)
    Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
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
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len != 0.0f) {
            return { v.x / len, v.y / len, v.z / len };
        }
        return v;
    }

}
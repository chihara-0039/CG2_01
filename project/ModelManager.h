#pragma once
#include <string>
#include <unordered_map>
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"

class ModelManager {
public:
    // 初期化（最初に一度だけ呼ぶ）
    static void Initialize(Object3dCommon* common);

    // 終了処理（モデルのメモリ解放）
    static void Finalize();

    // ★これが「関数一つ」の描画関数
    static void Draw(
        const std::string& modelName,
        const Vector3& pos,
        const Vector3& rot = { 0,0,0 },
        const Vector3& scale = { 1,1,1 },
        const Vector4& color = { 1,1,1,1 }
    );

    // カメラの設定（Updateの前に呼ぶ必要がある）
    static void SetCamera(const Matrix4x4& view, const Matrix4x4& projection);

private:
    static Object3dCommon* common_;
    static std::unordered_map<std::string, Model*> models_;
    static Object3d* internalObject_; // 描画用の使い回しインスタンス
    static Matrix4x4 viewMatrix_;
    static Matrix4x4 projectionMatrix_;
};
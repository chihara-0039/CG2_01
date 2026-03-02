#pragma once
#include <vector>
#include "WinApp.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "Input.h"
#include "Object3d.h"
#include "Model.h"
#include "Sprite.h"


#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"


class MyGame {
public:
    void Initialize();
    void Update();
    void Draw();
    void Finalize();
    bool IsRunning() { return !winApp->ProcessMessage(); }

private:
    // 基盤系
    WinApp* winApp = nullptr;
    DirectXCommon* dxCommon = nullptr;
    Input* input = nullptr;
    TextureManager* textureManager = nullptr;
    SpriteCommon* spriteCommon = nullptr;
    Object3dCommon* object3dCommon = nullptr;
    ParticleManager* particleManager = nullptr;

    // オブジェクト管理
    std::vector<Object3d*> objectList;
    std::vector<Model*> models; // モデル解放用
    Sprite* sprite = nullptr;
    Transform cameraTransform;

    // ヘルパー関数
    Object3d* CreateObject(Model* model, Vector3 pos);


    /// <summary>
	/// ImGuiの表示フラグ。
    /// </summary>
    bool showImGui_ = true;

    // カメラパラメータ（すでに cameraTransform を持ってるならそれを使う）
    Vector3 camTranslate_{ 0, 1.0f, -10.0f };
    Vector3 camRotate_{ 0, 0, 0 };
    float camFovY_ = 0.45f;
    float camNear_ = 0.1f;
    float camFar_ = 1000.0f;

    // ImGuiが使うSRVの予約index（例：0番を予約）
    uint32_t imguiSrvIndex_ = 0;
};
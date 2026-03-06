#pragma once
#include <vector>
#include <memory> // unique_ptr のために必須
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
#include "Camera.h"


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
    // 基盤系：すべて unique_ptr で管理
    std::unique_ptr<WinApp> winApp;
    std::unique_ptr<DirectXCommon> dxCommon;
    std::unique_ptr<Input> input;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<SpriteCommon> spriteCommon;
    std::unique_ptr<Object3dCommon> object3dCommon;
    std::unique_ptr<ParticleManager> particleManager;

    // シーン管理 (後述)
    //std::unique_ptr<SceneManager> sceneManager;

    // オブジェクト管理：ポインタ配列も unique_ptr にすることで、配列クリア時に自動解放
    std::vector<std::unique_ptr<Object3d>> objectList;
    std::vector<std::unique_ptr<Model>> models;
    std::unique_ptr<Sprite> sprite;
    std::unique_ptr<Camera> camera;

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
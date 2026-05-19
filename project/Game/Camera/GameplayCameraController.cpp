#include "GameplayCameraController.h"
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
#include <algorithm>

void GameplayCameraController::Initialize() {
    // 初期状態の極座標：少し上空から見下ろすデフォルト角度
    cameraAngle_ = 6.267f;
    cameraPitch_ = 0.400f;
    // ★追加
    cameraFov_ = 0.45f;
}

void GameplayCameraController::Update(Input* input, Camera* camera, WinApp* winApp,Player*player) {
    if (!input || !camera || !winApp||!player) return;

    const auto& mouse = input->GetMouseState();

    // ImGui ウィンドウにマウスがホバーされているかチェック
    // ホバー中の場合はゲームプレイカメラを回転させないためのガード
    bool isGuiCaptured = false;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (ImGui::GetCurrentContext()) {
        isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
    }
#endif

    // 現在の実際のウィンドウのクライアント領域サイズを取得
    RECT rect;
    GetClientRect(winApp->GetHwnd(), &rect);
    float currentClientW = static_cast<float>(rect.right - rect.left);
    float currentClientH = static_cast<float>(rect.bottom - rect.top);

    if (currentClientW <= 0.0f || currentClientH <= 0.0f) return;

    // 1. マウス座標を 1920x1080 (内部SwapChain解像度) 空間へスケーリング
    float scaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientW;
    float scaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientH;
    float swapMouseX = static_cast<float>(mouse.posX) * scaleX;
    float swapMouseY = static_cast<float>(mouse.posY) * scaleY;

    // 2. 1280x720 のゲーム画面オフセット (左パネル幅 320px) を差し引き、ゲーム内ビューポート座標へ変換
#ifdef NDEBUG
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float mouseX = swapMouseX - offsetX;
    float mouseY = swapMouseY - offsetY;

    float screenWidth = static_cast<float>(WinApp::kWindowWidth);
    float screenHeight = static_cast<float>(WinApp::kWindowHeight);
#else
    float offsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
    float offsetY = 0.0f;
    float mouseX = swapMouseX - offsetX;
    float mouseY = swapMouseY - offsetY;

    float screenWidth = static_cast<float>(WinApp::kClientWidth);
    float screenHeight = static_cast<float>(WinApp::kClientHeight);
#endif

    // 画面端からの反応エリアの比率 (10%)
    float edgeRatio = 0.1f;
    float leftEdge = screenWidth * edgeRatio;
    float rightEdge = screenWidth * (1.0f - edgeRatio);
    float topEdge = screenHeight * edgeRatio;
    float bottomEdge = screenHeight * (1.0f - edgeRatio);

    const float rotateSpeed = 0.025f; // 回転速度
    const float minPitch = 0.4f;      // 見下ろし角度の最小値
    const float maxPitch = 1.5f;      // 見下ろし角度の最大値
    const float upperLimit = 3.0f;

    // 左クリック中、かつImGuiパネル操作中でない場合のみカメラ回転を実行
    if (mouse.buttons[0] && !isGuiCaptured) {
        // --- 水平方向の回転（左右の画面端クリック） ---
        if (mouseX < leftEdge) {
            cameraAngle_ += rotateSpeed;
        } else if (mouseX > rightEdge) {
            cameraAngle_ -= rotateSpeed;
        }

        // --- 垂直方向の回転（上下の画面端クリック） ---
        if (mouseY < topEdge) {
            cameraPitch_ += rotateSpeed;
            if (cameraPitch_ > upperLimit) {
                cameraPitch_ = upperLimit;
            }
        } else if (mouseY > bottomEdge) {
            cameraPitch_ -= rotateSpeed;
            if (cameraPitch_ < minPitch) {
                cameraPitch_ = minPitch;
            }
        }
    }

    cameraPitch_ = std::clamp(cameraPitch_, minPitch, maxPitch);

    // ==========================================================
    // ★ ズームイン・ズームアウト
    // マウスホイール上：ズームイン
    // マウスホイール下：ズームアウト
    // ==========================================================

    if (!isGuiCaptured) {
        // 5段階くらいで最大・最小ズームに到達させる
        const float zoomStep = (maxFov_ - minFov_) / 5.0f;

        if (mouse.wheel > 0) {
            // ホイール上：ズームイン
            cameraFov_ -= zoomStep;
        } else if (mouse.wheel < 0) {
            // ホイール下：ズームアウト
            cameraFov_ += zoomStep;
        }

        cameraFov_ = std::clamp(cameraFov_, minFov_, maxFov_);
        camera->SetFov(cameraFov_);
    }

    // ==========================================================
   // ★ プレイヤー位置を基準にする
   // ==========================================================

   // ズーム前と同じステージ中央基準
    Vector3 pivot = { 4.0f, 9.0f, 4.5f };

    // カメラ位置は固定
    float distance = 35.0f;
    float height = 20.0f;

    // ★ これを追加
    Vector3 pos;

    // 極座標計算
    pos.x = pivot.x - std::cos(cameraPitch_) * std::sin(cameraAngle_) * distance;

    pos.y = pivot.y + std::sin(cameraPitch_) * height;

    pos.z = pivot.z - std::cos(cameraPitch_) * std::cos(cameraAngle_) * distance;


    camera->SetPosition(pos);
    camera->SetRotation({ cameraPitch_, cameraAngle_, 0.0f });
}

void GameplayCameraController::ResetCamera(Camera* camera,Player*player,int stageIndex) {
    if (!camera||!player) return;

    Vector3 pivot = { 4.0f, 9.0f, 4.5f };
    float distance = 35.0f;
    float height = 20.0f;

    cameraAngle_ = 1.5708f;
    cameraPitch_ = 0.75f;
    cameraFov_ = 0.45f;

    switch (stageIndex) {
    case 0:
        pivot = { 4.014f, 9.0f, 4.5f };
        cameraAngle_ = 6.267f;
        cameraPitch_ = 0.400f;
        distance = 35.0f;
        height = 20.0f;
        break;

    case 1:
        pivot = { 4.014f, 9.0f, 4.5f };
        cameraAngle_ = 6.267f;
        cameraPitch_ = 0.400f;
        distance = 35.0f;
        height = 20.0f;
        break;

    case 2:
        pivot = { 4.271f, 9.0f, 4.5f };
        cameraAngle_ = 3.15f;
        cameraPitch_ = 0.400f;
        distance = 32.236f;
        height = 20.970f;
        cameraFov_ = 0.550f;
        break;

    case 3:
        pivot = { 5.0f, 7.0f, 10.0f };
        cameraAngle_ = -1.5708f;
        cameraPitch_ = 0.7f;
        distance = 30.0f;
        height = 16.0f;
        break;

    case 4:
        pivot = { 8.0f, 12.0f, 8.0f };
        cameraAngle_ = 1.5708f;
        cameraPitch_ = 1.1f;
        distance = 38.0f;
        height = 25.0f;
        break;

    case 5:
        pivot = { 4.0f, 6.0f, 4.0f };
        cameraAngle_ = 1.5708f;
        cameraPitch_ = 0.6f;
        distance = 28.0f;
        height = 14.0f;
        break;
    }

    camera->SetFov(cameraFov_);


    Vector3 pos;
    pos.x = pivot.x - std::cos(cameraPitch_) * std::sin(cameraAngle_) * distance;
    pos.y = pivot.y + std::sin(cameraPitch_) * height;
    pos.z = pivot.z - std::cos(cameraPitch_) * std::cos(cameraAngle_) * distance;

    camera->SetPosition(pos);
    camera->SetRotation({ cameraPitch_, cameraAngle_, 0.0f });
    camera->Update();
}

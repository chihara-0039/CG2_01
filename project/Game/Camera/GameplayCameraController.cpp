#include "GameplayCameraController.h"
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void GameplayCameraController::Initialize() {
    cameraAngle_ = 0.0f;
    cameraPitch_ = 0.75f;
}

void GameplayCameraController::Update(Input* input, Camera* camera, WinApp* winApp) {
    if (!input || !camera || !winApp) return;

    const auto& mouse = input->GetMouseState();

    bool isGuiCaptured = false;
#ifdef USE_IMGUI
    if (ImGui::GetCurrentContext()) {
        isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
    }
#endif

    // 画面サイズ取得（実際のウィンドウサイズ）
    RECT rect;
    GetClientRect(winApp->GetHwnd(), &rect);
    float currentClientW = static_cast<float>(rect.right - rect.left);
    float currentClientH = static_cast<float>(rect.bottom - rect.top);

    if (currentClientW <= 0.0f || currentClientH <= 0.0f) return;

    // 1. マウス座標を 1920x1080 (SwapChainサイズ) 空間にスケールする
    float scaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientW;
    float scaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientH;
    float swapMouseX = static_cast<float>(mouse.posX) * scaleX;
    float swapMouseY = static_cast<float>(mouse.posY) * scaleY;

    // 2. 1280x720 のゲーム画面の開始位置（オフセット 320px）を引いて、ゲーム内座標に変換する
    float offsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
    float offsetY = 0.0f;
    float mouseX = swapMouseX - offsetX;
    float mouseY = swapMouseY - offsetY;

    float screenWidth = static_cast<float>(WinApp::kClientWidth);
    float screenHeight = static_cast<float>(WinApp::kClientHeight);

    float edgeRatio = 0.1f;
    float leftEdge = screenWidth * edgeRatio;
    float rightEdge = screenWidth * (1.0f - edgeRatio);
    float topEdge = screenHeight * edgeRatio;
    float bottomEdge = screenHeight * (1.0f - edgeRatio);

    const float rotateSpeed = 0.025f;
    const float minPitch = 0.4f;
    const float maxPitch = 1.5f;
    const float upperLimit = 3.0f;

    // クリック中かつImGui操作中でない場合のみ反応(左クリック)
    if (mouse.buttons[0] && !isGuiCaptured) {
        // 横回転
        if (mouseX < leftEdge) {
            cameraAngle_ += rotateSpeed;
        } else if (mouseX > rightEdge) {
            cameraAngle_ -= rotateSpeed;
        }

        // 縦回転
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

    if (cameraPitch_ > maxPitch) {
        cameraPitch_ = maxPitch;
    }

    // --- カメラ位置計算 ---
    Vector3 pivot = { 4.0f, 9.0f, 4.5f };
    float distance = 35.0f;
    float height = 20.0f;

    Vector3 pos;
    pos.x = pivot.x - std::cos(cameraPitch_) * std::sin(cameraAngle_) * distance;
    pos.y = pivot.y + std::sin(cameraPitch_) * height;
    pos.z = pivot.z - std::cos(cameraPitch_) * std::cos(cameraAngle_) * distance;

    camera->SetPosition(pos);
    camera->SetRotation({ cameraPitch_, cameraAngle_, 0.0f });
}

#include "GameplayCameraController.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void GameplayCameraController::Initialize() {
    cameraAngle_ = 6.267f;
    cameraPitch_ = 0.400f;
    cameraFov_ = 0.55f;

    cameraPivot_ = { 4.0f, 9.0f, 4.5f };
    cameraDistance_ = 35.0f;
    cameraHeight_ = 20.0f;

    cameraDirty_ = true;
}

void GameplayCameraController::Update(Input* input, Camera* camera, WinApp* winApp, Player* player) {
    if (!input || !camera || !winApp || !player) return;

    const auto& mouse = input->GetMouseState();

    bool isGuiCaptured = false;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (ImGui::GetCurrentContext()) {
        isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
    }
#endif

    bool changed = false;

    
    if (!isGuiCaptured && mouse.wheel != 0) {

        float minFov = minFov_;
        float maxFov = maxFov_;

        if (currentStageIndex_ == 3) {
            minFov = 0.25f;
            maxFov = 0.80f;
        }

        const float zoomStep = (maxFov - minFov) / 5.0f;

        if (mouse.wheel > 0) {
            cameraFov_ -= zoomStep;
        } else if (mouse.wheel < 0) {
            cameraFov_ += zoomStep;
        }

        cameraFov_ = std::clamp(cameraFov_, minFov, maxFov);
        camera->SetFov(cameraFov_);
        changed = true;
    }

    // ==========================================================
    // 回転：左クリックしている時だけ画面端判定
    // ==========================================================
    if (mouse.buttons[0] && !isGuiCaptured) {
        RECT rect;
        GetClientRect(winApp->GetHwnd(), &rect);

        float currentClientW = static_cast<float>(rect.right - rect.left);
        float currentClientH = static_cast<float>(rect.bottom - rect.top);

        if (currentClientW > 0.0f && currentClientH > 0.0f) {
            float scaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientW;
            float scaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientH;
            float swapMouseX = static_cast<float>(mouse.posX) * scaleX;
            float swapMouseY = static_cast<float>(mouse.posY) * scaleY;

#ifdef NDEBUG
            float mouseX = static_cast<float>(mouse.posX) *
                (static_cast<float>(WinApp::kClientWidth) / currentClientW);

            float mouseY = static_cast<float>(mouse.posY) *
                (static_cast<float>(WinApp::kClientHeight) / currentClientH);

            float screenWidth = static_cast<float>(WinApp::kClientWidth);
            float screenHeight = static_cast<float>(WinApp::kClientHeight);
#else

            float offsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
            float mouseX = swapMouseX - offsetX;
            float mouseY = swapMouseY;

            float screenWidth = static_cast<float>(WinApp::kClientWidth);
            float screenHeight = static_cast<float>(WinApp::kClientHeight);
#endif

            float edgeRatio = 0.1f;
            float leftEdge = screenWidth * edgeRatio;
            float rightEdge = screenWidth * (1.0f - edgeRatio);
            float topEdge = screenHeight * edgeRatio;
            float bottomEdge = screenHeight * (1.0f - edgeRatio);

            const float rotateSpeed = 0.025f;
             float minPitch = 0.3f;
             float maxPitch = 1.5f;

            if (currentStageIndex_ == 3) {
                minPitch = 0.2f;  // 下方向にもっと回せる
                maxPitch = 1.5f;
            }

            const float hitSize = 90.0f;
            const float half = hitSize * 0.5f;

            float leftX = screenWidth * edgeRatio * 0.5f;
            float rightX = screenWidth * (1.0f - edgeRatio * 0.5f);
            float topY = screenHeight * edgeRatio * 0.5f;
            float bottomY = screenHeight * (1.0f - edgeRatio * 0.5f);

            float centerX = screenWidth * 0.5f;
            float centerY = screenHeight * 0.5f;

#ifdef NDEBUG
            Vector2 leftOffset = { 0.0f, 0.0f };
            Vector2 rightOffset = { -40.0f, 0.0f };
            Vector2 upOffset = { 0.0f, 0.0f };
            Vector2 downOffset = { 0.0f, -60.0f };
#else
            Vector2 leftOffset = { 0.0f, 0.0f };
            Vector2 rightOffset = { -20.0f, 0.0f };
            Vector2 upOffset = { 0.0f, 0.0f };
            Vector2 downOffset = { 0.0f, -20.0f };
#endif

            auto CheckHitBox = [&](float x, float y) {
                return mouseX >= x - half &&
                    mouseX <= x + half &&
                    mouseY >= y - half &&
                    mouseY <= y + half;
                };

            bool hitLeft = CheckHitBox(leftX + leftOffset.x, centerY + leftOffset.y);
            bool hitRight = CheckHitBox(rightX + rightOffset.x, centerY + rightOffset.y);
            bool hitUp = CheckHitBox(centerX + upOffset.x, topY + upOffset.y);
            bool hitDown = CheckHitBox(centerX + downOffset.x, bottomY + downOffset.y);

            if (hitLeft) {
                cameraAngle_ += rotateSpeed;
                changed = true;
            } else if (hitRight) {
                cameraAngle_ -= rotateSpeed;
                changed = true;
            } else if (hitUp) {
                cameraPitch_ += rotateSpeed;
                changed = true;
            } else if (hitDown) {
                cameraPitch_ -= rotateSpeed;
                changed = true;
            }

            cameraPitch_ = std::clamp(cameraPitch_, minPitch, maxPitch);
        }
    }

    // 何も操作していないなら、sin/cos計算もSetPositionも行わない
    if (!changed && !cameraDirty_) {
        return;
    }

    ApplyCamera(camera);
    cameraDirty_ = false;
}

void GameplayCameraController::ApplyCamera(Camera* camera) {
    if (!camera) return;

    Vector3 target = cameraPivot_;

    Vector3 pos;
    pos.x = target.x - std::cos(cameraPitch_) * std::sin(cameraAngle_) * cameraDistance_;
    pos.y = target.y + std::sin(cameraPitch_) * cameraHeight_;
    pos.z = target.z - std::cos(cameraPitch_) * std::cos(cameraAngle_) * cameraDistance_;

    camera->SetPosition(pos);

    Vector3 diff = {
        target.x - pos.x,
        target.y - pos.y,
        target.z - pos.z
    };

    float yaw = std::atan2(diff.x, diff.z);
    float horizontal = std::sqrt(diff.x * diff.x + diff.z * diff.z);
    float pitch = -std::atan2(diff.y, horizontal);

    camera->SetRotation({ pitch, yaw, 0.0f });
}

void GameplayCameraController::ResetCamera(
    Camera* camera,
    Player* player,
    const StageMap& stageMap,
    int stageIndex
) {
    if (!camera || !player) return;

    currentStageIndex_ = stageIndex;

    float width = static_cast<float>(stageMap.GetWidth());
    float height = static_cast<float>(stageMap.GetHeight());
    float depth = static_cast<float>(stageMap.GetDepth());

    float maxSize = (std::max)(width, depth);

    // ==============================
    // ステージごとのカメラ設定
    // ==============================
    CameraPreset preset{};

    switch (stageIndex) {
    case 0:
        // 操作説明ステージ
        // ==========================================
// カメラの左右回転角度
// 0     = 正面
// 1.57f = 90度回転
// 3.14f = 背面
// 5.55f = 今の斜め俯瞰向き
// ==========================================
        preset.angle = 5.55f;

        // ==========================================
        // カメラの上下角度（見下ろし具合）
        // 小さい = 横視点寄り
        // 大きい = 真上寄り
        //
        // 0.4f くらい → 横から見る
        // 0.7f くらい → 箱庭向き
        // 1.2f くらい → かなり上空
        // ==========================================
        preset.pitch = 0.78f;

        // ==========================================
        // カメラとステージ中心の距離倍率
        // 小さいほど近い
        // 大きいほど遠い
        // ==========================================
        preset.distanceRate = 1.55f;

        // ==========================================
        // カメラの高さ倍率
        // 高いほど上から見下ろす
        // ==========================================
        preset.heightRate = 0.90f;

        // ==========================================
        // 視野角(FOV)
        //
        // 小さい = ズーム
        // 大きい = 広角
        //
        // 0.35f → かなりズーム
        // 0.55f → 標準
        // 0.80f → 広角
        // ==========================================
        preset.fov = 0.55f;

        // ==========================================
        // カメラが見る中心位置のY倍率
        //
        // 小さい → 足元を見る
        // 大きい → 上側を見る
        // ==========================================
        preset.pivotYRate = 0.45f;
        break;

    case 1:
        // 通常ステージ
        preset.angle = 0.78f;
        preset.pitch = 0.72f;
        preset.distanceRate = 1.85f;
        preset.heightRate = 1.10f;

        preset.fov = 1.0f;

        preset.pivotYRate = 0.38f;
        break;

    case 2:
        // 少し広めに見たいステージ
        preset.angle = 0.785f;
        preset.pitch = 0.68f;
        preset.distanceRate = 2.25f;
        preset.heightRate = 1.35f;
        preset.fov = 0.55f;
        preset.pivotYRate = 0.40f;
        break;

    case 3:
        // 高低差・複雑なステージ
        preset.angle = 0.785f;
        preset.pitch = 0.75f;
        preset.distanceRate = 2.40f;
        preset.heightRate = 1.45f;
        preset.fov = 0.60f;
        preset.pivotYRate = 0.45f;
        break;

    default:
        // 予備
        preset.angle = 0.785f;
        preset.pitch = 0.60f;
        preset.distanceRate = 2.00f;
        preset.heightRate = 1.20f;
        preset.fov = 0.50f;
        preset.pivotYRate = 0.35f;
        break;
    }

    // ステージ中心を見る
    cameraPivot_ = {
        (width - 1.0f) * 0.5f,
        height * preset.pivotYRate,
        (depth - 1.0f) * 0.5f
    };

    cameraAngle_ = preset.angle;
    cameraPitch_ = preset.pitch;
    cameraDistance_ = maxSize * preset.distanceRate;
    cameraHeight_ = maxSize * preset.heightRate;
    cameraFov_ = preset.fov;

    initialPivotYOffset_ = cameraPivot_.y - player->GetPosition().y;

    camera->SetFov(cameraFov_);
    ApplyCamera(camera);
    camera->Update();

    cameraDirty_ = false;
}
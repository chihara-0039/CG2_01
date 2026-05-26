#include "GameplayCameraController.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void GameplayCameraController::Initialize() {
    cameraAngle_ = 6.267f;
    cameraPitch_ = 0.400f;
    cameraFov_ = 0.45f;

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

    //// --- プレイヤー追従デッドゾーン処理 ---
    //Vector3 playerPos = player->GetPosition();
    //float deadZoneX = 5.0f;
    //float deadZoneZ = 5.0f;

    //float diffX = playerPos.x - cameraPivot_.x;
    //if (diffX > deadZoneX) {
    //    cameraPivot_.x = playerPos.x - deadZoneX;
    //    changed = true;
    //} else if (diffX < -deadZoneX) {
    //    cameraPivot_.x = playerPos.x + deadZoneX;
    //    changed = true;
    //}

    //float diffZ = playerPos.z - cameraPivot_.z;
    //if (diffZ > deadZoneZ) {
    //    cameraPivot_.z = playerPos.z - deadZoneZ;
    //    changed = true;
    //} else if (diffZ < -deadZoneZ) {
    //    cameraPivot_.z = playerPos.z + deadZoneZ;
    //    changed = true;
    //}

    //float targetPivotY = playerPos.y + initialPivotYOffset_;
    //float diffPivotY = targetPivotY - cameraPivot_.y;
    //if (std::abs(diffPivotY) > 2.0f) {
    //    cameraPivot_.y += diffPivotY * 0.1f; // ゆるやかにY軸追従
    //    changed = true;
    //}

    // ==========================================================
    // ズーム：ホイールが動いた時だけ処理
    // ==========================================================
    if (!isGuiCaptured && mouse.wheel != 0) {

        float minFov = minFov_;
        float maxFov = maxFov_;

        if (currentStageIndex_ == 3) {
            minFov = 0.25f; // もっとズームインできる
            maxFov = 0.80f; // もっとズームアウトできる
        }

        const float zoomStep = (maxFov_ - minFov_) / 5.0f;

        if (mouse.wheel > 0) {
            cameraFov_ -= zoomStep;
        } else if (mouse.wheel < 0) {
            cameraFov_ += zoomStep;
        }

        cameraFov_ = std::clamp(cameraFov_, minFov_, maxFov_);
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
             float minPitch = 0.4f;
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

    Vector3 pos;
    pos.x = cameraPivot_.x - std::cos(cameraPitch_) * std::sin(cameraAngle_) * cameraDistance_;
    pos.y = cameraPivot_.y + std::sin(cameraPitch_) * cameraHeight_;
    pos.z = cameraPivot_.z - std::cos(cameraPitch_) * std::cos(cameraAngle_) * cameraDistance_;

    camera->SetPosition(pos);
    camera->SetRotation({ cameraPitch_, cameraAngle_, 0.0f });
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

    // ステージサイズから中心を自動計算
    cameraPivot_ = {
        (width - 1.0f) * 0.5f,
        height * 0.35f,
        (depth - 1.0f) * 0.5f
    };

    // ステージサイズから距離を自動計算
    float maxSize = (std::max)(width, depth);

    cameraAngle_ = 0.785f;//45度
    cameraPitch_ = 0.55f;//見下ろし角度

    cameraDistance_ = maxSize * 2.2f;
    cameraHeight_ = maxSize * 1.3f;

    cameraFov_ = 0.55f;

    // ステージ3だけ少し広めに見る
    if (stageIndex == 3) {
        cameraPitch_ = 0.7f;
        cameraDistance_ = maxSize * 2.4f;
        cameraHeight_ = maxSize * 1.4f;
        cameraFov_ = 0.60f;
    }

    initialPivotYOffset_ = cameraPivot_.y - player->GetPosition().y;

    camera->SetFov(cameraFov_);
    ApplyCamera(camera);
    camera->Update();

    cameraDirty_ = false;
}
#include "GameplayUIManager.h"
#include "WinApp.h"
#include <cmath>

void GameplayUIManager::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager, SpriteCommon* spriteCommon, Object3dCommon* object3dCommon) {
    spriteCommon_ = spriteCommon;

    // カメラ回転用UIスプライト（4方向ごとの個別テクスチャ）
    cameraGuideLeftTextureHandle_ = textureManager->LoadTexture("Resources/UI/arrow/arrow_left.png");
    cameraGuideRightTextureHandle_ = textureManager->LoadTexture("Resources/UI/arrow/arrow_right.png");
    cameraGuideUpTextureHandle_ = textureManager->LoadTexture("Resources/UI/arrow/arrow_up.png");
    cameraGuideDownTextureHandle_ = textureManager->LoadTexture("Resources/UI/arrow/arrow_down.png");

    cameraGuideLeftSprite_ = std::make_unique<Sprite>();
    cameraGuideLeftSprite_->Initialize(spriteCommon, cameraGuideLeftTextureHandle_);

    cameraGuideRightSprite_ = std::make_unique<Sprite>();
    cameraGuideRightSprite_->Initialize(spriteCommon, cameraGuideRightTextureHandle_);

    cameraGuideUpSprite_ = std::make_unique<Sprite>();
    cameraGuideUpSprite_->Initialize(spriteCommon, cameraGuideUpTextureHandle_);

    cameraGuideDownSprite_ = std::make_unique<Sprite>();
    cameraGuideDownSprite_->Initialize(spriteCommon, cameraGuideDownTextureHandle_);

    // ドア用3D F UI
    doorPromptModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(
            dxCommon,
            "Resources/UI/F",
            "F.obj",
            textureManager
        )
    );

    doorPromptObject_ = std::make_unique<Object3d>();
    doorPromptObject_->Initialize(object3dCommon);
    doorPromptObject_->SetModel(doorPromptModel_.get());
    doorPromptObject_->SetEnableLighting(false);
    doorPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    pSwitchPromptObject_ = std::make_unique<Object3d>();
    pSwitchPromptObject_->Initialize(object3dCommon);
    pSwitchPromptObject_->SetModel(doorPromptModel_.get());
    pSwitchPromptObject_->SetEnableLighting(false);
    pSwitchPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    keyPromptObject_ = std::make_unique<Object3d>();
    keyPromptObject_->Initialize(object3dCommon);
    keyPromptObject_->SetModel(doorPromptModel_.get());
    keyPromptObject_->SetEnableLighting(false);
    keyPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    keyBlockPromptObject_ = std::make_unique<Object3d>();
    keyBlockPromptObject_->Initialize(object3dCommon);
    keyBlockPromptObject_->SetModel(doorPromptModel_.get());
    keyBlockPromptObject_->SetEnableLighting(false);
    keyBlockPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    // はしご用3D UI
    ladderPromptModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(
            dxCommon,
            "Resources/UI/radderUI",
            "radderUI.obj",
            textureManager
        )
    );

    ladderPromptObject_ = std::make_unique<Object3d>();
    ladderPromptObject_->Initialize(object3dCommon);
    ladderPromptObject_->SetModel(ladderPromptModel_.get());
    ladderPromptObject_->SetEnableLighting(false);
    ladderPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    // カメラモード表示用UIスプライトの初期化
    cameraModeStageTextureHandle_ = textureManager->LoadTexture("Resources/UI/stage_overview_icon.png");
    cameraModePlayerTextureHandle_ = textureManager->LoadTexture("Resources/UI/follow_player_icon.png");

    cameraModeStageSprite_ = std::make_unique<Sprite>();
    cameraModeStageSprite_->Initialize(spriteCommon, cameraModeStageTextureHandle_);
    cameraModeStageSprite_->SetPosition({ 1180.0f, 100.0f });
    cameraModeStageSprite_->SetSize({ 64.0f, 64.0f });

    cameraModePlayerSprite_ = std::make_unique<Sprite>();
    cameraModePlayerSprite_->Initialize(spriteCommon, cameraModePlayerTextureHandle_);
    cameraModePlayerSprite_->SetPosition({ 1180.0f, 100.0f });
    cameraModePlayerSprite_->SetSize({ 64.0f, 64.0f });
}

void GameplayUIManager::Update(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera) {
    /*UpdateCameraGuideSprites(isGamePlayMode);*/
    UpdateDoorPrompt3D(isGamePlayMode, player, camera, lightCamera);
    UpdateLadderPrompt3D(isGamePlayMode, player, camera, lightCamera);
    UpdatePSwitchPrompt3D(isGamePlayMode, player, camera, lightCamera);
    UpdateKeyPrompt3D(isGamePlayMode, player, camera, lightCamera);
    UpdateKeyBlockPrompt3D(isGamePlayMode, player, camera, lightCamera);

    if (cameraModeStageSprite_) {
        cameraModeStageSprite_->Update();
    }
    if (cameraModePlayerSprite_) {
        cameraModePlayerSprite_->Update();
    }
}


void GameplayUIManager::UpdateDoorPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera) {
    if (!doorPromptObject_ || !player) {
        return;
    }

    if (!isGamePlayMode || !player->IsNearDoor()) {
        return;
    }

    Vector3 pos = player->GetNearDoorWorldPos();

    doorPromptObject_->SetPosition(pos);
    doorPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    Vector3 camPos = camera->GetPosition();

    float angleY = std::atan2f(
        camPos.x - pos.x,
        camPos.z - pos.z
    );

    doorPromptObject_->SetRotation({ 0.0f, angleY, 0.0f });
    doorPromptObject_->SetCamera(
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix()
    );
    doorPromptObject_->Update(
        lightCamera->GetViewProjectionMatrix()
    );
}

static void UpdatePromptObject3D(
    Object3d* object,
    const Vector3& pos,
    Camera* camera,
    LightCamera* lightCamera)
{
    if (!object || !camera || !lightCamera) {
        return;
    }

    object->SetPosition(pos);
    object->SetScale({ 0.6f, 0.6f, 0.6f });

    Vector3 camPos = camera->GetPosition();

    float angleY = std::atan2f(
        camPos.x - pos.x,
        camPos.z - pos.z
    );

    object->SetRotation({ 0.0f, angleY, 0.0f });
    object->SetCamera(
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix()
    );
    object->Update(lightCamera->GetViewProjectionMatrix());
}

void GameplayUIManager::UpdatePSwitchPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera)
{
    if (!isGamePlayMode || !player || !player->IsNearPSwitch()) {
        return;
    }

    UpdatePromptObject3D(
        pSwitchPromptObject_.get(),
        player->GetNearPSwitchWorldPos(),
        camera,
        lightCamera
    );
}

void GameplayUIManager::UpdateKeyPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera)
{
    if (!isGamePlayMode || !player || !player->IsNearKey()) {
        return;
    }

    UpdatePromptObject3D(
        keyPromptObject_.get(),
        player->GetNearKeyWorldPos(),
        camera,
        lightCamera
    );
}

void GameplayUIManager::UpdateKeyBlockPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera)
{
    if (!isGamePlayMode || !player || !player->IsNearKeyBlock()) {
        return;
    }

    UpdatePromptObject3D(
        keyBlockPromptObject_.get(),
        player->GetNearKeyBlockWorldPos(),
        camera,
        lightCamera
    );
}

void GameplayUIManager::UpdateLadderPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera) {
    if (!ladderPromptObject_ || !player) {
        return;
    }

    if (!isGamePlayMode || !player->IsOnLadder()) {
        return;
    }

    Vector3 pos = player->GetLadderWorldPos();

    ladderPromptObject_->SetPosition(pos);
    ladderPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    Vector3 camPos = camera->GetPosition();

    float angleY = std::atan2f(
        camPos.x - pos.x,
        camPos.z - pos.z
    );

    ladderPromptObject_->SetRotation({ 0.0f, angleY, 0.0f });
    ladderPromptObject_->SetCamera(
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix()
    );
    ladderPromptObject_->Update(
        lightCamera->GetViewProjectionMatrix()
    );
}

void GameplayUIManager::UpdateCameraGuide(bool isGamePlay, Input* input, WinApp* winApp)
{
    if (!isGamePlay) {
        return;
    }

    if (!input || !winApp) {
        return;
    }

    if (!cameraGuideLeftSprite_ ||
        !cameraGuideRightSprite_ ||
        !cameraGuideUpSprite_ ||
        !cameraGuideDownSprite_) {
        return;
    }

    const auto& mouse = input->GetMouseState();

    float screenWidth = static_cast<float>(WinApp::kClientWidth);
    float screenHeight = static_cast<float>(WinApp::kClientHeight);

    float edgeRatio = 0.1f;

    float leftEdge = screenWidth * edgeRatio;
    float rightEdge = screenWidth * (1.0f - edgeRatio);
    float topEdge = screenHeight * edgeRatio;
    float bottomEdge = screenHeight * (1.0f - edgeRatio);

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

    // ふわふわ用時間
    cameraGuideTime_ += 1.0f / 60.0f;

    float floatPower = 6.0f;
    float floatSpeed = 3.0f;
    float floating = std::sin(cameraGuideTime_ * floatSpeed) * floatPower;

    RECT rect;
    GetClientRect(winApp->GetHwnd(), &rect);

    float currentClientW = static_cast<float>(rect.right - rect.left);
    float currentClientH = static_cast<float>(rect.bottom - rect.top);

    if (currentClientW <= 0.0f || currentClientH <= 0.0f) {
        return;
    }

    float scaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientW;
    float scaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientH;

    float swapMouseX = static_cast<float>(mouse.posX) * scaleX;
    float swapMouseY = static_cast<float>(mouse.posY) * scaleY;

    float offsetX = 0.0f;
    float offsetY = 0.0f;

#if defined(USE_IMGUI) && !defined(NDEBUG)
    offsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
#endif;

#ifdef NDEBUG
    // Release：UIは kClientWidth / kClientHeight 基準なので、
    // マウスも同じ座標系に合わせる
    float mouseX = static_cast<float>(mouse.posX) *
        (static_cast<float>(WinApp::kClientWidth) / currentClientW);

    float mouseY = static_cast<float>(mouse.posY) *
        (static_cast<float>(WinApp::kClientHeight) / currentClientH);
#else
    // develop：ImGuiぶんを引く
    float mouseX = swapMouseX - offsetX;
    float mouseY = swapMouseY - offsetY;
#endif

    float normalSize = 64.0f;
    float glowSize = 78.0f;

    Vector2 leftPos = {
        leftX + leftOffset.x,
        centerY + leftOffset.y + floating
    };

    Vector2 rightPos = {
        rightX + rightOffset.x,
        centerY + rightOffset.y + floating
    };

    Vector2 upPos = {
        centerX + upOffset.x,
        topY + upOffset.y + floating
    };

    Vector2 downPos = {
        centerX + downOffset.x,
        bottomY + downOffset.y + floating
    };

    auto CheckHitBox = [&](Vector2 pos) {
        return mouseX >= pos.x &&
            mouseX <= pos.x + normalSize &&
            mouseY >= pos.y &&
            mouseY <= pos.y + normalSize;
        };

    bool hoverLeft = CheckHitBox(leftPos);
    bool hoverRight = CheckHitBox(rightPos);
    bool hoverUp = CheckHitBox(upPos);
    bool hoverDown = CheckHitBox(downPos);

    cameraGuideLeftSprite_->SetPosition(leftPos);
    cameraGuideRightSprite_->SetPosition(rightPos);
    cameraGuideUpSprite_->SetPosition(upPos);
    cameraGuideDownSprite_->SetPosition(downPos);

    cameraGuideLeftSprite_->SetSize({
        hoverLeft ? glowSize : normalSize,
        hoverLeft ? glowSize : normalSize
        });

    cameraGuideRightSprite_->SetSize({
        hoverRight ? glowSize : normalSize,
        hoverRight ? glowSize : normalSize
        });

    cameraGuideUpSprite_->SetSize({
        hoverUp ? glowSize : normalSize,
        hoverUp ? glowSize : normalSize
        });

    cameraGuideDownSprite_->SetSize({
        hoverDown ? glowSize : normalSize,
        hoverDown ? glowSize : normalSize
        });

    cameraGuideLeftSprite_->SetRotation(0.0f);
    cameraGuideRightSprite_->SetRotation(0.0f);
    cameraGuideUpSprite_->SetRotation(0.0f);
    cameraGuideDownSprite_->SetRotation(0.0f);

    cameraGuideLeftSprite_->Update();
    cameraGuideRightSprite_->Update();
    cameraGuideUpSprite_->Update();
    cameraGuideDownSprite_->Update();
}


void GameplayUIManager::DrawSprites(bool isGamePlayMode, bool isFollowPlayerMode) {
    if (!isGamePlayMode) {
        return;
    }

    if (!cameraGuideLeftSprite_ ||
        !cameraGuideRightSprite_ ||
        !cameraGuideUpSprite_ ||
        !cameraGuideDownSprite_) {
        return;
    }

    spriteCommon_->PreDraw();

    cameraGuideLeftSprite_->Draw();
    cameraGuideRightSprite_->Draw();
    cameraGuideUpSprite_->Draw();
    cameraGuideDownSprite_->Draw();

    if (isFollowPlayerMode) {
        if (cameraModePlayerSprite_) {
            cameraModePlayerSprite_->Draw();
        }
    } else {
        if (cameraModeStageSprite_) {
            cameraModeStageSprite_->Draw();
        }
    }
}

void GameplayUIManager::Draw3DPrompts(bool isGamePlayMode, Player* player, Object3dCommon* object3dCommon, ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle) {
    if (!isGamePlayMode || !player) {
        return;
    }

    bool drawDoorPrompt = doorPromptObject_ && player->IsNearDoor();
    bool drawLadderPrompt = ladderPromptObject_ && player->IsOnLadder();
    bool drawPSwitchPrompt = pSwitchPromptObject_ && player->IsNearPSwitch();
    bool drawKeyPrompt = keyPromptObject_ && player->IsNearKey();
    bool drawKeyBlockPrompt = keyBlockPromptObject_ && player->IsNearKeyBlock();

    if (drawDoorPrompt ||
        drawLadderPrompt ||
        drawPSwitchPrompt ||
        drawKeyPrompt ||
        drawKeyBlockPrompt) {

        object3dCommon->PreDrawPlayerHighlight();

        if (drawDoorPrompt) {
            doorPromptObject_->Draw();
        }
        if (drawLadderPrompt) {
            ladderPromptObject_->Draw();
        }
        if (drawPSwitchPrompt) {
            pSwitchPromptObject_->Draw();
        }
        if (drawKeyPrompt) {
            keyPromptObject_->Draw();
        }
        if (drawKeyBlockPrompt) {
            keyBlockPromptObject_->Draw();
        }

        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowSrvHandle);
    }
    
}

void GameplayUIManager::Finalize() {
    doorPromptObject_.reset();
    doorPromptModel_.reset();
    pSwitchPromptObject_.reset();
    keyPromptObject_.reset();
    keyBlockPromptObject_.reset();
    ladderPromptObject_.reset();
    ladderPromptModel_.reset();
    cameraGuideLeftSprite_.reset();
    cameraGuideRightSprite_.reset();
    cameraGuideUpSprite_.reset();
    cameraGuideDownSprite_.reset();
    cameraModeStageSprite_.reset();
    cameraModePlayerSprite_.reset();
}

#pragma once
#include <memory>
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "Sprite.h"
#include "Object3d.h"
#include "Model.h"
#include "Player.h"
#include "Camera.h"
#include "LightCamera.h"

class GameplayUIManager {
public:
    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager, SpriteCommon* spriteCommon, Object3dCommon* object3dCommon);
    void Update(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);
    void DrawSprites(bool isGamePlayMode);
    void Draw3DPrompts(bool isGamePlayMode, Player* player, Object3dCommon* object3dCommon, ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle);
    void Finalize();

private:
    void UpdateCameraGuideSprites(bool isGamePlayMode);
    void UpdateDoorPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);
    void UpdateLadderPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);

    SpriteCommon* spriteCommon_ = nullptr;

    // カメラ回転用UIスプライト
    std::unique_ptr<Sprite> cameraGuideLeftSprite_;
    std::unique_ptr<Sprite> cameraGuideRightSprite_;
    std::unique_ptr<Sprite> cameraGuideUpSprite_;
    std::unique_ptr<Sprite> cameraGuideDownSprite_;
    uint32_t cameraGuideTextureHandle_ = 0;

    // ドア用3D F UI
    std::unique_ptr<Model> doorPromptModel_;
    std::unique_ptr<Object3d> doorPromptObject_;

    // はしご用3D UI
    std::unique_ptr<Model> ladderPromptModel_;
    std::unique_ptr<Object3d> ladderPromptObject_;
};

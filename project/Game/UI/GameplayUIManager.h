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
    void UpdateCameraGuide(bool isGamePlay, Input* input, WinApp* winApp);
    void DrawSprites(bool isGamePlayMode, bool isFollowPlayerMode);
    void Draw3DPrompts(bool isGamePlayMode, Player* player, Object3dCommon* object3dCommon, ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle);
    void Finalize();

private:
   
    void UpdateDoorPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);
    void UpdateLadderPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);
    

    SpriteCommon* spriteCommon_ = nullptr;

    // カメラ回転用UIスプライト
    std::unique_ptr<Sprite> cameraGuideLeftSprite_;
    std::unique_ptr<Sprite> cameraGuideRightSprite_;
    std::unique_ptr<Sprite> cameraGuideUpSprite_;
    std::unique_ptr<Sprite> cameraGuideDownSprite_;
    uint32_t cameraGuideLeftTextureHandle_ = 0;
    uint32_t cameraGuideRightTextureHandle_ = 0;
    uint32_t cameraGuideUpTextureHandle_ = 0;
    uint32_t cameraGuideDownTextureHandle_ = 0;

    // カメラモード表示用UIスプライト
    std::unique_ptr<Sprite> cameraModeStageSprite_;
    std::unique_ptr<Sprite> cameraModePlayerSprite_;
    uint32_t cameraModeStageTextureHandle_ = 0;
    uint32_t cameraModePlayerTextureHandle_ = 0;

    // ドア用3D F UI
    std::unique_ptr<Model> doorPromptModel_;
    std::unique_ptr<Object3d> doorPromptObject_;

    // はしご用3D UI
    std::unique_ptr<Model> ladderPromptModel_;
    std::unique_ptr<Object3d> ladderPromptObject_;

    //文字ふわふわ演出用
    float cameraGuideTime_ = 0.0f;
};

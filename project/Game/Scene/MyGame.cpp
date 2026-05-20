#include <filesystem>

#include "MyGame.h"
#include "Goal.h"
#include "ModelManager.h"
#include <memory>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include <cmath>
#include <cstdio>

namespace {
    // ベクトルと行列の乗算 (平行移動あり)
    Vector3 TransformCoord(const Vector3& v, const Matrix4x4& m) {
        float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
        if (std::abs(w) < 1e-5f) w = 1.0f;
        return {
            (v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0]) / w,
            (v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1]) / w,
            (v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2]) / w
        };
    }

    // Vector4と行列の乗算
    Vector4 TransformVec4(const Vector4& v, const Matrix4x4& m) {
        return {
            v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0],
            v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1],
            v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2],
            v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3]
        };
    }

    // ドット積
    float Dot(const Vector3& v1, const Vector3& v2) {
        return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    }

    // ベクトルの長さ
    float Length(const Vector3& v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }
}

// --- MyGameクラスの実装 ---
void MyGame::Initialize() {
    // 基盤系の生成（new ではなく std::make_unique を使用） 
    winApp = std::make_unique<WinApp>();
    winApp->Initialize();

	// DirectXCommon の生成と初期化。Initialize には winApp の生ポインタを渡す
    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get()); // get() で中身の生ポインタを貸し出す 

	// Input クラスも同様に std::make_unique で生成し、Initialize には winApp の生ポインタを渡す
    input = std::make_unique<Input>();
    input->Initialize(winApp.get()); // get() を使用

	// TextureManager は SpriteCommon と Object3dCommon の両方で必要になるので、先に生成しておく
    textureManager = std::make_unique<TextureManager>();
    textureManager->Initialize(dxCommon.get());

	// SpriteCommon と Object3dCommon はテクスチャ管理も必要になるので、TextureManagerのセットを忘れずに
    spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->SetTextureManager(textureManager.get());
    spriteCommon->Initialize(dxCommon.get());

	// Object3dCommon はテクスチャ管理も必要になるので、TextureManagerのセットを忘れずに
    object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->SetTextureManager(textureManager.get());
    object3dCommon->Initialize(dxCommon.get());

	// ParticleManager も同様に TextureManager をセットして初期化
    particleManager = std::make_unique<ParticleManager>();
    particleManager->Initialize(dxCommon.get(), textureManager.get());

    // シーン管理 
    titleScene_ = std::make_unique<TitleScene>();
    titleScene_->Initialize(object3dCommon.get(), input.get());

    //4/20 5/10 小林
    stageSelect_ = std::make_unique<StageSelect>();
    stageSelect_->Initialize(object3dCommon.get(), input.get());

    gameClearScene_ = std::make_unique<GameClearScene>();
    gameClearScene_->Initialize(object3dCommon.get());

    // モデル読み込み（vector<unique_ptr<Model>> に入れるため unique_ptr で包む） 
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/block", "block.obj", textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/axis", "axis.obj", textureManager.get())));

    /*models.push_back(modelPlane);
    models.push_back(modelAxis);*/

    // --- オブジェクト生成 ---
    // models[index].get() で生ポインタを取得して渡す
    CreateObject(models[0].get(), { 0.0f, 0.0f, 0.0f })->SetScale({ 10.0f, 1.0f, 10.0f });
    CreateObject(models[1].get(), { 2.0f, 0.0f, 0.0f });
    CreateObject(models[1].get(), { -2.0f, 0.0f, 0.0f });

    // スプライト
    uint32_t texHandle = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), texHandle);

    //サウンド初期化
    sound.Initialize();
    //読み込み
    wavSoundData = sound.SoundLoadFile("Resources/Sound/Alarm01.wav");

    mp4SoundData = sound.SoundLoadFile("Resources/Sound/AlarmMovie.mp4");

    mp3SoundData = sound.SoundLoadFile("Resources/Sound/maou_bgm_neorock83.mp3");

    // プレイヤーの生成
    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon.get(), models[0].get());
    player_->SetPosition({ 0.0f, 1.5f, 0.0f });

    // エディタ用カメラ
    camera = std::make_unique<Camera>();
   

    // 1. ステージマップのサイズ初期化
    stageMap_.Initialize(16, 10, 16);

    // ステージエディタ管理で初期化済み

    // --- 3. ビルド設定による初期化分岐 ---
#ifdef NDEBUG
    // 【Releaseビルド時】直接ゲームを開始する
    currentMode_ = AppMode::GamePlay;

    // "Stage1.txt" があれば自動ロード
    std::string startStage = "Resources/Stages/Stage1.txt";
    if (std::filesystem::exists(startStage)) {
        stageMap_.LoadFromFile(startStage);
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    }

#else

    // 【Debugビルド時】
    currentMode_ = AppMode::Title;


    // 2. ★手動配置を消して、保存した「プロトタイプ」をロードする
    std::string prototypePath = "Resources/Stages/stage1.txt"; // 保存したファイル名に合わせてください
    if (std::filesystem::exists(prototypePath)) {
        stageMap_.LoadFromFile(prototypePath);
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    }
#endif

    // 1. モデルのロード（フォルダとファイル名に注意）
    skydomeModel_ = std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/skydome", "skydome.obj", textureManager.get()));

    // 2. オブジェクトの生成と初期化
    skydomeObject_ = std::make_unique<Object3d>();
    skydomeObject_->Initialize(object3dCommon.get());
    skydomeObject_->SetModel(skydomeModel_.get());

    // 3. 設定：空は自ら光るのでライトをオフにする
    skydomeObject_->SetEnableLighting(false);

    // 4. 設定：ステージを包むサイズにする（500〜1000程度）
    skydomeObject_->SetScale({ 500.0f, 500.0f, 500.0f });

    // ステージ描画オブジェクトの生成と構築
    stageRenderer_ = std::make_unique<StageRenderer>();
    stageRenderer_->Initialize(object3dCommon.get());
    stageRenderer_->SetBlockScale({ 1.0f, 1.0f, 1.0f });
    stageRenderer_->BuildFromStageMap(stageMap_);

    // マップカーソルの初期化
    mapCursor_ = std::make_unique<MapCursor>();
    mapCursor_->Initialize(object3dCommon.get());
    mapCursor_->SetIndex({ 0, 0, 0 }, stageMap_);
    mapCursor_->SetScale({ 0.9f, 0.9f, 0.9f });

    gameplayCameraController_.SetAngle(1.5708f); // ★ここで開始時の向きを調整！
    gameplayCameraController_.SetPitch(0.75f);

    // ★ 影の初期化
    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon.get(), textureManager.get());

    lightCamera_ = std::make_unique<LightCamera>();
    lightCamera_->Initialize();

    bubblePickupController_.Initialize(
    &stageMap_,
    stageRenderer_.get(),
    &blockInventory_
    );

    blockPlacementController_.Initialize(
        &stageMap_,
        stageRenderer_.get(),
        &blockInventory_
    );
    // UI管理の初期化
    gameplayUIManager_ = std::make_unique<GameplayUIManager>();
    gameplayUIManager_->Initialize(dxCommon.get(), textureManager.get(), spriteCommon.get(), object3dCommon.get());

    // インベントリUIの初期化
    blockInventoryUI_ = std::make_unique<BlockInventoryUI>();
    blockInventoryUI_->Initialize(dxCommon.get(), spriteCommon.get(), textureManager.get(), &blockInventory_);

    gameplayCameraController_.Initialize();
    stageEditorController_.Initialize();

    // スキニングオブジェクトとデバッグ用の立方体モデルを初期化
    skinnedObject_ = std::make_unique<SkinnedObject>();
    skinnedObject_->Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    skinnedObject_->SetPosition({ 0.0f, 0.0f, 0.0f }); // 地面(Y=0.0f)に接地させる
    skinnedObject_->SetScale({ 1.0f, 1.0f, 1.0f });

    debugCubeModel_ = std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/cube", "cube.obj", textureManager.get()));

    // 地面グリッド線の生成 (-10m から 10m まで 1m刻み)
    for (int i = -10; i <= 10; ++i) {
        // X方向に並び、Z方向に伸びる線 (縦線)
        auto lineX = std::make_unique<Object3d>();
        lineX->Initialize(object3dCommon.get());
        lineX->SetModel(debugCubeModel_.get());
        lineX->SetPosition({ (float)i, 0.0f, 0.0f });
        lineX->SetScale({ 0.015f, 0.002f, 10.0f }); // 極めて細長く、薄い
        // X=0 の中心線は赤、それ以外はグレー
        if (i == 0) {
            lineX->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
        } else {
            lineX->SetColor({ 0.35f, 0.35f, 0.35f, 1.0f });
        }
        lineX->SetEnableLighting(false);
        gridLines_.push_back(std::move(lineX));

        // Z方向に並び、X方向に伸びる線 (横線)
        auto lineZ = std::make_unique<Object3d>();
        lineZ->Initialize(object3dCommon.get());
        lineZ->SetModel(debugCubeModel_.get());
        lineZ->SetPosition({ 0.0f, 0.0f, (float)i });
        lineZ->SetScale({ 10.0f, 0.002f, 0.015f });
        // Z=0 の中心線は青、それ以外はグレー
        if (i == 0) {
            lineZ->SetColor({ 0.0f, 0.0f, 1.0f, 1.0f });
        } else {
            lineZ->SetColor({ 0.35f, 0.35f, 0.35f, 1.0f });
        }
        lineZ->SetEnableLighting(false);
        gridLines_.push_back(std::move(lineZ));
    }

#ifndef NDEBUG
    if (camera) {
        camera->SetAspectRatio(1280.0f / 720.0f);
    }
#endif
}

// ヘルパー関数：モデルと位置を指定して3Dオブジェクトを生成し、リストに追加して返す
Object3d* MyGame::CreateObject(Model* model, Vector3 pos) {
    auto obj = std::make_unique<Object3d>(); // 一時的な unique_ptr 
    obj->Initialize(object3dCommon.get());
    obj->SetModel(model);
    obj->SetPosition(pos);
    obj->SetRotation({ 1.57f, 0.0f, 0.0f });

    Object3d* ptr = obj.get(); // 戻り値用に中身の住所を控える
    objectList.push_back(std::move(obj)); // push_back で vector に「所有権」を移動させる 
    return ptr;
}

// --- 更新処理 ---
void MyGame::Update() {

    // ImGuiはDebug（Release以外）ビルドでのみ描画・更新する
#ifndef NDEBUG
    dxCommon->BeginImGui();
    UpdateImGui();
#endif


    // モード遷移時のカメラ強制リセット
    if (currentMode_ != prevMode_) {
        if (currentMode_ == AppMode::SkinningEditor) {
            // スキニングエディタ移行時：モデルの目の前へカメラを合わせる
            camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
        } else if (currentMode_ == AppMode::StageEditor) {
            // ステージエディタ移行時：ステージ全体が見える位置へ
            camera->ForceReset({ 8.0f, 0.0f, 8.0f }, 20.0f, { 0.6f, 0.0f, 0.0f });
        }
        prevMode_ = currentMode_;
    }

    input->Update();
    UpdateSceneTransition();//05/14小林 ESCでステージ選択に戻る
    bool isGuiCaptured = false;
    // 2. カメラの更新（Blender風操作を適用）
#ifndef NDEBUG
    // Release時は ImGui::GetIO() を呼ばないようにガードする
    // マウスのみをガード (キーボードフォーカスがImGuiにあっても中ドラッグ等は反応させる)
    isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
#endif


    // ライトカメラの更新（プレイヤーやカメラの位置に追従させる）
    if (lightCamera_) {
        Vector3 targetPos = player_ ? player_->GetPosition() : camera->GetPosition();
        lightCamera_->Update({ 0.2f, -1.0f, 0.5f }, targetPos);
    }

    //  ここで「ライト視点の行列」を取得！これが全ての lightVP
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // --- 3. カメラの更新 ---
    if (currentMode_ != AppMode::GamePlay) {
        camera->UpdateBlenderStyle(input.get(), isGuiCaptured, winApp->GetHwnd());
    }

    if (skydomeObject_) {
        // 天球の座標を常にカメラと同じにする
        skydomeObject_->SetPosition(camera->GetPosition());
        skydomeObject_->Update(Math::MakeIdentity4x4());
    }

    //試しにサウンド更新 
    // SPACEでwav再生
    if (input->TriggerKey(DIK_SPACE)) {
        sound.SoundPlay(wavSoundData, wavVolume);
    }

    // Mキーでmp4音声再生
    if (input->TriggerKey(DIK_M)) {
        sound.SoundPlay(mp4SoundData, mp4Volume);

    }

    if (input->TriggerKey(DIK_N)) {
        sound.SoundPlay(mp3SoundData, mp3Volume);
    }

    //mp3版音量変更キー
    if (input->TriggerKey(DIK_UP)) {
        mp3Volume += 0.1f;
        if (mp3Volume > 1.0f) {
            mp3Volume = 1.0f;
        }
        OutputDebugStringA("[MyGame] mp3 音量アップ\n");
    }

    if (input->TriggerKey(DIK_DOWN)) {
        mp3Volume -= 0.1f;
        if (mp3Volume < 0.0f) {
            mp3Volume = 0.0f;
        }
        OutputDebugStringA("[MyGame] mp3 音量ダウン\n");
    }
   
    // --- ゲームループの更新 ---
    switch (currentMode_) {

    case AppMode::Title:
        UpdateTitle();
        break;

    case AppMode::StageSelect:
        UpdateStageSelect();
        break;

    case AppMode::DebugView:
        UpdateDebugView();
        break;

    case AppMode::StageEditor:
        stageEditorController_.Update(input.get(), stageMap_, stageRenderer_.get(), mapCursor_.get(), lightCamera_.get(), player_.get(), camera.get());
        break;

    case AppMode::GamePlay:
        UpdateGamePlay();
        break;

    // ブロックを置けるようになる画面 04/01 秋元
    case AppMode::GamePlay_BlockPlace:
        UpdateGamePlayBlockPlace();
        break;

    case AppMode::GameClear://4/13佐倉
        // ▼ SPACEで演出スキップ
        if (input->TriggerKey(DIK_SPACE)) {

            // まだ演出途中ならスキップ
            if (!gameClearScene_->IsFinished()) {
                gameClearScene_->SkipAnimation();
            }
            // 演出終了後ならステージ選択へ戻る
            else {

                stageSelect_->Initialize(object3dCommon.get(), input.get());
                gameClearScene_->Initialize(object3dCommon.get());

                isGoalReached_ = false;
                stageMap_.Clear();
                player_->Respawn();

                currentMode_ = AppMode::StageSelect;
            }
        }
        break;

    case AppMode::SkinningEditor:
        if (skinnedObject_) {
            // 左クリックが押された瞬間に、画面上のボーンとの衝突判定を行う
            const auto& mouse = input->GetMouseState();
            if (mouse.buttons[0] && input->TriggerMouseButton(0) && !isGuiCaptured) {
                // スクリーン座標を取得 (Windowsのクライアント領域)
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(winApp->GetHwnd(), &pt);

                // NDC（デバイス正規化座標）への変換
                float ndcX = (float)pt.x;
                // Debugビルド時は左側に 320px の UI パネルがあるので、ビューポート幅は 1280px。
                // 左側の 320px 分を差し引く。
                ndcX -= 320.0f;

                float x = (2.0f * ndcX) / 1280.0f - 1.0f;
                float y = 1.0f - (2.0f * (float)pt.y) / 720.0f;

                // ビュー・プロジェクション逆行列を用いて、カメラのレイを構築
                Matrix4x4 invVP = Math::Inverse(Math::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix()));
                Vector4 nearPoint = { x, y, 0.0f, 1.0f };
                Vector4 farPoint = { x, y, 1.0f, 1.0f };

                Vector4 rayStartWorld4 = ::TransformVec4(nearPoint, invVP);
                Vector4 rayEndWorld4 = ::TransformVec4(farPoint, invVP);

                Vector3 rayStart = { rayStartWorld4.x / rayStartWorld4.w, rayStartWorld4.y / rayStartWorld4.w, rayStartWorld4.z / rayStartWorld4.w };
                Vector3 rayEnd = { rayEndWorld4.x / rayEndWorld4.w, rayEndWorld4.y / rayEndWorld4.w, rayEndWorld4.z / rayEndWorld4.w };
                Vector3 rayDir = Math::Normalize({ rayEnd.x - rayStart.x, rayEnd.y - rayStart.y, rayEnd.z - rayStart.z });

                // 各ジョイント（関節）のグローバル位置とレイの最短距離を判定
                int closestJointIndex = -1;
                float minDistance = 0.15f; // クリック判定の許容半径 (15cm)

                const auto& joints = skinnedObject_->GetModel()->GetJoints();
                for (size_t i = 0; i < joints.size(); ++i) {
                    Vector3 jointPos = ::TransformCoord(
                        { 0.0f, 0.0f, 0.0f },
                        Math::Multiply(
                            joints[i].globalMatrix,
                            Math::MakeAffineMatrix(skinnedObject_->GetScale(), skinnedObject_->GetRotation(), skinnedObject_->GetPosition())
                        )
                    );

                    // レイと球の交差判定 (線分から球の中心への垂線の足)
                    Vector3 v = { jointPos.x - rayStart.x, jointPos.y - rayStart.y, jointPos.z - rayStart.z };
                    float t = ::Dot(v, rayDir);
                    if (t > 0.0f) {
                        Vector3 projPoint = { rayStart.x + rayDir.x * t, rayStart.y + rayDir.y * t, rayStart.z + rayDir.z * t };
                        float dist = ::Length({ jointPos.x - projPoint.x, jointPos.y - projPoint.y, jointPos.z - projPoint.z });
                        if (dist < minDistance) {
                            minDistance = dist;
                            closestJointIndex = (int)i;
                        }
                    }
                }

                if (closestJointIndex != -1) {
                    skinnedObject_->SetSelectedJointIndex(closestJointIndex);
                }
            }

            skinnedObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
            skinnedObject_->Update(dxCommon.get(), lightVP);
        }
        break;

        gameClearScene_->Update();
    }

    camera->Update();


    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();

    // ライト視点の行列を取得しておきます
    //const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // --- プレイヤーに最新のカメラ行列を教える ---
    if (player_) {
        // カメラ行列は常にセット
        player_->SetCamera(view, proj);

        // プレイモード中のみ移動などのロジックを更新
        if (currentMode_ == AppMode::GamePlay) {
            // ここは既存のコード（UpdateGamePlayの中から移動させてもOKです）
            // player_->Update(...) 
        } else {
            // ★ エディタモード等では、見た目（行列）の更新だけを行う
            player_->UpdateTransform(lightVP);
        }
    }

    // ★ 修正1：ウィンドウが最前面にない場合は即リターンして何もしない
    // これにより、ウィンドウ外をクリックした瞬間の挙動を無視できます
    if (GetActiveWindow() != winApp->GetHwnd()) {
        return;
    }

	// 3Dオブジェクトの更新
    if (debugFlags_.show3DObjects) {
        for (auto& obj : objectList) {
            if (obj) {
                obj->SetCamera(view, proj);
                obj->Update(lightVP);
            }
        }
    }

	// ステージ描画オブジェクトの更新
    if (stageRenderer_) {
        stageRenderer_->SetCamera(view, proj);
        // ※ StageRenderer内部でもObject3dのUpdate(lightVP)を呼ぶように修正が必要です
        stageRenderer_->Update(stageMap_,lightVP);
    }

	// マップカーソルの更新
    if (mapCursor_) {
        mapCursor_->SetCamera(view, proj);
        mapCursor_->Update(lightVP);
    }

	// スプライトの更新
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        sprite->Update();
    }

	// パーティクルの更新
    if (debugFlags_.showParticles) {
        particleManager->Update(view, proj);
    }

    // ★ ライトカメラの更新
    // プレイヤーの位置に合わせて影の範囲を動かすことで、常に綺麗な影を出します
    Vector3 lightDir = { 0.5f, -1.0f, 0.5f }; // ライトの向き（Object3dCommonの設定に合わせる）
    lightCamera_->Update(lightDir, player_->GetPosition());

    object3dCommon->SetLightDirection(lightDir);
    object3dCommon->SetCameraPosition(camera->GetPosition());

    // UI・プロンプト更新
    if (gameplayUIManager_) {
        gameplayUIManager_->Update(currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace, player_.get(), camera.get(), lightCamera_.get());
    }

    if (blockInventoryUI_) {
        bool isPlayOrPlace = (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);
        blockInventoryUI_->Update(input.get(), winApp.get(), isPlayOrPlace, &stageMap_);

        // ダブルクリックによる即時設置要求を処理
        if (blockInventoryUI_->ConsumeUseRequest()) {
            // 即座に配置モードに移行
            currentMode_ = AppMode::GamePlay_BlockPlace;

            // 選択されたブロックタイプとカスタムIDをコントローラーに同期
            BlockType doubleClickedType = blockInventoryUI_->GetSelectedBlockType();
            int selectedCustomId = blockInventoryUI_->GetSelectedCustomId();
            blockPlacementController_.SetPlaceBlockType(doubleClickedType);
            blockPlacementController_.SetPlaceCustomId(selectedCustomId);

            // 現在のカーソル位置に即時配置を試みる
            Int3 cursorPos = mapCursor_->GetIndex();
            if (blockPlacementController_.TryPlace(cursorPos)) {
                // 設置成功後、もしそのブロックの所持数が 0 になったら自動的に通常プレイに戻る
                bool hasRest = (doubleClickedType == BlockType::Ground) || blockInventory_.HasBlock(doubleClickedType, selectedCustomId);
                if (!hasRest) {
                    currentMode_ = AppMode::GamePlay;
                }
            }
        }
    }
}

//パーティクル発生のテスト（スペースキーを押すと発生）
void MyGame::UpdateDebugView() {
    if (input->TriggerKey(DIK_SPACE)) {
        particleManager->Emit({ 0, 0, 0 }, 10);
    }
}

void MyGame::UpdateGamePlay() {

    gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(),player_.get());
    // --- ステージマップの更新（崩れる足場のタイマー処理） ---

    //5/14佐倉追加
    if (gameplayUIManager_) {
        gameplayUIManager_->UpdateCameraGuide(
            currentMode_ == AppMode::GamePlay,
            input.get(),
            winApp.get()
        );
    }

    float deltaTime = 1.0f / 60.0f;
    totalTime_ += deltaTime;
    stageMap_.Update(deltaTime, totalTime_);

    
    stageRenderer_->UpdateEffect(stageMap_);

    // --- プレイヤー更新 ---
    if (player_) {
        player_->Update(input.get(), stageMap_, gameplayCameraController_.GetAngle(), lightCamera_->GetViewProjectionMatrix());
    }

    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->ApplyPSwitchVisualState(stageMap_);
        stageMap_.ClearRebuildFlag();
    }

    stageRespawnController_.Update(
        stageMap_,
        backupMap_,
        stageRenderer_.get(),
        player_.get(),
        &blockInventory_,
        &bubblePickupController_,
        &blockPlacementController_,
        &stageEditorController_
    );

    // --- ステージ再構築 ---
    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->BuildFromStageMap(stageMap_);
        stageMap_.ClearRebuildFlag();
    }

    /*==================================================
    ▼ プレイヤー座標取得
    ==================================================*/
    Vector3 pPos{};
    if (player_) {
        pPos = player_->GetPosition();
    }

    /*==================================================
    ▼ シャボン玉取得
    ==================================================*/
    if (player_) {
        bubblePickupController_.Update(pPos);
    }

    int gx = static_cast<int>(std::floor(pPos.x + 0.5f));
    int gy = static_cast<int>(std::floor(pPos.y));
    int gz = static_cast<int>(std::floor(pPos.z + 0.5f));

   

    /*==================================================
        ▼ ゴール判定（★追加部分）
    ==================================================*/
    Vector3 radius = { 0.4f, 0.9f, 0.4f }; // プレイヤーサイズに合わせる

    if (Goal::Check(pPos, radius, stageMap_)) {
        isGoalReached_ = true;
    }

    /*==================================================
        ▼ 配置モード切り替え
        インベントリUIがアクティブになった場合、またはBキーで配置モードへ
    ==================================================*/
    if (blockInventoryUI_ && blockInventoryUI_->IsActive()) {
        currentMode_ = AppMode::GamePlay_BlockPlace;
        mapCursor_->SetIndex({ gx, gy, gz }, stageMap_);
    } else if (input->TriggerKey(DIK_B) && blockInventory_.HasBlock()) {
        currentMode_ = AppMode::GamePlay_BlockPlace;
        mapCursor_->SetIndex({ gx, gy, gz }, stageMap_);
        if (blockInventoryUI_) {
            blockInventoryUI_->ToggleOpen(); // Bキーでもインベントリを開く
        }
    }

    /*==================================================
        ▼ クリア遷移
    ==================================================*/
    if (isGoalReached_) {
        currentMode_ = AppMode::GameClear;
    }
}


#ifndef NDEBUG
// ImGuiの更新と描画
void MyGame::UpdateImGui() {
    ImGuiIO& io = ImGui::GetIO();
    float panelWidth = 320.0f;
    float bottomHeight = 360.0f; // 下パネルのサイズを大きくしてピッタリはめる

    // ==========================================
    // 1. 左パネル (Information)
    // ==========================================
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y - bottomHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f); // 透過なし
    ImGui::Begin("Information", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::Text("FPS: %.1f (%.3f ms/f)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::SameLine(panelWidth - 60.0f);
    if (ImGui::Button("Exit", ImVec2(50, 20))) {
        PostQuitMessage(0); // アプリケーション終了
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Hierarchy / Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        int modeIndex = 0;
        switch (currentMode_) {
        case AppMode::DebugView:      modeIndex = 0; break;
        case AppMode::StageEditor:    modeIndex = 1; break;
        case AppMode::GamePlay:       modeIndex = 2; break;
        case AppMode::SkinningEditor: modeIndex = 3; break;
        }

        const char* modeNames[] = { "DebugView", "StageEditor", "GamePlay", "SkinningEditor" };
        if (ImGui::Combo("App Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
            switch (modeIndex) {
            case 0: currentMode_ = AppMode::DebugView; break;
            case 1: currentMode_ = AppMode::StageEditor; break;
            case 2: currentMode_ = AppMode::GamePlay; break;
            case 3: currentMode_ = AppMode::SkinningEditor; break;
            }
        }
        ImGui::Checkbox("Show 3D Objects", &debugFlags_.show3DObjects);
        ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
        ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);
    }

    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        camera->DrawImGui();
    }
    if (ImGui::CollapsingHeader("StageMap Info")) {
        stageMap_.DrawImGui();
    }
    if (ImGui::CollapsingHeader("Cursor Info")) {
        mapCursor_->DrawImGui();
    }
    ImGui::End();

    // ==========================================
    // 2. 右パネル (Stage Editor)
    // ==========================================
    stageEditorController_.DrawImGui(stageMap_, stageRenderer_.get(), mapCursor_.get(), player_.get());

    // ==========================================
    // 3. 下パネル (Tools & Controls / Custom Motion Timeline)
    // ==========================================
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - bottomHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, bottomHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f); // 透過なし
    ImGui::Begin("Tools & Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (currentMode_ == AppMode::SkinningEditor && skinnedObject_) {
        // スキニングモード時のタイムライン
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Custom Motion Animation Timeline ]");
        
        float duration = skinnedObject_->GetModel()->GetMotionDuration();
        float curTime = skinnedObject_->GetCurrentKeyframeTime();
        bool playCustom = skinnedObject_->IsPlayCustomAnimation();

        // タイムラインスライダー
        if (ImGui::SliderFloat("Current Time", &curTime, 0.0f, duration, "%.2f sec")) {
            skinnedObject_->SetCurrentKeyframeTime(curTime);
            if (!playCustom) {
                skinnedObject_->ApplyMotion(curTime);
            }
        }

        ImGui::Separator();
        ImGui::Text("Joint Name");
        ImGui::SameLine(180.0f);
        ImGui::Text("Registered KeyFrames (Click to jump / preview)");
        ImGui::SameLine(io.DisplaySize.x - 280.0f);
        ImGui::Text("Current Trans / Rot (Euler)");

        ImGui::Separator();

        // ジョイントリストとキーフレームのタイムライン表示
        auto& joints = skinnedObject_->GetModel()->GetJoints();
        int selectedJoint = skinnedObject_->GetSelectedJointIndex();

        // 描画領域をスクロール可能に
        ImGui::BeginChild("TimelineScroll", ImVec2(0, 0), true);
        for (size_t i = 0; i < joints.size(); ++i) {
            bool isSelected = (static_cast<int>(i) == selectedJoint);
            if (isSelected) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "> %s", joints[i].name.c_str());
            } else {
                ImGui::Text("  %s", joints[i].name.c_str());
            }

            // クリックしてジョイント選択
            if (ImGui::IsItemClicked()) {
                skinnedObject_->SetSelectedJointIndex(static_cast<int>(i));
            }

            // 横軸にキーフレームを描画
            ImGui::SameLine(180.0f);
            const auto& anim = joints[i].animation;
            if (anim.keyframes.empty()) {
                ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "(No Keyframes)");
            } else {
                for (size_t k = 0; k < anim.keyframes.size(); ++k) {
                    char btnId[32];
                    sprintf_s(btnId, sizeof(btnId), "%.2f##kf_%zu_%zu", anim.keyframes[k].time, i, k);
                    
                    // 現在のタイムライン時間に近いキーフレームは黄色くハイライト
                    bool isActive = (std::abs(anim.keyframes[k].time - curTime) < 0.05f);
                    if (isActive) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
                    }

                    if (ImGui::Button(btnId, ImVec2(45, 18))) {
                        // クリックしたらそのキーフレームの時間にジャンプ
                        skinnedObject_->SetCurrentKeyframeTime(anim.keyframes[k].time);
                        skinnedObject_->ApplyMotion(anim.keyframes[k].time);
                    }

                    if (isActive) {
                        ImGui::PopStyleColor();
                    }
                    ImGui::SameLine();
                }
                ImGui::NewLine(); // 行送り
            }

            // 現在のトランスフォーム値を右端に表示
            ImGui::SameLine(io.DisplaySize.x - 280.0f);
            Vector3 rotDeg = {
                joints[i].rotation.x * 180.0f / 3.14159265f,
                joints[i].rotation.y * 180.0f / 3.14159265f,
                joints[i].rotation.z * 180.0f / 3.14159265f
            };
            ImGui::Text("%.2f, %.2f, %.2f / %.1f, %.1f, %.1f",
                joints[i].translation.x, joints[i].translation.y, joints[i].translation.z,
                rotDeg.x, rotDeg.y, rotDeg.z);
        }
        ImGui::EndChild();

    } else {
        // 通常ゲームモード時の下パネル (操作説明など)
        ImGui::Columns(2, "BottomColumns", false);
        
        // 左カラム：操作説明
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Editor Controls ]");
        ImGui::Text("Move Cursor : W, A, S, D, Q(Up), E(Down)");
        ImGui::Text("Place Block : Enter");
        ImGui::Text("Remove Block: Space / Backspace");
        ImGui::Text("Rotate Block: R");
        ImGui::Text("Move Camera : I, J, K, L, U, O");

        ImGui::NextColumn();

        // 右カラム：現在のステータス
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Current Status ]");
        ImGui::Text("Selected Block: %s", BlockTypeToString(stageEditorController_.GetSelectedBlockType()));
        ImGui::Text("Placeable Blocks: %d", blockInventory_.GetBlockCount());
        
        if (currentMode_ == AppMode::StageEditor) {
            ImGui::Text("Mode: STAGE EDITOR");
        } else if (currentMode_ == AppMode::GamePlay) {
            ImGui::Text("Mode: GAME PLAY");
        } else {
            ImGui::Text("Mode: DEBUG VIEW");
        }

        ImGui::Columns(1);
    }
    ImGui::End();

    // ==========================================
    // 4. 右パネル (Skinning Editor Options)
    // ==========================================
    if (currentMode_ == AppMode::SkinningEditor && skinnedObject_) {
        // 右側にパネルを配置
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelWidth, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y - bottomHeight), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f); // 透過なし
        ImGui::Begin("Skinning Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Skinned Mesh Settings ]");
        
        bool playAnim = skinnedObject_->IsPlayAnimation();
        if (ImGui::Checkbox("Play Test Animation", &playAnim)) {
            skinnedObject_->SetPlayAnimation(playAnim);
        }

        float speed = skinnedObject_->GetAnimationSpeed();
        if (ImGui::SliderFloat("Anim Speed", &speed, 0.0f, 3.0f, "%.2f")) {
            skinnedObject_->SetAnimationSpeed(speed);
        }

        bool showSkeleton = skinnedObject_->IsShowSkeleton();
        if (ImGui::Checkbox("Show Skeleton Bones", &showSkeleton)) {
            skinnedObject_->SetShowSkeleton(showSkeleton);
        }

        if (ImGui::Button("Reset to Pose (Arms Down)", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->GetModel()->ResetPose();
        }

        // --- Camera Presets (Blender-Style) ---
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Camera Presets (Blender-Style) ]");
        if (ImGui::Button("Focus Model", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 24))) {
            camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
        }
        ImGui::SameLine();
        if (ImGui::Button("Front View", ImVec2(-FLT_MIN, 24))) {
            camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.0f, 0.0f, 0.0f });
        }
        if (ImGui::Button("Side View", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 24))) {
            camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.0f, 1.5708f, 0.0f });
        }
        ImGui::SameLine();
        if (ImGui::Button("Top View", ImVec2(-FLT_MIN, 24))) {
            camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 1.5708f, 0.0f, 0.0f });
        }

        // --- Custom Motion Editor ---
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Custom Motion Editor ]");

        bool playCustom = skinnedObject_->IsPlayCustomAnimation();
        if (ImGui::Checkbox("Play Custom Motion", &playCustom)) {
            skinnedObject_->SetPlayCustomAnimation(playCustom);
            if (playCustom) {
                skinnedObject_->SetPlayAnimation(false); // テストアニメーションと排他
            }
        }

        float duration = skinnedObject_->GetModel()->GetMotionDuration();
        if (ImGui::InputFloat("Motion Duration", &duration, 0.1f, 1.0f, "%.2f")) {
            if (duration < 0.1f) duration = 0.1f;
            skinnedObject_->GetModel()->SetMotionDuration(duration);
        }

        float curTime = skinnedObject_->GetCurrentKeyframeTime();
        if (ImGui::SliderFloat("Timeline Time", &curTime, 0.0f, duration, "%.2f sec")) {
            skinnedObject_->SetCurrentKeyframeTime(curTime);
            if (!playCustom) {
                skinnedObject_->ApplyMotion(curTime);
            }
        }

        if (ImGui::Button("Add Keyframe (Current Pose)", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->AddKeyframe(curTime);
        }

        if (ImGui::Button("Clear All Keyframes", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->ClearKeyframes();
        }

        if (ImGui::Button("Generate Walk Preset", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->GenerateWalkPreset();
        }

        if (ImGui::Button("Generate Run Preset", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->GenerateRunPreset();
        }

        static char motionPath[256] = "Resources/Animations/test_motion.txt";
        ImGui::InputText("Motion Path", motionPath, IM_ARRAYSIZE(motionPath));

        if (ImGui::Button("Save Motion to File", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 24))) {
            skinnedObject_->SaveMotion(motionPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Motion from File", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->LoadMotion(motionPath);
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Bone Transformations ]");

        auto& joints = skinnedObject_->GetModel()->GetJoints();
        int selectedJoint = skinnedObject_->GetSelectedJointIndex();
        
        // ジョイント名のリスト作成
        std::vector<const char*> jointNames;
        for (const auto& j : joints) {
            jointNames.push_back(j.name.c_str());
        }

        if (ImGui::Combo("Select Bone", &selectedJoint, jointNames.data(), static_cast<int>(jointNames.size()))) {
            skinnedObject_->SetSelectedJointIndex(selectedJoint);
        }

        if (selectedJoint >= 0 && selectedJoint < static_cast<int>(joints.size())) {
            auto& joint = joints[selectedJoint];
            
            ImGui::Text("Index: %d | Parent: %d", selectedJoint, joint.parentIndex);
            ImGui::Separator();

            // 回転スライダー (ラジアン -> デグリー)
            Vector3 rotDeg = {
                joint.rotation.x * 180.0f / 3.14159265f,
                joint.rotation.y * 180.0f / 3.14159265f,
                joint.rotation.z * 180.0f / 3.14159265f
            };

            ImGui::Text("Rotation (Degrees):");
            if (ImGui::SliderFloat("Rot X", &rotDeg.x, -180.0f, 180.0f, "%.1f")) {
                joint.rotation.x = rotDeg.x * 3.14159265f / 180.0f;
            }
            if (ImGui::SliderFloat("Rot Y", &rotDeg.y, -180.0f, 180.0f, "%.1f")) {
                joint.rotation.y = rotDeg.y * 3.14159265f / 180.0f;
            }
            if (ImGui::SliderFloat("Rot Z", &rotDeg.z, -180.0f, 180.0f, "%.1f")) {
                joint.rotation.z = rotDeg.z * 3.14159265f / 180.0f;
            }

            ImGui::Separator();
            ImGui::Text("Translation Offset:");
            ImGui::DragFloat3("Translate", &joint.translation.x, 0.01f, -2.0f, 2.0f, "%.3f");

            ImGui::Text("Scale:");
            ImGui::DragFloat3("Scale", &joint.scale.x, 0.01f, 0.1f, 5.0f, "%.3f");
        } else {
            ImGui::Text("No bone selected.");
        }

        ImGui::End();
    }
}
#endif




bool MyGame::IsPlayerHiddenByWall() const {
    if (!player_ || !camera) {
        return false;
    }

    Vector3 camPos = camera->GetPosition();
    Vector3 playerPos = player_->GetPosition();

    // プレイヤーの中心より少し上を狙う
    playerPos.y += 0.8f;

    Vector3 diff = {
        playerPos.x - camPos.x,
        playerPos.y - camPos.y,
        playerPos.z - camPos.z
    };

    float length = std::sqrt(
        diff.x * diff.x +
        diff.y * diff.y +
        diff.z * diff.z
    );

    if (length <= 0.001f) {
        return false;
    }

    Vector3 dir = {
        diff.x / length,
        diff.y / length,
        diff.z / length
    };

    const float step = 0.8f;

    for (float t = step; t < length - 1.0f; t += step) {
        Vector3 checkPos = {
            camPos.x + dir.x * t,
            camPos.y + dir.y * t,
            camPos.z + dir.z * t
        };

        int gx = static_cast<int>(std::floor(checkPos.x + 0.5f));
        int gy = static_cast<int>(std::floor(checkPos.y));
        int gz = static_cast<int>(std::floor(checkPos.z + 0.5f));

        const MapCell* cell = stageMap_.GetCell(gx, gy, gz);

        if (cell && cell->isSolid) {
            return true;
        }
    }

    return false;
}


void MyGame::Draw() {
    auto commandList = dxCommon->GetCommandList();

    // ==========================================================
    // 【パス1】 シャドウマップへの描き込み（影の生成）
    // ==========================================================
    // ※ タイトルやクリア画面で影が不要なら if で囲っても良いですが、
    //    まずは「常に生成する」方がバグが起きにくく安全です。
    shadowMap_->PreDraw(commandList);

    commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    commandList->SetPipelineState(object3dCommon->GetShadowPipelineState());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // 影を描く対象（動くものすべて）
    for (auto& obj : objectList) {
        if (obj) {
            obj->DrawShadow(lightVP);
        }
    }


    if (player_) {
        player_->DrawShadow(lightVP);
    }

    if (currentMode_ == AppMode::SkinningEditor && skinnedObject_) {
        skinnedObject_->DrawShadow(lightVP);
    }

    if (stageRenderer_) {
        stageRenderer_->DrawShadow(lightVP);
    }

    shadowMap_->PostDraw(commandList);

#ifdef NDEBUG
    // Releaseビルド時は全画面表示にする
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(WinApp::kWindowWidth), static_cast<float>(WinApp::kWindowHeight), 0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0, WinApp::kWindowWidth, WinApp::kWindowHeight };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
#else
    // Debugビルド時は ImGui パネル用にビューポートを狭める
    D3D12_VIEWPORT viewport = { 320.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    D3D12_RECT scissor = { 320, 0, 1600, 720 };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
#endif

    // ==========================================================
    // 【パス2】 メイン描画（通常のレンダリング ＋ 影の適用）
    // ==========================================================
    dxCommon->PreDraw();

    if (debugFlags_.show3DObjects) {
        // --- 1. ヒープと共通設定（影を出すための最重要準備） ---
        ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, heaps);

        object3dCommon->PreDraw();

        // ★重要：スロット4(t1)に影テクスチャを渡す
        // これを各シーンの描画（Draw）より「前」に呼ぶのが影を出す秘訣です
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());

        // --- 2. シーンごとの分岐描画 ---

        // A. タイトルシーン
        if (currentMode_ == AppMode::Title) {
            if (titleScene_) titleScene_->Draw();
        }
        // ステージセレクト追加　05/10小林
        else if (currentMode_ == AppMode::StageSelect)
        {
            if (stageSelect_) stageSelect_->Draw();
        }
        // B. クリアシーン
        else if (currentMode_ == AppMode::GameClear) {
            if (gameClearScene_) gameClearScene_->Draw();
        }
        else if (currentMode_ == AppMode::SkinningEditor) {
            // 背景（天球）
            if (skydomeObject_) {
                skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                skydomeObject_->Draw();
            }
            // 地面グリッドの描画
            for (auto& line : gridLines_) {
                line->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                line->Update(lightVP);
                line->Draw();
            }
            if (skinnedObject_) {
                skinnedObject_->Draw();
                skinnedObject_->DrawSkeleton(object3dCommon.get(), debugCubeModel_.get(), camera->GetViewMatrix(), camera->GetProjectionMatrix());
            }
        }
        // C. 通常ゲーム画面（エディタ・プレイ中・デバッグ）
        else {
            // 背景（天球）
            if (skydomeObject_) {
                skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                skydomeObject_->Draw();
            }

            // ステージとプレイヤー
            if (currentMode_ == AppMode::StageEditor ||
                currentMode_ == AppMode::GamePlay ||
                currentMode_ == AppMode::GamePlay_BlockPlace) {

                if (stageRenderer_) stageRenderer_->Draw();
                if (currentMode_ == AppMode::GamePlay)
                {
                    if (player_) player_->Draw();

                    // 壁で隠れている時だけ白強調
                    if (IsPlayerHiddenByWall()) {
                        object3dCommon->PreDrawPlayerHighlight();
                        player_->DrawHighlight();

                        // 通常描画設定に戻す
                        object3dCommon->PreDraw();
                        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
                    }

                    if (gameplayUIManager_) {
                        gameplayUIManager_->Draw3DPrompts(currentMode_ == AppMode::GamePlay, player_.get(), object3dCommon.get(), commandList, shadowMap_->GetSrvHandle());
                    }

                }               
               
                if ((currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace) && mapCursor_) {
                    mapCursor_->Draw();
                }


            }

            // デバッグビュー（リストの全表示）
            if (currentMode_ == AppMode::DebugView) {
                for (auto& obj : objectList) {
                    if (obj) {
                        obj->Draw();
                    }
                }
                if (player_) {
                    player_->Draw();
                }
            }
        }
    }

    // --- 3. パーティクル・スプライト（共通） ---
    if (debugFlags_.showParticles) {
        particleManager->Draw();
    }

    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        spriteCommon->PreDraw();
        if (sprite) sprite->Draw();
    }

    //5/11佐倉

    if (gameplayUIManager_) {
        gameplayUIManager_->DrawSprites(currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);
    }

    if (blockInventoryUI_ && (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        blockInventoryUI_->Draw();
    }

    // --- 4. ImGui と 最終出力 ---
#ifndef NDEBUG
    dxCommon->EndImGui();
#endif


    dxCommon->PostDraw();
}

void MyGame::Finalize() {
    // 1. GPUの完了を待機（リソースが「使用中」にならないようにする）
    if (dxCommon) {
        dxCommon->WaitForGpu();
    }


    sound.Finalize();

#ifndef NDEBUG
    dxCommon->FinalizeImGui();
#endif


    // 2. シーン（描画物の所有者）を先に消す
    // GameClearSceneの11文字（COURSECLEAR）はこのタイミングで unique_ptr により解放されます
    gameClearScene_.reset();
    titleScene_.reset();

    // 3. 静的マネージャーの解放
    // モデル本体を消去（Object3dCommonより先に消す必要がある）
    ModelManager::Finalize();

    // 4. 動的に生成したオブジェクトリストのクリア
    // unique_ptr の vector なので、clear() で中身のデストラクタが呼ばれます
    objectList.clear();
    models.clear();

   
    // 5. その他のゲームオブジェクトを reset
    if (gameplayUIManager_) gameplayUIManager_->Finalize();
    gameplayUIManager_.reset();
    if (blockInventoryUI_) blockInventoryUI_->Finalize();
    blockInventoryUI_.reset();
    player_.reset();
    skinnedObject_.reset();
    debugCubeModel_.reset();
    skydomeObject_.reset();
    skydomeModel_.reset();
    sprite.reset();
    stageRenderer_.reset();
    mapCursor_.reset();
    camera.reset();
    shadowMap_.reset();
    lightCamera_.reset();

    // 6. システムマネージャー類を reset
    particleManager.reset();
    object3dCommon.reset();
    spriteCommon.reset();
    textureManager.reset();
    input.reset();

    // 7. 最後：DirectX基盤とウィンドウを reset
    // ここで Device の Refcount が正常に 0 へ向かいます
    dxCommon.reset();
    winApp.reset();
}
/// <summary>
/// ブロックを置けるようになる画面 
/// </summary>
void MyGame::UpdateGamePlayBlockPlace()
{
    // 現在カーソル位置
    const Int3& cursor = mapCursor_->GetIndex();

    // カーソル移動処理
    stageEditorController_.HandleCursorInput(input.get(), stageMap_, mapCursor_.get(), lightCamera_.get());

    // インベントリで選択されているブロックタイプを同期
    BlockType selectedType = BlockType::Ground;
    int selectedCustomId = 0;
    if (blockInventoryUI_) {
        selectedType = blockInventoryUI_->GetSelectedBlockType();
        selectedCustomId = blockInventoryUI_->GetSelectedCustomId();
        blockPlacementController_.SetPlaceBlockType(selectedType);
        blockPlacementController_.SetPlaceCustomId(selectedCustomId);
    } else {
        blockPlacementController_.SetPlaceBlockType(BlockType::Ground);
        blockPlacementController_.SetPlaceCustomId(0);
    }

    // 🌟 半透明リアルタイムプレビューを毎フレーム更新！！！
    if (stageRenderer_) {
        stageRenderer_->SetPlacementPreview(stageMap_, cursor, selectedType, selectedCustomId);
    }

    // ② ブロックを置く決定処理 (Enterキー または ゲーム画面上の左クリック)
    bool clickOnGameScreen = false;
    if (input->GetMouseState().buttons[0] && blockInventoryUI_) {
        // マウス座標がインベントリパネル外のときのみゲーム画面のクリックと判定
        float screenWidth = static_cast<float>(WinApp::kClientWidth);
        RECT rect;
        GetClientRect(winApp->GetHwnd(), &rect);
        float currentClientW = static_cast<float>(rect.right - rect.left);
        float scaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientW;
        float swapMouseX = static_cast<float>(input->GetMouseState().posX) * scaleX;
        float offsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
        float mouseX = swapMouseX - offsetX;

        if (mouseX < blockInventoryUI_->GetPanelLeftX()) {
            clickOnGameScreen = true;
        }
    }

    // 前フレームからのマウスクリックトリガーを自前で管理する
    static bool prevLeftClick = false;
    bool mouseTrigger = clickOnGameScreen && !prevLeftClick;
    prevLeftClick = (input->GetMouseState().buttons[0] && clickOnGameScreen);

    if (input->TriggerKey(DIK_RETURN) || mouseTrigger) {
        Int3 cursorPos = mapCursor_->GetIndex();

        // コントローラーを使ってブロックを配置
        if (blockPlacementController_.TryPlace(cursorPos)) {
            // 設置完了後、所持数が 0 になったら自動的に通常プレイに戻る
            BlockType currentType = blockInventoryUI_ ? blockInventoryUI_->GetSelectedBlockType() : BlockType::Ground;
            int currentCustomId = blockInventoryUI_ ? blockInventoryUI_->GetSelectedCustomId() : 0;
            bool hasRest = (currentType == BlockType::Ground) || blockInventory_.HasBlock(currentType, currentCustomId);
            if (!hasRest) {
                currentMode_ = AppMode::GamePlay;
                if (stageRenderer_) {
                    stageRenderer_->ClearPlacementPreview(); // 🌟 プレビューをクリア！
                }
                if (blockInventoryUI_) {
                    blockInventoryUI_->ToggleOpen(); // インベントリを閉じる
                }
            }
        }
    }

    // ③ キャンセルして戻る処理 (Escapeキー または インベントリUIが閉じられたとき)
    if (input->TriggerKey(DIK_ESCAPE) || (blockInventoryUI_ && !blockInventoryUI_->IsActive())) {
        currentMode_ = AppMode::GamePlay;
        if (stageRenderer_) {
            stageRenderer_->ClearPlacementPreview(); // 🌟 プレビューをクリア！
        }
        if (blockInventoryUI_ && blockInventoryUI_->IsActive()) {
            blockInventoryUI_->ToggleOpen(); // キーキャンセル時はインベントリも閉じる
        }
    }

    // カメラ操作
    stageEditorController_.HandleCameraInput(input.get(), camera.get());
}

void MyGame::UpdateTitle() {
    if (titleScene_) {
        titleScene_->Update();
    //シーン変化用のキー入力
        if (titleScene_->IsFinished()) {
            currentMode_ = AppMode::StageSelect;
        }
    }
}

void MyGame::UpdateStageSelect()
{
    stageSelect_->Update();

    if (stageSelect_->IsFnished())
    {
        // ① ステージセレクトから、選ばれたファイル名をもらってくる
        std::string fileName = stageSelect_->GetSelectedFileName();

        // ② 正しいパスを作る (Resources/Stages/ フォルダの中の fileName)
        std::string filePath = "Resources/Stages/" + fileName;

        // ③ そのファイルを読み込む！
        if (std::filesystem::exists(filePath))
        {
            stageMap_.LoadFromFile(filePath);
            backupMap_ = stageMap_;//バックアップ　05/14小林
            stageRenderer_->BuildFromStageMap(stageMap_); // 見た目の更新

            //5/14佐倉追加
            //カメラリセット処理
            // プレイヤーの位置をスタート地点に戻すなどの処理
            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());

            gameplayCameraController_.ResetCamera(camera.get(), player_.get());
        }
        // ゲームプレイモードへ切り替え
        currentMode_ = AppMode::GamePlay;
    }
}

void MyGame::UpdateSceneTransition()
{
    if ((currentMode_==AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) &&input->TriggerKey(DIK_ESCAPE))
    {
        //保存したのを復元
        stageMap_ = backupMap_;

        stageRenderer_->BuildFromStageMap(stageMap_); // モデルを初期配置に戻す
        bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_); // 取得状況をリセット
        stageSelect_->Initialize(object3dCommon.get(),input.get());
        
        isGoalReached_ = false;

        if (player_){player_->Respawn();}
        currentMode_ = AppMode::StageSelect;
    }
}
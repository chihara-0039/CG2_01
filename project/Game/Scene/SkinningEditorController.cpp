#include "SkinningEditorController.h"
#include "Player.h"
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "ParticleManager.h"
#include "externals/imgui/imgui.h"

#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>   // sprintf_s

// ==========================================================
//  SkinningEditorController::Initialize
//  SkinnedObject・デバッグキューブ・グリッド線・モデルリストを生成する
// ==========================================================
void SkinningEditorController::Initialize(
    Object3dCommon* object3dCommon,
    DirectXCommon*  dxCommon,
    TextureManager* textureManager)
{
    // 依存ポインタを保存 (所有権は持たない)
    object3dCommon_ = object3dCommon;
    dxCommon_       = dxCommon;
    textureManager_ = textureManager;

    // ----------------------------------------------------------
    // 1. glTF モデルのスキャン (Resources/Models 以下を再帰探索)
    // ----------------------------------------------------------
    ScanGltfModels();

    // ----------------------------------------------------------
    // 2. デバッグ用立方体モデルの読み込み (スケルトン描画に使用)
    // ----------------------------------------------------------
    debugCubeModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon, "Resources/Models/cube", "cube.obj", textureManager));

    // ----------------------------------------------------------
    // 3. プレビュー用 SkinnedObject の生成と初期化
    //    起動時はインデックス 0 (デフォルト人型) で初期化する
    // ----------------------------------------------------------
    skinnedObject_ = std::make_unique<SkinnedObject>();
    ChangePreviewModel(0);
    skinnedObject_->SetPosition({ 0.0f, 0.0f, 0.0f }); // 地面 (Y=0) に接地
    skinnedObject_->SetScale({ 1.0f, 1.0f, 1.0f });

    // ----------------------------------------------------------
    // 4. デバッグ用グリッド線の生成 (-10m 〜 +10m / 1m 刻み / X軸・Z軸方向)
    // ----------------------------------------------------------
    for (int i = -10; i <= 10; ++i) {
        // === X 方向に並ぶ縦線 (Z 方向に伸びる) ===
        auto lineX = std::make_unique<Object3d>();
        lineX->Initialize(object3dCommon);
        lineX->SetModel(debugCubeModel_.get());
        lineX->SetPosition({ (float)i, 0.0f, 0.0f });
        lineX->SetScale({ 0.015f, 0.002f, 10.0f }); // 極細・薄い
        lineX->SetRotation({ 0.0f, 0.0f, 0.0f });
        lineX->SetEnableLighting(false);
        // 中央 (X=0) は赤 (X軸色)、その他はグレー
        lineX->SetColor((i == 0)
            ? Vector4{ 0.8f, 0.2f, 0.2f, 1.0f }
            : Vector4{ 0.35f, 0.35f, 0.38f, 1.0f });
        gridLines_.push_back(std::move(lineX));

        // === Z 方向に並ぶ横線 (X 方向に伸びる) ===
        auto lineZ = std::make_unique<Object3d>();
        lineZ->Initialize(object3dCommon);
        lineZ->SetModel(debugCubeModel_.get());
        lineZ->SetPosition({ 0.0f, 0.0f, (float)i });
        lineZ->SetScale({ 10.0f, 0.002f, 0.015f });
        lineZ->SetRotation({ 0.0f, 0.0f, 0.0f });
        lineZ->SetEnableLighting(false);
        // 中央 (Z=0) は青 (Z軸色)、その他はグレー
        lineZ->SetColor((i == 0)
            ? Vector4{ 0.2f, 0.2f, 0.8f, 1.0f }
            : Vector4{ 0.35f, 0.35f, 0.38f, 1.0f });
        gridLines_.push_back(std::move(lineZ));
    }
}

// ==========================================================
//  SkinningEditorController::Update
//  レイキャスト選択・SkinnedObject 更新・グリッド線更新
// ==========================================================
void SkinningEditorController::Update(
    DirectXCommon*       dxCommon,
    Input*               input,
    Camera*              camera,
    const Matrix4x4&     lightVP,
    bool                 isGuiCaptured,
    ParticleManager*     particleManager)
{
    // ----------------------------------------------------------
    // 1. レイキャストによるジョイントクリック選択
    //    OBJ モードはスケルトンがないためこのブロックをスキップする
    //    (以前は関数全体を return していたため、カメラ更新も止まっていた → 修正済み)
    // ----------------------------------------------------------
    if (!isObjPreviewMode_ && skinnedObject_) {

    // ----------------------------------------------------------
    // 1. レイキャストによるジョイントクリック選択
    //    ImGui がマウスをキャプチャしている時はスキップする
    // ----------------------------------------------------------
    const auto& mouse = input->GetMouseState();
    static bool mouse0Pre = false;
    bool mouse0Trigger = mouse.buttons[0] && !mouse0Pre; // 左クリック立ち上がり
    mouse0Pre = mouse.buttons[0];

    if (mouse0Trigger && !isGuiCaptured) {
        // デバッグビルド時は左側 320px のオフセットがあるため補正する
        float mouseX = static_cast<float>(mouse.posX);
#ifndef NDEBUG
        mouseX -= 320.0f;          // ビューポートの X オフセット (左パネル幅)
        float drawWidth = 1280.0f; // ビューポートの幅
#else
        float drawWidth = static_cast<float>(WinApp::kClientWidth);
#endif
        // NDC 座標に変換 (-1 〜 +1)
        float ndcX =  (2.0f * mouseX) / drawWidth - 1.0f;
        float ndcY = 1.0f - (2.0f * static_cast<float>(mouse.posY)) / WinApp::kClientHeight;

        // ビュー・プロジェクション行列の逆行列でレイをワールド空間に変換
        Matrix4x4 vp    = Math::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        Matrix4x4 invVP = Math::Inverse(vp);

        // クリップ空間の Near / Far 点を逆投影するラムダ
        auto transformVec = [](const Vector4& v, const Matrix4x4& m) -> Vector4 {
            return {
                v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0],
                v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1],
                v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2],
                v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3],
            };
        };

        Vector4 nearW4 = transformVec({ ndcX, ndcY, 0.0f, 1.0f }, invVP);
        Vector4 farW4  = transformVec({ ndcX, ndcY, 1.0f, 1.0f }, invVP);

        Vector3 nearWorld = { nearW4.x / nearW4.w, nearW4.y / nearW4.w, nearW4.z / nearW4.w };
        Vector3 farWorld  = { farW4.x  / farW4.w,  farW4.y  / farW4.w,  farW4.z  / farW4.w };

        Vector3 rayOrigin = nearWorld;
        Vector3 rayDir    = Math::Normalize(Math::Subtract(farWorld, nearWorld));

        // オブジェクトのワールド行列を使ってジョイントのワールド位置を計算
        const auto& joints = skinnedObject_->GetModel()->GetJoints();
        Matrix4x4 objWorld = Math::MakeAffineMatrix(
            skinnedObject_->GetScale(),
            skinnedObject_->GetRotation(),
            skinnedObject_->GetPosition());

        // 各ジョイントとレイの球交差判定 → 最も手前のジョイントを選択
        int   closestJointIndex = -1;
        float minT              = FLT_MAX;
        const float clickRadius = 0.22f; // ボーンが選択しやすいよう少し大きめに設定

        for (size_t i = 0; i < joints.size(); ++i) {
            Matrix4x4 jointWorld = Math::Multiply(joints[i].globalMatrix, objWorld);
            Vector3   jointPos   = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };

            // レイと球の交差判定 (代数的手法)
            Vector3 m  = Math::Subtract(rayOrigin, jointPos);
            float   b  = m.x * rayDir.x + m.y * rayDir.y + m.z * rayDir.z;
            float   c  = (m.x * m.x + m.y * m.y + m.z * m.z) - (clickRadius * clickRadius);

            if (c > 0.0f && b > 0.0f) { continue; } // 球の外側かつレイが逆方向 → スキップ

            float discr = b * b - c;
            if (discr < 0.0f) { continue; } // 判別式が負 → 交点なし

            float t = -b - std::sqrt(discr);
            if (t < 0.0f) { t = 0.0f; }

            if (t < minT) {
                minT = t;
                closestJointIndex = static_cast<int>(i);
            }
        }

        // 最も手前のジョイントを選択状態にする
        if (closestJointIndex != -1) {
            skinnedObject_->SetSelectedJointIndex(closestJointIndex);
        }
    }
    } // if (!isObjPreviewMode_ && skinnedObject_)

    // ----------------------------------------------------------
    // 2. モデルの更新 (OBJ / SkinnedObject を切り替える)
    //    OBJ モードでもここは必ず通る (レイキャストのみスキップした)
    // ----------------------------------------------------------
    if (isObjPreviewMode_ && objPreviewObject_) {
        // OBJ モード: 通常の Object3d を更新する
        objPreviewObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        objPreviewObject_->Update(lightVP);
    } else if (skinnedObject_) {
        // glTF / デフォルト人型モード: SkinnedObject を更新する
        skinnedObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skinnedObject_->Update(dxCommon, lightVP);
    }

    UpdateHandParticleEmitter(particleManager);

    // ----------------------------------------------------------
    // 3. グリッド線の更新 (カメラ行列のセットと定数バッファ転送)
    //    SkinningEditor モード中のみ呼ばれるため、ここで安全に更新する
    // ----------------------------------------------------------
    for (auto& line : gridLines_) {
        line->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        line->Update(lightVP);
    }
}

void SkinningEditorController::UpdateHandParticleEmitter(ParticleManager* particleManager) {
    if (!emitHandParticles_ || !particleManager || !skinnedObject_ || isObjPreviewMode_) {
        return;
    }

    // 初回だけ手に相当するジョイントを名前候補から探してキャッシュする。
    if (handParticleJointIndex_ < 0) {
        handParticleJointIndex_ = skinnedObject_->FindJointIndexByNameHints({
            "hand_r", "r_hand", "right_hand", "righthand", "hand.r",
            "hand_l", "l_hand", "left_hand", "lefthand", "hand.l", "hand"
        });
    }

    // アニメーション後のジョイント行列から、手の現在ワールド座標を取得する。
    Vector3 handPosition{};
    if (!skinnedObject_->TryGetJointWorldPosition(handParticleJointIndex_, handPosition)) {
        return;
    }

    // 毎フレーム出すと強すぎるため、一定間隔で小さな火花として発生させる。
    handParticleTimer_ += 1.0f / 60.0f;
    if (handParticleTimer_ < 0.18f) {
        return;
    }
    handParticleTimer_ = 0.0f;

    // 既存のヒットエフェクトを手元用に小さく調整して使う。
    ParticleManager::HitEffectSettings settings{};
    settings.size = 0.32f;
    settings.brightness = 0.85f;
    settings.lifeScale = 0.55f;
    settings.slashCount = 2;
    settings.sparkCount = 14;
    settings.sparkSpeed = 0.58f;
    settings.sparkLength = 0.45f;
    settings.scatterRadius = 0.22f;
    settings.ringPower = 0.15f;
    settings.corePower = 0.75f;
    settings.crossPower = 0.0f;
    settings.pillarPower = 0.0f;
    settings.lightningCount = 0;
    settings.randomizePosition = true;
    settings.randomizeDirection = true;
    settings.randomizeScale = true;
    settings.randomizeLifetime = true;
    settings.randomizeColor = true;
    settings.coreColor = { 1.0f, 0.52f, 0.14f, 1.0f };
    settings.slashColor = { 1.0f, 0.42f, 0.08f, 1.0f };
    settings.sparkColor = { 1.0f, 0.72f, 0.18f, 1.0f };
    settings.sparkSecondaryColor = { 0.34f, 0.72f, 1.0f, 1.0f };
    settings.ringColor = { 1.0f, 0.38f, 0.08f, 1.0f };

    particleManager->EmitHitEffect(handPosition, settings);
}

// ==========================================================
//  SkinningEditorController::Draw
//  グリッド線・スキニングメッシュ・スケルトンを描画する
// ==========================================================
void SkinningEditorController::Draw(Object3dCommon* object3dCommon, Camera* camera) {
    // グリッド線の描画 (モードに関わらず常に表示)
    for (auto& line : gridLines_) {
        line->Draw();
    }

    if (isObjPreviewMode_ && objPreviewObject_) {
        // OBJ モード: 通常の Object3d として描画 (スケルトンなし)
        objPreviewObject_->Draw();

    } else if (skinnedObject_) {
        // glTF / デフォルト人型モード: スキニングメッシュとスケルトンを描画
        skinnedObject_->Draw();
        skinnedObject_->DrawSkeleton(
            object3dCommon, debugCubeModel_.get(),
            camera->GetViewMatrix(), camera->GetProjectionMatrix());
    }
}

// ==========================================================
//  SkinningEditorController::DrawShadow
//  シャドウマップへの描画 (スキニングメッシュが影を落とすため)
// ==========================================================
void SkinningEditorController::DrawShadow(const Matrix4x4& lightVP) {
    if (isObjPreviewMode_ && objPreviewObject_) {
        // OBJ モード: 通常の Object3d で影描画
        objPreviewObject_->DrawShadow(lightVP);
    } else if (skinnedObject_) {
        // glTF / デフォルト人型モード
        skinnedObject_->DrawShadow(lightVP);
    }
}

// ==========================================================
//  SkinningEditorController::DrawImGuiTimeline
//  下パネル (Tools & Controls) に描画するタイムライン UI
// ==========================================================
void SkinningEditorController::DrawImGuiTimeline() {
    if (!skinnedObject_) { return; }

    // OBJ モードはスケルトン・タイムラインが存在しないため代替メッセージを表示
    if (isObjPreviewMode_) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[ OBJ Model - No Animation ]");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "This model is a static OBJ and does not support\n"
            "skeletal animation or keyframe editing.\n\n"
            "To add animations, export from Blender as .gltf or .glb.");
        return;
    }

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Custom Motion Animation Timeline ]");

    auto*  model      = skinnedObject_->GetModel();
    float  duration   = model->GetMotionDuration();
    float  curTime    = skinnedObject_->GetCurrentKeyframeTime();
    bool   playCustom = skinnedObject_->IsPlayCustomAnimation();

    // ----------------------------------------------------------
    // タイムラインシークスライダー (全幅)
    // ----------------------------------------------------------
    ImGui::PushItemWidth(-1.0f);
    if (ImGui::SliderFloat("##TimelineSlider", &curTime, 0.0f, duration,
                           "Current Time: %.2f sec / %.2f sec")) {
        skinnedObject_->SetCurrentKeyframeTime(curTime);
        if (!playCustom) {
            skinnedObject_->ApplyMotion(curTime); // 停止中はシークと同時に適用
        }
    }
    ImGui::PopItemWidth();

    // ----------------------------------------------------------
    // トラックのビジュアル描画 (キーフレームひし形・目盛り・再生カーソル)
    // ----------------------------------------------------------
    float     width          = ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList     = ImGui::GetWindowDrawList();
    ImVec2    cursorScreenPos = ImGui::GetCursorScreenPos();

    const float trackHeight = 22.0f;
    ImVec2 trackMin = cursorScreenPos;
    ImVec2 trackMax = ImVec2(trackMin.x + width, trackMin.y + trackHeight);

    // 背景トラック (暗いグレー)
    drawList->AddRectFilled(trackMin, trackMax, IM_COL32(40, 40, 42, 255), 4.0f);
    drawList->AddRect(trackMin, trackMax, IM_COL32(80, 80, 85, 255), 4.0f);

    // 目盛り (0.1 秒ごと / 0.5 秒は長め)
    for (float t = 0.0f; t <= duration; t += 0.1f) {
        float ratio  = t / duration;
        float posX   = trackMin.x + ratio * width;
        float lineLen = (std::fmod(t, 0.5f) < 0.01f || std::abs(t - duration) < 0.01f) ? 14.0f : 7.0f;
        drawList->AddLine(
            ImVec2(posX, trackMin.y),
            ImVec2(posX, trackMin.y + lineLen),
            IM_COL32(130, 130, 135, 255));
    }

    // キーフレームマーク (ひし形) の描画
    const auto& motionData = model->GetMotionData();
    std::vector<float> kfTimes;
    if (!motionData.jointAnimations.empty()) {
        for (const auto& kf : motionData.jointAnimations[0].keyframes) {
            kfTimes.push_back(kf.time);
        }
    }
    for (float kfTime : kfTimes) {
        float  ratio  = kfTime / duration;
        float  posX   = trackMin.x + ratio * width;
        ImVec2 center = ImVec2(posX, trackMin.y + trackHeight * 0.5f);
        float  r      = 6.0f;
        // ひし形内部 (ゴールド)
        drawList->AddQuadFilled(
            ImVec2(center.x, center.y - r), ImVec2(center.x + r, center.y),
            ImVec2(center.x, center.y + r), ImVec2(center.x - r, center.y),
            IM_COL32(255, 196, 0, 255));
        // ひし形輪郭 (白)
        drawList->AddQuad(
            ImVec2(center.x, center.y - r), ImVec2(center.x + r, center.y),
            ImVec2(center.x, center.y + r), ImVec2(center.x - r, center.y),
            IM_COL32(255, 255, 255, 200));
    }

    // 再生時間カーソルの縦線 (赤)
    float  currentRatio = curTime / duration;
    float  cursorX      = trackMin.x + currentRatio * width;
    drawList->AddLine(
        ImVec2(cursorX, trackMin.y - 3.0f),
        ImVec2(cursorX, trackMax.y + 3.0f),
        IM_COL32(255, 60, 60, 255), 2.5f);
    drawList->AddTriangleFilled(
        ImVec2(cursorX - 5.0f, trackMin.y - 3.0f),
        ImVec2(cursorX + 5.0f, trackMin.y - 3.0f),
        ImVec2(cursorX, trackMin.y + 4.0f),
        IM_COL32(255, 60, 60, 255));

    ImGui::Dummy(ImVec2(0.0f, trackHeight + 8.0f));

    // ----------------------------------------------------------
    // タイムライン詳細リスト (ジョイント別キーフレーム一覧)
    // ----------------------------------------------------------
    ImGui::Separator();
    ImGui::BeginChild("KeyframeDetails", ImVec2(0, 0), true);
    ImGui::Columns(3, "TimelineColumns", false);
    ImGui::SetColumnWidth(0, 160.0f);
    ImGui::SetColumnWidth(1, width - 400.0f);
    ImGui::SetColumnWidth(2, 240.0f);

    // ヘッダー行
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Joint Name");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Registered Keyframes (Click to jump / preview)");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Current Trans / Rot (Euler)");
    ImGui::NextColumn();
    ImGui::Separator();

    // ジョイントごとの行
    auto& joints = model->GetJoints();
    for (size_t i = 0; i < motionData.jointAnimations.size(); ++i) {
        const auto& anim = motionData.jointAnimations[i];

        // 選択中ジョイントはゴールドで強調
        if (static_cast<int>(i) == skinnedObject_->GetSelectedJointIndex()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s (Selected)", anim.name.c_str());
        } else {
            ImGui::Text("%s", anim.name.c_str());
        }
        ImGui::NextColumn();

        // キーフレームボタン (クリックで時間をシーク)
        if (anim.keyframes.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No keyframes registered.");
        } else {
            for (size_t k = 0; k < anim.keyframes.size(); ++k) {
                char btnLabel[64];
                sprintf_s(btnLabel, "%.2fs##%d_%d", anim.keyframes[k].time, (int)i, (int)k);
                if (ImGui::Button(btnLabel, ImVec2(48, 20))) {
                    skinnedObject_->SetCurrentKeyframeTime(anim.keyframes[k].time);
                    skinnedObject_->ApplyMotion(anim.keyframes[k].time);
                }
                ImGui::SameLine();
            }
        }
        ImGui::NextColumn();

        // 現在の Translation / Rotation (度数法) を表示
        if (i < joints.size()) {
            const float rad2deg = 180.0f / 3.14159265f;
            ImGui::Text("T:(%.1f, %.1f) R:(%.0f, %.0f, %.0f)",
                joints[i].translation.x, joints[i].translation.y,
                joints[i].rotation.x * rad2deg,
                joints[i].rotation.y * rad2deg,
                joints[i].rotation.z * rad2deg);
        }
        ImGui::NextColumn();
        ImGui::Separator();
    }
    ImGui::EndChild();
}

// ==========================================================
//  SkinningEditorController::DrawImGuiSidePanel
//  右パネル (Skinning Editor) の内容を描画する
// ==========================================================
void SkinningEditorController::DrawImGuiSidePanel(Camera* camera, Player* player, Model* defaultObjModel) {
    if (!skinnedObject_) { return; }

    // ----------------------------------------------------------
    // [ Model Selection ] モデル選択コンボボックス
    // ----------------------------------------------------------
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Model Selection ]");

    {
        // modelNames_ の char* 配列を作成してコンボに渡す
        std::vector<const char*> modelNamePtrs;
        for (const auto& name : modelNames_) {
            modelNamePtrs.push_back(name.c_str());
        }

        if (!modelNamePtrs.empty()) {
            int tempIdx = selectedModelIndex_;
            if (ImGui::Combo("##ModelList", &tempIdx,
                             modelNamePtrs.data(), static_cast<int>(modelNamePtrs.size()))) {
                if (tempIdx != selectedModelIndex_) {
                    ChangePreviewModel(tempIdx);
                }
            }
        }
    }

    // ゲームに反映ボタン (緑)
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.1f, 0.55f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.75f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.05f, 0.4f,  0.15f, 1.0f));
    if (ImGui::Button("Apply to Game Player", ImVec2(-FLT_MIN, 26))) {
        ApplyModelToPlayer(player, defaultObjModel);
    }
    ImGui::PopStyleColor(3);

    // OBJ モード中はスキニング操作が使えない旨を表示
    if (isObjPreviewMode_) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
            "[OBJ Mode] No skeleton / animation.");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Import a .gltf/.glb for rigging.");
    }

    // 現在ゲームに適用中のモデル名を緑で表示
    if (activeGameModelIndex_ >= 0 && activeGameModelIndex_ < static_cast<int>(modelNames_.size())) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f),
                           "Active: %s", modelNames_[activeGameModelIndex_].c_str());
    }

    // ----------------------------------------------------------
    // [ Animation Selection ] アニメーション (モーション) 選択
    // ----------------------------------------------------------
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Animation Selection ]");

    {
        auto* previewModel = skinnedObject_->GetModel();
        if (previewModel) {
            const auto& motions = previewModel->GetMotions();
            if (!motions.empty()) {
                // モーション名リストの構築 (name フィールドが空なら "Motion_N" で代替)
                std::vector<std::string> motionNames;
                for (size_t i = 0; i < motions.size(); ++i) {
                    motionNames.push_back(
                        motions[i].name.empty()
                        ? ("Motion_" + std::to_string(i))
                        : motions[i].name);
                }
                std::vector<const char*> motionNamePtrs;
                for (const auto& n : motionNames) {
                    motionNamePtrs.push_back(n.c_str());
                }

                int currentAnimIdx = previewModel->GetActiveMotionIndex();
                if (currentAnimIdx < 0) { currentAnimIdx = 0; }

                if (ImGui::Combo("##AnimList", &currentAnimIdx,
                                 motionNamePtrs.data(), static_cast<int>(motionNamePtrs.size()))) {
                    previewModel->SetActiveMotionIndex(currentAnimIdx);
                    // glTF アニメーション再生：Custom Animation ON / Test Animation OFF に切り替え
                    skinnedObject_->SetPlayCustomAnimation(true);
                    skinnedObject_->SetPlayAnimation(false);
                }

                ImGui::Text("Total Motions: %d", static_cast<int>(motions.size()));
                if (currentAnimIdx >= 0 && currentAnimIdx < static_cast<int>(motions.size())) {
                    ImGui::Text("Duration: %.2f sec", motions[currentAnimIdx].duration);
                }
            } else {
                ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "No animations in this model.");
            }
        }
    }

    // ----------------------------------------------------------
    // [ Skinned Mesh Settings ] スキニングメッシュ設定
    // ----------------------------------------------------------
    ImGui::Separator();
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

    bool showJointAxes = skinnedObject_->IsShowJointAxes();
    if (ImGui::Checkbox("Show Selected Bone Axes", &showJointAxes)) {
        skinnedObject_->SetShowJointAxes(showJointAxes);
    }

    // 手ジョイントの位置をエミッターとして使う評価課題用の確認機能。
    if (ImGui::Checkbox("Emit Particles From Hand", &emitHandParticles_)) {
        handParticleTimer_ = 0.0f;
        handParticleJointIndex_ = -1;
    }
    if (emitHandParticles_) {
        const int resolvedJoint = handParticleJointIndex_ >= 0
            ? handParticleJointIndex_
            : skinnedObject_->FindJointIndexByNameHints({
                "hand_r", "r_hand", "right_hand", "righthand", "hand.r",
                "hand_l", "l_hand", "left_hand", "lefthand", "hand.l", "hand"
            });
        if (resolvedJoint >= 0 && resolvedJoint < static_cast<int>(skinnedObject_->GetModel()->GetJoints().size())) {
            ImGui::Text("Emitter Joint: %s", skinnedObject_->GetModel()->GetJoints()[resolvedJoint].name.c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Emitter Joint: not found");
        }
    }

    if (ImGui::Button("Reset to T-Pose", ImVec2(-FLT_MIN, 24))) {
        skinnedObject_->GetModel()->ResetPose();
    }

    // ----------------------------------------------------------
    // [ Camera Presets ] Blender スタイルのカメラプリセット
    // ----------------------------------------------------------
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Camera Presets (Blender Style) ]");

    float halfW = ImGui::GetContentRegionAvail().x * 0.5f;
    if (ImGui::Button("Focus Model", ImVec2(halfW, 24))) {
        camera->SetTarget({ 0.0f, 1.0f, 0.0f });
        camera->SetDistance(3.5f);
        camera->SetRotation({ 0.1f, 0.0f, 0.0f }); // ほぼ正面
    }
    ImGui::SameLine();
    if (ImGui::Button("Front View", ImVec2(-FLT_MIN, 24))) {
        camera->SetTarget({ 0.0f, 1.0f, 0.0f });
        camera->SetDistance(3.5f);
        camera->SetRotation({ 0.0f, 0.0f, 0.0f }); // 完全正面
    }
    if (ImGui::Button("Side View", ImVec2(halfW, 24))) {
        camera->SetTarget({ 0.0f, 1.0f, 0.0f });
        camera->SetDistance(3.5f);
        camera->SetRotation({ 0.0f, 1.5708f, 0.0f }); // 右横
    }
    ImGui::SameLine();
    if (ImGui::Button("Top View", ImVec2(-FLT_MIN, 24))) {
        camera->SetTarget({ 0.0f, 1.0f, 0.0f });
        camera->SetDistance(3.5f);
        camera->SetRotation({ 1.5708f, 0.0f, 0.0f }); // 真上
    }

    // ----------------------------------------------------------
    // [ Custom Motion Editor ] 手動キーフレームエディタ
    // ----------------------------------------------------------
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Custom Motion Editor ]");

    bool playCustom = skinnedObject_->IsPlayCustomAnimation();
    if (ImGui::Checkbox("Play Custom Motion", &playCustom)) {
        skinnedObject_->SetPlayCustomAnimation(playCustom);
        if (playCustom) {
            skinnedObject_->SetPlayAnimation(false); // テストアニメーションと排他
        }
    }

    if (ImGui::InputText("Motion Name", motionName_, IM_ARRAYSIZE(motionName_))) {
        skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
    }

    float duration = skinnedObject_->GetModel()->GetMotionDuration();
    if (ImGui::InputFloat("Motion Duration", &duration, 0.1f, 1.0f, "%.2f")) {
        if (duration < 0.1f) { duration = 0.1f; }
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
        motionStatus_ = "Keyframe added at " + std::to_string(curTime) + " sec";
    }
    if (ImGui::Button("Clear All Keyframes", ImVec2(-FLT_MIN, 24))) {
        skinnedObject_->ClearKeyframes();
        motionStatus_ = "Cleared all keyframes";
    }
    if (ImGui::Button("New Empty Motion", ImVec2(-FLT_MIN, 24))) {
        skinnedObject_->ClearKeyframes();
        skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
        skinnedObject_->SetCurrentKeyframeTime(0.0f);
        skinnedObject_->ApplyMotion(0.0f);
        motionStatus_ = "Started a new empty motion";
    }
    if (ImGui::Button("Generate Walk Preset", ImVec2(-FLT_MIN, 24))) {
        skinnedObject_->GenerateWalkPreset();
        strncpy_s(motionName_, "WalkPreset", _TRUNCATE);
        skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
        motionStatus_ = "Generated walk preset";
    }
    if (ImGui::Button("Generate Run Preset", ImVec2(-FLT_MIN, 24))) {
        skinnedObject_->GenerateRunPreset();
        strncpy_s(motionName_, "RunPreset", _TRUNCATE);
        skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
        motionStatus_ = "Generated run preset";
    }

    ImGui::InputText("Motion Path", motionPath_, IM_ARRAYSIZE(motionPath_));

    float saveLoadW = ImGui::GetContentRegionAvail().x * 0.5f;
    if (ImGui::Button("Save Motion to File", ImVec2(saveLoadW, 24))) {
        skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
        if (skinnedObject_->SaveMotion(motionPath_)) {
            hasCustomMotionFile_ = true;
            motionStatus_ = std::string("Saved: ") + motionPath_;
        } else {
            motionStatus_ = std::string("Save failed: ") + motionPath_;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Motion from File", ImVec2(-FLT_MIN, 24))) {
        if (skinnedObject_->LoadMotion(motionPath_)) {
            hasCustomMotionFile_ = true;
            const std::string& loadedName = skinnedObject_->GetModel()->GetActiveMotionName();
            strncpy_s(motionName_, loadedName.c_str(), _TRUNCATE);
            skinnedObject_->SetCurrentKeyframeTime(0.0f);
            motionStatus_ = std::string("Loaded: ") + motionPath_;
        } else {
            motionStatus_ = std::string("Load failed: ") + motionPath_;
        }
    }
    if (!motionStatus_.empty()) {
        ImGui::TextWrapped("%s", motionStatus_.c_str());
    }

    // ----------------------------------------------------------
    // [ Bone Transformations ] ボーン選択と回転・平行移動の手動調整
    // ----------------------------------------------------------
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Bone Transformations ]");

    auto& joints      = skinnedObject_->GetModel()->GetJoints();
    int   selectedJoint = skinnedObject_->GetSelectedJointIndex();

    // ジョイント名のドロップダウンリスト
    std::vector<const char*> jointNames;
    for (const auto& j : joints) {
        jointNames.push_back(j.name.c_str());
    }
    if (ImGui::Combo("Select Bone", &selectedJoint,
                     jointNames.data(), static_cast<int>(jointNames.size()))) {
        skinnedObject_->SetSelectedJointIndex(selectedJoint);
    }

    if (selectedJoint >= 0 && selectedJoint < static_cast<int>(joints.size())) {
        auto& joint = joints[selectedJoint];

        ImGui::Text("Index: %d | Parent: %d | Children: %d",
                    selectedJoint,
                    joint.parentIndex,
                    static_cast<int>(joint.childIndices.size()));
        ImGui::Separator();

        // 回転スライダー (ラジアン ↔ 度数法 で変換して表示)
        const float deg2rad = 3.14159265f / 180.0f;
        const float rad2deg = 180.0f / 3.14159265f;
        Vector3 rotDeg = {
            joint.rotation.x * rad2deg,
            joint.rotation.y * rad2deg,
            joint.rotation.z * rad2deg
        };

        ImGui::Text("Rotation (Degrees):");
        if (ImGui::SliderFloat("Rot X", &rotDeg.x, -180.0f, 180.0f, "%.1f")) {
            joint.rotation.x = rotDeg.x * deg2rad;
        }
        if (ImGui::SliderFloat("Rot Y", &rotDeg.y, -180.0f, 180.0f, "%.1f")) {
            joint.rotation.y = rotDeg.y * deg2rad;
        }
        if (ImGui::SliderFloat("Rot Z", &rotDeg.z, -180.0f, 180.0f, "%.1f")) {
            joint.rotation.z = rotDeg.z * deg2rad;
        }

        ImGui::Separator();
        ImGui::Text("Translation Offset:");
        ImGui::DragFloat3("Translate", &joint.translation.x, 0.01f, -2.0f, 2.0f, "%.3f");

        ImGui::Text("Scale:");
        ImGui::DragFloat3("Scale", &joint.scale.x, 0.01f, 0.1f, 5.0f, "%.3f");
    } else {
        ImGui::Text("No bone selected.");
    }
}

// ==========================================================
//  SkinningEditorController::ScanGltfModels  [private]
//  Resources/Models 以下の .gltf/.glb ファイルをスキャンしてリストに追加する
// ==========================================================
void SkinningEditorController::ScanGltfModels() {
    modelPaths_.clear();
    modelNames_.clear();

    // ----------------------------------------------------------
    // インデックス 0 : デフォルト人型 (組み込みスキニング)
    // ----------------------------------------------------------
    modelNames_.push_back("Default Humanoid (Skinning)");
    modelPaths_.push_back("Default");

    // ----------------------------------------------------------
    // インデックス 1以降 : Resources/Models 以下の OBJ を再帰スキャン
    //   OBJ は静止モデルとして Object3d で表示する
    // ----------------------------------------------------------
    objStartIndex_ = static_cast<int>(modelPaths_.size()); // OBJ の開始位置を記録
    const std::string modelsDir = "Resources/Models";
    if (std::filesystem::exists(modelsDir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(modelsDir)) {
            if (!entry.is_regular_file()) { continue; }
            std::string ext = entry.path().extension().string();
            if (ext != ".obj") { continue; }

            std::string relPath = entry.path().string();
            std::replace(relPath.begin(), relPath.end(), '\\', '/');
            modelPaths_.push_back(relPath);
            // ファイル名だけ表示 (例: player.obj)
            modelNames_.push_back("[OBJ] " + entry.path().filename().string());
        }
    }

    // ----------------------------------------------------------
    // OBJ の後 : glTF / GLB を再帰スキャン
    //   glTF は SkinnedObject でアニメーション付き表示する
    // ----------------------------------------------------------
    gltfStartIndex_ = static_cast<int>(modelPaths_.size()); // glTF の開始位置を記録
    if (std::filesystem::exists(modelsDir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(modelsDir)) {
            if (!entry.is_regular_file()) { continue; }
            std::string ext = entry.path().extension().string();
            if (ext != ".gltf" && ext != ".glb") { continue; }

            std::string relPath = entry.path().string();
            std::replace(relPath.begin(), relPath.end(), '\\', '/');
            modelPaths_.push_back(relPath);
            modelNames_.push_back("[glTF] " + entry.path().filename().string());
        }
    }
}

// ==========================================================
//  SkinningEditorController::ChangePreviewModel  [private]
//  プレビュー SkinnedObject を指定インデックスのモデルで再初期化する
// ==========================================================
void SkinningEditorController::ChangePreviewModel(int index) {
    if (index < 0 || index >= static_cast<int>(modelPaths_.size())) {
        return;
    }
    selectedModelIndex_ = index;

    // モデルごとにジョイント名が違うため、プレビュー切り替え時に検索をやり直す。
    handParticleJointIndex_ = -1;
    handParticleTimer_ = 0.0f;

    if (index == 0) {
        // ----------------------------------------------------------
        // デフォルト人型 (組み込みスキニング) : SkinnedObject で表示
        // ----------------------------------------------------------
        isObjPreviewMode_ = false;
        skinnedObject_->Initialize(object3dCommon_, dxCommon_, textureManager_);

    } else if (index >= objStartIndex_ && index < gltfStartIndex_) {
        // ----------------------------------------------------------
        // OBJ モデル : Object3d + Model で表示 (スキニングなし)
        //   directoryPath と filename に分割して CreateFromOBJ に渡す
        // ----------------------------------------------------------
        isObjPreviewMode_ = true;

        // フルパスからディレクトリとファイル名を分離する
        std::string fullPath = modelPaths_[index];
        size_t lastSlash = fullPath.rfind('/');
        std::string dir  = (lastSlash != std::string::npos) ? fullPath.substr(0, lastSlash) : ".";
        std::string file = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

        // OBJ を Model としてロードして Object3d にセット
        objPreviewModel_ = std::unique_ptr<Model>(
            Model::CreateFromOBJ(dxCommon_, dir, file, textureManager_));

        objPreviewObject_ = std::make_unique<Object3d>();
        objPreviewObject_->Initialize(object3dCommon_);
        objPreviewObject_->SetModel(objPreviewModel_.get());
        objPreviewObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
        objPreviewObject_->SetScale({ 1.0f, 1.0f, 1.0f });
        objPreviewObject_->SetRotation({ 0.0f, 0.0f, 0.0f });

    } else if (index >= gltfStartIndex_) {
        // ----------------------------------------------------------
        // glTF モデル : SkinnedObject でアニメーション付き表示
        // ----------------------------------------------------------
        isObjPreviewMode_ = false;
        skinnedObject_->InitializeFromGltf(
            object3dCommon_, dxCommon_, modelPaths_[index], textureManager_);
    }
}

// ==========================================================
//  SkinningEditorController::ApplyModelToPlayer  [private]
//  現在選択中のモデルをゲームプレイ用プレイヤーに反映する
// ==========================================================
void SkinningEditorController::ApplyModelToPlayer(Player* player, Model* defaultObjModel) {
    activeGameModelIndex_ = selectedModelIndex_;
    if (!player) { return; }

    if (activeGameModelIndex_ == 0) {
        // ----------------------------------------------------------
        // デフォルト人型スキニング
        // ----------------------------------------------------------
        player->InitializeWithDefaultSkinned(object3dCommon_, dxCommon_, textureManager_);

    } else if (activeGameModelIndex_ >= objStartIndex_ && activeGameModelIndex_ < gltfStartIndex_) {
        // ----------------------------------------------------------
        // OBJ モデルをプレイヤーに適用
        //   Player::Initialize() は Model* を受け取るため、
        //   プレビュー用の objPreviewModel_ をそのまま渡す
        //   (Player は Model の所有権を持たないので安全)
        // ----------------------------------------------------------
        if (objPreviewModel_) {
            player->Initialize(object3dCommon_, objPreviewModel_.get());
        } else {
            // フォールバック: デフォルト OBJ モデルを使用
            player->Initialize(object3dCommon_, defaultObjModel);
        }

    } else if (activeGameModelIndex_ >= gltfStartIndex_) {
        // ----------------------------------------------------------
        // glTF モデルをプレイヤーに適用
        // ----------------------------------------------------------
        player->InitializeWithSkinnedGltf(
            object3dCommon_, dxCommon_, modelPaths_[activeGameModelIndex_], textureManager_);
    }

    if (hasCustomMotionFile_ && player->IsSkinned() && player->GetSkinnedObject()) {
        if (player->GetSkinnedObject()->LoadMotion(motionPath_)) {
            player->GetSkinnedObject()->SetPlayAnimation(false);
            player->GetSkinnedObject()->SetPlayCustomAnimation(true);
            motionStatus_ = std::string("Applied model and motion to player: ") + motionPath_;
        } else {
            motionStatus_ = std::string("Applied model, but motion load failed: ") + motionPath_;
        }
    }

    // プレイヤーをデフォルトのスタート位置に配置
    player->SetPosition({ 0.0f, 1.5f, 0.0f });
}

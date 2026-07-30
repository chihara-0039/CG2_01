#include "SkinnedObject.h"
#include "MyMath.h"
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <cctype>

namespace {
// ジョイント名の部分一致検索で大文字小文字を無視するための小文字化。
std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

// このプロジェクトの行列は平行移動を 4 行目に持つため、そこから座標を抜き出す。
Vector3 ExtractTranslation(const Matrix4x4& matrix) {
    return { matrix.m[3][0], matrix.m[3][1], matrix.m[3][2] };
}

// 軸方向が潰れている場合でもデバッグ描画が破綻しないようにする。
Vector3 NormalizeSafe(const Vector3& value, const Vector3& fallback) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.0001f) {
        return fallback;
    }
    return { value.x / length, value.y / length, value.z / length };
}

// Mixamo等が持つ末端補助Jointは変形階層には必要だが、
// ボーン表示へ含めると頭頂や指先に長い突起が出るため通常表示から除外する。
bool IsTerminalHelperJoint(const std::string& jointName) {
    const std::string lowerName = ToLower(jointName);
    return lowerName.find("_end") != std::string::npos ||
        lowerName.ends_with("end") ||
        lowerName.find("nub") != std::string::npos;
}

// デフォルト表示する名前を主要な人体Jointへ絞る。
// 指の全節を同時表示すると文字が重なるため、選択中Jointはこの判定に関係なく表示する。
bool IsMajorJointName(const std::string& jointName) {
    const std::string lowerName = ToLower(jointName);
    if (IsTerminalHelperJoint(jointName)) {
        return false;
    }

    static const std::vector<std::string> majorNames = {
        "hips", "pelvis", "spine", "neck", "head",
        "shoulder", "arm", "forearm", "hand",
        "upleg", "thigh", "leg", "knee", "foot"
    };
    return std::any_of(majorNames.begin(), majorNames.end(), [&](const std::string& name) {
        return lowerName.find(name) != std::string::npos;
    });
}

// 画面表示用に名前空間部分を外す。例: "mixamorig:LeftHand" -> "LeftHand"。
std::string MakeJointDisplayName(const std::string& jointName) {
    const size_t separator = jointName.find_last_of(":|/");
    return separator == std::string::npos
        ? jointName
        : jointName.substr(separator + 1);
}

// 三角形1枚を非インデックス頂点列へ追加する。
// デバッグ形状は面数が少ないため、面ごとに法線を持たせて輪郭を読みやすくする。
void AddDebugTriangle(
    std::vector<ModelVertexData>& vertices,
    const Vector3& a,
    const Vector3& b,
    const Vector3& c)
{
    const Vector3 edgeAB = Math::Subtract(b, a);
    const Vector3 edgeAC = Math::Subtract(c, a);
    const Vector3 normal = NormalizeSafe(Math::Cross(edgeAB, edgeAC), { 0.0f, 1.0f, 0.0f });

    vertices.push_back({ { a.x, a.y, a.z, 1.0f }, { 0.5f, 0.0f }, normal });
    vertices.push_back({ { b.x, b.y, b.z, 1.0f }, { 0.0f, 1.0f }, normal });
    vertices.push_back({ { c.x, c.y, c.z, 1.0f }, { 1.0f, 1.0f }, normal });
}

// BlenderのOctahedral Boneに近い、Y軸方向の先細りボーンを生成する。
// 下端が親Joint、上端が子Jointに対応し、中央より親側を太くして向きも判別できる。
std::vector<ModelVertexData> CreateOctahedralBoneVertices() {
    const Vector3 head = { 0.0f, -0.5f, 0.0f };
    const Vector3 tail = { 0.0f,  0.5f, 0.0f };
    const Vector3 ringXPositive = {  0.22f, -0.18f,  0.0f };
    const Vector3 ringXNegative = { -0.22f, -0.18f,  0.0f };
    const Vector3 ringZPositive = {  0.0f,  -0.18f,  0.22f };
    const Vector3 ringZNegative = {  0.0f,  -0.18f, -0.22f };

    std::vector<ModelVertexData> vertices;
    vertices.reserve(24);

    AddDebugTriangle(vertices, head, ringZPositive, ringXPositive);
    AddDebugTriangle(vertices, head, ringXNegative, ringZPositive);
    AddDebugTriangle(vertices, head, ringZNegative, ringXNegative);
    AddDebugTriangle(vertices, head, ringXPositive, ringZNegative);

    AddDebugTriangle(vertices, tail, ringXPositive, ringZPositive);
    AddDebugTriangle(vertices, tail, ringZPositive, ringXNegative);
    AddDebugTriangle(vertices, tail, ringXNegative, ringZNegative);
    AddDebugTriangle(vertices, tail, ringZNegative, ringXPositive);
    return vertices;
}

// 2点を結ぶ細い四角柱を追加する。
// Octahedral Boneの各辺を黒い立体線として描くために使用する。
void AddDebugEdgePrism(
    std::vector<ModelVertexData>& vertices,
    const Vector3& start,
    const Vector3& end,
    float radius)
{
    const Vector3 direction = NormalizeSafe(Math::Subtract(end, start), { 0.0f, 1.0f, 0.0f });
    const Vector3 reference =
        std::abs(direction.y) < 0.9f
        ? Vector3{ 0.0f, 1.0f, 0.0f }
        : Vector3{ 1.0f, 0.0f, 0.0f };
    const Vector3 side = NormalizeSafe(Math::Cross(direction, reference), { 1.0f, 0.0f, 0.0f });
    const Vector3 vertical = NormalizeSafe(Math::Cross(side, direction), { 0.0f, 0.0f, 1.0f });

    const Vector3 sideOffset = { side.x * radius, side.y * radius, side.z * radius };
    const Vector3 verticalOffset = {
        vertical.x * radius,
        vertical.y * radius,
        vertical.z * radius
    };
    const auto makeCorner = [](const Vector3& center, const Vector3& a, const Vector3& b) {
        return Vector3{ center.x + a.x + b.x, center.y + a.y + b.y, center.z + a.z + b.z };
    };
    const Vector3 negativeSide = { -sideOffset.x, -sideOffset.y, -sideOffset.z };
    const Vector3 negativeVertical = { -verticalOffset.x, -verticalOffset.y, -verticalOffset.z };

    const Vector3 s0 = makeCorner(start, sideOffset, verticalOffset);
    const Vector3 s1 = makeCorner(start, negativeSide, verticalOffset);
    const Vector3 s2 = makeCorner(start, negativeSide, negativeVertical);
    const Vector3 s3 = makeCorner(start, sideOffset, negativeVertical);
    const Vector3 e0 = makeCorner(end, sideOffset, verticalOffset);
    const Vector3 e1 = makeCorner(end, negativeSide, verticalOffset);
    const Vector3 e2 = makeCorner(end, negativeSide, negativeVertical);
    const Vector3 e3 = makeCorner(end, sideOffset, negativeVertical);

    AddDebugTriangle(vertices, s0, e0, e1);
    AddDebugTriangle(vertices, s0, e1, s1);
    AddDebugTriangle(vertices, s1, e1, e2);
    AddDebugTriangle(vertices, s1, e2, s2);
    AddDebugTriangle(vertices, s2, e2, e3);
    AddDebugTriangle(vertices, s2, e3, s3);
    AddDebugTriangle(vertices, s3, e3, e0);
    AddDebugTriangle(vertices, s3, e0, s0);
    AddDebugTriangle(vertices, s0, s1, s2);
    AddDebugTriangle(vertices, s0, s2, s3);
    AddDebugTriangle(vertices, e0, e3, e2);
    AddDebugTriangle(vertices, e0, e2, e1);
}

// BlenderのOctahedral表示と同じ12本の稜線を立体的な黒線として生成する。
// 単なる拡大輪郭ではなく、中央リングと両端へ収束する線を明示する。
std::vector<ModelVertexData> CreateOctahedralBoneEdgeVertices() {
    const Vector3 head = { 0.0f, -0.5f, 0.0f };
    const Vector3 tail = { 0.0f,  0.5f, 0.0f };
    const Vector3 ring[] = {
        {  0.22f, -0.18f,  0.0f },
        {  0.0f,  -0.18f,  0.22f },
        { -0.22f, -0.18f,  0.0f },
        {  0.0f,  -0.18f, -0.22f }
    };

    std::vector<ModelVertexData> vertices;
    vertices.reserve(12 * 36);
    // 深度無効のX-Ray表示では裏側の稜線も重なるため、太すぎると面全体が黒くなる。
    // Blenderの細いワイヤ表示に近づけ、面色を十分残せる半径にする。
    constexpr float kEdgeRadius = 0.008f;
    for (size_t index = 0; index < 4; ++index) {
        const size_t nextIndex = (index + 1) % 4;
        AddDebugEdgePrism(vertices, head, ring[index], kEdgeRadius);
        AddDebugEdgePrism(vertices, tail, ring[index], kEdgeRadius);
        AddDebugEdgePrism(vertices, ring[index], ring[nextIndex], kEdgeRadius);
    }
    return vertices;
}

// Joint位置を示す低ポリゴン球を生成する。
// Octahedral Bone本体と形状を明確に区別でき、関節の回転中心も読み取りやすい。
std::vector<ModelVertexData> CreateJointMarkerVertices() {
    std::vector<ModelVertexData> vertices;
    constexpr int kLatitudeSegments = 8;
    constexpr int kLongitudeSegments = 12;
    constexpr float kPi = 3.14159265358979323846f;
    vertices.reserve(kLatitudeSegments * kLongitudeSegments * 6);

    const auto spherePoint = [](float latitude, float longitude) {
        const float horizontalRadius = std::cos(latitude);
        return Vector3{
            horizontalRadius * std::cos(longitude),
            std::sin(latitude),
            horizontalRadius * std::sin(longitude)
        };
    };

    for (int latitudeIndex = 0; latitudeIndex < kLatitudeSegments; ++latitudeIndex) {
        const float latitude0 =
            -kPi * 0.5f + kPi * static_cast<float>(latitudeIndex) /
            static_cast<float>(kLatitudeSegments);
        const float latitude1 =
            -kPi * 0.5f + kPi * static_cast<float>(latitudeIndex + 1) /
            static_cast<float>(kLatitudeSegments);

        for (int longitudeIndex = 0; longitudeIndex < kLongitudeSegments; ++longitudeIndex) {
            const float longitude0 =
                2.0f * kPi * static_cast<float>(longitudeIndex) /
                static_cast<float>(kLongitudeSegments);
            const float longitude1 =
                2.0f * kPi * static_cast<float>(longitudeIndex + 1) /
                static_cast<float>(kLongitudeSegments);

            const Vector3 p00 = spherePoint(latitude0, longitude0);
            const Vector3 p01 = spherePoint(latitude0, longitude1);
            const Vector3 p10 = spherePoint(latitude1, longitude0);
            const Vector3 p11 = spherePoint(latitude1, longitude1);
            AddDebugTriangle(vertices, p00, p10, p11);
            AddDebugTriangle(vertices, p00, p11, p01);
        }
    }
    return vertices;
}

#ifdef USE_IMGUI
// ワールド座標を現在のエディタビューポート上のピクセル座標へ変換する。
bool ProjectJointLabelToScreen(
    const Vector3& worldPosition,
    const Matrix4x4& view,
    const Matrix4x4& projection,
    ImVec2& screenPosition)
{
    const Matrix4x4 viewProjection = Math::Multiply(view, projection);
    const float clipX =
        worldPosition.x * viewProjection.m[0][0] +
        worldPosition.y * viewProjection.m[1][0] +
        worldPosition.z * viewProjection.m[2][0] +
        viewProjection.m[3][0];
    const float clipY =
        worldPosition.x * viewProjection.m[0][1] +
        worldPosition.y * viewProjection.m[1][1] +
        worldPosition.z * viewProjection.m[2][1] +
        viewProjection.m[3][1];
    const float clipW =
        worldPosition.x * viewProjection.m[0][3] +
        worldPosition.y * viewProjection.m[1][3] +
        worldPosition.z * viewProjection.m[2][3] +
        viewProjection.m[3][3];
    if (clipW <= 0.0001f) {
        return false;
    }

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

#ifdef NDEBUG
    const float viewportX = 0.0f;
    const float viewportY = 0.0f;
    const float viewportWidth = displaySize.x;
    const float viewportHeight = displaySize.y;
#else
    // GameRuntimeRenderingのUnity風レイアウトと同じ比率を使用する。
    const float viewportX = std::clamp(displaySize.x * 0.15f, 240.0f, 300.0f);
    const float inspectorWidth = std::clamp(displaySize.x * 0.20f, 340.0f, 400.0f);
    const float viewportY = 38.0f;
    float bottomPanel = std::clamp(displaySize.y * 0.32f, 280.0f, 420.0f);
    if (displaySize.y < 820.0f) {
        bottomPanel = std::clamp(displaySize.y * 0.28f, 220.0f, 320.0f);
    }
    const float viewportWidth = (std::max)(480.0f, displaySize.x - viewportX - inspectorWidth);
    const float viewportHeight = (std::max)(300.0f, displaySize.y - viewportY - bottomPanel);
#endif

    screenPosition.x = viewportX + (ndcX * 0.5f + 0.5f) * viewportWidth;
    screenPosition.y = viewportY + (-ndcY * 0.5f + 0.5f) * viewportHeight;
    return screenPosition.x >= viewportX &&
        screenPosition.x <= viewportX + viewportWidth &&
        screenPosition.y >= viewportY &&
        screenPosition.y <= viewportY + viewportHeight;
}

// 黒い縁取りを付け、モデルの明暗にかかわらずJoint名を読み取れるようにする。
void DrawOutlinedJointName(const ImVec2& position, const char* name, ImU32 color) {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList || !name || name[0] == '\0') {
        return;
    }

    const ImU32 outline = IM_COL32(10, 12, 16, 230);
    const ImVec2 textPosition = { position.x + 7.0f, position.y - 8.0f };
    drawList->AddText({ textPosition.x - 1.0f, textPosition.y }, outline, name);
    drawList->AddText({ textPosition.x + 1.0f, textPosition.y }, outline, name);
    drawList->AddText({ textPosition.x, textPosition.y - 1.0f }, outline, name);
    drawList->AddText({ textPosition.x, textPosition.y + 1.0f }, outline, name);
    drawList->AddText(textPosition, color, name);
}
#endif

// 選択中ジョイントのローカル軸を細い棒として描画する。
void DrawAxisRod(
    Object3d& axisObject,
    const Vector3& start,
    const Vector3& direction,
    const Vector4& color,
    float length,
    const Matrix4x4& view,
    const Matrix4x4& projection)
{
    const Vector3 dir = NormalizeSafe(direction, { 0.0f, 1.0f, 0.0f });
    const Vector3 center = {
        start.x + dir.x * length * 0.5f,
        start.y + dir.y * length * 0.5f,
        start.z + dir.z * length * 0.5f
    };
    const float yaw = std::atan2(dir.x, dir.z);
    const float pitch = std::atan2(std::sqrt(dir.x * dir.x + dir.z * dir.z), dir.y);

    axisObject.SetCamera(view, projection);
    axisObject.SetPosition(center);
    axisObject.SetRotation({ pitch, yaw, 0.0f });
    axisObject.SetScale({ 0.008f, length * 0.5f, 0.008f });
    axisObject.SetColor(color);
    axisObject.SetEnableLighting(false);
    axisObject.Update(Math::MakeIdentity4x4());
    axisObject.Draw();
}
}

void SkinnedObject::Initialize(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, TextureManager* textureManager) {
    // 再初期化では、旧Modelへの生ポインタを持つObject3dを先に破棄する。
    ResetModelDependentResources();

    // 1. スキニングモデルの生成
    skinnedModel_ = std::make_unique<SkinnedModel>();
    skinnedModel_->Initialize(dxCommon, textureManager);

    // 2. 表示用のObject3dを初期化して、SkinnedModel内部のModelを登録
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(skinnedModel_->GetModel());
    whiteTextureHandle_ = textureManager->LoadTexture("Resources/Models/Work/human/white.png");
    CreateSkeletonDebugModels(dxCommon, textureManager);
}

void SkinnedObject::InitializeFromGltf(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, const std::string& filePath, TextureManager* textureManager) {
    // 再初期化では、旧Modelへの生ポインタを持つObject3dを先に破棄する。
    ResetModelDependentResources();

    // 1. スキニングモデルをglTFから生成
    skinnedModel_ = std::make_unique<SkinnedModel>();
    skinnedModel_->InitializeFromGltf(dxCommon, filePath, textureManager);

    // 2. 表示用のObject3dを初期化して、SkinnedModel内部のModelを登録
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(skinnedModel_->GetModel());
    whiteTextureHandle_ = textureManager->LoadTexture("Resources/Models/Work/human/white.png");
    CreateSkeletonDebugModels(dxCommon, textureManager);
}

void SkinnedObject::ResetModelDependentResources() {
    // Object3dはModelを生ポインタで参照するため、Modelより先に必ず破棄する。
    jointVisuals_.clear();
    jointOutlineVisuals_.clear();
    boneVisuals_.clear();
    boneOutlineVisuals_.clear();
    axisVisuals_.clear();
    object3d_.reset();

    // デバッグ用ModelとスキニングModelは、参照元を消した後に解放する。
    jointDebugModel_.reset();
    boneDebugModel_.reset();
    boneEdgeDebugModel_.reset();
    skinnedModel_.reset();

    // 新しいSkeletonの範囲外を選択・表示しないよう状態も初期化する。
    selectedJointIndex_ = -1;
    lastDrawnBoneCount_ = 0;
    skinningDispatchedThisFrame_ = false;
}

void SkinnedObject::CreateSkeletonDebugModels(DirectXCommon* dxCommon, TextureManager* textureManager) {
    if (!dxCommon || !textureManager) {
        return;
    }

    // 色はObject3dのMaterial定数で付けるため、元画像は白一色を使用する。
    const uint32_t whiteTexture =
        textureManager->LoadTexture("Resources/Models/Work/human/white.png");

    boneDebugModel_ = std::make_unique<Model>();
    boneDebugModel_->InitializeFromVertices(
        dxCommon,
        CreateOctahedralBoneVertices(),
        whiteTexture);

    boneEdgeDebugModel_ = std::make_unique<Model>();
    boneEdgeDebugModel_->InitializeFromVertices(
        dxCommon,
        CreateOctahedralBoneEdgeVertices(),
        whiteTexture);

    jointDebugModel_ = std::make_unique<Model>();
    jointDebugModel_->InitializeFromVertices(
        dxCommon,
        CreateJointMarkerVertices(),
        whiteTexture);
}

void SkinnedObject::Update(DirectXCommon* dxCommon, const Matrix4x4& lightVP) {
    // 影パスと通常パスで同じCompute Shaderを二重実行しないよう、
    // フレーム開始時に実行済みフラグを戻す。
    skinningDispatchedThisFrame_ = false;

    // 1. CPU側で現在フレームのボーン姿勢を決める。
    // playAnimation_ は組み込みテスト用、playCustomAnimation_ は glTF/自作モーション用。
    if (playAnimation_) {
        // 60FPS想定で時間を進める
        animationTime_ += (1.0f / 60.0f);
        skinnedModel_->ApplyTestAnimation(animationTime_, animationSpeed_);
    } else if (playCustomAnimation_) {
        // カスタムキーフレームモーションの再生
        const float deltaTime = (1.0f / 60.0f) * animationSpeed_;
        animationTime_ += deltaTime;
        float motionDuration = skinnedModel_->GetMotionDuration();
        if (!std::isfinite(motionDuration) || motionDuration <= 0.0f) {
            motionDuration = 0.001f;
        }
        currentKeyframeTime_ = std::fmod(animationTime_, motionDuration);
        if (currentKeyframeTime_ < 0.0f) {
            currentKeyframeTime_ += motionDuration;
        }

        if (playBlendAnimation_) {
            // モーション切り替え時は、現在のモーションからターゲットへ一定時間で補間する。
            blendElapsed_ += deltaTime;
            blendRate_ = blendDuration_ > 0.0f
                ? std::clamp(blendElapsed_ / blendDuration_, 0.0f, 1.0f)
                : 1.0f;
            skinnedModel_->ApplyMotionBlend(
                blendFromMotionIndex_,
                blendTargetMotionIndex_,
                currentKeyframeTime_,
                blendRate_);

            if (blendRate_ >= 1.0f) {
                skinnedModel_->SetActiveMotionIndex(blendTargetMotionIndex_);
                playBlendAnimation_ = false;
                blendFromMotionIndex_ = blendTargetMotionIndex_;
            }
        } else {
            skinnedModel_->ApplyMotion(currentKeyframeTime_);
        }
    }

    // 2. SkinnedModel 側でボーン行列を更新し、GPU用パレットへ転送する。
    skinnedModel_->Update(dxCommon);

    // 3. 更新後のボーンパレットからGPUスキニング済み頂点を生成する。
    DispatchSkinningOnce(dxCommon);

    // 3. 通常の Object3d と同じワールド行列を更新し、描画時の WVP に反映する。
    if (object3d_) {
        object3d_->SetPosition(position_);
        object3d_->SetRotation(rotation_);
        object3d_->SetScale(scale_);
        object3d_->SetCamera(viewMatrix_, projectionMatrix_);
        object3d_->Update(lightVP);
    }
}

void SkinnedObject::DispatchSkinningOnce(DirectXCommon* dxCommon) {
    if (skinningDispatchedThisFrame_ || !skinnedModel_ || !dxCommon) {
        return;
    }

    skinnedModel_->DispatchSkinning(dxCommon);
    skinningDispatchedThisFrame_ = true;
}

void SkinnedObject::Draw() {
    if (!skinnedModel_ || !object3d_) {
        return;
    }
    
    auto commandList = object3d_->GetObject3dCommon()->GetDxCommon()->GetCommandList();

    auto* object3dCommon = object3d_->GetObject3dCommon();
    if (object3dCommon->GetRootSignature()) {
        commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    }
    if (object3dCommon->GetPipelineState()) {
        commandList->SetPipelineState(object3dCommon->GetPipelineState());
    }
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 0. マテリアル
    commandList->SetGraphicsRootConstantBufferView(0, object3d_->GetMaterialResource()->GetGPUVirtualAddress());
    // 1. トランスフォーム
    commandList->SetGraphicsRootConstantBufferView(1, object3d_->GetTransformationResource()->GetGPUVirtualAddress());
    // 2. 平行光源
    commandList->SetGraphicsRootConstantBufferView(2, object3d_->GetObject3dCommon()->GetLightGPUVirtualAddress());
    // 3. テクスチャ
    if (object3d_->GetObject3dCommon()->GetTextureManager()) {
        const uint32_t textureHandle =
            useModelTexture_ ? skinnedModel_->GetTextureHandle() : whiteTextureHandle_;
        auto gpuHandle = object3d_->GetObject3dCommon()->GetTextureManager()->GetSrvHandleGPU(textureHandle);
        commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);

        auto environmentHandle = object3d_->GetObject3dCommon()->GetTextureManager()->GetSrvHandleGPU(object3d_->GetObject3dCommon()->GetEnvironmentTextureHandle());
        commandList->SetGraphicsRootDescriptorTable(6, environmentHandle);
    }

    const D3D12_VERTEX_BUFFER_VIEW& skinnedVertexBufferView = skinnedModel_->GetSkinnedVertexBufferView();
    commandList->IASetVertexBuffers(0, 1, &skinnedVertexBufferView);
    commandList->DrawInstanced(static_cast<UINT>(skinnedModel_->GetVertexCount()), 1, 0, 0);
}

void SkinnedObject::DrawShadow(const Matrix4x4& lightViewProjection) {
    if (!skinnedModel_ || !object3d_) {
        return;
    }

    // Object3dが保持している共通描画機能から、現在のコマンドリストを取得する。
    auto* object3dCommon = object3d_->GetObject3dCommon();
    auto* dxCommon = object3dCommon ? object3dCommon->GetDxCommon() : nullptr;
    auto* commandList = dxCommon ? dxCommon->GetCommandList() : nullptr;
    if (!commandList || !object3dCommon->GetRootSignature() ||
        !object3dCommon->GetShadowPipelineState()) {
        return;
    }

    commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    commandList->SetPipelineState(object3dCommon->GetShadowPipelineState());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // Object3d::Updateで書き込まれたライト用WVPを、影パスのTransform定数バッファへ設定する。
    commandList->SetGraphicsRootConstantBufferView(
        1, object3d_->GetTransformationResource()->GetGPUVirtualAddress());

    // 元モデルの頂点ではなく、Compute Shaderが更新したスキニング済み頂点を描画する。
    const D3D12_VERTEX_BUFFER_VIEW& skinnedVertexBufferView =
        skinnedModel_->GetSkinnedVertexBufferView();
    commandList->IASetVertexBuffers(0, 1, &skinnedVertexBufferView);
    commandList->DrawInstanced(
        static_cast<UINT>(skinnedModel_->GetVertexCount()), 1, 0, 0);

    // lightViewProjectionはObject3d::Update時点で定数バッファへ反映済み。
    // Object3d::DrawShadowと同じインターフェースを維持するため引数自体は残している。
    (void)lightViewProjection;
}
void SkinnedObject::DrawSkeleton(Object3dCommon* object3dCommon, Model* cubeModel, const Matrix4x4& view, const Matrix4x4& projection) {
    if (!showSkeleton_) {
        return;
    }

    // 初期化失敗時だけ従来モデルへフォールバックし、通常は専用の白い形状を使う。
    Model* jointModel = jointDebugModel_ ? jointDebugModel_.get() : cubeModel;
    Model* boneModel = boneDebugModel_ ? boneDebugModel_.get() : cubeModel;
    Model* boneEdgeModel = boneEdgeDebugModel_ ? boneEdgeDebugModel_.get() : boneModel;
    if (!jointModel || !boneModel) {
        return;
    }

    const auto& joints = skinnedModel_->GetJoints();
    size_t jointCount = joints.size();

    // ジョイントごとの球体マーカーを必要数だけ作成し、以後は使い回す。
    if (jointVisuals_.size() < jointCount) {
        jointVisuals_.resize(jointCount);
        jointOutlineVisuals_.resize(jointCount);
        for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
            jointVisuals_[jointIndex] = std::make_unique<Object3d>();
            jointVisuals_[jointIndex]->Initialize(object3dCommon);
            jointVisuals_[jointIndex]->SetModel(jointModel);
            jointVisuals_[jointIndex]->SetScale({ 0.04f, 0.04f, 0.04f });

            jointOutlineVisuals_[jointIndex] = std::make_unique<Object3d>();
            jointOutlineVisuals_[jointIndex]->Initialize(object3dCommon);
            jointOutlineVisuals_[jointIndex]->SetModel(jointModel);
        }
    }

    Matrix4x4 objWorld = Math::MakeAffineMatrix(scale_, rotation_, position_);

    // ボーンとジョイントはモデル内部でも確認できるよう、深度判定なしの
    // 不透明デバッグ専用パイプラインで常に手前へ描画する。
    object3dCommon->PreDrawDebugOverlay();

    // まず各ジョイント位置を点として描画する。選択中Jointだけ黄色で強調する。
#ifdef USE_IMGUI
    std::vector<ImVec2> occupiedLabelPositions;
#endif
    for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
        Matrix4x4 jointWorld = Math::Multiply(joints[jointIndex].globalMatrix, objWorld);

        jointVisuals_[jointIndex]->SetCamera(view, projection);
        
        Vector3 globalPos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };
        const bool terminalHelper = IsTerminalHelperJoint(joints[jointIndex].name);

        // インポートデータによってはEnd Jointだけ極端に離れている場合がある。
        // 階層は表示しつつ、デバッグ形状だけを親から一定距離以内へ収める。
        if (terminalHelper && joints[jointIndex].parentIndex >= 0) {
            const Matrix4x4 parentWorld = Math::Multiply(
                joints[static_cast<size_t>(joints[jointIndex].parentIndex)].globalMatrix,
                objWorld);
            const Vector3 parentPosition = ExtractTranslation(parentWorld);
            const Vector3 parentToEnd = Math::Subtract(globalPos, parentPosition);
            const float endDistance = std::sqrt(
                parentToEnd.x * parentToEnd.x +
                parentToEnd.y * parentToEnd.y +
                parentToEnd.z * parentToEnd.z);
            constexpr float kMaximumEndBoneLength = 0.22f;
            if (endDistance > kMaximumEndBoneLength) {
                const Vector3 direction = NormalizeSafe(parentToEnd, { 0.0f, 1.0f, 0.0f });
                globalPos = {
                    parentPosition.x + direction.x * kMaximumEndBoneLength,
                    parentPosition.y + direction.y * kMaximumEndBoneLength,
                    parentPosition.z + direction.z * kMaximumEndBoneLength
                };
            }
        }

        // 本体より少し大きい黒形状を先に描き、輪郭として残す。
        auto& jointOutline = jointOutlineVisuals_[jointIndex];
        jointOutline->SetCamera(view, projection);
        jointOutline->SetPosition(globalPos);
        jointOutline->SetRotation({ 0, 0, 0 });
        // 球がボーン本体を覆わないよう、回転中心を判別できる最小限の大きさにする。
        const float jointRadius = terminalHelper ? 0.008f : 0.015f;
        jointOutline->SetScale({
            jointRadius * 1.22f,
            jointRadius * 1.22f,
            jointRadius * 1.22f
        });
        jointOutline->SetColor({ 0.025f, 0.03f, 0.04f, 1.0f });
        jointOutline->SetEnableLighting(false);
        jointOutline->Update(Math::MakeIdentity4x4());
        jointOutline->Draw();

        jointVisuals_[jointIndex]->SetPosition(globalPos);
        jointVisuals_[jointIndex]->SetRotation({ 0, 0, 0 });
        jointVisuals_[jointIndex]->SetScale({ jointRadius, jointRadius, jointRadius });

        if (static_cast<int>(jointIndex) == selectedJointIndex_) {
            jointVisuals_[jointIndex]->SetColor({ 1.0f, 0.78f, 0.12f, 1.0f });
        } else {
            jointVisuals_[jointIndex]->SetColor({ 0.72f, 0.84f, 0.96f, 1.0f });
        }

        jointVisuals_[jointIndex]->SetEnableLighting(false);
        jointVisuals_[jointIndex]->Update(Math::MakeIdentity4x4());
        jointVisuals_[jointIndex]->Draw();

#ifdef USE_IMGUI
        const bool selectedJoint = static_cast<int>(jointIndex) == selectedJointIndex_;
        if (showJointNames_ && ImGui::GetCurrentContext() &&
            (selectedJoint || IsMajorJointName(joints[jointIndex].name))) {
            ImVec2 labelPosition{};
            if (ProjectJointLabelToScreen(globalPos, view, projection, labelPosition)) {
                const bool overlapsExistingLabel = !selectedJoint &&
                    std::any_of(
                        occupiedLabelPositions.begin(),
                        occupiedLabelPositions.end(),
                        [&](const ImVec2& occupied) {
                            return std::abs(occupied.x - labelPosition.x) < 76.0f &&
                                std::abs(occupied.y - labelPosition.y) < 13.0f;
                        });
                if (overlapsExistingLabel) {
                    continue;
                }
                occupiedLabelPositions.push_back(labelPosition);

                const ImU32 labelColor =
                    selectedJoint
                    ? IM_COL32(255, 208, 64, 255)
                    : IM_COL32(232, 236, 242, 245);
                const std::string displayName = MakeJointDisplayName(joints[jointIndex].name);
                DrawOutlinedJointName(labelPosition, displayName.c_str(), labelColor);
            }
        }
#endif
    }

    // 親から子へ向くOctahedral形状で、ボーンの長さと方向を可視化する。
    size_t boneVisualCount = 0;
    size_t drawnBoneCount = 0;
    for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
        int parentJointIndex = joints[jointIndex].parentIndex;
        if (parentJointIndex == -1) {
            continue;
        }

        if (boneVisuals_.size() <= boneVisualCount) {
            auto boneObj = std::make_unique<Object3d>();
            boneObj->Initialize(object3dCommon);
            boneObj->SetModel(boneModel);
            boneVisuals_.push_back(std::move(boneObj));

            auto outlineObj = std::make_unique<Object3d>();
            outlineObj->Initialize(object3dCommon);
            outlineObj->SetModel(boneEdgeModel);
            boneOutlineVisuals_.push_back(std::move(outlineObj));
        }

        auto& boneObj = boneVisuals_[boneVisualCount];
        auto& boneOutline = boneOutlineVisuals_[boneVisualCount];
        boneVisualCount++;

        Matrix4x4 parentJointWorld = Math::Multiply(joints[parentJointIndex].globalMatrix, objWorld);
        Matrix4x4 childJointWorld = Math::Multiply(joints[jointIndex].globalMatrix, objWorld);

        Vector3 parentPosition = { parentJointWorld.m[3][0], parentJointWorld.m[3][1], parentJointWorld.m[3][2] };
        Vector3 childPosition = { childJointWorld.m[3][0], childJointWorld.m[3][1], childJointWorld.m[3][2] };

        Vector3 parentToChild = Math::Subtract(childPosition, parentPosition);
        float boneLength = std::sqrt(
            parentToChild.x * parentToChild.x +
            parentToChild.y * parentToChild.y +
            parentToChild.z * parentToChild.z);
        if (boneLength < 0.001f) {
            continue;
        }

        Vector3 boneDirection = {
            parentToChild.x / boneLength,
            parentToChild.y / boneLength,
            parentToChild.z / boneLength
        };

        // End Jointは欠落させず、異常に長い補助ボーンだけ表示長を制限する。
        if (IsTerminalHelperJoint(joints[jointIndex].name)) {
            constexpr float kMaximumEndBoneLength = 0.22f;
            if (boneLength > kMaximumEndBoneLength) {
                boneLength = kMaximumEndBoneLength;
                childPosition = {
                    parentPosition.x + boneDirection.x * boneLength,
                    parentPosition.y + boneDirection.y * boneLength,
                    parentPosition.z + boneDirection.z * boneLength
                };
            }
        }

        Vector3 centerPos = {
            (parentPosition.x + childPosition.x) * 0.5f,
            (parentPosition.y + childPosition.y) * 0.5f,
            (parentPosition.z + childPosition.z) * 0.5f
        };

        float boneYaw = std::atan2(boneDirection.x, boneDirection.z);
        float bonePitch = std::atan2(
            std::sqrt(boneDirection.x * boneDirection.x + boneDirection.z * boneDirection.z),
            boneDirection.y);

        boneObj->SetCamera(view, projection);
        boneObj->SetPosition(centerPos);
        boneObj->SetRotation({ bonePitch, boneYaw, 0.0f });
        // キャラクター本体と重なっても面が認識できる太さを確保する。
        const float boneWidth = std::clamp(boneLength * 0.42f, 0.032f, 0.15f);

        boneObj->SetScale({ boneWidth, boneLength, boneWidth });

        const bool selectedBone =
            static_cast<int>(jointIndex) == selectedJointIndex_ ||
            parentJointIndex == selectedJointIndex_;
        boneObj->SetColor(selectedBone
            ? Vector4{ 1.0f, 0.78f, 0.12f, 1.0f }
            : Vector4{ 0.90f, 0.92f, 0.96f, 1.0f });
        boneObj->SetEnableLighting(false);

        boneObj->Update(Math::MakeIdentity4x4());
        boneObj->Draw();

        // 本体の後から12本の黒い稜線を重ね、Blender風の立体構造を明示する。
        boneOutline->SetCamera(view, projection);
        boneOutline->SetPosition(centerPos);
        boneOutline->SetRotation({ bonePitch, boneYaw, 0.0f });
        boneOutline->SetScale({ boneWidth, boneLength, boneWidth });
        boneOutline->SetColor({ 0.025f, 0.045f, 0.075f, 1.0f });
        boneOutline->SetEnableLighting(false);
        boneOutline->Update(Math::MakeIdentity4x4());
        boneOutline->Draw();
        drawnBoneCount++;
    }
    lastDrawnBoneCount_ = drawnBoneCount;

    if (showJointAxes_ &&
        selectedJointIndex_ >= 0 &&
        selectedJointIndex_ < static_cast<int>(jointCount)) {
        if (axisVisuals_.size() < 3) {
            axisVisuals_.resize(3);
            for (auto& axis : axisVisuals_) {
                axis = std::make_unique<Object3d>();
                axis->Initialize(object3dCommon);
                axis->SetModel(boneModel);
            }
        }

        const Matrix4x4 selectedJointWorld =
            Math::Multiply(joints[static_cast<size_t>(selectedJointIndex_)].globalMatrix, objWorld);
        const Vector3 jointPos = ExtractTranslation(selectedJointWorld);
        const Vector3 localX = { selectedJointWorld.m[0][0], selectedJointWorld.m[0][1], selectedJointWorld.m[0][2] };
        const Vector3 localY = { selectedJointWorld.m[1][0], selectedJointWorld.m[1][1], selectedJointWorld.m[1][2] };
        const Vector3 localZ = { selectedJointWorld.m[2][0], selectedJointWorld.m[2][1], selectedJointWorld.m[2][2] };

        DrawAxisRod(*axisVisuals_[0], jointPos, localX, { 1.0f, 0.15f, 0.15f, 1.0f }, 0.28f, view, projection);
        DrawAxisRod(*axisVisuals_[1], jointPos, localY, { 0.15f, 1.0f, 0.15f, 1.0f }, 0.28f, view, projection);
        DrawAxisRod(*axisVisuals_[2], jointPos, localZ, { 0.20f, 0.35f, 1.0f, 1.0f }, 0.28f, view, projection);
    }

    object3dCommon->PreDraw();
}

int SkinnedObject::FindJointIndexByNameHints(const std::vector<std::string>& nameHints) const {
    if (!skinnedModel_) {
        return -1;
    }

    const auto& joints = skinnedModel_->GetJoints();
    for (size_t i = 0; i < joints.size(); ++i) {
        const std::string jointName = ToLower(joints[i].name);
        for (const std::string& hint : nameHints) {
            if (hint.empty()) {
                continue;
            }

            const std::string loweredHint = ToLower(hint);
            if (jointName.find(loweredHint) != std::string::npos) {
                return static_cast<int>(i);
            }
        }
    }

    return -1;
}

bool SkinnedObject::TryGetJointWorldPosition(int jointIndex, Vector3& outPosition) const {
    Matrix4x4 jointWorld{};
    if (!TryGetJointWorldMatrix(jointIndex, jointWorld)) {
        return false;
    }

    outPosition = ExtractTranslation(jointWorld);
    return true;
}

bool SkinnedObject::TryGetJointWorldMatrix(int jointIndex, Matrix4x4& outMatrix) const {
    if (!skinnedModel_) {
        return false;
    }

    const auto& joints = skinnedModel_->GetJoints();
    if (jointIndex < 0 || jointIndex >= static_cast<int>(joints.size())) {
        return false;
    }

    const Matrix4x4 objWorld = Math::MakeAffineMatrix(scale_, rotation_, position_);
    outMatrix =
        Math::Multiply(joints[static_cast<size_t>(jointIndex)].globalMatrix, objWorld);
    return true;
}

bool SkinnedObject::TryGetJointWorldPosition(
    const std::vector<std::string>& nameHints,
    Vector3& outPosition) const
{
    const int jointIndex = FindJointIndexByNameHints(nameHints);
    return TryGetJointWorldPosition(jointIndex, outPosition);
}

void SkinnedObject::StartMotionBlend(int targetMotionIndex, float duration) {
    if (!skinnedModel_) {
        return;
    }

    const auto& motions = skinnedModel_->GetMotions();
    int activeMotionIndex = skinnedModel_->GetActiveMotionIndex();
    if (targetMotionIndex < 0 || targetMotionIndex >= static_cast<int>(motions.size()) ||
        activeMotionIndex < 0 || activeMotionIndex >= static_cast<int>(motions.size()) ||
        targetMotionIndex == activeMotionIndex) {
        return;
    }

    // Blend starts from the currently active animation and gradually reaches the selected target.
    blendFromMotionIndex_ = activeMotionIndex;
    blendTargetMotionIndex_ = targetMotionIndex;
    blendDuration_ = duration < 0.01f ? 0.01f : duration;
    blendElapsed_ = 0.0f;
    blendRate_ = 0.0f;
    playBlendAnimation_ = true;
    playCustomAnimation_ = true;
    playAnimation_ = false;
}


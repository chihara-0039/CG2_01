#include "Player.h"
#include "LadderTraversal.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr float kPlayerModelForwardYawOffset = 3.14159265f;

// 名前比較の表記揺れを減らすため、英字を小文字へそろえる。
std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// 名前に候補語のいずれかが含まれるかを、大文字小文字を無視して調べる。
bool ContainsAnyNameHint(const std::string &text, const std::vector<std::string> &hints) {
    const std::string lowerText = ToLower(text);
    for (const auto &hint : hints) {
        if (lowerText.find(hint) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// 候補名を順に照合し、プレイヤーの状態に合うモーションを探す。
int FindMotionIndexByName(const std::vector<MotionData> &motions, const std::vector<std::string> &hints) {
    for (size_t i = 0; i < motions.size(); ++i) {
        if (ContainsAnyNameHint(motions[i].name, hints)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}
}

Player::~Player() = default;

// 初期化：描画用コンポーネントとモデルを設定
void Player::Initialize(Object3dCommon *common, Model *model) {
    object_ = std::make_unique<Object3d>();
    object_->Initialize(common);
    object_->SetModel(model);
    // キノピオ隊長のように、モデルを直立させるための初期回転
    object_->SetRotation({0.0f, 0.0f, 0.0f});

    // 自機キャラクターの微小光沢・高級メタル反射設定
    object_->SetShininess(0.6f);
    object_->SetMetallic(0.15f);
    object_->SetEmissive(0.0f);

    skinnedObject_.reset();
    isSkinned_ = false;
    actionFlow_.Reset();
    animationState_ = nullptr;
}

// glTFプレイヤーを読み込み、モーションと描画設定を準備する。
void Player::InitializeWithSkinnedGltf(Object3dCommon *common, DirectXCommon *dxCommon, const std::string &gltfPath,
                                       TextureManager *textureManager) {
    skinnedObject_ = std::make_unique<SkinnedObject>();
    char legacy[8]{};
    GetEnvironmentVariableA("CG2_LEGACY_MODEL", legacy, sizeof(legacy));
    const bool isPlayerMesh = gltfPath.ends_with("/human/walk.gltf") || gltfPath.ends_with("\\human\\walk.gltf");
    skinnedObject_->InitializeFromGltf(common, dxCommon, gltfPath, textureManager, isPlayerMesh && legacy[0] != '1');
    if (auto *model = skinnedObject_->GetModel()) {
        model->EnsureDefaultPlayerMotions();
    }
    // カスタムモーション再生モードにして、再生時間とインデックスをプログラム側で制御する
    skinnedObject_->SetPlayAnimation(false);
    skinnedObject_->SetPlayCustomAnimation(true);
    skinnedObject_->SetAnimationSpeed(1.0f);

    object_.reset();
    isSkinned_ = true;
    actionFlow_.Reset();
    animationState_ = nullptr;
}

// 標準の人型モデルを用意し、スキニング付きプレイヤーとして初期化する。
void Player::InitializeWithDefaultSkinned(Object3dCommon *common, DirectXCommon *dxCommon,
                                          TextureManager *textureManager) {
    skinnedObject_ = std::make_unique<SkinnedObject>();
    skinnedObject_->Initialize(common, dxCommon, textureManager);
    if (auto *model = skinnedObject_->GetModel()) {
        model->EnsureDefaultPlayerMotions();
    }
    skinnedObject_->SetPlayAnimation(false);
    skinnedObject_->SetPlayCustomAnimation(true);
    skinnedObject_->SetAnimationSpeed(1.0f);

    object_.reset();
    isSkinned_ = true;
    actionFlow_.Reset();
    animationState_ = nullptr;
}

struct Player::MovementFrame {
    StageMap *map;
    float cameraYaw;
    int gx, ladderY, gz;
    float ladderX, ladderZ;
};
// 入力と衝突の事実だけを状態へ渡す。センサーの真偽値は状態の代用にしない。
PlayerActionFacts Player::ActionFacts() const {
    Vector3 dir{};
    if (input_) {
        dir.x = input_->GetGamePadState().leftStickX + (input_->PushKey(DIK_D) ? 1.f : 0.f) -
                (input_->PushKey(DIK_A) ? 1.f : 0.f);
        dir.z = input_->GetGamePadState().leftStickY + (input_->PushKey(DIK_W) ? 1.f : 0.f) -
                (input_->PushKey(DIK_S) ? 1.f : 0.f);
    }
    const float length = std::hypot(dir.x, dir.z);
    if (length > 1) {
        dir.x /= length;
        dir.z /= length;
    }
    return {isOnLadder_, isGrounded_, length > 0 || std::abs(velocity_.x) > .01f || std::abs(velocity_.z) > .01f,
            length > 0 && IsRunInputActive(dir), IsJumpTriggered()};
}
// 梯子状態から呼ばれる衝突付きの登攀処理。出口判定は従来の手順を維持する。
void Player::MoveLadder() {
    auto &map = *movementFrame_->map;
    const auto collision = [&](const Vector3 &pos) { return CheckCollision(pos, map, false); };
    const float cameraRotY = movementFrame_->cameraYaw;
    const int gx = movementFrame_->gx, ladderY = movementFrame_->ladderY, gz = movementFrame_->gz;
    const float ladderInputX = movementFrame_->ladderX, ladderInputZ = movementFrame_->ladderZ;

#pragma region はしご

    velocity_ = {0, 0, 0}; // 登攀前の慣性を持ち越さない

    // カメラから見て梯子へ向かう入力で登り、反対方向で降りる。
    const auto ladderMove =
        LadderTraversal::ResolveMovement(map, gx, ladderY, gz, cameraRotY, ladderInputX, ladderInputZ);
    const float moveVertical = ladderMove.vertical;
    const float moveSide = std::hypot(ladderMove.side.x, ladderMove.side.z);

    // 1. 登る・下りる入力がある時だけ上下移動と吸い寄せを行う
    if (moveVertical != 0.0f) {
        Vector3 nextPos = position_;
        nextPos.y += moveVertical * walkSpeed_;
        if (moveVertical > 0.0f) {
            int topY = ladderY;
            while (LadderTraversal::IsLadder(map, gx, topY + 1, gz)) {
                ++topY;
            }
            nextPos.y = (std::min)(nextPos.y, topY + 0.5f - 0.001f);
        }
        // 下端でも、実際に押しているワールド方向の出口を優先する。
        const float exitYaw = std::atan2(-ladderMove.world.x, -ladderMove.world.z);
        Vector3 exit{};
        if (moveVertical > 0.0f && LadderTraversal::TryExit(map, gx, ladderY, gz, position_, collision, exit)) {
            position_ = exit;
            isOnLadder_ = false;
        } else if (moveVertical < 0.0f &&
                   LadderTraversal::TryBottomExit(map, gx, ladderY, gz, position_, exitYaw, collision, exit)) {
            position_ = exit;
            isOnLadder_ = false;
            ladderBottomExitUntilRelease_ = true;
        } else if (!CheckCollision(nextPos, map)) {
            position_.y = nextPos.y;

            // ★修正：ハシゴの芯に吸い寄せる際にも壁判定を行う！
            // これで横からハシゴに触れてもブロックにめり込みません
            Vector3 targetPosX = position_;
            targetPosX.x += (static_cast<float>(gx) - position_.x) * 0.6f;
            if (!CheckCollision(targetPosX, map)) {
                position_.x = targetPosX.x;
            }

            Vector3 targetPosZ = position_;
            targetPosZ.z += (static_cast<float>(gz) - position_.z) * 0.6f;
            if (!CheckCollision(targetPosZ, map)) {
                position_.z = targetPosZ.z;
            }
        }

    } else if (moveSide != 0.0f) {
        // 梯子に沿う横方向の入力は、通常歩行と同じ向きへ移動する。
        const Vector3 side{ladderMove.side.x * walkSpeed_, 0, ladderMove.side.z * walkSpeed_};
        Vector3 nextPos = position_;
        nextPos.x += side.x;
        if (!CheckCollision(nextPos, map)) {
            position_.x = nextPos.x;
        }
        nextPos = position_;
        nextPos.z += side.z;
        if (!CheckCollision(nextPos, map)) {
            position_.z = nextPos.z;
        }
    }

    // ハシゴ中は接地扱いにしてジャンプなどを可能にする
    isGrounded_ = true;

#pragma endregion
}
// 地上・空中状態で共有する積分と衝突判定。状態が入力速度の係数を決める。
void Player::MoveGround(float inputScale, bool jumpRequested) {
    auto &map = *movementFrame_->map;
    const auto *input = input_;
    const float cameraRotY = movementFrame_->cameraYaw;
    Vector3 move{};

    // --- 【通常移動モード】（ここが消えていたのでフリーズしていました） ---

    // 入力方向をカメラの水平向きに合わせてワールド移動へ変換する。
    Vector3 inputDir = {0, 0, 0};
    if (input->PushKey(DIK_W)) {
        inputDir.z += 1.0f;
    }
    if (input->PushKey(DIK_S)) {
        inputDir.z -= 1.0f;
    }
    if (input->PushKey(DIK_A)) {
        inputDir.x -= 1.0f;
    }
    if (input->PushKey(DIK_D)) {
        inputDir.x += 1.0f;
    }
    inputDir.x += input->GetGamePadState().leftStickX;
    inputDir.z += input->GetGamePadState().leftStickY;

    if (inputDir.x != 0 || inputDir.z != 0) {
        float inputLength = std::sqrt(inputDir.x * inputDir.x + inputDir.z * inputDir.z);
        if (inputLength > 1.0f) {
            inputDir.x /= inputLength;
            inputDir.z /= inputLength;
        }
        const float movementScale = inputScale;
        inputDir.x *= movementScale;
        inputDir.z *= movementScale;

        move.x = inputDir.x * std::cos(cameraRotY) + inputDir.z * std::sin(cameraRotY);
        move.z = -inputDir.x * std::sin(cameraRotY) + inputDir.z * std::cos(cameraRotY);
        rotation_.y = std::atan2f(move.x, move.z) + kPlayerModelForwardYawOffset;
    }

#pragma region 滑る足場

    // 1. 足元のブロックを特定
    int gx = static_cast<int>(std::floor(position_.x + 0.5f));
    int gyBelow = static_cast<int>(std::floor(position_.y - 0.1f)); // 足の少し下
    int gz = static_cast<int>(std::floor(position_.z + 0.5f));

    const MapCell *cellBelow = map.GetCell(gx, gyBelow, gz);
    bool isOnIce = (cellBelow && cellBelow->type == BlockType::IceBlock);

    // 2. 加速度と摩擦係数を決定
    float acceleration = isOnIce ? 0.01f : 0.08f; // 氷なら加速が鈍い
    float friction = isOnIce ? 0.98f : 0.7f;      // 氷なら速度が減りにくい（1.0に近いほど滑る）

    // 加速
    velocity_.x += move.x * acceleration;
    velocity_.z += move.z * acceleration;

    // 摩擦（減速）
    velocity_.x *= friction;
    velocity_.z *= friction;

#pragma endregion

#pragma region 動く足場

    // ▼ ▼ 追加：動く足場への追従処理 ▼ ▼
    // 足元より少し下の位置をチェック
    Vector3 footCheckPos = position_;
    // プレイヤーの足元よりほんの少し低い位置を判定するための座標
    float footCheckY = position_.y - 0.05f;
    // 左右の判定半径を少しだけ小さく（0.8倍に）することで、ギリギリの端っこに乗ったときのガタつきを防ぐ
    float footRadiusX = radius_.x * 0.8f;
    float footRadiusZ = radius_.z * 0.8f;
    float footRadiusY = 0.05f; // 足元チェック用の薄い判定ボックスの高さ

    const MapCell *ridingFloor =
        map.GetIntersectingMovingFloor(position_.x, footCheckY, position_.z, footRadiusX, footRadiusY, footRadiusZ);
    if (ridingFloor) {
        // 足場が動いた分（deltaOffset）だけ、プレイヤーの座標も強制的に動かす
        position_.x += ridingFloor->deltaOffsetX;
        position_.y += ridingFloor->deltaOffsetY;
        position_.z += ridingFloor->deltaOffsetZ;

        // 足場の上に乗っているので、接地フラグを立てて重力による落下速度をリセット
        isGrounded_ = true;
        velocity_.y = 0.0f;
    }
    // ▲ ▲ ここまで ▲ ▲

#pragma endregion

#pragma region 中間地点

    // 🌟 追加：中間地点の判定
    // プレイヤーの現在位置のブロック座標を計算
    int px = static_cast<int>(std::floor(position_.x + 0.5f));
    int py = static_cast<int>(std::floor(position_.y + 0.5f)); // キャラクターの中心あたりの高さ
    int pz = static_cast<int>(std::floor(position_.z + 0.5f));

    const MapCell *cell = map.GetCell(px, py, pz);
    if (cell && cell->type == BlockType::Checkpoint) {
        // 中間地点に触れたらリスポーン地点を更新する
        const Vector3 checkpointPos = position_; // The current foot position is already collision-safe.
        SetRespawnPosition(checkpointPos);

        // ※必要に応じて「触れた瞬間に旗が上がる」ような演出を入れたり、
        // 何度も判定されないように cell->variant = 1; のようにして
        // 状態をフラグ管理するとより本格的になります！
    }

#pragma endregion

    // ジャンプ
    if ((isGrounded_ || coyoteFramesRemaining_ > 0) && jumpRequested) {
        velocity_.y = 0.2f;
        isGrounded_ = false;
        coyoteFramesRemaining_ = 0;
    }

    // 重力適用
    velocity_.y += gravity_;

    // X軸衝突判定
    Vector3 nextPosX = position_;
    nextPosX.x += velocity_.x;
    if (!CheckCollision(nextPosX, map)) {
        position_.x = nextPosX.x;
    }

    // Z軸衝突判定
    Vector3 nextPosZ = position_;
    nextPosZ.z += velocity_.z;
    if (!CheckCollision(nextPosZ, map)) {
        position_.z = nextPosZ.z;
    }

    // Y軸衝突判定
    Vector3 nextPosY = position_;
    nextPosY.y += velocity_.y;
    if (CheckCollision(nextPosY, map)) {
        if (velocity_.y < 0) {
            isGrounded_ = true;
        }
        velocity_.y = 0;
    } else {
        position_.y = nextPosY.y;
        isGrounded_ = false;
    }
}

// 更新：移動・重力・当たり判定の処理
void Player::Update(const Input *input, StageMap &map, float cameraRotY, const Matrix4x4 &lightVP,
                    DirectXCommon *dxCommon, bool deferFailure, bool deferTransform) {
    input_ = input;

    // UI表示フラグを毎フレームリセット
    isNearPSwitch_ = false;
    isNearKey_ = false;
    isNearKeyBlock_ = false;

    // 歩行と同じカメラ基準の入力で、足場の縁にある梯子へ降り始める。
    const auto collision = [&](const Vector3 &pos) { return CheckCollision(pos, map, false); };
    float ladderInputX = input->GetGamePadState().leftStickX;
    float ladderInputZ = input->GetGamePadState().leftStickY;
    if (input->PushKey(DIK_D)) {
        ladderInputX += 1;
    }
    if (input->PushKey(DIK_A)) {
        ladderInputX -= 1;
    }
    if (input->PushKey(DIK_W)) {
        ladderInputZ += 1;
    }
    if (input->PushKey(DIK_S)) {
        ladderInputZ -= 1;
    }
    ladderInputX = std::clamp(ladderInputX, -1.0f, 1.0f);
    ladderInputZ = std::clamp(ladderInputZ, -1.0f, 1.0f);
    const float ladderInputLength = std::hypot(ladderInputX, ladderInputZ);
    if (ladderInputLength < .2f) {
        ladderBottomExitUntilRelease_ = false;
    }
    Vector3 entry{};
    if (stageCollisionEnabled_ && !isOnLadder_ && !ladderBottomExitUntilRelease_ &&
        LadderTraversal::TryEnterFromTop(map, position_, collision, entry, cameraRotY, ladderInputX, ladderInputZ)) {
        position_ = entry;
        velocity_ = {0, 0, 0};
    }
    int gx = static_cast<int>(std::floor(position_.x + 0.5f));
    int gz = static_cast<int>(std::floor(position_.z + 0.5f));
    const int gyBottom = static_cast<int>(std::floor(position_.y + 0.5f));
    const int gyWaist = static_cast<int>(std::floor(position_.y + 0.8f + 0.5f));
    const MapCell *cellBottom = map.GetCell(gx, gyBottom, gz);
    const MapCell *cellWaist = map.GetCell(gx, gyWaist, gz);
    const MapCell *ladderCell = nullptr;
    int ladderY = gyBottom;
    if (cellBottom && cellBottom->type == BlockType::Ladder) {
        ladderCell = cellBottom;
    } else if (cellWaist && cellWaist->type == BlockType::Ladder) {
        ladderCell = cellWaist;
        ladderY = gyWaist;
    }
    const bool isOnLadder = stageCollisionEnabled_ && ladderCell;
    isOnLadder_ = isOnLadder;
    if (isOnLadder_) {
        ladderWorldPos_ = {static_cast<float>(gx), ladderY + 1.2f, static_cast<float>(gz)};
    }

    MovementFrame frame{&map, cameraRotY, gx, ladderY, gz, ladderInputX, ladderInputZ};
    movementFrame_ = &frame;
    actionFlow_.Update(*this);
    movementFrame_ = nullptr;

    if (isOnLadder_) {
        coyoteFramesRemaining_ = 0;
    } else if (isGrounded_) {
        coyoteFramesRemaining_ = kCoyoteFrames;
    } else if (coyoteFramesRemaining_ > 0) {
        --coyoteFramesRemaining_;
    }
    CrumbleUpdate(map);
    PSwitchUpdate(map);
    DoorWarp(map);
    OnOffSwitchUpdate(map);

    // ▼ 追加：鍵の取得チェック
    KeyUpdate(map);

    //鍵を持った状態で触れているかチェック
    KeyBlockUIUpdate(map);

    // --- 敵やトゲとの接触判定 ---
    {
        int px = static_cast<int>(std::floor(position_.x + 0.5f));
        int py = static_cast<int>(std::floor(position_.y + 0.5f));
        int pz = static_cast<int>(std::floor(position_.z + 0.5f));

        float pMinX = position_.x - radius_.x;
        float pMaxX = position_.x + radius_.x;
        float pMinY = position_.y;
        float pMaxY = position_.y + radius_.y * 2.0f;
        float pMinZ = position_.z - radius_.z;
        float pMaxZ = position_.z + radius_.z;

        // ① トゲとの接触判定（静的ブロックのためプレイヤーの周囲のみを高速探索）
        for (int dy = -1; dy <= 2; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int cx = px + dx;
                    int cy = py + dy;
                    int cz = pz + dz;

                    const MapCell *cell = map.GetCell(cx, cy, cz);
                    if (cell && !cell->isHidden) {
                        if (cell->type == BlockType::Spike) {
                            float eX = static_cast<float>(cx) + cell->currentOffsetX;
                            float eY = static_cast<float>(cy) + cell->currentOffsetY;
                            float eZ = static_cast<float>(cz) + cell->currentOffsetZ;

                            float eMinX = eX - 0.4f;
                            float eMaxX = eX + 0.4f;
                            float eMinY = eY - 0.5f;
                            float eMaxY = eY - 0.2f; // トゲ床は低い
                            float eMinZ = eZ - 0.4f;
                            float eMaxZ = eZ + 0.4f;

                            if (pMinX <= eMaxX && pMaxX >= eMinX && pMinY <= eMaxY && pMaxY >= eMinY &&
                                pMinZ <= eMaxZ && pMaxZ >= eMinZ) {
                                if (deferFailure) {
                                    failurePending_ = true;
                                } else {
                                    Respawn();
                                }
                                goto collision_end;
                            }
                        }
                    }
                }
            }
        }

        // ② 敵キャラクターとの接触判定（動いている敵の現在座標との正確な判定）
        for (const auto &enemyRef : map.GetEnemies()) {
            const MapCell *cell = map.GetCell(enemyRef.x, enemyRef.y, enemyRef.z);
            if (cell && !cell->isHidden) {
                float eX = static_cast<float>(enemyRef.x) + cell->currentOffsetX;
                float eY = static_cast<float>(enemyRef.y) + cell->currentOffsetY;
                float eZ = static_cast<float>(enemyRef.z) + cell->currentOffsetZ;

                float eMinX = eX - 0.4f;
                float eMaxX = eX + 0.4f;
                float eMinY = eY - 0.4f;
                float eMaxY = eY + 0.4f;
                float eMinZ = eZ - 0.4f;
                float eMaxZ = eZ + 0.4f;

                if (pMinX <= eMaxX && pMaxX >= eMinX && pMinY <= eMaxY && pMaxY >= eMinY && pMinZ <= eMaxZ &&
                    pMaxZ >= eMinZ) {
                    if (deferFailure) {
                        failurePending_ = true;
                    } else {
                        Respawn();
                    }
                    goto collision_end;
                }
            }
        }
    }
collision_end:

    // --- 表示更新 ---
    if (isSkinned_ && skinnedObject_) {
        skinnedObject_->SetPosition(position_);
        skinnedObject_->SetRotation(rotation_);
        skinnedObject_->SetScale({1.0f, 1.0f, 1.0f});

        ApplySkinnedAnimation(actionFlow_.Current());

        if (!deferTransform) {
            skinnedObject_->Update(dxCommon, lightVP);
        }
    } else if (object_) {
        object_->SetPosition(position_);
        object_->SetRotation(rotation_);
        if (!deferTransform) {
            object_->Update(lightVP);
        }
    }
}

// Object3d の行列を更新する（ライトカメラの行列も渡す）
void Player::UpdateTransform(const Matrix4x4 &lightVP, DirectXCommon *dxCommon) {
    if (isSkinned_ && skinnedObject_) {
        skinnedObject_->SetPosition(position_);
        skinnedObject_->SetRotation(rotation_);
        skinnedObject_->SetScale({presentationScale_, presentationScale_, presentationScale_});
        // 演出中もGPUスキニング頂点を生成する。nullptrでは着地まで未生成になる。
        skinnedObject_->Update(dxCommon, lightVP);
    } else if (object_) {
        // 物理更新を止めた到着演出でも、外部指定の座標を描画へ反映する。
        object_->SetPosition(position_);
        object_->SetRotation(rotation_);
        object_->SetScale({presentationScale_, presentationScale_, presentationScale_});
        object_->Update(lightVP);
    }
}

// ドアに触れているか判定して、触れていてかつFキーがトリガーされたらワープする
void Player::DoorWarp(StageMap &map) {
    // 1. プレイヤーの足元のグリッド座標（整数）を計算
    int gx = static_cast<int>(std::floor(position_.x + 0.5f));
    int gyBottom = static_cast<int>(std::floor(position_.y + 0.1f));
    int gz = static_cast<int>(std::floor(position_.z + 0.5f));

    // 現在足元にあるセルを取得
    int doorY = static_cast<int>(std::floor(position_.y + 0.5f));
    const MapCell *cell = map.GetCell(gx, doorY, gz);
    if (!cell || cell->type != BlockType::Door) {
        doorY = gyBottom;
        cell = map.GetCell(gx, doorY, gz);
    }

    // 毎フレーム一度falseに戻す
    isNearDoor_ = false;

    // 足元がドアブロックだった場合
    if (cell && cell->type == BlockType::Door) {
        isNearDoor_ = true;

        // ドアの上に「Fキー」などのUIを出すワールド座標を設定
        nearDoorWorldPos_ = {static_cast<float>(gx), static_cast<float>(doorY) + 1.0f, static_cast<float>(gz)};

        // 【ワープ実行処理】
        // ドアの中にいて、Fキーが押され、かつ「ワープ直後フラグ」が立っていない場合のみ実行
        if (IsInteractTriggered() && !hasJustWarped_) {
            // 2. マップ全体から、自分（gx, gyBottom, gz）と同じドア番号(variant)を持つ相方の座標を検索
            // ※前回 StageMap に追加した関数を呼び出します
            Int3 destination = map.FindPairedDoor(gx, doorY, gz);

            // 3. 相方のドアが見つかった場合（検索結果が現在の座標と異なる場合）
            if (destination.x != gx || destination.y != doorY || destination.z != gz) {
                // ワープ先の座標を設定
                // ※ destination.y（ブロックの底面）にプレイヤーの足元がぴったり乗るよう、
                //    キノピオ隊長の着地位置として少しだけ高さを浮かせます（+0.1fなど環境に合わせて調整）
                const Vector3 exitPosition = {static_cast<float>(destination.x),
                                              static_cast<float>(destination.y) + 0.1f,
                                              static_cast<float>(destination.z)};
                // Inspect the exit without opening gates or consuming the held key.
                if (CheckCollision(exitPosition, map, false)) {
                    return;
                }
                position_ = exitPosition;
                coyoteFramesRemaining_ = 0;
                isGrounded_ = false;

                // ワープした衝撃で物理移動がバグらないよう、落下速度や移動慣性を完全にゼロにリセット
                velocity_ = {0.0f, 0.0f, 0.0f};

                // ワープ直後フラグを立てる（これでこのフレームや次フレームでの連続誤作動を防ぐ）
                hasJustWarped_ = true;
            }
        }
    } else {
        // 4. ドアから完全に離れたら、再ワープ防止フラグをリセットする
        // これにより、別のドア（または一度離れて入り直した時）で再びワープができるようになります
        hasJustWarped_ = false;
    }
}

// 接触回収の鍵と操作で拾う扉用の鍵を判定し、回収数と所持状態を更新する。
void Player::KeyUpdate(StageMap &map) {
    // プレイヤーの中心付近（足元〜腰）のマスを取得
    int gx = static_cast<int>(std::floor(position_.x + 0.5f));
    int gy = static_cast<int>(std::floor(position_.y + 0.5f));
    int gz = static_cast<int>(std::floor(position_.z + 0.5f));

    const MapCell *cell = map.GetCell(gx, gy, gz);

    if (cell && cell->type == BlockType::Key && cell->variant == 1) {
        ++collectedLevelKeys_;
        map.RemoveBlock(gx, gy, gz);
        map.RequestRebuild();
        return;
    }
    if (cell && cell->type == BlockType::Key && !hasKey_) {
        isNearKey_ = true;
        nearKeyWorldPos_ = {static_cast<float>(gx), static_cast<float>(gy) + 1.0f, static_cast<float>(gz)};

        if (IsInteractTriggered()) {
            hasKey_ = true;
            ++collectedLevelKeys_;
            map.RemoveBlock(gx, gy, gz);
            map.RequestRebuild();
        }
    }
}

//鍵ブロックUI判定関数

void Player::KeyBlockUIUpdate(StageMap &map) {
    if (!hasKey_) {
        return;
    }

    int px = static_cast<int>(std::floor(position_.x + 0.5f));
    int py = static_cast<int>(std::floor(position_.y + 0.5f));
    int pz = static_cast<int>(std::floor(position_.z + 0.5f));

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                const MapCell *cell = map.GetCell(px + dx, py + dy, pz + dz);

                if (cell && cell->type == BlockType::KeyBlock) {
                    isNearKeyBlock_ = true;
                    nearKeyBlockWorldPos_ = {static_cast<float>(px + dx), static_cast<float>(py + dy) + 1.0f,
                                             static_cast<float>(pz + dz)};
                    return;
                }
            }
        }
    }
}

bool Player::IsJumpTriggered() const {
    return input_ && (input_->TriggerKey(DIK_SPACE) || input_->TriggerControllerButton(XINPUT_GAMEPAD_A));
}

bool Player::IsInteractTriggered() const {
    return input_ && (input_->TriggerKey(DIK_F) || input_->TriggerControllerButton(XINPUT_GAMEPAD_X));
}

// キーやスティックの強さから、走行入力が有効かを判定する。
bool Player::IsRunInputActive(const Vector3 &inputDir) const {
    if (!input_) {
        return false;
    }

    if (input_->PushKey(DIK_LSHIFT) || input_->PushKey(DIK_RSHIFT)) {
        return true;
    }

    const float stickPower = std::sqrt(inputDir.x * inputDir.x + inputDir.z * inputDir.z);
    const GamePadState &gamePad = input_->GetGamePadState();
    return input_->IsGamePadConnected() && (stickPower > 0.85f || gamePad.rightTrigger > 0.35f);
}

// プレイヤーの状態に合うモーションを選び、切り替えと再生時刻を反映する。
void Player::ApplySkinnedAnimation(const IPlayerState &state) {
    if (!skinnedObject_) {
        return;
    }

    auto *model = skinnedObject_->GetModel();
    if (!model) {
        return;
    }

    const auto &motions = model->GetMotions();
    if (motions.empty()) {
        return;
    }

    const bool stateChanged = animationState_ != &state;

    // 自動生成される Idle はバインドポーズなので、停止時に選ぶとTポーズへ戻ってしまう。
    // 待機専用モーションがないモデルでは、直前のGPUスキニング結果を速度0で保持する。
    if (state.HoldPose()) {
        const int activeMotionIndex = model->GetActiveMotionIndex();
        if (activeMotionIndex >= 0 && activeMotionIndex < static_cast<int>(motions.size())) {
            const std::string &activeMotionName = motions[static_cast<size_t>(activeMotionIndex)].name;
            const bool activeMotionIsGeneratedIdle = ToLower(activeMotionName) == "idle";

            // 通常の移動停止では直前の姿勢を保持する。
            // Jump/Ladderから戻った直後は空中・登攀姿勢を固定せず、
            // 下の処理で歩行モーション由来の待機姿勢へ明示的に戻す。
            const bool canHoldCurrentPose = !animationState_ || animationState_->CanHoldAfter();
            if (!activeMotionIsGeneratedIdle && canHoldCurrentPose) {
                animationState_ = &state;
                skinnedObject_->SetAnimationSpeed(0.0f);
                skinnedObject_->SetPlayCustomAnimation(true);
                return;
            }
        }

        // 起動直後など、既に生成Idleが選ばれている場合は歩行モーションの
        // 自然なフレームへ切り替えて停止姿勢として保持する。
        int holdMotionIndex = FindMotionIndexByName(motions, {"walk", "run", "sprint"});
        if (holdMotionIndex < 0) {
            holdMotionIndex = 0;
        }
        model->SetActiveMotionIndex(holdMotionIndex);
        if (stateChanged || skinnedObject_->GetCurrentKeyframeTime() <= 0.0f) {
            const float holdTime = (std::max)(0.0f, motions[static_cast<size_t>(holdMotionIndex)].duration * 0.25f);
            skinnedObject_->SetCurrentKeyframeTime(holdTime);
        }
        animationState_ = &state;
        skinnedObject_->SetAnimationSpeed(0.0f);
        skinnedObject_->SetPlayCustomAnimation(true);
        return;
    }

    int targetMotionIndex = FindMotionIndexByName(motions, state.MotionHints());
    if (targetMotionIndex < 0) { targetMotionIndex = FindMotionIndexByName(motions, state.FallbackHints()); }
    if (targetMotionIndex < 0) {
        targetMotionIndex = state.FallbackMotion(static_cast<int>(motions.size()));
    }

    model->SetActiveMotionIndex(targetMotionIndex);

    animationState_ = &state;

    if (stateChanged) {
        skinnedObject_->SetCurrentKeyframeTime(0.0f);
    }

    // モーションが1本しかないモデルでも再生を止めない。
    // 停止時にカスタムアニメーションを無効化すると、GPUスキニングへ
    // バインドポーズ（Tポーズ）が渡るため、待機中も有効な姿勢を維持する。
    skinnedObject_->SetAnimationSpeed(state.AnimationSpeed());
    skinnedObject_->SetPlayCustomAnimation(true);
}

// 衝突判定ロジック
bool Player::CheckCollision(const Vector3 &pos, StageMap &map, bool allowKeyUse) {
    // DebugView は表示中の地形と StageMap が別物なので、グリッドの壁判定を使わない。
    // 足元だけは簡易床として残し、自由移動中に落下し続けないようにする。
    if (!stageCollisionEnabled_) {
        if (pos.y < 0.0f) {
            return true;
        }
        return CheckExternalCollision(pos);
    }

    // 描画と同じく整数座標をセル中心として扱い、プレイヤーAABBが重なる
    // 全セルを調べる。旧処理だけY座標に+0.5がなく、床から約半マス浮いていた。
    constexpr float kBoundaryEpsilon = 0.0001f;
    const int minX = static_cast<int>(std::floor(pos.x - radius_.x + 0.5f));
    const int maxX = static_cast<int>(std::floor(pos.x + radius_.x + 0.5f - kBoundaryEpsilon));
    const int minY = static_cast<int>(std::floor(pos.y + 0.5f));
    const int maxY = static_cast<int>(std::floor(pos.y + radius_.y * 2.0f + 0.5f - kBoundaryEpsilon));
    const int minZ = static_cast<int>(std::floor(pos.z - radius_.z + 0.5f));
    const int maxZ = static_cast<int>(std::floor(pos.z + radius_.z + 0.5f - kBoundaryEpsilon));

    for (int gy = minY; gy <= maxY; ++gy) {
        for (int gz = minZ; gz <= maxZ; ++gz) {
            for (int gx = minX; gx <= maxX; ++gx) {

                // 指定した座標のセル情報を取得
                const MapCell *cell = map.GetCell(gx, gy, gz);

                // ▼ 追加：鍵ブロックの判定と破壊 ▼
                if (cell && cell->type == BlockType::KeyBlock) {
                    if (hasKey_ && allowKeyUse) {
                        // 鍵を持っている場合は開ける（消費する）
                        hasKey_ = false;
                        // ★ 変更：1マスだけではなく、繋がっている塊をすべて消す
                        map.RemoveConnectedKeyBlocks(gx, gy, gz);
                        map.RequestRebuild(); // ステージの見た目を再構築

                        // ブロックが消えたので、ここには壁が無いこととして判定を続ける
                        continue;
                    } else {
                        // 鍵を持っていない場合は普通の壁として扱う
                        return true;
                    }
                }
                // ▲ ここまで ▲

                // ★ 変更：動く足場は固定グリッド判定から除外する
                if (cell && cell->isSolid && cell->type != BlockType::MovingFloor) {
                    return true;
                }

                // Switches maintain each cell's solidity. An opened gate must not
                // return early and skip another wall intersecting the same player AABB.
            }
        }
    }

    // ★ 追加：動いている足場のワールド座標判定（floatにバラして渡す）
    if (map.GetIntersectingMovingFloor(pos.x, pos.y, pos.z, radius_.x, radius_.y, radius_.z)) {
        return true;
    }

    if (CheckExternalCollision(pos)) {
        return true;
    }

    return false;
}

// Blender側から取り込んだ当たり判定と、プレイヤーの重なりを検査する。
bool Player::CheckExternalCollision(const Vector3 &pos) const {
    // 外部レベルを使わない従来ステージでは、追加判定を行わず即座に終了する。
    if (!externalCollisionBoxes_) {
        return false;
    }

    // position_は足元基準なので、Y方向だけ上へプレイヤー身長を伸ばす。
    const Vector3 playerMin = {pos.x - radius_.x, pos.y, pos.z - radius_.z};
    const Vector3 playerMax = {pos.x + radius_.x, pos.y + radius_.y * 2.0f, pos.z + radius_.z};

    // 辺同士が触れているだけの状態は「重なり」に含めない。
    // これにより床の上へ静止しているプレイヤーが常時衝突状態になるのを防ぐ。
    for (const WorldCollisionBox &box : *externalCollisionBoxes_) {
        if (playerMin.x < box.maximum.x && playerMax.x > box.minimum.x && playerMin.y < box.maximum.y &&
            playerMax.y > box.minimum.y && playerMin.z < box.maximum.z && playerMax.z > box.minimum.z) {
            return true;
        }
    }
    return false;
}

// リスポーン処理：座標をリスポーンポイントに戻し、速度と回転をリセット
void Player::Respawn(bool preserveKey) {
    actionFlow_.Reset();
    animationState_ = nullptr;
    failurePending_ = false;
    presentationScale_ = 1;
    position_ = respawnPosition_;
    velocity_ = {0.0f, 0.0f, 0.0f};
    rotation_ = {0.0f, 0.0f, 0.0f};

    // A retry keeps the already-collected world state; losing its removed key would soft-lock the gate.
    if (!preserveKey) {
        hasKey_ = false;
        collectedLevelKeys_ = 0;
    }
    isGrounded_ = false;
    isOnLadder_ = false;
    ladderBottomExitUntilRelease_ = false;
    hasJustWarped_ = false;
    coyoteFramesRemaining_ = 0;
}

// Pスイッチの更新：足元のセルをチェックして、Pスイッチがあればマップに状態変更を通知
void Player::PSwitchUpdate(StageMap &map) {
    // プレイヤーの中心座標から足元のインデックスを計算
    int gx = static_cast<int>(std::floor(position_.x + 0.5f));
    // 0.1fだと浮いている判定になりやすいので、少し余裕を持たせるか
    // 現在の座標(position_.y)の真下を正確に狙います
    int gyBottom = static_cast<int>(std::floor(position_.y + 0.1f));
    int gz = static_cast<int>(std::floor(position_.z + 0.5f));

    const MapCell *cellBelow = map.GetCell(gx, static_cast<int>(std::floor(position_.y + 0.5f)), gz);
    if (!cellBelow || cellBelow->type != BlockType::PSwitch) {
        cellBelow = map.GetCell(gx, gyBottom, gz);
    }

    if (cellBelow && cellBelow->type == BlockType::PSwitch) {
        isNearPSwitch_ = true;
        nearPSwitchWorldPos_ = {static_cast<float>(gx), static_cast<float>(gyBottom) + 1.0f, static_cast<float>(gz)};

        if (IsInteractTriggered()) {
            map.SetPSwitchActive(cellBelow->variant);
        }
    }
}

// 操作範囲のON/OFFスイッチを確認し、入力時にステージ状態を反転する。
void Player::OnOffSwitchUpdate(StageMap &map) {
    if (IsInteractTriggered()) {
        // プレイヤーの現在位置（マス目）
        int px = static_cast<int>(std::floor(position_.x + 0.5f));
        int py = static_cast<int>(std::floor(position_.y + 0.5f));
        int pz = static_cast<int>(std::floor(position_.z + 0.5f));

        bool switchFound = false;

        // プレイヤーの周囲（上下左右前後 3x3x3マス）をチェック
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dz = -1; dz <= 1; ++dz) {
                    const MapCell *cell = map.GetCell(px + dx, py + dy, pz + dz);
                    // 近くにスイッチがあればフラグを立てる
                    if (cell && cell->type == BlockType::OnOffSwitch) {
                        switchFound = true;
                    }
                }
            }
        }

        // スイッチが見つかっていればON/OFFを切り替える
        if (switchFound) {
            map.ToggleOnState();
            // （効果音を鳴らす処理をここに入れると気持ちいいです！）
        }
    }
}

// 描画：内部で持っている Object3d を描画
void Player::Draw() {
    if (isSkinned_ && skinnedObject_) {
        skinnedObject_->Draw();
    } else if (object_) {
        object_->Draw();
    }
}

// 影の描画：ライトカメラの行列を渡して影を描く
void Player::DrawShadow(const Matrix4x4 &lightViewProjection) {
    if (isSkinned_ && skinnedObject_) {
        skinnedObject_->DrawShadow(lightViewProjection);
    } else if (object_) {
        object_->DrawShadow(lightViewProjection);
    }
}

// プレイヤーの位置を壁越しにも示すため、専用設定で形状を描く。
void Player::DrawHighlight() {
    // Reuse the current GPU-skinned vertices and unchanged transform/material buffers.
    if (isSkinned_ && skinnedObject_) {
        skinnedObject_->Draw(true);
    } else if (object_) {
        object_->Draw();
    }
}

// 足元の崩れる床を検出し、崩壊の進行を開始する。
void Player::CrumbleUpdate(StageMap &map) {
    int gx = static_cast<int>(std::floor(position_.x));
    int gyBottom = static_cast<int>(std::floor(position_.y - 0.05f));
    int gz = static_cast<int>(std::floor(position_.z));

    const MapCell *cellBelow = map.GetCell(gx, gyBottom, gz);

    if (cellBelow && cellBelow->type == BlockType::CrumblingFloor && !cellBelow->isHidden) {
        map.StartCrumbling({gx, gyBottom, gz});
    }
}

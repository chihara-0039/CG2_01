#define NOMINMAX
#include "SkinnedModel.h"
#include "MyMath.h"
#include <cassert>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

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

    // 方向ベクトルと行列の乗算 (平行移動なし、回転のみ)
    Vector3 TransformNormal(const Vector3& n, const Matrix4x4& m) {
        Vector3 res = {
            n.x * m.m[0][0] + n.y * m.m[1][0] + n.z * m.m[2][0],
            n.x * m.m[0][1] + n.y * m.m[1][1] + n.z * m.m[2][1],
            n.x * m.m[0][2] + n.y * m.m[1][2] + n.z * m.m[2][2]
        };
        return Math::Normalize(res);
    }
}

void SkinnedModel::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager) {
    // 1. ジョイントの構造を構築
    CreateHumanoidSkeleton();

    // 2. メッシュ (頂点データ) を生成
    GenerateHumanoidMesh();
    
    // 3. 関節付近のウェイトをブレンドする
    SmoothWeights();

    // 4. animatedVertices_ を bindPoseVertices_ で初期化
    animatedVertices_ = bindPoseVertices_;

    // 5. テクスチャをロード
    uint32_t texHandle = 0;
    if (textureManager) {
        texHandle = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    }

    // 6. 描画用の Model クラスを生成
    model_ = std::make_unique<Model>();
    model_->InitializeFromVertices(dxCommon, animatedVertices_, texHandle);
}

void SkinnedModel::ResetPose() {
    for (auto& joint : joints_) {
        joint.rotation = { 0.0f, 0.0f, 0.0f };
        joint.scale = { 1.0f, 1.0f, 1.0f };
        // translation は CreateHumanoidSkeleton で初期化されたままとする
    }
}

void SkinnedModel::CreateHumanoidSkeleton() {
    joints_.clear();

    // ボーン定義用の一時構造体 (世界座標での定義)
    struct JointDef {
        std::string name;
        Vector3 globalPos;
        int parentIndex;
    };

    // 人型スケルトン定義 (合計15ジョイント)
    std::vector<JointDef> defs = {
        { "Pelvis",        { 0.0f, 0.8f, 0.0f },    -1 }, // 0
        { "Spine",         { 0.0f, 1.1f, 0.0f },     0 }, // 1
        { "Head",          { 0.0f, 1.4f, 0.0f },     1 }, // 2
        
        { "LeftShoulder",  { -0.25f, 1.3f, 0.0f },   1 }, // 3
        { "LeftElbow",     { -0.5f, 1.3f, 0.0f },    3 }, // 4
        { "LeftHand",      { -0.7f, 1.3f, 0.0f },    4 }, // 5
        
        { "RightShoulder", { 0.25f, 1.3f, 0.0f },    1 }, // 6
        { "RightElbow",    { 0.5f, 1.3f, 0.0f },     6 }, // 7
        { "RightHand",     { 0.7f, 1.3f, 0.0f },     7 }, // 8
        
        { "LeftHip",       { -0.15f, 0.7f, 0.0f },   0 }, // 9
        { "LeftKnee",      { -0.15f, 0.35f, 0.0f },  9 }, // 10
        { "LeftFoot",      { -0.15f, 0.0f, 0.0f },   10 }, // 11
        
        { "RightHip",      { 0.15f, 0.7f, 0.0f },    0 }, // 12
        { "RightKnee",     { 0.15f, 0.35f, 0.0f },  12 }, // 13
        { "RightFoot",     { 0.15f, 0.0f, 0.0f },   13 }  // 14
    };

    joints_.resize(defs.size());

    // 各ジョイントの初期ローカル・グローバル行列を求める
    for (size_t i = 0; i < defs.size(); ++i) {
        joints_[i].name = defs[i].name;
        joints_[i].scale = { 1.0f, 1.0f, 1.0f };
        joints_[i].rotation = { 0.0f, 0.0f, 0.0f };
        joints_[i].parentIndex = defs[i].parentIndex;

        // 親からの相対座標 (ローカル translation) の計算
        if (defs[i].parentIndex == -1) {
            joints_[i].translation = defs[i].globalPos;
        } else {
            int parentIdx = defs[i].parentIndex;
            joints_[i].translation = {
                defs[i].globalPos.x - defs[parentIdx].globalPos.x,
                defs[i].globalPos.y - defs[parentIdx].globalPos.y,
                defs[i].globalPos.z - defs[parentIdx].globalPos.z
            };
        }
    }

    // バインドポーズの初期グローバル行列およびその逆行列を前計算する
    for (size_t i = 0; i < joints_.size(); ++i) {
        Matrix4x4 localTrans = Math::MakeTranslateMatrix(joints_[i].translation);
        // 初期状態は回転・スケールなしなので localMatrix は単に平行移動行列
        joints_[i].localMatrix = localTrans;

        if (joints_[i].parentIndex == -1) {
            joints_[i].globalMatrix = joints_[i].localMatrix;
        } else {
            int parentIdx = joints_[i].parentIndex;
            joints_[i].globalMatrix = Math::Multiply(joints_[i].localMatrix, joints_[parentIdx].globalMatrix);
        }

        // 逆行列 (Inverse Bind Pose Matrix)
        joints_[i].offsetMatrix = Math::Inverse(joints_[i].globalMatrix);
    }
}

void SkinnedModel::GenerateHumanoidMesh() {
    bindPoseVertices_.clear();
    influences_.clear();

    // 各部位の Cube メッシュを生成して結合する
    // Pelvis (腰)
    AddCubeMesh({ 0.0f, 0.8f, 0.0f }, { 0.3f, 0.2f, 0.2f }, 0);
    // Spine (胸)
    AddCubeMesh({ 0.0f, 1.1f, 0.0f }, { 0.35f, 0.4f, 0.22f }, 1);
    // Head (頭)
    AddCubeMesh({ 0.0f, 1.45f, 0.0f }, { 0.2f, 0.2f, 0.2f }, 2);

    // 左腕
    AddCubeMesh({ -0.375f, 1.3f, 0.0f }, { 0.25f, 0.1f, 0.1f }, 3); // 左肩〜左肘の間
    AddCubeMesh({ -0.6f, 1.3f, 0.0f }, { 0.2f, 0.08f, 0.08f }, 4);  // 左肘〜左手の間
    AddCubeMesh({ -0.725f, 1.3f, 0.0f }, { 0.07f, 0.07f, 0.07f }, 5); // 左手先

    // 右腕
    AddCubeMesh({ 0.375f, 1.3f, 0.0f }, { 0.25f, 0.1f, 0.1f }, 6);
    AddCubeMesh({ 0.6f, 1.3f, 0.0f }, { 0.2f, 0.08f, 0.08f }, 7);
    AddCubeMesh({ 0.725f, 1.3f, 0.0f }, { 0.07f, 0.07f, 0.07f }, 8);

    // 左足
    AddCubeMesh({ -0.15f, 0.525f, 0.0f }, { 0.12f, 0.35f, 0.12f }, 9); // 左大腿
    AddCubeMesh({ -0.15f, 0.175f, 0.0f }, { 0.1f, 0.35f, 0.1f }, 10);  // 左下腿
    AddCubeMesh({ -0.15f, -0.05f, 0.05f }, { 0.1f, 0.1f, 0.15f }, 11); // 左足先

    // 右足
    AddCubeMesh({ 0.15f, 0.525f, 0.0f }, { 0.12f, 0.35f, 0.12f }, 12); // 右大腿
    AddCubeMesh({ 0.15f, 0.175f, 0.0f }, { 0.1f, 0.35f, 0.1f }, 13);  // 右下腿
    AddCubeMesh({ 0.15f, -0.05f, 0.05f }, { 0.1f, 0.1f, 0.15f }, 14); // 右足先
}

void SkinnedModel::AddCubeMesh(const Vector3& center, const Vector3& size, int jointIndex) {
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    // 8頂点
    Vector3 localVertices[8] = {
        { center.x - hx, center.y - hy, center.z - hz }, // 0: 左下前
        { center.x + hx, center.y - hy, center.z - hz }, // 1: 右下前
        { center.x - hx, center.y + hy, center.z - hz }, // 2: 左上前
        { center.x + hx, center.y + hy, center.z - hz }, // 3: 右上前
        { center.x - hx, center.y - hy, center.z + hz }, // 4: 左下奥
        { center.x + hx, center.y - hy, center.z + hz }, // 5: 右下奥
        { center.x - hx, center.y + hy, center.z + hz }, // 6: 左上奥
        { center.x + hx, center.y + hy, center.z + hz }  // 7: 右上奥
    };

    // 6面定義 (反時計回りカリングに適合するインデックス)
    // 既存エンジンが左手系と想定して、前面の巻順を調整
    struct Face {
        int idx[4];
        Vector3 normal;
    };
    Face faces[6] = {
        { { 0, 2, 3, 1 }, { 0.0f, 0.0f, -1.0f } }, // 前面
        { { 1, 3, 7, 5 }, { 1.0f, 0.0f, 0.0f } },  // 右面
        { { 5, 7, 6, 4 }, { 0.0f, 0.0f, 1.0f } },  // 後面
        { { 4, 6, 2, 0 }, { -1.0f, 0.0f, 0.0f } }, // 左面
        { { 2, 6, 7, 3 }, { 0.0f, 1.0f, 0.0f } },  // 上面
        { { 4, 0, 1, 5 }, { 0.0f, -1.0f, 0.0f } }  // 下面
    };

    for (int f = 0; f < 6; ++f) {
        // 各面を2つの三角形に分割 (合計6頂点)
        int indices[6] = {
            faces[f].idx[0], faces[f].idx[1], faces[f].idx[2],
            faces[f].idx[0], faces[f].idx[2], faces[f].idx[3]
        };

        Vector2 uvs[6] = {
            { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f },
            { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f }
        };

        for (int i = 0; i < 6; ++i) {
            ModelVertexData v;
            v.position = { localVertices[indices[i]].x, localVertices[indices[i]].y, localVertices[indices[i]].z, 1.0f };
            v.normal = faces[f].normal;
            v.texcoord = uvs[i];

            bindPoseVertices_.push_back(v);

            VertexInfluence inf;
            inf.jointIndices[0] = jointIndex;
            inf.weights[0] = 1.0f;
            influences_.push_back(inf);
        }
    }
}

void SkinnedModel::SmoothWeights() {
    // 頂点数が influences_ と一致することを確認
    assert(bindPoseVertices_.size() == influences_.size());

    // 各頂点について、隣接する関節との距離に基づきスキンウェイトを線形補間する
    for (size_t i = 0; i < bindPoseVertices_.size(); ++i) {
        Vector3 pos = { bindPoseVertices_[i].position.x, bindPoseVertices_[i].position.y, bindPoseVertices_[i].position.z };
        VertexInfluence& inf = influences_[i];

        int primaryJoint = inf.jointIndices[0];

        // 1. Spine (1) と Pelvis (0) の境界付近の補間
        if (primaryJoint == 1 && pos.y < 1.0f) {
            // Y: 0.8 〜 1.0 の範囲で徐々に腰の影響を入れる
            float t = (pos.y - 0.8f) / 0.2f;
            t = std::max(0.0f, std::min(1.0f, t));
            inf.jointIndices[0] = 1; // Spine
            inf.weights[0] = t;
            inf.jointIndices[1] = 0; // Pelvis
            inf.weights[1] = 1.0f - t;
        }
        else if (primaryJoint == 0 && pos.y > 0.85f) {
            // Y: 0.8 〜 0.9 の範囲で徐々に胸の影響を入れる
            float t = (pos.y - 0.8f) / 0.1f;
            t = std::max(0.0f, std::min(1.0f, t));
            inf.jointIndices[0] = 0; // Pelvis
            inf.weights[0] = 1.0f - t;
            inf.jointIndices[1] = 1; // Spine
            inf.weights[1] = t;
        }

        // 2. 腕関節 (左肘) X: -0.4 〜 -0.6
        if (primaryJoint == 3 && pos.x < -0.45f) {
            // 肩(3)から肘(4)へ
            float t = (pos.x - (-0.45f)) / (-0.1f);
            t = std::max(0.0f, std::min(1.0f, t));
            inf.weights[0] = 1.0f - t;
            inf.jointIndices[1] = 4;
            inf.weights[1] = t;
        }
        else if (primaryJoint == 4 && pos.x > -0.55f) {
            // 肘(4)から肩(3)へ
            float t = (pos.x - (-0.55f)) / 0.1f;
            t = std::max(0.0f, std::min(1.0f, t));
            inf.weights[0] = 1.0f - t;
            inf.jointIndices[1] = 3;
            inf.weights[1] = t;
        }

        // 3. 腕関節 (右肘) X: 0.4 〜 0.6
        if (primaryJoint == 6 && pos.x > 0.45f) {
            float t = (pos.x - 0.45f) / 0.1f;
            t = std::max(0.0f, std::min(1.0f, t));
            inf.weights[0] = 1.0f - t;
            inf.jointIndices[1] = 7;
            inf.weights[1] = t;
        }
        else if (primaryJoint == 7 && pos.x < 0.55f) {
            float t = (pos.x - 0.55f) / (-0.1f);
            t = std::max(0.0f, std::min(1.0f, t));
            inf.weights[0] = 1.0f - t;
            inf.jointIndices[1] = 6;
            inf.weights[1] = t;
        }

        // 4. 左膝関節 (Y: 0.3 〜 0.4)
        if (primaryJoint == 9 && pos.y < 0.42f) {
            float t = (pos.y - 0.32f) / 0.1f;
            t = std::max(0.0f, std::min(1.0f, t));
            inf.weights[0] = t;
            inf.jointIndices[1] = 10;
            inf.weights[1] = 1.0f - t;
        }
        else if (primaryJoint == 10 && pos.y > 0.32f) {
            float t = (pos.y - 0.32f) / 0.1f;
            t = std::max(0.0f, std::min(1.0f, t));
            inf.weights[0] = 1.0f - t;
            inf.jointIndices[1] = 9;
            inf.weights[1] = t;
        }

        // 5. 右膝関節 (Y: 0.3 〜 0.4)
        if (primaryJoint == 12 && pos.y < 0.42f) {
            float t = (pos.y - 0.32f) / 0.1f;
            t = std::max(0.0f, std::min(1.0f, t));
            inf.weights[0] = t;
            inf.jointIndices[1] = 13;
            inf.weights[1] = 1.0f - t;
        }
        else if (primaryJoint == 13 && pos.y > 0.32f) {
            float t = (pos.y - 0.32f) / 0.1f;
            t = std::max(0.0f, std::min(1.0f, t));
            inf.weights[0] = 1.0f - t;
            inf.jointIndices[1] = 12;
            inf.weights[1] = t;
        }
    }
}

void SkinnedModel::Update(DirectXCommon* dxCommon) {
    // 1. スケルトン行列の更新 (親から順にトラバース)
    for (size_t i = 0; i < joints_.size(); ++i) {
        // local = Affine
        joints_[i].localMatrix = Math::MakeAffineMatrix(joints_[i].scale, joints_[i].rotation, joints_[i].translation);

        // global = parent.global * local
        if (joints_[i].parentIndex == -1) {
            joints_[i].globalMatrix = joints_[i].localMatrix;
        } else {
            int parentIdx = joints_[i].parentIndex;
            joints_[i].globalMatrix = Math::Multiply(joints_[i].localMatrix, joints_[parentIdx].globalMatrix);
        }
    }

    // 2. CPUスキニング計算
    for (size_t i = 0; i < bindPoseVertices_.size(); ++i) {
        const ModelVertexData& bindV = bindPoseVertices_[i];
        const VertexInfluence& inf = influences_[i];

        Vector3 originalPos = { bindV.position.x, bindV.position.y, bindV.position.z };
        Vector3 originalNormal = bindV.normal;

        Vector3 blendedPos = { 0.0f, 0.0f, 0.0f };
        Vector3 blendedNormal = { 0.0f, 0.0f, 0.0f };

        float weightSum = 0.0f;

        for (int j = 0; j < 4; ++j) {
            int jointIdx = inf.jointIndices[j];
            float weight = inf.weights[j];

            if (jointIdx != -1 && weight > 0.0f) {
                // スキニング行列 M = G * Offset (InvBindPose)
                Matrix4x4 skinningMatrix = Math::Multiply(joints_[jointIdx].offsetMatrix, joints_[jointIdx].globalMatrix);

                // 頂点の変形
                Vector3 deformedPos = TransformCoord(originalPos, skinningMatrix);
                blendedPos.x += deformedPos.x * weight;
                blendedPos.y += deformedPos.y * weight;
                blendedPos.z += deformedPos.z * weight;

                // 法線の変形 (平行移動なしの回転変形)
                Vector3 deformedNormal = TransformNormal(originalNormal, skinningMatrix);
                blendedNormal.x += deformedNormal.x * weight;
                blendedNormal.y += deformedNormal.y * weight;
                blendedNormal.z += deformedNormal.z * weight;

                weightSum += weight;
            }
        }

        if (weightSum > 0.0f) {
            // スキンウェイトの正規化
            blendedPos.x /= weightSum;
            blendedPos.y /= weightSum;
            blendedPos.z /= weightSum;
            blendedNormal = Math::Normalize(blendedNormal);
        } else {
            blendedPos = originalPos;
            blendedNormal = originalNormal;
        }

        animatedVertices_[i].position = { blendedPos.x, blendedPos.y, blendedPos.z, 1.0f };
        animatedVertices_[i].normal = blendedNormal;
    }

    // 3. GPUへ頂点バッファを再転送
    model_->UpdateVertexBuffer(animatedVertices_);
}

void SkinnedModel::Draw(ID3D12GraphicsCommandList* commandList) {
    if (model_) {
        model_->Draw(commandList);
    }
}

void SkinnedModel::ApplyTestAnimation(float time, float speed) {
    // 簡易的なアニメーション計算
    float t = time * speed;

    // 1. 骨盤 (腰) の上下運動 (呼吸・重心移動)
    joints_[0].rotation.z = std::sin(t) * 0.02f;
    joints_[0].translation.y = 0.8f + std::sin(t * 2.0f) * 0.02f;

    // 2. 脊椎 (胸) の左右ねじれ
    joints_[1].rotation.y = std::sin(t) * 0.05f;

    // 3. 歩行アニメーション (足の前後スイング、腕の逆連動)
    // 左太もも (9) & 右太もも (12)
    joints_[9].rotation.x = std::sin(t) * 0.4f;
    joints_[12].rotation.x = -std::sin(t) * 0.4f;

    // 左膝 (10) & 右膝 (13)
    joints_[10].rotation.x = (std::sin(t + 1.5f) + 1.0f) * 0.3f; // 膝は後ろにしか曲がらない
    joints_[13].rotation.x = (-std::sin(t + 1.5f) + 1.0f) * 0.3f;

    // 左肩 (3) & 右肩 (6) - 腕を前後に振る
    joints_[3].rotation.x = -std::sin(t) * 0.3f;
    joints_[6].rotation.x = std::sin(t) * 0.3f;

    // 左肘 (4) & 右肘 (7) - 肘を軽く曲げる
    joints_[4].rotation.z = -0.1f - std::abs(std::sin(t)) * 0.2f;
    joints_[7].rotation.z = 0.1f + std::abs(std::sin(t)) * 0.2f;
}

void SkinnedModel::AddKeyframe(float time) {
    // 1. 各ジョイント用のアニメーションデータの確保
    if (motionData_.jointAnimations.empty()) {
        motionData_.jointAnimations.resize(joints_.size());
        for (size_t i = 0; i < joints_.size(); ++i) {
            motionData_.jointAnimations[i].name = joints_[i].name;
        }
    }

    // 2. 各ジョイントの現在の状態をキーフレームとして記録
    for (size_t i = 0; i < joints_.size(); ++i) {
        auto& jointAnim = motionData_.jointAnimations[i];
        
        // 既に同じ時間のキーフレームがあるか確認し、あれば上書きする
        bool found = false;
        for (auto& kf : jointAnim.keyframes) {
            if (std::abs(kf.time - time) < 1e-4f) {
                kf.translation = joints_[i].translation;
                kf.rotation = joints_[i].rotation;
                kf.scale = joints_[i].scale;
                found = true;
                break;
            }
        }

        if (!found) {
            JointKeyframe kf;
            kf.time = time;
            kf.translation = joints_[i].translation;
            kf.rotation = joints_[i].rotation;
            kf.scale = joints_[i].scale;
            jointAnim.keyframes.push_back(kf);

            // 時間順にソート
            std::sort(jointAnim.keyframes.begin(), jointAnim.keyframes.end(), [](const JointKeyframe& a, const JointKeyframe& b) {
                return a.time < b.time;
            });
        }
    }
}

void SkinnedModel::ClearKeyframes() {
    for (auto& jointAnim : motionData_.jointAnimations) {
        jointAnim.keyframes.clear();
    }
    motionData_.jointAnimations.clear();
}

bool SkinnedModel::SaveMotion(const std::string& filePath) {
    // ディレクトリ作成
    std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;

    ofs << "Duration " << motionData_.duration << "\n";
    ofs << "NumJoints " << joints_.size() << "\n";

    for (const auto& jointAnim : motionData_.jointAnimations) {
        ofs << "JointName " << jointAnim.name << "\n";
        ofs << "NumKeyframes " << jointAnim.keyframes.size() << "\n";
        for (const auto& kf : jointAnim.keyframes) {
            ofs << "Keyframe " << kf.time
                << " " << kf.translation.x << "," << kf.translation.y << "," << kf.translation.z
                << " " << kf.rotation.x << "," << kf.rotation.y << "," << kf.rotation.z
                << " " << kf.scale.x << "," << kf.scale.y << "," << kf.scale.z << "\n";
        }
    }

    return true;
}

bool SkinnedModel::LoadMotion(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) return false;

    ClearKeyframes();

    std::string line;
    float duration = 2.0f;
    int numJoints = 0;

    motionData_.jointAnimations.resize(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) {
        motionData_.jointAnimations[i].name = joints_[i].name;
    }

    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string command;
        ss >> command;

        if (command == "Duration") {
            ss >> duration;
            motionData_.duration = duration;
        } else if (command == "NumJoints") {
            ss >> numJoints;
        } else if (command == "JointName") {
            std::string jointName;
            ss >> jointName;

            // 対応するジョイントのアニメーションを探す
            int targetIdx = -1;
            for (size_t i = 0; i < joints_.size(); ++i) {
                if (joints_[i].name == jointName) {
                    targetIdx = static_cast<int>(i);
                    break;
                }
            }

            // キーフレーム数を読み取る
            std::string nextLine;
            if (std::getline(ifs, nextLine)) {
                std::stringstream ssKey(nextLine);
                std::string cmdKey;
                int numKeyframes = 0;
                ssKey >> cmdKey >> numKeyframes;

                if (cmdKey == "NumKeyframes" && targetIdx != -1) {
                    auto& jointAnim = motionData_.jointAnimations[targetIdx];
                    jointAnim.keyframes.clear();
                    
                    for (int k = 0; k < numKeyframes; ++k) {
                        if (!std::getline(ifs, nextLine)) break;
                        std::stringstream ssKf(nextLine);
                        std::string cmdKf;
                        float time = 0.0f;
                        std::string transStr, rotStr, scaleStr;
                        ssKf >> cmdKf >> time >> transStr >> rotStr >> scaleStr;

                        if (cmdKf == "Keyframe") {
                            JointKeyframe kf;
                            kf.time = time;

                            auto parseVec3 = [](const std::string& str) -> Vector3 {
                                std::stringstream ssv(str);
                                std::string item;
                                Vector3 v{};
                                if (std::getline(ssv, item, ',')) v.x = std::stof(item);
                                if (std::getline(ssv, item, ',')) v.y = std::stof(item);
                                if (std::getline(ssv, item, ',')) v.z = std::stof(item);
                                return v;
                            };

                            kf.translation = parseVec3(transStr);
                            kf.rotation = parseVec3(rotStr);
                            kf.scale = parseVec3(scaleStr);

                            jointAnim.keyframes.push_back(kf);
                        }
                    }
                }
            }
        }
    }

    return true;
}

void SkinnedModel::ApplyMotion(float time) {
    if (motionData_.jointAnimations.empty()) return;

    // 時間を Duration 内にループさせる
    float loopedTime = std::fmod(time, motionData_.duration);
    if (loopedTime < 0.0f) loopedTime += motionData_.duration;

    for (size_t i = 0; i < joints_.size(); ++i) {
        const auto& jointAnim = motionData_.jointAnimations[i];
        if (jointAnim.keyframes.empty()) continue;

        auto& joint = joints_[i];

        // 1つだけの場合は補間なし
        if (jointAnim.keyframes.size() == 1) {
            joint.translation = jointAnim.keyframes[0].translation;
            joint.rotation = jointAnim.keyframes[0].rotation;
            joint.scale = jointAnim.keyframes[0].scale;
            continue;
        }

        // 時間が最初のキーフレームより前
        if (loopedTime <= jointAnim.keyframes.front().time) {
            const auto& first = jointAnim.keyframes.front();
            joint.translation = first.translation;
            joint.rotation = first.rotation;
            joint.scale = first.scale;
            continue;
        }

        // 時間が最後のキーフレームより後ろ
        if (loopedTime >= jointAnim.keyframes.back().time) {
            const auto& last = jointAnim.keyframes.back();
            joint.translation = last.translation;
            joint.rotation = last.rotation;
            joint.scale = last.scale;
            continue;
        }

        // 前後のキーフレームを探す
        for (size_t k = 0; k < jointAnim.keyframes.size() - 1; ++k) {
            const auto& kfA = jointAnim.keyframes[k];
            const auto& kfB = jointAnim.keyframes[k + 1];

            if (loopedTime >= kfA.time && loopedTime <= kfB.time) {
                float t = (loopedTime - kfA.time) / (kfB.time - kfA.time);

                // 線形補間
                joint.translation = {
                    kfA.translation.x + t * (kfB.translation.x - kfA.translation.x),
                    kfA.translation.y + t * (kfB.translation.y - kfA.translation.y),
                    kfA.translation.z + t * (kfB.translation.z - kfA.translation.z)
                };

                // 角度の補間 (オイラー角の単純な Lerp)
                joint.rotation = {
                    kfA.rotation.x + t * (kfB.rotation.x - kfA.rotation.x),
                    kfA.rotation.y + t * (kfB.rotation.y - kfA.rotation.y),
                    kfA.rotation.z + t * (kfB.rotation.z - kfA.rotation.z)
                };

                joint.scale = {
                    kfA.scale.x + t * (kfB.scale.x - kfA.scale.x),
                    kfA.scale.y + t * (kfB.scale.y - kfA.scale.y),
                    kfA.scale.z + t * (kfB.scale.z - kfA.scale.z)
                };
                break;
            }
        }
    }
}

void SkinnedModel::GenerateWalkPreset() {
    ClearKeyframes();

    // 2.0秒のアニメーションを0.1秒刻み（21キーフレーム）で生成
    motionData_.duration = 2.0f;
    float step = 0.1f;

    // 一時的にポーズを退避
    std::vector<Vector3> origTrans(joints_.size());
    std::vector<Vector3> origRot(joints_.size());
    std::vector<Vector3> origScale(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) {
        origTrans[i] = joints_[i].translation;
        origRot[i] = joints_[i].rotation;
        origScale[i] = joints_[i].scale;
    }

    for (float t = 0.0f; t <= 2.0f + 1e-4f; t += step) {
        // テスト用歩行アニメーションのロジックを適用してポーズを計算
        // 2.0秒で1サイクル（周期 2 * PI）にするため、スピードを調節する
        // スピード speed = PI (t = 2.0秒のとき、入力値 = 2.0 * PI となる)
        ApplyTestAnimation(t, 3.14159265f);

        // その瞬間のポーズをキーフレームとして記録
        AddKeyframe(t);
    }

    // 元のポーズ（編集状態）を復元
    for (size_t i = 0; i < joints_.size(); ++i) {
        joints_[i].translation = origTrans[i];
        joints_[i].rotation = origRot[i];
        joints_[i].scale = origScale[i];
    }
}

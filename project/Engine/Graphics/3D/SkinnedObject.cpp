#include "SkinnedObject.h"
#include "MyMath.h"
#include <cmath>

void SkinnedObject::Initialize(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, TextureManager* textureManager) {
    // 1. スキニングモデルの生成
    skinnedModel_ = std::make_unique<SkinnedModel>();
    skinnedModel_->Initialize(dxCommon, textureManager);

    // 2. 表示用のObject3dを初期化して、SkinnedModel内部のModelを登録
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(skinnedModel_->GetModel());
}

void SkinnedObject::InitializeFromGltf(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, const std::string& filePath, TextureManager* textureManager) {
    // 1. スキニングモデルをglTFから生成
    skinnedModel_ = std::make_unique<SkinnedModel>();
    skinnedModel_->InitializeFromGltf(dxCommon, filePath, textureManager);

    // 2. 表示用のObject3dを初期化して、SkinnedModel内部のModelを登録
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(skinnedModel_->GetModel());
}

void SkinnedObject::Update(DirectXCommon* dxCommon, const Matrix4x4& lightVP) {
    // 1. アニメーションの再生
    if (playAnimation_) {
        // 60FPS想定で時間を進める
        animationTime_ += (1.0f / 60.0f);
        skinnedModel_->ApplyTestAnimation(animationTime_, animationSpeed_);
    } else if (playCustomAnimation_) {
        // カスタムキーフレームモーションの再生
        animationTime_ += (1.0f / 60.0f) * animationSpeed_;
        float dur = skinnedModel_->GetMotionDuration();
        currentKeyframeTime_ = std::fmod(animationTime_, dur);
        if (currentKeyframeTime_ < 0.0f) currentKeyframeTime_ += dur;
        skinnedModel_->ApplyMotion(currentKeyframeTime_);
    }

    // 2. スキニング頂点計算の実行とGPUへの転送
    skinnedModel_->Update(dxCommon);

    // 3. Object3d のトランスフォーム設定と行列更新
    if (object3d_) {
        object3d_->SetPosition(position_);
        object3d_->SetRotation(rotation_);
        object3d_->SetScale(scale_);
        object3d_->SetCamera(viewMatrix_, projectionMatrix_);
        object3d_->Update(lightVP);
    }
}

void SkinnedObject::Draw() {
    if (object3d_) {
        object3d_->Draw();
    }
}

void SkinnedObject::DrawShadow(const Matrix4x4& lightViewProjection) {
    if (object3d_) {
        object3d_->DrawShadow(lightViewProjection);
    }
}

void SkinnedObject::DrawSkeleton(Object3dCommon* object3dCommon, Model* cubeModel, const Matrix4x4& view, const Matrix4x4& projection) {
    if (!showSkeleton_ || !cubeModel) return;

    // スケルトンの各関節を描画するために Object3d を作成 (毎フレーム再構築すると重いので、必要分をキャッシュ)
    const auto& joints = skinnedModel_->GetJoints();
    size_t numJoints = joints.size();

    if (jointVisuals_.size() < numJoints) {
        jointVisuals_.resize(numJoints);
        for (size_t i = 0; i < numJoints; ++i) {
            jointVisuals_[i] = std::make_unique<Object3d>();
            jointVisuals_[i]->Initialize(object3dCommon);
            jointVisuals_[i]->SetModel(cubeModel);
            jointVisuals_[i]->SetScale({ 0.04f, 0.04f, 0.04f }); // 小さな立方体
        }
    }

    // オブジェクトのワールド行列を取得 (Object3dが計算したWorld行列を利用する)
    // 通常の Object3d の Update が終わっていれば、モデル全体のワールド変換が適用された
    // ジョイントの位置を求めるために、このオブジェクトのワールド行列を取得したい
    // Object3d のメンバから取得できないため、オブジェクトの Affine 行列を自前で計算する
    Matrix4x4 objWorld = Math::MakeAffineMatrix(scale_, rotation_, position_);

    // 関節 (ジョイント) の描画
    for (size_t i = 0; i < numJoints; ++i) {
        // ジョイントのグローバル行列に、オブジェクト自体のワールド行列を掛け合わせる
        Matrix4x4 jointWorld = Math::Multiply(joints[i].globalMatrix, objWorld);

        // カメラ設定
        jointVisuals_[i]->SetCamera(view, projection);
        
        // 通常の Object3d::Update は transformationMatrix の定数バッファを書き換える
        // 自前でジョイント用の行列を書き込むため、Object3dのパラメータをセットして更新する
        // jointWorld から position, rotation, scale を抽出するのは難しいため、
        // Object3d::Update の代わりに、ジョイントの globalMatrix を直接 Object3d に設定できるように
        // するか、あるいは translation を直接与えて、回転はジョイントに追従させる。
        // 最も簡単なのは、jointVisuals_[i] の Position に jointWorld の並進成分をそのままセットすること。
        // スケールは 0.04 固定。回転は無し（球体に見立てた立方体なので回転しなくてもよい）。
        Vector3 globalPos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };
        jointVisuals_[i]->SetPosition(globalPos);
        jointVisuals_[i]->SetRotation({ 0, 0, 0 });
        jointVisuals_[i]->SetScale({ 0.04f, 0.04f, 0.04f });

        // 色分け (選択中のボーンは緑、その他は赤)
        if (static_cast<int>(i) == selectedJointIndex_) {
            jointVisuals_[i]->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
        } else {
            jointVisuals_[i]->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
        }

        // ライティングをオフにして見やすくする
        jointVisuals_[i]->SetEnableLighting(false);

        // 更新して描画
        jointVisuals_[i]->Update(Math::MakeIdentity4x4());
        jointVisuals_[i]->Draw();
    }

    // 骨 (ボーン間の接続線) の描画
    // 親子の関係があるものを結ぶ
    size_t boneVisualCount = 0;
    for (size_t i = 0; i < numJoints; ++i) {
        int parentIdx = joints[i].parentIndex;
        if (parentIdx == -1) continue;

        // キャッシュ用の Object3d が足りなければ生成
        if (boneVisuals_.size() <= boneVisualCount) {
            auto boneObj = std::make_unique<Object3d>();
            boneObj->Initialize(object3dCommon);
            boneObj->SetModel(cubeModel);
            boneVisuals_.push_back(std::move(boneObj));
        }

        auto& boneObj = boneVisuals_[boneVisualCount];
        boneVisualCount++;

        // 親と子のグローバル座標
        Matrix4x4 pJointWorld = Math::Multiply(joints[parentIdx].globalMatrix, objWorld);
        Matrix4x4 cJointWorld = Math::Multiply(joints[i].globalMatrix, objWorld);

        Vector3 pPos = { pJointWorld.m[3][0], pJointWorld.m[3][1], pJointWorld.m[3][2] };
        Vector3 cPos = { cJointWorld.m[3][0], cJointWorld.m[3][1], cJointWorld.m[3][2] };

        // 親から子へのベクトル
        Vector3 v = Math::Subtract(cPos, pPos);
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len < 0.001f) continue;

        Vector3 dir = { v.x / len, v.y / len, v.z / len };

        // 中間位置
        Vector3 centerPos = {
            (pPos.x + cPos.x) * 0.5f,
            (pPos.y + cPos.y) * 0.5f,
            (pPos.z + cPos.z) * 0.5f
        };

        // Y軸 (0, 1, 0) から dir への回転角度
        float phi_y = std::atan2(dir.x, dir.z);
        float phi_x = std::atan2(std::sqrt(dir.x * dir.x + dir.z * dir.z), dir.y);

        // 骨オブジェクトの設定
        boneObj->SetCamera(view, projection);
        boneObj->SetPosition(centerPos);
        boneObj->SetRotation({ phi_x, phi_y, 0.0f });
        boneObj->SetScale({ 0.015f, len * 0.5f, 0.015f }); // 細長い棒 (立方体は中心から上下に広がるので長さの半分)

        // 色は黄色がかった白
        boneObj->SetColor({ 0.9f, 0.9f, 0.5f, 1.0f });
        boneObj->SetEnableLighting(false);

        boneObj->Update(Math::MakeIdentity4x4());
        boneObj->Draw();
    }
}

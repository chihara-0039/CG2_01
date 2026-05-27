#pragma once
#include "Object3dCommon.h"
#include "Model.h"
#include "MyMath.h"


// ==============================================================
//  TransformationMatrix 構造体
//
//  GPU (頂点シェーダー) に渡す行列の定数バッファ。
//  HLSL 側では "cbuffer TransformationBuffer : register(b0)" に対応。
//
//  【各行列の意味】
//  WVP   : World × View × Projection を掛け合わせた合成行列。
//          頂点をスクリーン座標に変換するために使う。
//          最終的に "float4 pos = mul(float4(v.pos,1), WVP);" で使用する。
//
//  World : ワールド空間への変換行列 (移動・回転・拡縮)。
//          ライティング計算はワールド空間で行うため別途渡す。
//
//  lightViewProjection : ライトカメラ視点の VP 行列。
//          シャドウマップのサンプリング位置を求めるために使う。
//          ピクセルシェーダー内で「自分が影の中にいるか」を判定する。
// ==============================================================
struct TransformationMatrix {
    Matrix4x4 WVP;                 // ワールド×ビュー×プロジェクション合成行列
    Matrix4x4 World;               // ワールド変換行列 (ライティング計算用)
    Matrix4x4 lightViewProjection; // ライトカメラの VP 行列 (シャドウマップ参照用)
};

// ==============================================================
//  Material 構造体
//
//  GPU (ピクセルシェーダー) に渡すマテリアル情報の定数バッファ。
//  HLSL 側では "cbuffer MaterialBuffer : register(b2)" に対応。
//
//  【各フィールドの意味】
//  color         : RGBA。アルファ < 1.0 で半透明。
//  enableLighting: 0 = ライティングなし (スカイドーム・グリッド線など自発光物体)
//                  1 = ライティングあり (通常の 3D オブジェクト)
//  shininess     : 鏡面反射の鋭さ。0.0 = マット、1.0 = ピカピカ。
//  metallic      : 金属感。0.0 = プラスチック、1.0 = 鉄・金属。
//  emissive      : 自己発光量。0.0 = 発光なし、1.0 以上 = 暗闇でも光る。
//  uvTransform   : UV スクロール・タイリング用の行列。
//                  デフォルトは単位行列 (1倍・オフセットなし)。
// ==============================================================
struct Material {
    Vector4   color;           // 色 RGBA (アルファで透明度制御)
    int32_t   enableLighting;  // ライティング有効フラグ (0 or 1)
    float     shininess;       // 鏡面反射の強さ・鋭さ (0.0 〜 1.0)
    float     metallic;        // 金属感 (0.0:非金属 〜 1.0:金属)
    float     emissive;        // 自己発光量 (0.0:発光なし 〜 1.0以上:発光)
    Matrix4x4 uvTransform;     // UV 変換行列 (スクロール・タイリングに使用)
};

// ==============================================================
//  Object3d
//
//  シーンに配置する 3D オブジェクト 1 個を表すクラス。
//
//  ─── 設計概要 ─────────────────────────────────────────────
//  Object3d は「見た目の設定値」を持つだけで、
//  実際のメッシュデータ (頂点・インデックス) は Model が持っている。
//  Object3d と Model を分けることで、
//  「同じ Model を複数の Object3d に共有させる」ことができる。
//
//    Model  ─── 頂点バッファ / テクスチャ / メッシュ情報 (重い・1個だけ保持)
//    Object3d ─ 位置・回転・スケール / マテリアル色 (軽い・たくさん作れる)
//
//  ─── 描画の流れ ───────────────────────────────────────────
//  1. Initialize()  : 定数バッファ 2 枚 (TransformationMatrix / Material) を GPU に確保。
//  2. SetModel()    : 描画に使う Model を設定。
//  3. SetCamera()   : View / Projection 行列を受け取って保存。
//  4. Update()      : World 行列を計算し、TransformationMatrix バッファに書き込む。
//  5. Draw()        : Model->Draw() でメッシュを描画。
//
//  ─── 定数バッファのスロット割り当て ─────────────────────
//  b0 : TransformationMatrix (WVP / World / lightVP)
//  b1 : DirectionalLight      (Object3dCommon が管理)
//  b2 : Material              (色・ライティング設定)
//  t0 : テクスチャ SRV        (Model が持つテクスチャ)
//  t1 : ShadowMap SRV         (影判定用深度テクスチャ)
// ==============================================================
class Object3d {
public:
    // -------------------------------------------------------
    //  Initialize : 定数バッファを GPU 上に確保し、
    //  TransformationMatrix と Material のデフォルト値を書き込む。
    //  モデルが決まっていなくてもここで呼んでよい。
    // -------------------------------------------------------
    void Initialize(Object3dCommon* object3dCommon);

    // -------------------------------------------------------
    //  Update : 毎フレーム呼ぶ。
    //  transform_ から World 行列を計算し、
    //  View / Projection と組み合わせて WVP を作成。
    //  TransformationMatrix 定数バッファを GPU に書き込む。
    //  lightVP はシャドウマップ参照のために lightViewProjection に入れる。
    // -------------------------------------------------------
    void Update(const Matrix4x4& lightVP);

    // -------------------------------------------------------
    //  Draw : オブジェクトを描画する。
    //  Object3dCommon::PreDraw() が事前に呼ばれている前提で動作する。
    //  内部で TransformationMatrix・Material の定数バッファをバインドし、
    //  Model::Draw() を呼んでメッシュを描画する。
    // -------------------------------------------------------
    void Draw();

    // ── モデル・トランスフォームの設定 ──────────────────────

    /// <summary>
    /// 描画に使うモデルをセット。所有権は渡さない (Model は呼び出し元が管理)。
    /// </summary>
    void SetModel(Model* model) { model_ = model; }

    /// <summary>ワールド空間での位置をセット (メートル単位)</summary>
    void SetPosition(const Vector3& position) { transform_.translate = position; }

    /// <summary>回転角をオイラー角 (ラジアン) でセット。XYZ の順に適用。</summary>
    void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }

    /// <summary>拡大縮小をセット。1.0 = 等倍 / 2.0 = 2倍 / 0.5 = 半分。</summary>
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    // -------------------------------------------------------
    //  DrawShadow : シャドウマップへの書き込み描画。
    //  ライトカメラ視点の VP 行列 (lightVP) を使って
    //  深度値のみを ShadowMap リソースに書き込む。
    //  ShadowMap::PreDraw() 後に呼ぶこと。
    // -------------------------------------------------------
    void DrawShadow(const Matrix4x4& lightViewProjection);

    // -------------------------------------------------------
    //  SetCamera : ビュー行列とプロジェクション行列をセット。
    //  Camera::GetViewMatrix() / GetProjectionMatrix() の戻り値を渡す。
    //  Update() より前に毎フレーム呼ぶこと。
    // -------------------------------------------------------
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_       = view;
        projectionMatrix_ = projection;
    }

    // ── マテリアル制御 ────────────────────────────────────
    // これらのセッターは内部の Material 定数バッファを直接書き換える。
    // 毎フレーム呼んでも GPU 転送のコストはかからない (既にマップ済みのため)。

    /// <summary>表示色を RGBA で設定。alpha < 1.0 で半透明。</summary>
    void SetColor(const Vector4& color) { if (materialData_) materialData_->color = color; }

    /// <summary>ライティングを有効/無効にする。スカイドームなど自発光物体は false に。</summary>
    void SetEnableLighting(bool enable) { if (materialData_) materialData_->enableLighting = (enable ? 1 : 0); }

    /// <summary>鏡面反射の強さ (0.0:マット 〜 1.0:ピカピカ)</summary>
    void SetShininess(float shininess) { if (materialData_) materialData_->shininess = shininess; }

    /// <summary>金属感 (0.0:プラスチック 〜 1.0:金属)</summary>
    void SetMetallic(float metallic) { if (materialData_) materialData_->metallic = metallic; }

    /// <summary>自己発光量。0.0 超でライトがなくても表示される。</summary>
    void SetEmissive(float emissive) { if (materialData_) materialData_->emissive = emissive; }

    /// <summary>UV 変換行列をセット (スクロール・タイリング用)</summary>
    void SetUVTransform(const Transform& uvTransform);

    // ── ゲッター ─────────────────────────────────────────

    /// <summary>設定中の Model ポインタ (変更・確認に使用)</summary>
    Model* GetModel() const { return model_; }

    /// <summary>トランスフォーム構造体 (位置・回転・スケールを一括取得)</summary>
    const Transform& GetTransform() const { return transform_; }

    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

    /// <summary>マテリアルデータのコピーを取得 (デバッグ用)</summary>
    const Material& GetMaterial() const { return *materialData_; }

    /// <summary>ワールド位置を取得 (当たり判定・UI 追従などに使用)</summary>
    const Vector3& GetPosition() const { return transform_.translate; }

private:
    // ── 参照 (所有しない) ────────────────────────────────
    Object3dCommon* object3dCommon_ = nullptr; // PreDraw() などを呼ぶための参照
    Model*          model_          = nullptr;  // メッシュデータの参照 (所有権なし)

    // ── トランスフォーム ──────────────────────────────────
    // scale / rotate / translate をひとつの構造体にまとめている。
    // Update() でこれらから World 行列 (アフィン変換行列) を生成する。
    Transform transform_ = { {1,1,1}, {0,0,0}, {0,0,0} }; // デフォルト: 等倍・回転なし・原点

    // ── カメラ行列 (毎フレーム SetCamera() で更新) ──────────
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    // ── GPU 定数バッファ ─────────────────────────────────
    // GPU 上に確保されたバッファ。Map で CPU 側ポインタを取得し、
    // データを書き込むだけで GPU に即反映される (アップロードヒープのため)。

    /// <summary>WVP / World / lightVP 行列を格納する定数バッファ (register b0)</summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
    TransformationMatrix* transformationData_ = nullptr; // CPU 側書き込みポインタ

    /// <summary>色・ライティング設定を格納するマテリアルバッファ (register b2)</summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr; // CPU 側書き込みポインタ
};
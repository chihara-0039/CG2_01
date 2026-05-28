#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"

// ==============================================================
//  DirectionalLight 構造体
//
//  GPU (HLSL シェーダー) に渡す平行光源データ。
//  「太陽光」のように方向は決まっているが光源が無限遠にある照明。
//
//  【HLSL 側の対応バッファ】
//    cbuffer LightBuffer : register(b1) { DirectionalLight light; }
//
//  【各フィールドの意味】
//    color         : 光の色 (RGBA)。白 (1,1,1,1) が自然な昼光。
//    direction     : 光が降ってくる方向 (正規化ベクトル)。
//                    例: (0,-1,0) → 真上から降る光。
//                    SetLightDirection() で自動正規化される。
//    intensity     : 光の強さ (0.0:暗い ~ 1.0:通常 ~ 2.0以上:明るい)
//    cameraPosition: スペキュラー (鏡面反射) とリムライトの計算に
//                    カメラ位置が必要なため追加。
//    paddingLight  : HLSL の 16 バイトアライメント要件を満たすための
//                    空きパディング。必ず保持すること。
// ==============================================================
struct DirectionalLight {
    Vector4 color;           // 光の色 RGBA
    Vector3 direction;       // 光の方向 (正規化済み)
    float   intensity;       // 光の強さ
    Vector3 cameraPosition;  // スペキュラー計算用カメラ位置
    float   paddingLight;    // 16バイトアライメント用パディング
};

// ==============================================================
//  Object3dCommon
//
//  シーン内の全 3D オブジェクトが共有する描画リソースを管理するクラス。
//
//  ─── 主な役割 ─────────────────────────────────────────────
//  1. RootSignature の生成・保持
//     → シェーダーに何を渡すか (定数バッファ・テクスチャなど) を定義する
//
//  2. PSO (Pipeline State Object) の生成・保持
//     → 頂点シェーダー・ピクセルシェーダー・ブレンド・ラスタライザーを
//        まとめてひとつの「描画設定セット」として GPU に登録したもの。
//        描画する種類ごとに PSO が存在する (通常 / 影 / インスタンシング など)
//
//  3. 平行光源 (DirectionalLight) の定数バッファ管理
//     → ゲームワールド全体で共有する光の方向・色・強度を GPU に送る
//
//  ─── 使い方 ───────────────────────────────────────────────
//  Object3d::Draw() の直前に PreDraw() を呼ぶと、
//  適切な RootSignature と PSO が CommandList にバインドされる。
//
//  ─── PSO の種類 ───────────────────────────────────────────
//  通常 PSO             : ライティング・テクスチャあり。一般的な 3D オブジェクト用。
//  影 PSO               : 深度値のみ書き込む。ShadowMap 生成用。
//  プレイヤー強調 PSO   : 壁に隠れたプレイヤーをシルエットで表示するための特殊 PSO。
//  インスタンシング PSO : 同じモデルを複数まとめて 1 ドローコールで描くための PSO。
//  半透明インスタンシング PSO : ブロックの半透明表示用。
// ==============================================================
class Object3dCommon {
public:
    // -------------------------------------------------------
    //  初期化。RootSignature / PSO / LightBuffer を生成する。
    //  TextureManager は SetTextureManager() で事前にセットしておくこと。
    // -------------------------------------------------------
    void Initialize(DirectXCommon* dxCommon);

    // -------------------------------------------------------
    //  PreDraw : 描画前に CommandList へ描画設定をバインドする。
    //  具体的には以下を行う:
    //    ・RootSignature をセット (どのスロットに何を渡すか)
    //    ・PSO をセット (シェーダー・ラスタライザー設定)
    //    ・プリミティブトポロジーをセット (TRIANGLELIST)
    //    ・平行光源の定数バッファを b1 スロットにバインド
    //  各オブジェクトは Draw() 前にこれが呼ばれていることを前提とする。
    // -------------------------------------------------------
    void PreDraw();

    // ── ゲッター ─────────────────────────────────────────

    /// <summary>DirectXCommon へのアクセス (テクスチャロード時などに使用)</summary>
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    /// <summary>
    /// 通常描画用の RootSignature。
    /// シャドウマップ描画前に commandList->SetGraphicsRootSignature() に渡す。
    /// </summary>
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

    /// <summary>
    /// 通常描画用の PSO (ライティング・テクスチャあり)。
    /// ここで null が返る場合は Initialize() での PSO 生成に失敗している。
    /// </summary>
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

    /// <summary>
    /// 影描画用の PSO (深度値のみ書き込む・ピクセルシェーダーなし)。
    /// ShadowMap::PreDraw() 後にこれをセットして DrawShadow() を呼ぶ。
    /// </summary>
    ID3D12PipelineState* GetShadowPipelineState() const { return shadowPipelineState_.Get(); }

    // ── インスタンシング用ゲッター ────────────────────────
    // インスタンシング = 同じモデルを大量に 1 ドローコールで描く技法。
    // ブロックのようにたくさん並ぶオブジェクトに有効。

    /// <summary>インスタンシング用 RootSignature</summary>
    ID3D12RootSignature* GetInstancedRootSignature() const { return instancedRootSignature_.Get(); }

    /// <summary>インスタンシング通常描画 PSO</summary>
    ID3D12PipelineState* GetInstancedPipelineState() const { return instancedPipelineState_.Get(); }

    /// <summary>インスタンシング影描画 PSO</summary>
    ID3D12PipelineState* GetInstancedShadowPipelineState() const { return instancedShadowPipelineState_.Get(); }

    /// <summary>インスタンシング半透明描画 PSO (壁の透明化などに使用)</summary>
    ID3D12PipelineState* GetInstancedAlphaPipelineState() const { return instancedAlphaPipelineState_.Get(); }

    // ── ライト制御 ────────────────────────────────────────
    // ステージごとに光の方向・色・強さを変えることで雰囲気が変わる。
    // StageMap から読み取った値を毎フレーム Update() でここに設定する。

    /// <summary>デフォルトのライト設定に戻す (白い光・斜め上から照らす)</summary>
    void SetDefaultLight();

    /// <summary>平行光源の方向をセット。内部で自動正規化される。</summary>
    void SetLightDirection(const Vector3& direction) {
        if (lightData_) lightData_->direction = Math::Normalize(direction);
    }

    /// <summary>平行光源の色をセット (RGBA)</summary>
    void SetLightColor(const Vector4& color) {
        if (lightData_) lightData_->color = color;
    }

    /// <summary>平行光源の強さをセット (0.0 〜 2.0 程度が実用的)</summary>
    void SetLightIntensity(float intensity) {
        if (lightData_) lightData_->intensity = intensity;
    }

    /// <summary>
    /// スペキュラー計算用のカメラ位置をセット。
    /// MyGame::Update() で毎フレーム camera->GetPosition() を渡すこと。
    /// </summary>
    void SetCameraPosition(const Vector3& cameraPosition) {
        if (lightData_) lightData_->cameraPosition = cameraPosition;
    }

    /// <summary>GPU 上の平行光源バッファの仮想アドレス (定数バッファのバインドに使用)</summary>
    D3D12_GPU_VIRTUAL_ADDRESS GetLightGPUVirtualAddress() const {
        return lightResource_->GetGPUVirtualAddress();
    }

    // ── TextureManager ────────────────────────────────────

    /// <summary>
    /// TextureManager をセットする。Initialize() より前に必ず呼ぶこと。
    /// テクスチャの SRV ヒープ参照に使う。
    /// </summary>
    void SetTextureManager(TextureManager* textureManager) { textureManager_ = textureManager; }

    /// <summary>TextureManager へのポインタを返す</summary>
    TextureManager* GetTextureManager() const { return textureManager_; }

    // -------------------------------------------------------
    //  PreDrawPlayerHighlight
    //  カメラとプレイヤーの間に壁がある場合に
    //  プレイヤーを壁越しでもシルエット表示するための特殊 PSO に切り替える。
    //  この後 Player::DrawHighlight() を呼び、終わったら PreDraw() で戻す。
    // -------------------------------------------------------
    void PreDrawPlayerHighlight();

private:
    // ── 初期化内部関数 ────────────────────────────────────
    // 各リソースの生成を役割ごとに分割。順番に依存関係があるため注意。

    void CreateRootSignature();             // 通常描画用 RootSignature
    void CreateGraphicsPipeline();          // 通常描画用 PSO
    void CreateLightBuffer();              // 平行光源の定数バッファ
    void CreateShadowPipeline();           // 影描画用 PSO
    void CreateInstancedRootSignature();   // インスタンシング用 RootSignature
    void CreateInstancedGraphicsPipeline();// インスタンシング通常 PSO
    void CreateInstancedShadowPipeline();  // インスタンシング影 PSO
    void CreatePlayerHighlightPipeline();  // プレイヤーシルエット用 PSO
    void CreateInstancedAlphaPipeline();   // 半透明インスタンシング PSO

private:
    DirectXCommon*  dxCommon_       = nullptr;
    TextureManager* textureManager_ = nullptr;

    // ── RootSignature / PSO ───────────────────────────────
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;     // 通常描画用
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;     // 通常描画用

    // ── 平行光源定数バッファ ──────────────────────────────
    // GPU 上に確保されたバッファに lightData_ ポインタ経由で CPU から書き込む。
    // マップ済みなので毎フレーム書き込んだ内容がそのまま GPU に反映される。
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr; // GPU バッファへの CPU 側ポインタ

    // ── 特殊 PSO 群 ──────────────────────────────────────
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;          // 影用
    Microsoft::WRL::ComPtr<ID3D12PipelineState> playerHighlightPipelineState_; // シルエット用

    // ── インスタンシング用リソース ────────────────────────
    Microsoft::WRL::ComPtr<ID3D12RootSignature> instancedRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedShadowPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedAlphaPipelineState_; // 半透明用
};
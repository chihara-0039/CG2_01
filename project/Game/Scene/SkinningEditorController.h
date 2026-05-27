#pragma once
#include "SkinnedObject.h"
#include "Object3d.h"
#include "Model.h"
#include "Camera.h"
#include "Input.h"
#include "MyMath.h"
#include <vector>
#include <string>
#include <memory>

// 前方宣言 (循環インクルードを防ぐ)
class Object3dCommon;
class DirectXCommon;
class TextureManager;
class Player;

/// <summary>
/// スキニングエディターモードの制御クラス。
///
/// 役割：
///   - スキャン/切り替え/反映 の3操作 (ScanGltfModels / ChangePreviewModel / ApplyModelToPlayer)
///   - プレビュー用 SkinnedObject の所有と更新・描画
///   - グリッド線 (地面補助グリッド) の所有と更新・描画
///   - スケルトン描画用デバッグ立方体モデルの所有
///   - レイキャストによるジョイントのクリック選択
///   - ImGui の下パネル (タイムライン) と右パネル (設定・ボーン操作) の描画
///
/// MyGame はこのクラスへ委譲することで、スキニング関連のメンバーをすべてここに集約する。
/// </summary>
class SkinningEditorController {
public:
    SkinningEditorController()  = default;
    ~SkinningEditorController() = default;

    // ========== 初期化 ==========

    /// <summary>
    /// 初期化。SkinnedObject・デバッグキューブ・グリッド線・モデルリストを構築する。
    /// </summary>
    /// <param name="object3dCommon">Object3dCommon へのポインタ (非所有)</param>
    /// <param name="dxCommon">DirectXCommon へのポインタ (非所有)</param>
    /// <param name="textureManager">TextureManager へのポインタ (非所有)</param>
    void Initialize(
        Object3dCommon* object3dCommon,
        DirectXCommon*  dxCommon,
        TextureManager* textureManager);

    // ========== 毎フレーム処理 ==========

    /// <summary>
    /// 毎フレームの更新。
    /// レイキャストによるジョイント選択・SkinnedObject の更新・グリッド線の更新を行う。
    /// </summary>
    /// <param name="dxCommon">スキニング計算に使用する DirectXCommon</param>
    /// <param name="input">マウスクリックによるジョイント選択に使用する Input</param>
    /// <param name="camera">ビュー・プロジェクション行列の取得に使用する Camera</param>
    /// <param name="lightVP">影行列 (SkinnedObject の Update に渡す)</param>
    /// <param name="isGuiCaptured">ImGui がマウスをキャプチャしているか (クリック判定のガード用)</param>
    void Update(
        DirectXCommon*       dxCommon,
        Input*               input,
        Camera*              camera,
        const Matrix4x4&     lightVP,
        bool                 isGuiCaptured);

    /// <summary>グリッド線・スキニングメッシュ・スケルトンを描画する</summary>
    /// <param name="object3dCommon">スケルトン描画の PreDraw に使用</param>
    /// <param name="camera">スケルトン描画のビュー・プロジェクション行列</param>
    void Draw(Object3dCommon* object3dCommon, Camera* camera);

    /// <summary>シャドウマップへの描画 (影を生成するため)</summary>
    void DrawShadow(const Matrix4x4& lightVP);

    // ========== ImGui 描画 ==========

    /// <summary>
    /// 下パネル (Tools &amp; Controls) 内に描画するタイムライン UI。
    /// キーフレームのビジュアルタイムラインとジョイント別のキーフレームリストを描画する。
    /// </summary>
    void DrawImGuiTimeline();

    /// <summary>
    /// 右パネル (Skinning Editor) に描画するサイドパネル UI。
    /// モデル選択・アニメーション選択・ボーン操作・カメラプリセットを描画する。
    /// </summary>
    /// <param name="camera">カメラプリセットボタンの操作対象</param>
    /// <param name="player">「ゲームに反映」ボタン押下時に更新するプレイヤー</param>
    /// <param name="defaultObjModel">インデックス 1 (OBJ プレイヤー) のモデル</param>
    void DrawImGuiSidePanel(Camera* camera, Player* player, Model* defaultObjModel);

    // ========== ゲッター ==========

    /// <summary>プレビュー用 SkinnedObject へのポインタを返す (非所有)</summary>
    SkinnedObject* GetPreviewObject() const { return skinnedObject_.get(); }

    /// <summary>プレビュー用 SkinnedObject が有効かどうか</summary>
    bool HasPreviewObject() const { return skinnedObject_ != nullptr; }

private:
    // ========== 内部処理 ==========

    /// <summary>
    /// Resources/Models 以下の .gltf / .glb ファイルを再帰スキャンし、
    /// modelPaths_ / modelNames_ に追加する。
    /// インデックス 0 : Default Humanoid (組み込みスキニング人型)
    /// インデックス 1 : Default Player (.obj 静止モデル)
    /// インデックス 2以降 : スキャンした glTF ファイル
    /// </summary>
    void ScanGltfModels();

    /// <summary>
    /// 指定インデックスのモデルをプレビュー SkinnedObject にロードする。
    /// インデックス 0/1 はデフォルト人型、2 以降は glTF ファイルから初期化する。
    /// </summary>
    void ChangePreviewModel(int index);

    /// <summary>
    /// 現在選択中のモデルをプレイヤーに反映する。
    /// インデックス 0: デフォルトスキニング人型
    /// インデックス 1: OBJ プレイヤー
    /// インデックス 2以降: glTF モデル
    /// </summary>
    void ApplyModelToPlayer(Player* player, Model* defaultObjModel);

private:
    // ========== 所有リソース ==========

    std::unique_ptr<SkinnedObject>           skinnedObject_;  ///< スキニングプレビュー用オブジェクト
    std::unique_ptr<Model>                   debugCubeModel_; ///< スケルトン描画用のデバッグ立方体モデル
    std::vector<std::unique_ptr<Object3d>>   gridLines_;      ///< 地面補助グリッド線のオブジェクト群

    // ========== モデルリスト ==========

    std::vector<std::string> modelPaths_; ///< ファイルパス (glTF は実際のパス, Default 系は識別子)
    std::vector<std::string> modelNames_; ///< UI 表示用のモデル名
    int selectedModelIndex_   = 0;        ///< 現在プレビュー中のモデルインデックス
    int activeGameModelIndex_ = 1;        ///< ゲームに反映済みのモデルインデックス (初期: OBJ)

    // ========== 非所有ポインタ (依存参照) ==========

    Object3dCommon* object3dCommon_  = nullptr; ///< Object3dCommon への参照 (所有しない)
    DirectXCommon*  dxCommon_        = nullptr; ///< DirectXCommon への参照 (所有しない)
    TextureManager* textureManager_  = nullptr; ///< TextureManager への参照 (所有しない)
};

# CG4 評価課題1「演出王 Part3」

DirectX 12で制作した、ゲーム向けエフェクトの作成・編集・鑑賞アプリケーションです。

一瞬の攻撃演出である **Hit Effect** と、一定時間継続する環境演出 **Tempest Storm（暴風雷）** の2系統を実装しています。作成したプリセットは、Release版のEffect Showcaseで連続鑑賞できます。

## 見どころ

- 斬撃、火花、衝撃波、柱、枝分かれ雷を組み合わせたHit Effect
- 黒雲、斜めに降る雨、横風、ランダム落雷による持続型の暴風雷
- 加算合成とアルファ合成を用途ごとに分離
- エフェクトの発光色に連動する動的ポイントライト
- 雷光がterrainへ拡散反射・スペキュラー反射する表現
- CPUパーティクルのインスタンシング描画
- JSONによるプリセット保存・読込
- Release版で操作に迷わないEffect Showcase

## エフェクト

### Hit Effect

攻撃が命中した瞬間を想定した短時間エフェクトです。

- 中心のフラッシュ
- 斬撃軌跡
- 放射状の火花
- 衝撃波リング
- 交差光
- 発光する柱
- 本線と分岐を持つ雷

サイズ、明るさ、寿命、角度、粒子数、色、ランダム性などをEffect Editorから調整できます。

### Tempest Storm

戦闘フィールドやボス演出を想定した持続型エフェクトです。

- ゆっくり出現・消滅する黒雲
- 風向きを感じる雨と光の筋
- ランダムな間隔で発生する枝分かれ雷
- 雷の着地点に追従する青白いポイントライト
- 暴風雷中の暗い背景とterrainへの照明反映

Storm Editorでは、雲・雨・風・雷を別々に調整できます。

## Effect Preview / Editor

Debugビルドで起動し、左側の `App Mode` から `EffectPreview` を選択します。

右側のEffect Editorにある `Effect Type` から編集対象を切り替えます。

- `Hit Effect`：瞬間系エフェクト用エディター
- `Tempest Storm`：持続系エフェクト用Storm Editor

### Hit Effectプリセット

1. パラメーターを調整する
2. `Preset Name`を入力する
3. `Include in Showcase`を有効にする
4. `Save Preset`を押す

保存先：`Resources/presets/effect_presets.json`

### Stormプリセット

1. `Effect Type`を`Tempest Storm`へ変更する
2. Storm Area、Dark Clouds、Rain、Wind、Lightningを調整する
3. `Storm Preset Name`を入力する
4. `Include in Showcase`を有効にする
5. `Save Storm Preset`を押す

保存先：`Resources/presets/storm_effect_presets.json`

## Effect Showcase

ReleaseビルドではEffect Showcaseから起動します。

`Include in Showcase`が有効なHit EffectとStormプリセットを順番に鑑賞できます。通常エフェクトは約2.5秒、暴風雷は約10秒表示されます。

| 操作 | 内容 |
|---|---|
| `←` / `→` | エフェクトを選択 |
| `Space` / `H` | 再生・再スタート |
| `A` | 自動再生のON/OFF |
| `R` | プリセット一覧を再読込 |
| `Tab` | ステージ選択へ移動 |
| マウス中ボタンドラッグ | カメラ回転 |
| `Shift` + マウス中ボタンドラッグ | カメラ平行移動 |
| マウスホイール | ズーム |

## ビルド方法

### 必要環境

- Windows 10 / 11
- Visual Studio 2022
- C++20
- Windows SDK
- DirectX 12対応GPU

### 手順

1. Visual Studioで `CG2_01.sln` を開く
2. プラットフォームを `x64` に設定する
3. 編集する場合は `Debug`、提出・鑑賞する場合は `Release` を選択する
4. ソリューションをビルドして実行する

シェーダーや画像、JSONは相対パスで読み込むため、実行時の作業ディレクトリはプロジェクトフォルダーにしてください。

## 主な構成

```text
Engine/Graphics/Particle/ParticleManager.*  パーティクル生成・更新・描画
Engine/Graphics/3D/Object3dCommon.*         ライトと3D描画の共通処理
Engine/Graphics/PostProcessRenderer.*       ポストエフェクト
Game/Scene/MyGame.*                         モード・Editor・Showcaseの統合
Resources/shaders/hlsl/                     HLSLシェーダー
Resources/presets/                          エフェクト・天候プリセット
```

## 評価時に確認してほしい点

- **かっこよさ**：複数レイヤーと時間差、色付き雷光による画面の変化
- **見やすさ**：Showcaseの名称表示、操作ガイド、自動再生
- **ゲームで使えそう度**：瞬間系と持続系を分けた設計、JSONプリセット、terrainへの照明反映


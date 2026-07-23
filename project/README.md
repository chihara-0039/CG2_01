# CG4 評価課題1「演出王 Part3」

DirectX 12で制作したエフェクト作品です。

瞬間的に発生する「Hit Effect」と、黒雲・雨・風・雷が継続する「Tempest Storm（暴風雷）」を実装しました。Development版のImGuiからエフェクトの編集と鑑賞ができます。

## 実装したエフェクト

### Hit Effect

命中した瞬間に発生する短時間のエフェクトです。

- 中心フラッシュ
- 斬撃軌跡
- 放射状の火花
- 衝撃波リング
- 交差光と発光する柱
- 本線と分岐を持つ雷

### Tempest Storm

一定時間継続する暴風雷エフェクトです。

- ランダムな位置と大きさで発生する黒雲
- 斜めに降る雨と、地面に当たった際の飛沫
- 横方向へ流れる風
- 枝分かれする雷と雷光
- 同時落雷と時間差による連続落雷
- 雷の着地点に追従するポイントライト
- terrainへの雷光の反射

## 技術要素

- CPUパーティクルのインスタンシング描画
- 加算合成とアルファ合成の使い分け
- 複数レイヤーと時間差によるエフェクト構成
- パーティクルの位置・大きさ・色・寿命・方向のランダム化
- 動的ポイントライトによる地面への発光反映
- JSONによるエフェクトプリセットの保存と読み込み

## 操作方法

### モードの切り替え

Development版を起動し、左側の `App Mode` から使用するモードを選択します。

- `EffectPreview`：エフェクトの編集と動作確認
- `EffectShowcase`：保存したエフェクトの鑑賞

### Effect Showcase

| 操作 | 内容 |
| --- | --- |
| `←` / `→` | エフェクトを選択 |
| `Space` / `H` | エフェクトを再生・再スタート |
| `A` | 自動再生のON/OFF |
| `R` | プリセット一覧を再読み込み |
| マウス中ボタンドラッグ | カメラ回転 |
| `Shift` + マウス中ボタンドラッグ | カメラ平行移動 |
| マウスホイール | ズーム |

### Effect Preview / Editor

左側の `App Mode` を `EffectPreview` に変更すると、右側のImGuiから編集できます。

`Effect Type`で編集対象を選択します。

- `Hit Effect`：サイズ、明るさ、寿命、粒子数、形状、色、雷、ランダム性などを調整
- `Tempest Storm`：雲、雨、風、雷の各パラメーターを調整

Tempest Stormでは、雲・雨・雷の発生範囲、落雷頻度、雷の大きさ、同時落雷数、連続回数、枝の本数・長さ・太さなどを変更できます。各種ランダム設定のON/OFFも可能です。

調整した内容は名前を入力して保存でき、`Include in Showcase`を有効にするとEffect Showcaseの鑑賞対象になります。

## ビルド・実行方法

### 必要環境

- Windows 10 / 11
- Visual Studio 2022
- Windows SDK
- DirectX 12対応GPU

### 手順

1. Visual Studioで `CG2_01.sln` を開く
2. プラットフォームを `x64` に設定する
3. 構成を `Development` に設定する
4. ソリューションをビルドして実行する

シェーダー、画像、JSONは相対パスで読み込むため、実行時の作業ディレクトリはプロジェクトフォルダーにしてください。

## CG5 Evaluation Task 1: PostEffect

### Required Feature

This project implements scene rendering through Grayscale.

- `PostProcessRenderer` first renders the normal 3D scene into a RenderTexture.
- `Resources/shaders/hlsl/Grayscale.PS.hlsl` samples that RenderTexture, converts RGB to luminance, and writes the grayscale result to the back buffer.
- `Release` starts from the normal `StageSelect` game flow. Post-effect showcase modes remain available from development tools.
- At runtime, the setting can be confirmed from ImGui: `Offscreen Rendering (RenderTexture)` > `Enable Offscreen Rendering` and `Post Effect: Grayscale`.

### Extra PostEffects

The following additional PostEffects can also be selected from the same ImGui combo:

- Vignetting
- BoxFilter 3x3 / 5x5
- GaussianFilter
- LuminanceBasedOutline
- DepthBasedOutline
- RadialBlur
- Dissolve
- Random

Vignette / DepthBasedOutline / RadialBlur / Dissolve / Random expose adjustable parameters in ImGui.

### PostEffect Showcase Controls

`PostEffectShowcase` remains available as a development/evaluation mode, but it is no longer the default `Release` startup mode.
This mode disables the particle showcase effects so the screen-space PostEffect is easy to inspect.
Use number keys to switch the visible PostEffect:

| Key | PostEffect |
| --- | --- |
| `1` | Grayscale |
| `2` | Vignetting |
| `3` | GaussianFilter / Smoothing |
| `4` | BoxFilter 3x3 |
| `5` | BoxFilter 5x5 |
| `6` | LuminanceBasedOutline |
| `7` | DepthBasedOutline |
| `8` | RadialBlur |
| `9` | Dissolve |
| `0` | Random |

`TAB` returns to Stage Select. Mouse middle-drag orbits the camera, `Shift + middle-drag` pans, and the mouse wheel zooms.

### Integration In The Game

PostEffects are connected to the normal playable `GamePlay` scene, not only to an engine test or editor screen.
Every stage uses the RenderTexture path and starts with the original-color `Normal` presentation. Press `1` at any time to demonstrate the required `Grayscale` scene rendering, or use `N` to return to the normal game presentation.
While controlling the player, use the number keys to change the screen presentation in real time:

| Key | In-game use |
| --- | --- |
| `N` | Normal/original-color presentation |
| `1` | Grayscale presentation |
| `2` | Vignetting presentation |
| `3` | Gaussian smoothing |
| `4`, `5` | 3x3 / 5x5 BoxFilter smoothing |
| `6` | Luminance-based outline presentation |
| `7` | Depth-based outline presentation |
| `8` | RadialBlur presentation |
| `9` | Dissolve transition presentation |
| `0` | Animated Random presentation |

The current effect and the controls are displayed at the upper-right of the game screen in both Development and Release builds. The separate `PostEffectShowcase` mode is retained for close comparison and parameter adjustment.

Gameplay starts with the camera following the player. Press `V` to switch to a fixed overview camera centered on the whole stage, and press `V` again to resume player tracking. The player model uses emissive lighting and continuously carries a blue-white point light so nearby geometry remains readable. The renderer supports up to eight simultaneous point lights; Tempest lightning uses a separate light slot and illuminates the scene without disabling the player light.

Future gameplay systems can call `Object3dCommon::ClearPointLights()` once per frame and then register player lights, lightning, torches, projectiles, or stage lamps with `AddPointLight(position, intensity, color, radius)`. The same light array is evaluated by normal, skinned, and instanced 3D rendering.

### Weather Ground-impact Effects

Weather presets now select a dedicated ground-impact style. `Heavy Rain` and its copy reuse the compact rain splash created for `Tempest Storm`; `Snowy` creates a soft snow puff and a few slowly fading flakes instead of a water splash. The `Ground Impact` combo in the Stage Editor can change this behavior between `None`, `Rain`, and `Snow` before saving the preset.

`Tempest Storm` is also available directly in the Stage Editor weather preset list. It loads the editable `Tempest Storm` entry from `Resources/presets/storm_effect_presets.json`, follows the player, and combines its cloud, rain, wind, lightning, light flash, and rain-impact behavior. Adjusting and saving that Effect Editor preset therefore updates the weather version without duplicating the storm parameters.

The old background clouds made from grouped sphere objects have been replaced by the same particle-cloud renderer used by the storm. Each weather preset can independently control `Enable Particle Clouds`, cloud color, density, size, and `Cloud Height Above Stage`. The engine periodically finds the highest occupied stage cell and keeps both ambient and Tempest clouds above that height plus the configured margin.

Weather presets also store `Sky Color` and `Sky Brightness`. These values tint both the skydome and cubemap skybox in real time and can be edited from `Stage Editor > Weather / Environment > Sky Settings`.

### Main Files

- `Engine/Graphics/PostProcessRenderer.h`
- `Engine/Graphics/PostProcessRenderer.cpp`
- `Resources/shaders/hlsl/Fullscreen.VS.hlsl`
- `Resources/shaders/hlsl/Grayscale.PS.hlsl`
- `Resources/shaders/hlsl/BoxFilter3x3.PS.hlsl`
- `Resources/shaders/hlsl/BoxFilter5x5.PS.hlsl`
- `Resources/shaders/hlsl/GaussianFilter.PS.hlsl`
- `Resources/shaders/hlsl/LuminanceBasedOutline.PS.hlsl`
- `Resources/shaders/hlsl/DepthBasedOutline.PS.hlsl`
- `Resources/shaders/hlsl/RadialBlur.PS.hlsl`
- `Resources/shaders/hlsl/Dissolve.PS.hlsl`
- `Resources/shaders/hlsl/Random.PS.hlsl`

## CG4 Evaluation Task 2 Additions

- SkinningModel display and ComputeShader skinning are implemented in the Skinning Editor mode.
- Bone debug display is available in the Skinning Editor viewport.
- Hand particle emission is available from the character hand joint.
- Animation blend preview was added. In `Skinning Editor > Animation Selection`, select a blend target and press `Blend To Target`; translation and scale are linearly interpolated, while rotation is blended with quaternion Slerp.
## 評価課題2 追加内容

### SkinnedModel / Compute Shader Skinning

- `SkinnedModel` は glTF の skin / joint / animation を読み込み、ジョイント行列を更新します。
- `Resources/shaders/hlsl/Skinning.CS.hlsl` を使い、スキニング済み頂点を GPU バッファへ書き出します。
- `SkinnedObject::Draw()` では Compute Shader のスキニング結果を描画に使用します。

### Bone Debug Display

- `SkinningEditor` モードでスケルトンを表示できます。
- ジョイントは赤いキューブ、親子関係は黄色いボーンで表示します。
- 選択中ジョイントは緑色で強調表示します。
- `Show Selected Bone Axes` を有効にすると、選択中ジョイントのローカル X/Y/Z 軸を赤/緑/青で表示します。
- 右パネルの `Select Bone` またはビューポート上のクリックでジョイントを選択できます。

### Particle From Hand

- `SkinningEditor` の `Emit Particles From Hand` を有効にすると、手のジョイント位置からパーティクルを発生させます。
- `RightHand` / `LeftHand` / `Hand_R` / `Hand_L` などの名前を検索し、見つかったジョイントのワールド座標をエミッター位置として使用します。
- ジョイント位置は `SkinnedObject::TryGetJointWorldPosition()` で取得し、既存の `ParticleManager::EmitHitEffect()` に渡しています。

### 操作手順

1. `Development|x64` で起動します。
2. 左側の `App Mode` から `SkinningEditor` を選択します。
3. 右側の `Skinning Editor` パネルで skinned glTF モデルを選びます。
4. `Show Skeleton Bones` と `Show Selected Bone Axes` を有効にして骨の状態を確認します。
5. `Emit Particles From Hand` を有効にすると、手ジョイントからパーティクルが出ます。

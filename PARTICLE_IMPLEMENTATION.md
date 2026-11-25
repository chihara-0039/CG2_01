# エフェクト・パーティクル実装メモ

確認日：2026年11月25日。現在のCG2_01のコードを読むためのメモ。
技術名だけでなく、「どこで何をして、何のために使ったか」を対応させる。

## 1. 全体の流れ

今回追加した砂埃・回収・配置・ハンコなどは、**CPUで粒の状態を更新し、GPUでインスタンシング描画する方式**。

```text
ゲーム側で出来事を検出
    ↓
EmitDust / EmitAdventureEffect / EmitSceneEffectで粒を生成
    ↓
ParticleManager::Updateで寿命・座標・大きさ・透明度を更新
    ↓
粒ごとの行列・色・形状番号をインスタンスバッファへ書く
    ↓
頂点シェーダーで座標変換 → ピクセルシェーダーで輪郭と色を計算
    ↓
深度判定とブレンドを使って画面へ合成
```

入口は[ParticleManager.cpp]、粒のデータ定義は[ParticleManager.h]。
`Particle`は座標・速度・色・経過時間`lifeTime`・寿命`maxTime`などを持つ。
`origin`は発生位置、`target`は吸い込み先、`baseScale`は元の大きさ。

## 2. 何をきっかけに発生させているか

| 場面 | 発生を決める場所 | そこでしていること |
|---|---|---|
| 歩行 | [GameRuntimeGameplay.cpp]の`UpdateGamePlay` → `FootstepEmitter::Update` | 専用クラスで接地中の歩幅を蓄積し、0.65を超えた結果を受けて`EmitDust`。進行方向の後ろへ煙を流す |
| ジャンプ・着地 | 同上 | 前後の接地状態を比較。離陸・着地の瞬間だけ`EmitAdventureEffect`を呼ぶ |
| 鍵取得 | 同上 | 取得した鍵の数が増えたら金色の粒を出す |
| 梯子 | 同上 | 梯子に乗っている状態が変わったときだけ小さな星を出す |
| Pスイッチ・回転操作 | 同上 | Pスイッチの状態変更、または回転操作の成功時に発生させる |
| 近くの目標 | 同上 | 近傍の鍵・シャボン玉・解放済みゴールへ少量の光を出す |
| パーツ配置・取消 | [BlockPlacementController.cpp]の`TryPlace` / `TryUndo` | マップ・在庫の変更が成功した後に`feedback_`で通知する |
| シャボン玉回収 | [BubblePickupController.cpp] | 回収時のコールバックで位置を通知する |
| 配置・取消・回収の接続 | [GameRuntimeLifecycle.cpp] | 通知を`EmitAdventureEffect`へつなぐ。回収・取消の到着先は主人公の位置＋高さ0.8 |
| スター取得 | [StarGetPresentation.cpp] | 取得演出のタイミングに合わせて`AdventureEffect::Star`を発生させる |
| ハンコ | [GameClearScene.cpp]の`Update` | 打刻時刻をまたいだ瞬間に音と粒を同時に発生させる |
| 落下 | [GameOver.cpp]の`Update` | 落下の芝居中だけ、現在のプレイヤー位置から風の筋を出す |
| 再挑戦 | [GameRuntimeScenes.cpp]の`RunGamePlayScene` | リスポーン直後に`SceneEffect::Retry`を出す |
| リザルトの花火 | [GameRuntimeGameplay.cpp]の`UpdateGameClear` | 手帳の打刻演出が終わった後、間隔・位置・同時発生数を変えながら打ち上げる |

**コールバック**は「処理が成功したことを別の処理へ知らせる仕組み」。
配置処理の中に粒の計算を直接書かず、成功通知をエフェクトへつなぐことで、失敗した配置で達成感のある光が出るのを避けている。

## 3. 粒をどう動かしているか

### 寿命による制御と時間差発生

[ParticleManager.cpp]の`Update`で経過時間を進め、寿命を過ぎた粒を削除する。
`t = lifeTime / maxTime`で進行度を0〜1にそろえ、位置・大きさ・透明度の計算に使う。

`lifeTime`を負の値で作ると、0になるまで更新・描画を待つ。
リングを0秒・0.1秒、光柱を0秒・0.09秒・0.18秒にずらして出すなど、**時間差で複数の層を重ねる演出**に使っている。
待機中も粒のリストには入っているので、発生数の上限チェックの対象になる。

透明度は、発生直後と寿命の終わりに滑らかに変化させる。
`s*s*(3-2*s)`という補間を使い、突然現れたり消えたりする印象を抑える。

### 砂埃：減衰する移動＋拡大＋フェード

生成は`EmitDust`、更新は`Particle::Type::Dust`の分岐。
粒の発生位置・大きさ・寿命に乱数を混ぜ、同じ形の煙が規則的に並ぶのを避ける。

横方向の移動には`(1-exp(-age*4))/4`を使う。
最初は動き、時間がたつと移動速度が弱まる。煙を徐々に膨らませ、少し浮かせながら透明にする。
流体シミュレーションではなく、少数の半透明の板を動かして土煙に見せている。

### 回収：螺旋状の吸い込み

[AdventureParticlePath.h]の`Sample`で、発生位置から道具袋付近へ進ませる。
基本の移動に`sin`・`cos`の回転成分を加え、途中だけ軌跡が膨らむようにする。
膨らみを`sin(t*π)`で0→最大→0にするので、最後は到着先へ収束する。
3本の流れを位相と発生時刻でずらし、星が連なって吸い込まれる形にしている。
シャボン玉が弾ける輪は、別の短命な板パーティクルで外へ広げる。

### 配置：かけらが集まる動き

同じファイルの`Assemble`を使う。
`1-(1-t)^3`で、外側のかけらを最初は速く、到着直前はゆっくり動かす。
高さに弧を足し、四角い光がパーツの周りへ集まるようにしている。
これに二重リング・3層の光柱・12本の光筋を重ねる。

### 取消：一度ほどけてから戻す

`Undo`は、最初の25%の時間を外へほどける動きに使う。
その後、滑らかな補間で主人公へ戻す。粒も到着に向けて小さくする。
回収と色だけを変えるのではなく、動き出しの順序も変えて操作を見分けやすくしている。

### 花火：方向・速度・色・寿命のランダム化

`EmitFireworkBurst`で球状・輪・5枚の花びら状の3パターンを選ぶ。
球状では方位角と高さ方向の成分から飛ぶ方向を作る。
花びら状は`cos(角度*5)`で広がりに強弱を付ける。
一輪につき90〜140粒程度を生成し、速度・寿命・配色・大きさを変える。
更新時には下向きの加速度と縮小を加え、火花が落ちて消える動きにする。

## 4. GPUでどう描いているか

### ビルボード

普通の粒は**カメラの方を向く板ポリゴン**。
`ParticleManager::Update`でビュー行列の逆行列からカメラの向きを取り出し、板の回転に使う。
どの方向から見ても輪郭が見えるため、小さな星・煙・風を少ないポリゴンで表現できる。

### リングと円柱のメッシュ

`CreateRingMesh` / `CreateCylinderMesh`で専用の頂点を作る。
`worldAligned`な粒はビルボードを使わず、設定した向きでワールド空間に置く。
配置時の地面のリングや光柱を、カメラを回しても地面・パーツに固定された見た目にするため。

### インスタンシング描画

同じ形の粒について、頂点は共通で持ち、行列・色・形状番号だけを粒ごとに用意する。
[Particle.VS.hlsl]はインスタンスごとのWVP行列で頂点を変換する。
WVPは、粒の配置・カメラの視点・遠近感をまとめた変換行列。

`ParticleManager::Draw`の`DrawInstanced`で、同じメッシュの複数粒をまとめて描く。
板・通常合成の板・リング・円柱、さらにグループごとに描画を分ける。
「全パーティクルを必ず1回の描画命令で描く」という意味ではない。

### ピクセルシェーダーによる輪郭生成

[Particle.PS.hlsl]ではUV（板の上の位置）を中央基準の-1〜1へ変換し、数式で透明度を決める。

| `shape` | 形 | 計算の考え方 |
|---|---|---|
| 0 | 柔らかい丸 | 中心からの距離で端を透明にする |
| 1 | 光・風の筋 | 横幅と両端をぼかした細長い形 |
| 2 | 小さな雲 | 3個の滑らかな塊を重ねて雲の濃さを作る |
| 3 | 四方向に尖る星 | `abs(x)^0.55 + abs(y)^0.55`を使って輪郭を作る |
| 4 | 砂埃 | 雲と同じ密度を使い、別のしきい値で柔らかく抜く |
| 5 | 四角いかけら | x・yの絶対値の大きい方で四角形の外周を決める |
| 6 | シャボン玉の輪 | 中心から一定距離の部分だけを残す |

星や煙ごとに新しい画像を用意するのではなく、共通の白テクスチャに数式の輪郭と色を掛けている。
リング・円柱のメッシュ側には別の`ParticlePrimitive.PS.hlsl`を使う。

### 合成と深度判定

光る粒は**加算合成**。おおよそ「粒の色×透明度＋背景色」で重ね、光が加わる見た目にする。
煙や赤いインクは**通常のアルファ合成**。「粒の色×透明度＋背景色×(1-透明度)」で混ぜる。
煙まで加算にすると発光して見えるため、用途で描画設定を分けている。

グループ内の粒は視点から奥→手前に並べ替える。
深度テストは有効、深度の書き込みは無効。地形の裏の粒は隠しつつ、透明な板で後続の粒を不自然に遮らないようにする。
ただし、別グループ・別描画種類をまたぐ完全な透明ソートではない。

ゲーム中の光はBloomと組み合わせてにじませる。一方、手帳の粒はポストエフェクト後に描くため、同じBloomが掛かるとは説明しない。

## 5. ハンコ・落下で特に工夫したところ

**ハンコ**は[GameClearScene.cpp]専用の`ParticleManager`を持つ。
背景花火とは分け、本と同じカメラ・深度バッファで、本を描いた後に粒を描く。
`JournalResult::Crossed`で2.20秒の打刻時刻をまたいだときだけ発生させるため、押している間ずっと粒が増えることはない。
ハンコ本体に隠れにくいよう外周から散らし、赤いインクと金色の星で合成方法を分ける。
表紙を閉じ始めたら残りの粒を消す。開き直しても打刻時刻は巻き戻さない。

**落下**の風は[GameOver.cpp]で0.07秒間隔、演出開始から1.6秒未満だけ発生させる。
青白い細長い板を上へ動かし、落ちる体との相対的な動きで速度感を出す。
メニュー・非アクティブ・遷移中は追加しない。処理落ちしても未発生分をまとめて出さない。

体の落下自体はパーティクルではなく、[GameOverTimeline.cpp]の演出制御。
ゲーム中の1フレームの縦移動を秒単位の速度へ直して渡し、落下演出でもその速度から動かす。
敵接触用の0.18秒のヒットストップを落下には使わず、一瞬停止する問題を直している。

**再挑戦**の星は0.25秒待ってから発生させ、暗転が開く前に消え切らないようにしている。

## 6. 雲の描画との違い

周囲を流れる小さな雲は、ビルボードパーティクル。
`Particle.PS.hlsl`の`CloudDensity`で3つの塊を重ね、密度の勾配から疑似的な法線を作って陰影を付ける。
実際に厚みのある雲の体積を積分しているわけではない。

背景の大きな雲海は[CloudSea.PS.hlsl]の**レイマーチング**。
視線上の雲の層を最大24ステップで調べ、3Dノイズの密度から色と透過を積み重ねる。
時間で参照座標をずらして雲を流す。光源方向への追加レイマーチングは行っていない。
この雲海は、今回の小さな星・砂埃の粒とは別の描画方式。

## 7. 負荷対策と、説明するときの注意

- 近くの目標は11×11×6セルを毎秒3回調べ、1回につき最大4地点へ各2粒。全ステージを毎フレーム走査しない。
- 発生時に既定グループの粒数上限4096を確認する。描画も種類ごとのインスタンスバッファ容量で制限する。専用マネージャーや複数グループもあるため、ゲーム全体の粒が常に合計4096以下とは限らない。
- 光・煙・立体メッシュを使い分け、数だけを増やさず、形・軌跡・時間差で華やかさを作る。
- 軌跡の数式は時間刻みが変わっても同じ時刻に同じ位置を求められる。ただし通常ゲームの共通更新には固定1/60秒が残っており、全処理が実時間で完全にFPS非依存という実装ではない。
- エンジンにはCompute Shaderによる別のGPUパーティクル経路もあるが、今回の砂埃・回収・打刻などをGPUで物理更新しているわけではない。
- 「発生通知・在庫・再挑戦のテスト」と「録画による見た目の確認」は別。全演出を手動で通しプレイした、FPS改善を定量確認した、とはしていない。

## 8. 発表で説明するなら

> パーティクルはCPUで生成と寿命・軌跡を管理し、GPUでインスタンシング描画しています。星や煙はビルボードに数式で輪郭を付け、地面のリングと光柱は専用メッシュを使いました。回収は螺旋、配置は収束、取消は一度ほどけてから戻る動きにして、操作ごとの意味が伝わるようにしています。

> ゲーム処理が成功した後のコールバックで発生させ、失敗した配置などで誤って光が出ないようにしました。リザルトのハンコは本専用のカメラと深度判定を使い、音と同じ瞬間にインクと星を出しています。

コードを追うときは、まず発生側の呼出し→`Emit...`→`Update`→`Draw`→シェーダーの順に読む。
実装の対象・形・軌跡・合成方法を一つずつ対応させると説明しやすい。

[ParticleManager.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Engine/Graphics/Particle/ParticleManager.cpp
[ParticleManager.h]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Engine/Graphics/Particle/ParticleManager.h
[GameRuntimeGameplay.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/GameRuntimeGameplay.cpp
[BlockPlacementController.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Block/BlockPlacementController.cpp
[BubblePickupController.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Block/BubblePickupController.cpp
[GameRuntimeLifecycle.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/GameRuntimeLifecycle.cpp
[GameRuntimeScenes.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/GameRuntimeScenes.cpp
[StarGetPresentation.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/StarGetPresentation.cpp
[GameClearScene.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/GameClearScene.cpp
[GameOver.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/GameOver.cpp
[GameOverTimeline.cpp]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/GameOverTimeline.cpp
[AdventureParticlePath.h]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Engine/Graphics/Particle/AdventureParticlePath.h
[Particle.VS.hlsl]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Resources/shaders/hlsl/Particle.VS.hlsl
[Particle.PS.hlsl]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Resources/shaders/hlsl/Particle.PS.hlsl
[CloudSea.PS.hlsl]: C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Resources/shaders/hlsl/CloudSea.PS.hlsl

# SR1 01_01 カプセル化

## 課題の対象

2026年11月25日の講義画像にある「自分のコードの分かりやすいまとまりを、カプセル化の原則に従ってクラス化する」に対応する変更。
作業ブランチは`SR1_Refactoring`。

今回選んだまとまりは、プレイヤーの歩幅を数え、砂埃を発生させる一歩を判定する処理。
加えて、ParticleManagerが内部の粒リストを外へ公開する経路を閉じた。

## 1. 足元の砂埃をFootstepEmitterへまとめた

### 変更前

GameRuntimeのメンバ`stepEffectDistance_`に歩いた距離を保存し、GameRuntimeGameplay.cppで次の処理をしていた。

- 移動距離を加算する。
- 一歩分の距離を超えたかを判定する。
- 移動速度に応じた砂埃の強さを求める。
- 空中になったら累積距離を戻す。

状態の置き場所と判定処理が分かれ、ゲーム全体の制御クラスが歩幅の細かいルールまで持っていた。
なお、元の累積距離もGameRuntimeのprivateメンバだった。元からpublicだった変数をprivateへ直した例ではなく、関連する状態と処理の責務を専用クラスへ集めた例。

### 変更後

[FootstepEmitter.h](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Player/FootstepEmitter.h)を追加した。

| クラス内にまとめたもの | 役割 |
|---|---|
| `accumulatedDistance_` | 次の一歩までに歩いた距離。privateで保持 |
| `kStepDistance` | 一歩分の基準0.65。privateの定数 |
| `kMaximumFrameDistance` | 1フレームで0.4以上動いた場合はワープなどと扱う基準 |
| `Update(before, after, walking)` | 移動を受け取り、必要なときだけ一歩分の発生情報を返す |
| `Reset()` | 歩幅を初期状態へ戻す |

`Update`の戻り値は`std::optional<Emission>`。
まだ一歩分に達していなければ空、一歩分を超えたときだけ移動方向と強さを値で返す。
呼び出し側は内部の累積距離を取得・設定する必要がない。

GameRuntime側では次の順番で接続している。

```cpp
const auto step = footstepEmitter_.Update(
    beforePosition, player_->GetPosition(),
    wasGrounded && player_->IsGrounded() && !player_->IsOnLadder());

if (step) {
    particleManager->EmitDust(position, step->motion, step->strength);
}
```

この抜粋は実装内でParticleManagerが有効な分岐にある部分。
実際の接続先は[GameRuntimeGameplay.cpp](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/GameRuntimeGameplay.cpp)。
GameRuntimeは「歩いた事実を伝える」「発生結果を描画へつなぐ」を担当し、歩幅のルールは専用クラスへ任せる。
FootstepEmitter自身は描画エンジンやプレイヤークラスを参照しないため、DirectXを起動せずに判定を検証できる。

### クラス内で守るルール

1. 接地歩行として渡された移動だけを数える。
2. 累積距離が0.65を超えたときだけ一歩分を返し、その後に距離をリセットする。
3. 空中・梯子・ワープ相当の大移動・不正な移動量では累積距離をリセットする。
4. 砂埃の強さは0.45〜0.9へ制限する。
5. ステージのゲームプレイ開始時にも`Reset()`を呼び、前のステージの歩幅を持ち越さない。

停止中は移動距離が増えないので粒を発生させない。通常の小さな歩行時の発生基準・強さは既存の値を使っている。
ワープや梯子を挟んだ場合に、以前の端数距離を持ち越さないよう整理した。

## 2. ParticleManagerの内部リストを隠した

変更前のParticleManagerは、publicな`ParticleGroup`と、グループへのポインタを返す`FindParticleGroup`を持っていた。
そのため外部コードが粒リストを直接`clear()`したり、描画用の個数・開始位置を書き換えたりできる構造だった。

参照元を検索した結果、現在のプロジェクトにこの公開検索関数の呼び出しはなかった。
そこで次の変更をした。

- `ParticleGroup`をprivateへ移した。
- mutable版・const版の`FindParticleGroup`を公開APIから削除した。
- 発生は`Emit...`、全消去は`ClearParticles`、テクスチャ変更は`SetTexture`という用途別の公開操作を使う。

単にconst参照へ替える方法もあるが、今回はグループの内部構造を外から読む用途自体がなかったため、構造を公開しない形にした。
粒リストを消去する際の描画カウントの初期化なども、マネージャーの中でまとめて行える。

対象は[ParticleManager.h](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Engine/Graphics/Particle/ParticleManager.h)と[ParticleManager.cpp](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Engine/Graphics/Particle/ParticleManager.cpp)。

## 3. 講義の原則との対応

| 原則・注意点 | 今回の対応 |
|---|---|
| データと操作をまとめる | 歩幅の状態・閾値・加算・発生判定・初期化をFootstepEmitterへまとめた |
| 必要なインターフェースだけ公開する | `Update`と`Reset`を公開。累積距離を任意設定するsetterは作らない |
| 内部データの直接変更を防ぐ | 累積距離とParticleGroupをprivateにした |
| グローバル変数の乱用を避ける | 歩幅はGameRuntimeが所有するインスタンスに保持。新しい可変グローバル変数を追加していない |
| friendを安易に使わない | friendを追加せず、テストも公開操作だけで行う |
| 内部構造をそのまま返さない | 粒リストを返すAPIを削除。Emissionは内部への参照を含まない、一歩分の結果のコピー |
| protectedや継承を安易に使わない | 新クラスは継承せず、GameRuntimeのメンバとして所有する |

Emissionの値を書き換えても、クラスの累積距離や次の発生条件は変わらない。
「内部の管理データ」と「呼び出し側へ渡す結果」を分けている。

## 4. 検証

[StageEditorTests.cpp](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Tests/StageEditorTests.cpp)に、公開操作だけを使うテストを追加した。

- 0.2ずつ4回歩くと4回目だけ一歩分が返る。
- 停止しても追加の一歩は返らない。
- 空中・梯子を表す非歩行状態、ワープ相当の距離、無限大の移動量で端数を捨てる。
- Reset後は前の端数を持ち越さない。
- 結果を書き換えても次の発生強度はクラス側のルールで決まる。

CPUテスト、Debug/Releaseのx64ビルド、4ステージの選択・クリア・選択画面復帰の自動確認を通過。
`AuditSourceStyle.py --check`と`git diff --check`も通過。手動での通しプレイは今回実施していない。
この整理によるFPSの向上は測定していない。今回の目的は、状態変更の規則と責務を明確にすること。
その後のプロジェクト全体の点検・追加修正は[全体点検メモ](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/SR1_ENCAPSULATION_REVIEW.md)にまとめた。

## 説明用の短い文章

> 足元の砂埃を出す処理から、歩幅の蓄積と発生判定をFootstepEmitterクラスへまとめました。累積距離と判定基準をprivateにし、外部には移動を知らせるUpdateと初期化のResetだけを公開しています。発生結果は値で返すため、受け取った側から内部状態を変更できません。また、ParticleManagerの内部リストを返す不要なAPIを削除し、粒の操作を用途別のメンバ関数に限定しました。

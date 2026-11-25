# SR1 01_04 デザインパターン

State / Singleton以外で、実際のゲーム処理へ適用した箇所。既存の挙動を保ちながら、変更の責任を分ける。

| パターン | 対象 | 採用した理由 |
| --- | --- | --- |
| Command（今回追加） | `project/Game/Block/PlaceBlocksCommand.*` | 一回の配置計画、変更前後のセル、在庫の消費・返却を同じ操作オブジェクトに保持し、実行と取消を対応させる。 |
| Observer（今回追加） | `project/Engine/Utility/EventSignal.h`、`BlockPlacementController`、`GameRuntimeLifecycle.cpp` | 成功した配置・取消を通知し、UI表示とパーティクルを別々の購読者として更新する。配置処理はUIやParticleManagerを知らなくてよい。 |
| Dirty Flag（今回追加） | `project/Engine/Utility/CameraMatrixCache.h`、`Camera.*` | 姿勢と投影設定を別々に比較し、変更された行列だけ計算する。毎フレームUpdateしても、静止中の逆行列と投影行列の再計算を省く。 |
| Strategy（既存） | `project/Engine/Graphics/Particle/ParticleMotion.*`、`project/Game/Stage/StageCellBehavior.*` | 共通の呼び出し口から種類ごとの更新処理を選ぶ。ステートレスな実装を共有し、粒ごとの仮想オブジェクト生成は増やさない。 |

## 配置から取消まで

`BlockPlacementController::Evaluate`で既存地形・プレイヤー・回転領域・在庫を確認する。`TryPlace`は計画からCommandを作り、Executeが成功した場合だけ直前の操作として所有する。失敗した配置によって以前の取消対象を失わない。

Commandは実行直前にセルを再検証する。在庫は一回だけ消費し、適用途中で失敗したら適用済みセルと在庫を復元する。Undoは変更後の地形とプレイヤーの位置を確認してから元に戻す。動く足場の時間変化は許容するが、その定義が編集されていたら取消を拒否する。回転時には復元情報も回す。

現在必要なのは一種類の配置操作なので、共通Command基底クラスは追加していない。取消は従来どおり直前の一操作。複数段のUndoやRedoを新設したわけではない。

## 成功通知の流れ

配置・取消成功 → EventSignal → エフェクト購読者／HUD購読者。Connectionを破棄すると自動的に解除される。通知元が先に破棄されても弱参照により安全に解除できる。通知中に解除された購読は呼ばず、追加された購読は次回から呼ぶ。メインスレッド用であり、並列通知は扱わない。

ConnectionのprivateコンストラクタをEventSignalだけに公開するfriendは、購読ハンドルの発行を通知元に限定するためのもの。ゲームクラスの内部データを外部から編集するためには使っていない。

## カメラの更新

Camera::Updateは現在の姿勢とレンズ設定をキャッシュへ渡す。エディタが姿勢を直接編集した場合も、更新時の値比較で検知する。ビューの変更は投影行列の再計算を発生させない。計算式とUpdate後に行列を取得する利用方法は維持している。実FPSの改善率は未計測。

SceneFactoryは既存のSimple Factoryであり、仮想生成関数を使うGoFのFactory Methodとは区別する。Singletonの追加は行っていない。

## 確認方法

`project/Tests/StageEditorTests.vcxproj`でCommandの二重実行・二重取消防止、Observerの購読と寿命、行列の再計算回数と従来式との一致を検証する。本体の`CG2_BUILD_VERIFY=1`では配置失敗時の在庫維持、回転、動く足場、取消の17項目を検証する。

### 2026-11-26の結果

- Release / Debug / Development（x64）ビルド成功。v143 / SDK 10.0.22621.0を維持。
- StageEditorTests：新規のCommand / Observer / Dirty Flagと既存回帰テストに成功。
- 本体の配置検証17項目：すべて成功。
- 本体の連続操作：回収10回・配置5回・取消5回、保存読込、在庫不足拒否に成功。
- キャンペーン4ステージ：選択、鍵の解放条件、最終カメラ一致、クリアと選択復帰の自動検証に成功。
- ソース検査：波括弧省略0、説明のない非自明関数0、公開メンバーデータ0、可変参照の公開0。friendは購読ハンドル発行用の1箇所。

上記は自動テストとゲーム本体の検証モードによる確認。人が操作しての画面確認とFPS計測は今回行っていない。
作業ブランチ：RS1_01_04。今回の変更は未コミット。

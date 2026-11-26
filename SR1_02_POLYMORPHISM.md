# SR1 01_02 ポリモーフィズム

## 再提出対応（2026-11-26 / state-fix）

Engine側の適用を確認しやすいよう、実際の利用経路を明記する。

1. `project/Engine/Utility/CameraControlFlow.h`：仮想デストラクタと純粋仮想`Next` / `Update`を持つ`ICameraControlState`。
2. 同`.cpp`：Idle / Orbit / Pan / Suspendedの具象クラスがoverride。Flowが基底参照で呼び、`Camera::UpdateBlenderStyle`から実行される。
3. 既存の`project/Engine/Graphics/Particle/ParticleMotion.h/.cpp`もEngine内の適用箇所。`ParticleManager.cpp`の`SelectParticleMotion(*it).Update(*it, deltaTime)`が基底参照を介した実際の呼び出し。
4. アプリ側には`IPlayerState`と5つの行動状態、既存のBaseScene / StageCellBehaviorがある。
5. `SceneFactory.cpp`内の9つの具体シーンは独立した`.h/.cpp`へ移した。BaseSceneはDraw / DrawBackgroundを維持し、将来UIパスを追加する際は既定の空実装を設け、必要なシーンのみoverrideする方針。

再提出では、これらのファイルを含む最終コードが対象になっていることを確認する。前回の「Engine側が見当たらない」という指摘と、現行のParticleMotionの存在には差があるが、採点時にどのコミットを参照したかは未確認。


確認日：2026年11月25日。作業ブランチ：`SR1_01_02`。
課題画像の「自分のコード内で適用できるところへ適用する」に合わせ、既存シーン管理を確認し、パーティクルとステージの更新へ追加適用した。

## 1. 今回の変更

### パーティクルの動きを基底参照から呼ぶ

変更前はParticleManager::Update内に、演出粒・砂埃・冒険用エフェクト・花火の更新分岐がまとまっていた。
変更後は以下の構成にした。

| 基底クラス | 派生クラス | 担当する振る舞い |
|---|---|---|
| `ParticleMotion` | `LinearMotion` | 速度から座標を更新する。雨などの基本移動にも使う |
| 同上 | `AccentMotion` | ハンコ・きらめき・落下の風など、経過時間による移動と縮小・回転 |
| 同上 | `DustMotion` | 減衰する移動と拡大による砂埃 |
| 同上 | `AdventureMotion` | 回収・取消・組立の軌跡と、リング・柱の拡縮 |
| 同上 | `FireworkMotion` | 火花の移動、下向きの加速度、寿命による縮小 |

[ParticleMotion.h](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Engine/Graphics/Particle/ParticleMotion.h)の共通契約は次の形。

```cpp
class ParticleMotion {
  public:
    virtual ~ParticleMotion() = default;
    virtual void Update(Particle &particle, float deltaTime) const = 0;
};
```

派生側は`Update(...) const override`を実装する。
[ParticleManager.cpp](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Engine/Graphics/Particle/ParticleManager.cpp)の更新ループでは、次のように呼ぶ。

```cpp
SelectParticleMotion(*it).Update(*it, deltaTime);
```

`SelectParticleMotion`の戻り値は`const ParticleMotion&`。
呼び出す側はDustMotionなどの具体的な型を知らず、実際に選ばれた派生クラスのUpdateが実行される。
派生クラスと種類の対応付けは[ParticleMotion.cpp](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Engine/Graphics/Particle/ParticleMotion.cpp)に置いた。

粒そのものの状態は[Particle.h](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Engine/Graphics/Particle/Particle.h)の値データに残す。
振る舞いは状態を持たないconstオブジェクトを共有し、粒ごと・フレームごとのnewは行わない。
寿命、発生待ち、地形との衝突、半透明の並べ替え、インスタンシング描画はParticleManagerに残している。

これは**Strategyパターン**に相当する。Particleが継承する構成ではなく、更新の「やり方」を共通の基底型へ揃えて選択する構成。
`DustMotion is a ParticleMotion`という関係が成立する。

### ステージの振る舞いを基底ポインタから呼ぶ

変更前はStageMap::Update内で、種類ごとのif文に移動や崩壊の処理を書いていた。
変更後は[StageCellBehavior.h](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Stage/StageCellBehavior.h)の基底クラスへ揃えた。

```cpp
class StageCellBehavior {
  public:
    virtual ~StageCellBehavior() = default;
    virtual void Update(MapCell &cell, const StageCellContext &context) const = 0;
};
```

| 派生クラス | 担当する振る舞い |
|---|---|
| `TimedBlockBehavior` | グループと出現順から、実体化する時間帯を判定 |
| `CrumblingFloorBehavior` | 乗った後の崩壊、復活、色と透明度の変化 |
| `MovingFloorBehavior` | 指定方向への往復と、乗っているプレイヤー用の移動差分 |
| `EnemyWalkerBehavior` | 横方向の往復 |
| `EnemyFlyerBehavior` | 上下の往復 |
| `EnemyChaserBehavior` | 範囲内の追尾と、範囲外で出現地点へ戻る動き |

実装は[StageCellBehavior.cpp](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Stage/StageCellBehavior.cpp)。
[StageMap.cpp](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Stage/StageMap.cpp)は、基底ポインタを介して呼び出す。

```cpp
if (const auto *behavior = FindStageCellBehavior(cell.type)) {
    behavior->Update(cell, context);
}
```

`context`は経過時間・フレーム時間・セルの基準座標・プレイヤー座標・時間制ブロックの順序情報を渡す値。
実際のコードでは現在のセル用にStageCellContextを組み立てている。
時間制ブロックのグループ情報はStageMapで一度集計し、参照で共有する。各セルで同じ集計を繰り返さない。
セルの状態はMapCellに保持し、振る舞いオブジェクト自身には保持しない。
更新を必要としない地面や空セルには振る舞いを割り当てず、nullなら呼ばない。

「移動する」「追尾する」ではなく「セルの状態を一フレーム進める」を共通契約にしている。
崩れる床が追尾メソッドを空実装するような設計にはしていない。
`MovingFloorBehavior is a StageCellBehavior`、`EnemyChaserBehavior is a StageCellBehavior`という関係になる。

## 2. 講義の4条件との対応

| 条件 | パーティクル | ステージ |
|---|---|---|
| 基底にvirtual関数 | `ParticleMotion::Update` | `StageCellBehavior::Update` |
| 派生がoverride | 5種類がUpdateを実装 | 6種類がUpdateを実装 |
| 基底の参照／ポインタを通して呼ぶ | `const ParticleMotion&`経由 | `const StageCellBehavior*`経由 |
| virtualデストラクタ | `virtual ~ParticleMotion() = default` | `virtual ~StageCellBehavior() = default` |

「virtualを書いた」だけで終わらず、実際のゲームの更新ループから基底型経由で呼んでいる。
共有実装は通常実行中にdeleteしないが、基底型で所有する使い方にも対応できるようvirtualデストラクタを用意した。

## 3. 既存のシーン管理も確認した

[BaseScene.h](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/BaseScene.h)は、virtualなInitialize・Update・Draw・Finalizeとvirtualデストラクタを持つ。
TitleScene、GameClearScene、SceneFactory内の各シーンはoverrideを実装している。
[SceneManager.cpp](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Game/Scene/SceneManager.cpp)は`std::unique_ptr<BaseScene>`で所有し、`currentScene_->Update(...)`等を呼んでいる。
基底型経由の更新と破棄が成立しており、既存の適用例として使える。

シーンごとの背景モデルを描くDrawBackgroundは、描画パスの任意フックとして既定の空処理を持つ。
射撃や飛行のような一部のシーンだけに意味があるゲーム機能を、全シーンに要求するためのメソッドではない。
シーン固有のプレイヤー操作や手帳の処理は、それぞれの実装へ置いている。

## 4. 分岐やデータとして残したところと理由

課題画像の例外条件に合わせ、振る舞いの差と、数値・表示・形式の差を分けた。

| 対象 | 残した理由 |
|---|---|
| SceneFactoryの種類別生成 | 具象クラスを選ぶ入口。生成後の処理は基底型から呼ぶ。Factoryの対応付けまでvirtualにする必要はない |
| セル種類・保存用のenum | 既存のステージ保存形式と対応する識別値。データ識別と振る舞いの実装を分離する |
| ブロック名、UIの色・表示、カメラプリセット | 主に文字列・色・数値の違い。設定表やデータ駆動の対象であり、一値ごとの派生クラスは作らない |
| Particleのshape・色・寿命 | 描画や演出のパラメータ。星・四角・輪のすべてをC++の派生オブジェクトにしない |
| AdventureMotion内の形状調整・軌跡選択 | 共通する拡縮と、既存の数式ヘルパーの組み合わせ。粒の軌跡・形状のデータを使い、細かな派生クラスの増加を抑える |
| ポストエフェクトのパイプライン選択 | 同じフルスクリーン描画に対するGPUパイプラインと定数バッファの選択。GPU状態の対応表として扱う余地がある。今回の更新オブジェクトの継承には混ぜない |
| シェーダー内のshape分岐、Compute Shader版の粒 | GPU上のデータ駆動処理。C++の仮想関数をGPUへそのまま持ち込む設計にはしない |
| Win32メッセージのswitch | OSのメッセージ番号を扱うプロトコルの入口。今回のゲームオブジェクトのis-a関係とは別の役割 |

例えば「敵も動くからMovingFloorを継承する」とはしていない。
共通の更新契約をStageCellBehaviorとして用意し、それぞれを同じ階層に置くことでis-a関係を守る。

## 5. 元の挙動とカプセル化を維持する方針

- 元の更新式、速度、崩壊・復活時間、追尾範囲、描画方法、保存形式は維持する。
- StageMapのprivateなセル管理を外へ公開し直さない。所有者がセルと更新条件を渡す。
- 粒のデータと、DirectXを必要としない振る舞いを分離し、実装をCPUテストできるようにする。
- 振る舞いを共有しても、別の粒やセルの経過時間・位置は共有しない。
- 継承を増やしたことでFPSが上がったとはしていない。仮想呼び出しは増えるため、性能は測定して判断する。

## 6. 検証

[PolymorphismChecks.h](C:/Users/shigo/Documents/ChatGPT/学校/CG2_01/project/Tests/PolymorphismChecks.h)から実際の振る舞いを基底型経由で呼ぶテストを追加した。

- 基底がabstractで、virtualデストラクタを持つことをコンパイル時に確認。
- 基底のunique_ptrで派生オブジェクトを所有し、派生デストラクタの実行を確認。
- 30・60・144FPS相当で、直線移動・砂埃・演出粒・花火・回収・取消・配置・リング・柱を確認。
- 共有した歩行敵の振る舞いでも、2セルのタイマーや座標が混ざらないことを確認。
- 動く床の位置・差分、飛行敵、追尾と帰還、時間制ブロックの出現順、床の崩壊・復活を確認。
- 既存のステージ、回転、梯子、ドア、配置、保存・読込のCPUテストも継続する。

Release / Debug / Developmentのx64ビルドを通過。
4ステージの選択・クリア・選択画面復帰、43物理ケース、回収10回・配置5回・取消5回のネイティブ検証も通過。
比較プリセットの録画から回収・配置・取消の表示を確認した。
スタイル検査は226ファイルで波括弧不足0、対象の処理コメント不足0。カプセル化の補助チェックも通過。
実行結果のコピーと集計は`generated/sr1-polymorphism/verification-summary.json`に保存した。
自動検証と録画確認は、手動の通しプレイやFPS比較とは分けて扱う。

## 説明用の短い文章

> パーティクルとステージの種類別更新へポリモーフィズムを適用しました。基底クラスに共通のUpdateを定義し、派生側で砂埃・花火・動く床・敵などの振る舞いを実装しています。実際の更新ループは基底の参照やポインタから呼び出します。状態は粒やセルに持たせ、状態を持たない振る舞いを共有することで、大量のオブジェクト生成を避けています。基底にはvirtualデストラクタを用意し、継承は共通の更新契約としてis-a関係が成立する範囲に使いました。


## 再提出修正の自動確認

- Engineの編集カメラ4状態と、Playerの行動5状態：基底経由の呼び出し、状態自身の遷移、ジャンプ入力受付、GUI停止・復帰、インスタンス間の独立性をテスト。
- 実Player::Update / StageMap::Updateの物理43ケース：すべて成功。コヨーテ、梯子、動く足場、各種スイッチ・鍵・再挑戦を含む。開始地点を個別に設定する自動検証であり、人が連続クリアした結果ではない。
- キャンペーン4ステージ：選択、鍵条件、ゴール、シーン側からのクリア要求、選択復帰、タイトル復帰に成功。
- 配置と取消17項目、ステージ移動時の在庫・取消・UI・モードの初期化：成功。
- 波括弧省略0、説明のない非自明関数0。実行結果は`generated/resubmission`に保存。

今回の確認は自動テスト／本体の検証モード。手動プレイは未実施。変更はstate-fix上で未コミットであり、提出サービスへの再提出やタグの変更は行っていない。

Release / Debug（x64）ビルド成功。MSVC v143 / Windows SDK 10.0.22621.0を維持。

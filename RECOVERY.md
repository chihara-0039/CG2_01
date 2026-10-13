# 画面遷移コンテスト向けの素材復旧

元PCの未追跡OBJを取得できないため、元モデルの再現より起動と既存の画面遷移を優先した復旧です。
ユーザーの指定により新設した `scene_transition_recovery` ブランチで変更しています。masterは変更していません。
クラウド側のSDKとツールセットの設定値は変更していません。
学校PCの未コミット変更は受け取れていないため、学校PC側の設定変更はこのブランチには含まれていません。

## 調査と復旧内容

調査時、Resources以下のモデルOBJは0件でした。`.gitignore` の `*.obj` がモデル素材も除外していました。
`Model::LoadObjFile` は存在しないOBJで例外を投げ、最初のblock読み込みで初期化が止まる状態でした。

| 読み込み箇所 | 不足モデルと復旧方法 |
| --- | --- |
| GameRuntimeLifecycle | block、axis、player、skydome、terrain：立方体・軸・簡易人形・球・平面 |
| StageRenderer | wall、ladder、soapBubbles、star、door、switch、CollapsedBlocks、iceBlock、key、midpoint、spike：種別を区別できる簡易形状。block/star等は共有 |
| TitleScene | title、pressSpace：読める文字メッシュ。star・sun・moon：星と球 |
| StageSelect | stageSelect：仮キューブ。別メッシュでTUTORIAL／STAGE 1～5と戻る操作を表示 |
| GameClearScene | C/O/U/R/S/E/L/A：文字メッシュでCOURSE CLEARの既存アニメーションを維持 |
| GameplayUIManager / MapCursor | F、radderUI、choice：F・W/Sの文字と仮カーソル |
| SkinningEditor / 動的読み込み | cube、その他MTLだけ残ったフォルダにも仮OBJを追加。未知の不足OBJは白い単位立方体へフォールバックし、デバッグ出力へパスを記録 |

合計49個のOBJを追跡可能にしました（既存素材のパス42個と新しい案内7個）。
タイトル・開始案内・クリア用の画像は単色で、文字形状自体は元OBJが担っていました。
復旧用タイトルは仮表記 `CG2_01`、開始案内は `SPACE / A : START` です。元のタイトル名・意匠を再現したものではありません。
既存の2D操作案内・チュートリアル・ポーズ画像は引き続き使用します。
MTLに残った元PCの絶対画像パスは、同じフォルダの画像名で解決できるようにしました。

`.h` は宣言、`.cpp` は実装と説明コメントを持ちます。
今回変更したImGuiコードはUSE_IMGUIの条件内に置きました。
既存のフェード（暗転→シーン切替→黒画面保持→明転）とゴール条件は維持しています。

## 操作とWindowsでの提出前チェック

このLinux環境ではMSVC/Windows SDK/DirectX 12のビルド・実行はできません。
以下は**実機で未確認**のチェック手順です。各項目をWindowsで確認してから提出してください。

1. 学校PCの未コミット変更を保全し、必要なSDK・ツールセット設定を復旧ブランチへ引き継いでから、Visual StudioでRelease/x64をビルドする。
   コマンドを使う場合はリポジトリのルートから
   `msbuild project/CG2_01.sln /p:Platform=x64,Configuration=Release`。
   Releaseは提出用の全画面描画、Developmentは既存のエディタ表示です。
2. `generated/outputs/Release/CG2_01.exe`、`dxcompiler.dll`、`dxil.dll` を確認する。
   作業ディレクトリを `project` にして起動し、タイトルの文字・装飾と開始案内を確認する。
   配布時はResources一式も含める。Resourcesを参照できる場所を実行時の作業ディレクトリにする。
3. タイトルの登場演出後にSPACE／パッドA → ステージ選択。
   WASD／十字キーで六面を選び、TUTORIAL／STAGE 1～5の表示と選択ファイルの対応を確認する。
   SPACE／A → ゲーム。各切替でフェードが完了して再操作できることを確認する。
4. 全シャボン玉を回収してゴールに接触 → 取得演出約2.5秒 → クリア画面。
   COURSE CLEARの全文字・花火・既存2D案内・戻る案内の可読性を確認する。
   チュートリアルはシャボン玉0個のためゴール接触でクリア条件を満たす。
5. 戻る操作を確認する。
   - 選択画面：ESC／B → タイトル（以前の終了処理を修正）。
   - ゲーム：ESC／STARTでポーズ、ENTER／Yで選択へ、T／Xでタイトルへ。
   - クリア：文字演出終了後、SPACE／Aでタイトル、ESC／Bで選択へ。
   - タイトル：ESC／Bで終了。
   ブロック配置・インベントリ中のキャンセルは既存の専用操作を優先する。
6. クリアからタイトルへ戻り、スター取得HUDやポーズ状態が残らないことを確認する。
   選択→ゲームをもう一度繰り返し、読み込み例外がないことを確認する。

## この環境での検証

リポジトリのルートで `python3 project/Tests/validate_recovery.py` を実行できます。
Python標準ライブラリとC++20コンパイラのみを使い、生成物は一時ディレクトリへ置きます。

- 49 OBJ／15,766三角形の頂点・UV・法線参照と三角形形式を検査。
- 固定文字列のOBJ読み込み35箇所（26種類）、クリア文字8種類、六つのステージの存在・開始位置・ゴールを確認。
- glTF 4ファイルの外部バッファ・画像が存在すること、MSBuild XMLが解析できることを確認。
- モデルOBJが無視されず、中間OBJが無視されること、仮OBJ再生成の再現性を確認。
- StageSelectionの552範囲ケースと六面の到達性をC++で実行。
- **実際のSceneManager**で10回のシーン入退場・更新・描画呼び出し・二重終了をC++で実行。
  シーン本体とGameRuntimeはテスト用Probeなので、実際の画面描画・入力・ゴール成立・フェードの実行検証ではありません。

Windowsのアプリ全体のビルド、見た目、効果音、実入力、クリア到達、フェード時間とGPU動作は未確認です。
仮モデルのUVや細部は元モデルと異なり、学校PCでサイズ・色・文字の可読性を最終確認する必要があります。

## 元モデルを回収できた場合

仮OBJの先頭には `# Recovery placeholder OBJ; not the original artwork.` を付けています。
同じパスへ回収した元OBJを置けば読み込み箇所は変わりません。実物の形状・向きに合わせた調整は別途必要です。
`python3 project/Tools/recover_models.py --write` は欠けたファイルと上記マーカー付き仮モデルだけを生成し、
元モデルなどマーカーのない既存ファイルを上書きしません。引数なしなら検査だけを行います。
復旧用タイトル・開始案内・クリア文字・選択表示を元の意匠に戻す場合は、それぞれのシーンの向き・表示も合わせて見直してください。

# チュートリアル性能・画質改善（2026-11-10）

対象は通常プレイのチュートリアル。Debugの最適化設定は変更していない。v143 / SDK 10.0.22621.0を維持。

## 変更

- インベントリのカスタムパーツ5枠のSprite再初期化を廃止。テクスチャ変更時だけ差し替える。閉じたUIのボタン更新を省略。
- Spriteの位置・回転が同じなら行列を再計算しない。サイズ・切り抜きが同じなら頂点の再転送を省略。
- ON/OFFブロックの表示は状態変更時だけ更新。崩れる床は対象インスタンスのみ処理。
- 60Hz上限は保持。VSyncとsleepの二重待ちを、高精度waitable timerのペーシングに変更。フレームごとのGPUフェンスは保持。
- tutorial.txtをMiniatureに設定。面取り地形、草・土・砂岩の質感、マットな材質と控えめな色差を適用。追加3D雲はDiorama天候だけに限定。地形座標・当たり判定・ルールは変更していない。

## 自動計測

同じチュートリアル開始位置の静止状態で、ウォームアップ120フレーム後の600フレーム。録画なし、デバッガ未接続のDebug/Release実行。非アクティブ時の更新停止を計測中だけ無効化。CPU欄は前フレーム終了から描画送信前までの壁時計時間で、OSによる停止も含む。

| CSV | 平均FPS | CPU平均ms | GPU平均ms | フレームp95 ms | 最大ms |
|---|---:|---:|---:|---:|---:|
| tutorial-debug-before | 56.57 | 8.435 | 1.796 | 18.675 | 26.60 |
| tutorial-debug-after | 55.70 | 2.508 | 2.298 | 16.844 | 789.18 |
| tutorial-debug-final-1 | 60.00 | 1.104 | 1.513 | 16.878 | 17.42 |
| tutorial-debug-final-2 | 56.63 | 2.450 | 2.481 | 16.886 | 612.41 |
| tutorial-debug-miniature | 60.00 | 0.991 | 1.540 | 16.853 | 18.16 |
| tutorial-release-miniature | 60.00 | 0.579 | 2.564 | 16.847 | 17.74 |

beforeは旧ペーシング・旧ステージ走査・旧インベントリ更新を環境変数で有効化した比較。final-1/2は画質更新前、miniatureは画質更新後。Sprite共通キャッシュは最終ビルドで有効。
一部試行に0.6〜0.8秒のCPU側停止があり、その値も平均に含めた。原因は未特定。Visual StudioのF5デバッガ接続中に30FPSになる現象は未再現で、解消したとは断定できない。
再計測: `powershell -File project/Tools/MeasureTutorial.ps1 -Configuration Debug -Name tutorial-manual`。既存名のCSVは上書きされるため別名を指定すること。

## 確認

- Debug/x64、Release/x64ビルド成功。既存のdinputバージョン通知あり。
- StageEditorTests成功（拡張・保存読込・見た目設定・2列選択）。
- CG2_BUILD_VERIFYの配置・取り消し等14項目すべてtrue。
- Releaseの実録画から地形・質感・開始演出のフレームを視認。操作しての全ギミック確認は今回未実施。
- エンジン録画: MP4/GIF生成成功、232キャプチャ、queueDrops=0。録画設定30FPSはゲームの実行FPSとは別。
- 画像: generated/performance/tutorial-visual/tutorial.png
- 動画/GIF: generated/performance/tutorial-visual/cinematic-arrival.*

作業ブランチ: feature/large-stage-editor-inventory。基点26be64a。既存編集を保持し、コミット・masterマージはしていない。

# 月次提出用の統合状態

PR1_01を取り込んだmaster `de4f131` と、最後に作業した `scene_transition_recovery` の `c10facf` を統合しました。
PR1の霧探索・配置と取り消し・ジオラマ・発表資料と撮影ツールを維持し、周回飛行・雲抜け・ゲームオーバーとチェックポイント再挑戦を追加しています。

## 統合時の調整

- PlayerとGameRuntimeの競合5ファイルを解消し、スキンモデルのGPU更新と表示倍率の復旧を維持。
- 演出中は配置取り消しとスキャン入力を停止。PR1の自動撮影・検証は従来の即リスポーンを維持。
- 原来の6ステージの順序を維持し、PR1のジオラマを含む7ステージ・対応ラベルを検査。
- SDK・プラットフォームツールセット設定は統合前masterから変更していません。

## 確認済み

Linux上の `python3 project/Tests/validate_recovery.py`、FogExplorerTestsのC++テスト、`python3 project/Tools/VerifyDioramaStage.py` が通過。
素材53 OBJ、7ステージ、選択・画面遷移の代替テスト、開始・失敗演出の数値テスト、霧のクールダウン／一時停止／向き、ジオラマの2つの橋とゴール経路を確認しました。

## Windowsで提出前に確認すること

この環境ではWindows/DirectX12のビルド・実行、撮影ツールの実動作は未確認です。

1. Visual StudioでRelease/x64をビルドする。
2. タイトル→選択→ゲーム→クリアと戻る操作、7番目のジオラマを確認。
3. 周回飛行中のプレイヤー・カメラ、敵／トゲ／落下のGAME OVER、再挑戦・選択へ戻る操作を確認。
4. PR1の探索と配置を `project/Tools/StartExploration.ps1`、`project/Tools/StartBuilding.ps1` で確認。
5. リポジトリ直下のPowerShellで提出フォルダを作成する。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\project\Tools\PackageSubmission.ps1
```

デスクトップに `LE3B_20_チハラ_シゴウ_回転の国の探検隊` が作られます。
配布フォルダの `START_GAME.bat` から再度起動を確認してから提出してください。
この統合作業だけでは実行ファイルの生成と提出の完了を意味しません。

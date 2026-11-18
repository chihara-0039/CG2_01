# 空の旅のUI

青・白・金色を使い、丸いパネルと星のアイコン、丸みのある太字へ変更。
タイトルの操作帯、ステージ選択、鍵HUD、プレイ・配置の操作帯、道具袋、ポーズ、再挑戦、星取得、手帳の操作帯を共通テーマへそろえた。
選択中の島には実時間で小さく脈動する星を追加。道具袋のフォーカスは丸い青と金色の枠に変更。
3D手帳の紙面・見取り図・印影・表紙は既存の質感を維持する。

生成: `.venv/Scripts/python.exe project/Tools/BuildUnifiedUI.py`
共通定義: `project/Tools/AdventureTheme.py`

確認:
- Release / Debug / Development x64ビルド成功。
- Release実行録画からステージ選択・プレイ中・道具袋の画面を確認。
- 生成テキストの画像外へのはみ出し検出は0件。
- ソースの波括弧・コメント検査とgit diff --check成功。
- 録画と抽出画像: generated/sky-ui-review。

作業ブランチ clear。コミット・マージ・提出ZIPの更新は行っていない。

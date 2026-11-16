# 自作コードの波括弧・処理コメント整理

対象は project 内の自作C++・ヘッダー・HLSL 212ファイルと制作スクリプト。
externalsなどの外部ライブラリ、自動生成バイナリ、素材、既存提出ZIPは対象外。

- if / else / for / 範囲for / while / do の本体に波括弧を付けた。else ifの連鎖は維持。
- 省略の検出数は整理前296件、整理後0件。
- 条件分岐を1行へ詰めず、4スペースのインデントへ統一。
- 説明のなかった629関数とGPU計算の入口へ、処理の目的を表す日本語コメントを追加。
- 単純な取得関数まで逐語的に説明せず、関数・重要な処理のまとまりを中心に記載。
- 制作スクリプト48ファイルへ実行目的と43件の処理コメントを補足。Pythonの構文木一致、PowerShellの構文エラー0件を確認。
- 古い英字クリア演出を説明していたコメントなど、現在の挙動に合わない記述を修正。

変更前の作業内容は generated/style-review 配下の snapshot-* / comments-* / tools-* に退避。
退避した207ソースと現在のコードを照合し、波括弧・空白・コメントを除くトークンの不一致は0件。
条件、呼び出し、値、文字列、SDK・ツールセットの指定は変更していない。

検証結果:

- Release/x64、Debug/x64、Development/x64 のビルド成功。
- StageEditorTests、FogExplorerTests 成功。
- 実際のPlayer更新を使う27項目、本編4ステージのクリア条件・画面復帰を自動検証して成功。
- Release版を描画付きで起動し、720フレームの自動終了まで正常動作。
- git diff --check 成功。今回の確認は自動検証であり、人の手による全ステージ通しプレイではない。

継続用設定は .clang-format。省略検出は以下で再実行できる。

```powershell
.venv\Scripts\python.exe project\Tools\AuditSourceStyle.py --check --report generated/style-review/final.json
```

監査用のPython環境に tree-sitter 0.26.1 / tree-sitter-cpp 0.23.4、
整形用に clang-format 23.1.5 を導入。ゲームの実行には不要。

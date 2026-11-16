# 新Windows PCでの開発

リポジトリ直下のファイルをダブルクリックして使える。

- `OPEN_PROJECT.cmd`：Visual Studioでソリューションを開く。
- `BUILD_AND_RUN.cmd`：Release/x64ビルドに成功したらゲームを起動する。
- `RUN_GAME.cmd`：最後にビルドしたゲームを起動する。

`project/CG2_01.sln` をVisual Studio 2026 Communityで開き、Release / x64でビルドする。
プロジェクト指定のMSVC v143・Windows SDK 10.0.22621.0を維持する。

PowerShellではリポジトリ直下から次を実行する。

```powershell
& .\project\Tools\BuildRelease.ps1
& .\project\Tools\StartGame.ps1
```

StartGame.ps1は作業ディレクトリをprojectにし、ユーザー環境変数CG2_FFMPEGを読み込む。
ビルド出力は `generated/outputs/Release`。単独配布ではexeの隣にResources、dxcompiler.dll、dxil.dllを揃える。

録画はF8。音声は収録しない。撮影スクリプトでは `-Ffmpeg <ffmpeg.exe>` も指定できる。
新しいターミナルを開くと、設定済みのユーザー環境変数が反映される。

Blender 5.2.3 LTSには同梱 `project/Tools/Blender/level_editor.py` を登録済み。
3D Viewのサイドバー（Nキー）にあるCG2 LevelからレベルJSONを読み書きできる。

制作スクリプト用Pythonは `.venv/Scripts/python.exe`。
Pillow、matplotlib、Shapelyを導入済み。仮想環境のベースはこのPCのCodex同梱Python 3.12.14であり、ベースの削除・移動時には仮想環境の再作成が必要。
日本語フォントはリポジトリの隣の `Tools/Fonts/NotoSansCJKjp-Bold.otf`。
Linuxの既定フォントパスを使うUI生成スクリプトは `--font` と `--title-font` でWindows上のファイルを指定する。
DejaVu Sans Boldはmatplotlibのmpl-data/fonts/ttfに含まれる。

2026-11-03確認：Release/x64ビルド、演出CPUテスト、配置14項目、箱庭ルートの回収・配置・取消・ゴール検証、開始／落下演出の録画・GIF出力に成功。
画面上でタイトルと単独実行版の描画を確認。実キーボード操作による選択・移動・カメラ・インベントリ・再挑戦は別途確認が必要。

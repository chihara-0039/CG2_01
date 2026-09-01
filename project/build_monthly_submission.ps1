param(
    [string]$ProjectRoot = $PSScriptRoot,
    [string]$OutputRoot = "",
    [string]$VideoPath = "C:\Users\shigo\Videos\Captures\自作エンジン 2026-08-31 21-03-18.mp4"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ProjectRoot "monthly_submission_output"
}

$accountName = "LE3C_15_チハラ_シゴウ"
$portfolioName = "${accountName}_ポートフォリオ.pdf"
$videoName = "${accountName}_作品紹介動画.mp4"
$programName = "${accountName}_プログラム説明資料.pdf"
$packageRoot = Join-Path $OutputRoot $accountName
$zipPath = Join-Path $OutputRoot "${accountName}.zip"
$docsSource = Join-Path $ProjectRoot "monthly_submission_docs"
$repositoryRoot = Split-Path $ProjectRoot -Parent
$releaseSource = Join-Path $repositoryRoot "generated\outputs\Release"

$portfolioSource = Join-Path $docsSource $portfolioName
$programSource = Join-Path $docsSource $programName
$runtimeFiles = @("CG2_01.exe", "dxcompiler.dll", "dxil.dll")

$requiredFiles = @($portfolioSource, $programSource, $VideoPath, (Join-Path $releaseSource "Resources"))
$requiredFiles += $runtimeFiles | ForEach-Object { Join-Path $releaseSource $_ }
foreach ($path in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required submission file was not found: $path"
    }
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

$documentsDir = Join-Path $packageRoot "00_提出資料"
$videoDir = Join-Path $packageRoot "01_作品紹介動画"
$runtimeDir = Join-Path $packageRoot "02_実行ファイル"
$sourceDir = Join-Path $packageRoot "03_ソースコード・アセット"
New-Item -ItemType Directory -Force -Path $documentsDir, $videoDir, $runtimeDir, $sourceDir | Out-Null

Copy-Item -LiteralPath $portfolioSource -Destination (Join-Path $documentsDir $portfolioName)
Copy-Item -LiteralPath $programSource -Destination (Join-Path $documentsDir $programName)
Copy-Item -LiteralPath $VideoPath -Destination (Join-Path $videoDir $videoName)

Copy-Item -LiteralPath (Join-Path $releaseSource "CG2_01.exe") -Destination (Join-Path $runtimeDir "3D探索アクション.exe")
Copy-Item -LiteralPath (Join-Path $releaseSource "dxcompiler.dll") -Destination $runtimeDir
Copy-Item -LiteralPath (Join-Path $releaseSource "dxil.dll") -Destination $runtimeDir
Copy-Item -LiteralPath (Join-Path $releaseSource "Resources") -Destination $runtimeDir -Recurse

# ビルド可能なプロジェクト一式をコピーし、生成物・個人設定・提出物の再帰混入を除外する。
$robocopyArgs = @(
    $ProjectRoot,
    $sourceDir,
    "/E", "/R:1", "/W:1",
    "/XD", ".vs", "monthly_submission_docs", "monthly_submission_output", "monthly_submission_ready_20260831", $OutputRoot, "video_check",
    "/XF", "*.user", "*.suo", "*.VC.db", "*.VC.opendb", "*.vsidx", "*.ipch", "imgui.ini"
)
& robocopy @robocopyArgs | Out-Null
if ($LASTEXITCODE -ge 8) {
    throw "Source copy failed. robocopy exit code: $LASTEXITCODE"
}

$readMe = @"
# 3D探索アクション

自作DirectX 12エンジンで制作中の、ステージを探索して星の位置へ到達する3Dアクションゲームです。

## フォルダ構成

- `00_提出資料`: ポートフォリオ、プログラム説明資料
- `01_作品紹介動画`: 現在のゲームプレイと実装機能の紹介動画
- `02_実行ファイル`: ビルド済みRelease版と実行用アセット
- `03_ソースコード・アセット`: ビルド可能なプロジェクト一式

## 基本実装

- プレイヤーの移動、ジャンプ、追従カメラ
- Xboxコントローラー操作
- ステージ探索とスター取得判定
- タイトル、ステージセレクト、ゲーム、クリアのシーン遷移
- スター取得演出、COURSE CLEAR表示、継続する花火演出
- 地形、ライト、スカイボックスの描画
- ステージエディタと外部レベルJSON読込
- Blender編集データの変更検知と再読込

## 発展実装

- 天候プリセット、雨・雪・雷・雲の演出
- 複数光源、プレイヤーライト、雷光
- GPUパーティクル
- GPUスキニング、アニメーション補間
- 骨・ジョイントのデバッグ表示
- 右手武器と左手パーティクルのジョイント追従
- ポストエフェクトとエフェクトエディタ

※BaseScene、SceneManager、SceneFactoryを用いて各シーンのライフサイクルを管理しています。
"@
Set-Content -LiteralPath (Join-Path $packageRoot "ReadMe.md") -Value $readMe -Encoding utf8

Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal

# フォームへ個別提出する3ファイルも、ZIPと同じ出力フォルダに配置する。
Copy-Item -LiteralPath $portfolioSource -Destination (Join-Path $OutputRoot $portfolioName) -Force
Copy-Item -LiteralPath $programSource -Destination (Join-Path $OutputRoot $programName) -Force
Copy-Item -LiteralPath $VideoPath -Destination (Join-Path $OutputRoot $videoName) -Force

Write-Host "Monthly submission created:"
Write-Host "  $zipPath"
Write-Host "  $(Join-Path $OutputRoot $portfolioName)"
Write-Host "  $(Join-Path $OutputRoot $videoName)"
Write-Host "  $(Join-Path $OutputRoot $programName)"

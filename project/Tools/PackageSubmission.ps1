param(
    [string]$RepositoryRoot = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent),
    [string]$DesktopPath = [Environment]::GetFolderPath('Desktop')
)

# 学校PCのSDK設定をそのまま使い、提出用Releaseをビルドしてデスクトップへ配置する。
# 古い提出フォルダは削除せず、日時付きのバックアップとして残す。
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$projectPath = Join-Path $RepositoryRoot 'project'
$solution = Join-Path $projectPath 'CG2_01.sln'
$outputPath = Join-Path $RepositoryRoot 'generated\outputs\Release'
$folderName = 'LE3B_20_チハラ_シゴウ_回転の国の探検隊'

if (-not (Test-Path -LiteralPath $solution -PathType Leaf)) {
    throw "Solution not found: $solution"
}
if ([string]::IsNullOrWhiteSpace($DesktopPath) -or -not (Test-Path -LiteralPath $DesktopPath -PathType Container)) {
    throw 'Desktop folder could not be located. Specify -DesktopPath.'
}
$msbuild = Get-Command 'msbuild.exe' -ErrorAction SilentlyContinue
if (-not $msbuild) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $buildPaths = @(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe')
        if ($buildPaths.Count -gt 0) { $msbuild = Get-Item -LiteralPath $buildPaths[0] }
    }
}
if (-not $msbuild) { throw 'MSBuild not found. Run from Visual Studio Developer PowerShell.' }
$msbuildPath = if ($msbuild.Path) { $msbuild.Path } else { $msbuild.FullName }

# 失敗したビルドの古いexeを提出しないよう、終了コードを確認してからコピーする。
Push-Location $projectPath
try {
    & $msbuildPath $solution '/m:2' '/p:Platform=x64,Configuration=Release' '/nologo'
    if ($LASTEXITCODE -ne 0) { throw "Release build failed (exit $LASTEXITCODE). No submission folder was replaced." }
} finally { Pop-Location }

foreach ($file in @('CG2_01.exe', 'dxcompiler.dll', 'dxil.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $outputPath $file) -PathType Leaf)) {
        throw "Required runtime file missing: $file"
    }
}
$resources = Join-Path $projectPath 'Resources'
if (-not (Test-Path -LiteralPath (Join-Path $resources 'Models\title\title.obj'))) {
    throw 'Recovery title OBJ is missing. Update scene_transition_recovery before packaging.'
}

# まず別の一時フォルダへ完成形を作る。コピー失敗時も以前の提出物を維持する。
$destination = Join-Path $DesktopPath $folderName
$staging = Join-Path $DesktopPath ($folderName + '_準備中_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging | Out-Null
Copy-Item -LiteralPath (Join-Path $outputPath 'CG2_01.exe') -Destination $staging
Get-ChildItem -LiteralPath $outputPath -Filter '*.dll' -File | Copy-Item -Destination $staging
Copy-Item -LiteralPath $resources -Destination (Join-Path $staging 'Resources') -Recurse
Copy-Item -LiteralPath (Join-Path $projectPath 'Tools\TITLE_FONT_LICENSE.txt') -Destination $staging

# 相対Resourcesの読み込み先を確実にする。ASCIIだけのbatなので文字コードに依存しない。
$launcher = "@echo off`r`ncd /d `"%~dp0`"`r`nstart `"`" `"%~dp0CG2_01.exe`"`r`n"
Set-Content -LiteralPath (Join-Path $staging 'START_GAME.bat') -Value $launcher -Encoding ASCII
$readme = @'
回転の国の探検隊
LE3B_20_チハラ_シゴウ

起動：START_GAME.bat をダブルクリックしてください。
Resources と DLL は exe と一緒に配置したままにしてください。
Windows 10/11 x64、DirectX 12対応の描画環境が必要です。
VCRUNTIME140.dll / MSVCP140.dll 等が見つからない場合は、Microsoft公式の
Visual C++ 2015–2022 Redistributable (x64) をインストールしてください。
https://aka.ms/vs/17/release/vc_redist.x64.exe

タイトル：SPACE / パッドAで開始
ステージ選択：WASD / 十字キーで選択、SPACE / Aで決定、ESC / Bで戻る
ゲーム：WASDで移動、SPACEでジャンプ。画面内の操作案内を参照してください。
ポーズ：ESC / START。ENTER / Yで選択へ、T / Xでタイトルへ。
クリア：文字演出後、SPACE / Aでタイトルへ、ESC / Bで選択へ。
タイトルでESC / B：終了

モデルは元PCから回収できなかったため仮モデルを使用しています。
配布フォルダで起動・画面遷移を確認してから提出してください。
'@
Set-Content -LiteralPath (Join-Path $staging 'README.txt') -Value $readme -Encoding UTF8

# コピーしたexeと素材が元データと一致するか確認する（起動検証とは別）。
$sourceFiles = @(Get-Item -LiteralPath (Join-Path $outputPath 'CG2_01.exe')) +
    @(Get-ChildItem -LiteralPath $outputPath -Filter '*.dll' -File)
foreach ($file in $sourceFiles) {
    $copy = Join-Path $staging $file.Name
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $copy).Hash) {
        throw "Runtime copy verification failed: $($file.Name)"
    }
}
foreach ($file in Get-ChildItem -LiteralPath $resources -File -Recurse) {
    $relative = $file.FullName.Substring($resources.Length).TrimStart('\', '/')
    $copy = Join-Path (Join-Path $staging 'Resources') $relative
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $copy).Hash) {
        throw "Resource copy verification failed: $relative"
    }
}
if (Test-Path -LiteralPath $destination) {
    $backup = $destination + '_backup_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,6)
    Move-Item -LiteralPath $destination -Destination $backup
    Write-Host "Previous submission preserved: $backup"
}
Move-Item -LiteralPath $staging -Destination $destination
Write-Host "Submission folder created: $destination"
Write-Host 'Launch START_GAME.bat and verify the scene transitions before submitting.'
Invoke-Item -LiteralPath $destination

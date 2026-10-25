param(
    [string]$Ffmpeg = '',
    [string]$OutputDirectory = '',
    [ValidateSet('all','baseline','bubble','scan')][string]$Mode = 'all',
    [ValidateSet('','footpath','branch','character','route')][string]$Shot = '',
    [switch]$LegacyModel
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$repositoryRoot=Split-Path $projectRoot -Parent
$executable=Join-Path $repositoryRoot 'generated\outputs\Release\CG2_01.exe'
if (!(Test-Path -LiteralPath $executable)) { throw '先にRelease/x64をビルドしてください。' }
if (!$OutputDirectory) { $OutputDirectory=Join-Path $repositoryRoot 'generated\PR1_01\media' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$oldCompare=$env:CG2_COMPARE; $oldOutput=$env:CG2_CAPTURE_DIR; $oldFfmpeg=$env:CG2_FFMPEG
$oldShot=$env:CG2_SHOT; $oldLegacy=$env:CG2_LEGACY_MODEL
try {
    $env:CG2_SHOT=$Shot; $env:CG2_LEGACY_MODEL=if($LegacyModel){'1'}else{'0'}
    $env:CG2_CAPTURE_DIR=[IO.Path]::GetFullPath($OutputDirectory)
    if ($Ffmpeg) { $env:CG2_FFMPEG=[IO.Path]::GetFullPath($Ffmpeg) }
    $modes=if($Mode -eq 'all') { @('baseline','bubble','scan') } else { @($Mode) }
    foreach($captureMode in $modes) {
        $env:CG2_COMPARE=$captureMode
        Write-Output "Recording $captureMode"
        $capture=Start-Process -FilePath $executable -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
        if (!$capture.WaitForExit(120000)) { throw "録画が時間内に終了しませんでした。PID=$($capture.Id)" }
        if($capture.ExitCode -ne 0) { throw "録画失敗: $captureMode / exit $($capture.ExitCode)" }
    }
} finally {
    $env:CG2_COMPARE=$oldCompare; $env:CG2_CAPTURE_DIR=$oldOutput; $env:CG2_FFMPEG=$oldFfmpeg
    $env:CG2_SHOT=$oldShot; $env:CG2_LEGACY_MODEL=$oldLegacy
}
Write-Output $OutputDirectory

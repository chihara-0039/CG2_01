# 回転試験用の起動設定を適用し、開始後に環境変数を復元する。
$ErrorActionPreference='Stop'
$previousRotationTrial=$env:CG2_ROTATION
try {
 $env:CG2_ROTATION='1'
 & (Join-Path $PSScriptRoot 'StartGame.ps1')
} finally { $env:CG2_ROTATION=$previousRotationTrial }

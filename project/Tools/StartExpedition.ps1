# 総合探検ステージ用の起動設定を適用し、終了後に設定を復元する。
$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$previous=$env:CG2_EXPEDITION
try {
 $env:CG2_EXPEDITION='1'
 & (Join-Path $PSScriptRoot 'StartGame.ps1')
} finally {$env:CG2_EXPEDITION=$previous}

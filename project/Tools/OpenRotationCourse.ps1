# 指定した回転ステージをプレイまたは編集用に開き、起動用の環境設定を復元する。
param([ValidateSet('bridge','relay')][string]$Course='bridge',[switch]$Editor)
$ErrorActionPreference='Stop'
$savedCourse=$env:CG2_ROTATION_COURSE
$savedRotation=$env:CG2_ROTATION
try {
    $env:CG2_ROTATION_COURSE=$Course
    if ($Editor) {
        & (Join-Path $PSScriptRoot 'StartEditor.ps1')
    } else {
        $env:CG2_ROTATION='1'
        & (Join-Path $PSScriptRoot 'StartGame.ps1')
    }
} finally {
    $env:CG2_ROTATION_COURSE=$savedCourse
    $env:CG2_ROTATION=$savedRotation
}

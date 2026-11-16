# 編集用構成をビルドし、ステージエディタを開く。
$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild=@(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe') | Select-Object -First 1
if(!$msbuild){throw 'Visual Studio MSBuild was not found.'}
& $msbuild (Join-Path $repoRoot 'project/CG2_01.sln') /m:2 /nr:false /p:Configuration=Development /p:Platform=x64 /nologo
if($LASTEXITCODE -ne 0){throw "Development build failed: $LASTEXITCODE"}
$saved=$env:CG2_OPEN_EDITOR
try {
    $env:CG2_OPEN_EDITOR='1'
    Start-Process -FilePath (Join-Path $repoRoot 'generated/outputs/Development/CG2_01.exe') -WorkingDirectory (Join-Path $repoRoot 'project')
} finally {
    $env:CG2_OPEN_EDITOR=$saved
}

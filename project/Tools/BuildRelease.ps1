# プロジェクト指定の環境を使い、Release/x64をビルドする。
$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild=@(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe') | Select-Object -First 1
if(!$msbuild){throw 'Visual Studio MSBuild was not found.'}
& $msbuild (Join-Path $repoRoot 'project/CG2_01.sln') /m:2 /nr:false /p:Configuration=Release /p:Platform=x64 /nologo
if($LASTEXITCODE -ne 0){throw "Release build failed: $LASTEXITCODE"}

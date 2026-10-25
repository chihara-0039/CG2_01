param([ValidateSet('collision','undo','route')][string]$Shot='collision', [string]$Name='building', [string]$OutputDirectory='', [string]$Executable='', [string]$Ffmpeg='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$repositoryRoot=Split-Path $projectRoot -Parent
if(!$Executable){$Executable=Join-Path $repositoryRoot 'generated\outputs\Release\CG2_01.exe'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $repositoryRoot 'generated\PR1_01\build\media'}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$keys=@('CG2_BUILD_SHOT','CG2_BUILD_NAME','CG2_CAPTURE_DIR','CG2_COMPARE','CG2_FFMPEG','CG2_BUILD_VERIFY','CG2_BUILD_TRIAL','CG2_SHOT','CG2_VERIFY_TRIAL','CG2_TRIAL')
$saved=@{};foreach($key in $keys){$saved[$key]=[Environment]::GetEnvironmentVariable($key)}
try {
 foreach($key in @('CG2_BUILD_VERIFY','CG2_BUILD_TRIAL','CG2_SHOT','CG2_VERIFY_TRIAL','CG2_TRIAL')){[Environment]::SetEnvironmentVariable($key,'')}
 $env:CG2_BUILD_SHOT=$Shot; $env:CG2_BUILD_NAME=$Name; $env:CG2_COMPARE=''
 $env:CG2_CAPTURE_DIR=[IO.Path]::GetFullPath($OutputDirectory)
 if($Ffmpeg){$env:CG2_FFMPEG=[IO.Path]::GetFullPath($Ffmpeg)}
 $process=Start-Process -FilePath $Executable -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
 if(!$process.WaitForExit(120000)){throw "撮影が終了しませんでした。PID=$($process.Id)"}
 if($process.ExitCode -ne 0){throw "撮影に失敗しました: $($process.ExitCode)"}
} finally {foreach($key in $keys){[Environment]::SetEnvironmentVariable($key,$saved[$key])}}

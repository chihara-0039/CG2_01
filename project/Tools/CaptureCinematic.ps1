param([ValidateSet('arrival','fall','hit','title','arrival_transition')][string]$Shot='arrival')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$repoRoot=Split-Path $projectRoot -Parent
$keys=@('CG2_CINEMATIC_SHOT','CG2_CAPTURE_DIR','CG2_FFMPEG','CG2_DIORAMA','CG2_DIORAMA_CAPTURE','CG2_COMPARE','CG2_BUILD_SHOT','CG2_BUILD_TRIAL','CG2_BUILD_VERIFY','CG2_TRIAL','CG2_VERIFY_TRIAL')
$saved=@{};foreach($key in $keys){$saved[$key]=[Environment]::GetEnvironmentVariable($key);[Environment]::SetEnvironmentVariable($key,'')}
try {
 $env:CG2_CINEMATIC_SHOT=$Shot
 $env:CG2_CAPTURE_DIR=Join-Path $repoRoot 'generated/cinematic-review/media'
 $env:CG2_FFMPEG='C:/GAME_SSD/game/Tools/FFmpeg/ffmpeg-9.0.2-full_build/bin/ffmpeg.exe'
 $process=Start-Process -FilePath (Join-Path $repoRoot 'generated/outputs/Release/CG2_01.exe') -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
 if(!$process.WaitForExit(60000)){throw "Cinematic capture timed out: $($process.Id)"}
 if($process.ExitCode -ne 0){throw "Cinematic capture failed: $($process.ExitCode)"}
}finally{foreach($key in $keys){[Environment]::SetEnvironmentVariable($key,$saved[$key])}}

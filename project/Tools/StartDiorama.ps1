param([switch]$Capture,[ValidateSet('route','overview','rotation','undo')][string]$Shot='route')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$repoRoot=Split-Path $projectRoot -Parent
$keys=@('CG2_DIORAMA','CG2_DIORAMA_CAPTURE','CG2_DIORAMA_SHOT','CG2_CAPTURE_DIR','CG2_FFMPEG','CG2_BUILD_TRIAL','CG2_BUILD_VERIFY','CG2_BUILD_SHOT','CG2_COMPARE','CG2_TRIAL','CG2_VERIFY_TRIAL')
$saved=@{};foreach($key in $keys){$saved[$key]=[Environment]::GetEnvironmentVariable($key)}
try {
 foreach($key in $keys){[Environment]::SetEnvironmentVariable($key,'')}
 $env:CG2_DIORAMA_SHOT=$Shot; $env:CG2_DIORAMA='1';$env:CG2_DIORAMA_CAPTURE=if($Capture){'1'}else{''}
 $env:CG2_CAPTURE_DIR=Join-Path $repoRoot 'generated/PR1_01/diorama/media'
 $env:CG2_FFMPEG='C:/GAME_SSD/game/Tools/FFmpeg/ffmpeg-9.0.2-full_build/bin/ffmpeg.exe'
 $exe=Join-Path $repoRoot 'generated/outputs/Release/CG2_01.exe'
 if($Capture){
  $process=Start-Process -FilePath $exe -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
  if(!$process.WaitForExit(120000)){throw "撮影が終了しませんでした。PID=$($process.Id)"}
  if($process.ExitCode -ne 0){throw "箱庭の統合確認に失敗しました。exit=$($process.ExitCode)"}
  $clip=Get-ChildItem -LiteralPath $env:CG2_CAPTURE_DIR -Filter '*.mp4' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  $movie=Join-Path $repoRoot "generated/PR1_01/diorama/diorama-$Shot.mp4"
  $gif=Join-Path $repoRoot "generated/PR1_01/diorama/diorama-$Shot.gif"
  $framing=if($Shot -eq 'route'){'crop=600:338:180:110,scale=960:540'}else{'scale=960:540'}
  & $env:CG2_FFMPEG -hide_banner -loglevel error -y -i $clip.FullName -vf $framing -an -c:v libx264 -crf 19 $movie
  if($LASTEXITCODE -ne 0){throw '発表用動画の切り抜きに失敗しました'}
  & $env:CG2_FFMPEG -hide_banner -loglevel error -y -i $movie -filter_complex 'fps=12,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer' $gif
  if($LASTEXITCODE -ne 0){throw '発表用GIFの生成に失敗しました'}
 }else{Start-Process -FilePath $exe -WorkingDirectory $projectRoot}
}finally{foreach($key in $keys){[Environment]::SetEnvironmentVariable($key,$saved[$key])}}

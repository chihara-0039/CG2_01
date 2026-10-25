param([switch]$Verify)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$exe=Join-Path (Split-Path $projectRoot -Parent) 'generated\outputs\Release\CG2_01.exe'
$keys=@('CG2_BUILD_TRIAL','CG2_BUILD_VERIFY','CG2_BUILD_SHOT','CG2_COMPARE')
$saved=@{};foreach($key in $keys){$saved[$key]=[Environment]::GetEnvironmentVariable($key)}
try {
 $env:CG2_BUILD_TRIAL='1';$env:CG2_BUILD_VERIFY=if($Verify){'1'}else{''};$env:CG2_BUILD_SHOT='';$env:CG2_COMPARE=''
 if($Verify){
  $process=Start-Process -FilePath $exe -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
  if(!$process.WaitForExit(60000)){throw "検証が終了しませんでした。PID=$($process.Id)"}
  if($process.ExitCode -ne 0){throw "配置の統合検証に失敗しました。exit=$($process.ExitCode)"}
 }else{Start-Process -FilePath $exe -WorkingDirectory $projectRoot}
}finally{foreach($key in $keys){[Environment]::SetEnvironmentVariable($key,$saved[$key])}}

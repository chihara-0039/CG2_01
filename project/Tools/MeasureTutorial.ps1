# チュートリアルを起動し、フレーム時間の計測結果を収集する。
param([ValidateSet('Debug','Release')][string]$Configuration='Debug',[string]$Name='tutorial-manual')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$keys=@('CG2_PERF_LOG','CG2_PERF_SCENE','CG2_DIORAMA','CG2_DIORAMA_CAPTURE','CG2_CINEMATIC_SHOT','CG2_LEGACY_FRAME_PACING','CG2_PERF_LEGACY_STAGE_SCAN','CG2_PERF_LEGACY_INVENTORY')
$saved=@{}
try {
 foreach($key in $keys){$saved[$key]=[Environment]::GetEnvironmentVariable($key);[Environment]::SetEnvironmentVariable($key,'')}
 $env:CG2_PERF_LOG=Join-Path $repoRoot "generated/performance/$Name.csv"
 $env:CG2_PERF_SCENE='tutorial'
 $exe=Join-Path $repoRoot "generated/outputs/$Configuration/CG2_01.exe"
 $p=Start-Process $exe -WorkingDirectory (Join-Path $repoRoot 'project') -PassThru
 if(!$p.WaitForExit(60000)){throw "計測が継続中です。PID=$($p.Id)"}
 if($p.ExitCode -ne 0){throw "計測失敗: $($p.ExitCode)"}
 $rows=Import-Csv -LiteralPath $env:CG2_PERF_LOG
 $mean=($rows|ForEach-Object{[double]$_.frame_ms}|Measure-Object -Average).Average
 Write-Output ('Average FPS: {0:N2}; CSV: {1}' -f (1000/$mean),$env:CG2_PERF_LOG)
} finally {foreach($key in $keys){[Environment]::SetEnvironmentVariable($key,$saved[$key])}}

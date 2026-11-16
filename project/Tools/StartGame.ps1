# 必要なプレビューを更新し、Release版をproject作業ディレクトリで起動する。
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$repoRoot=Split-Path $projectRoot -Parent
if(!$env:CG2_FFMPEG){$env:CG2_FFMPEG=[Environment]::GetEnvironmentVariable('CG2_FFMPEG','User')}
$exe=Join-Path $repoRoot 'generated/outputs/Release/CG2_01.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Build Release/x64 first using BuildRelease.ps1.'}
# Refresh selection dioramas after editing a curated map; never regenerate the map itself.
$courseNames=@('beginnerTrail.txt','mechanismGardens.txt','skylineRelay.txt','expeditionGrandTour.txt')
$refreshPreview=$false
for ($courseIndex=0;$courseIndex -lt $courseNames.Count;$courseIndex++) {
 $courseSource=Join-Path $projectRoot ('Resources/Stages/'+$courseNames[$courseIndex])
 $coursePreview=Join-Path $projectRoot ('Resources/Models/StagePreviews/course-'+$courseIndex+'.obj')
 if (!(Test-Path -LiteralPath $coursePreview) -or (Get-Item -LiteralPath $courseSource).LastWriteTimeUtc -gt (Get-Item -LiteralPath $coursePreview).LastWriteTimeUtc) { $refreshPreview=$true }
}
if ($refreshPreview) {
 $previewPython=Join-Path $repoRoot '.venv/Scripts/python.exe'
 & $previewPython (Join-Path $PSScriptRoot 'BuildStagePreviews.py')
 if ($LASTEXITCODE -ne 0) { throw 'Stage preview generation failed.' }
 & $previewPython (Join-Path $PSScriptRoot 'BuildUnifiedUI.py')
 if ($LASTEXITCODE -ne 0) { throw 'Adventure UI generation failed.' }
}
Start-Process -FilePath $exe -WorkingDirectory $projectRoot

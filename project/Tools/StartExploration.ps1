param([switch]$Verify)
$projectRoot=Split-Path $PSScriptRoot -Parent
$executable=Join-Path (Split-Path $projectRoot -Parent) 'generated\outputs\Release\CG2_01.exe'
if (!(Test-Path -LiteralPath $executable)) { throw '先にRelease/x64をビルドしてください。' }
$oldTrial=$env:CG2_TRIAL; $oldVerify=$env:CG2_VERIFY_TRIAL; $oldCompare=$env:CG2_COMPARE
try {
    $env:CG2_COMPARE=''; $env:CG2_TRIAL='1'
    $env:CG2_VERIFY_TRIAL=if($Verify){'1'}else{''}
    if ($Verify) {
        $test=Start-Process -FilePath $executable -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
        if (!$test.WaitForExit(60000)) { throw "検証が時間内に終了しませんでした。PID=$($test.Id)" }
        if ($test.ExitCode -ne 0) { throw "探索ステージの統合検証失敗: exit $($test.ExitCode)" }
    } else {
        Start-Process -FilePath $executable -WorkingDirectory $projectRoot
    }
} finally { $env:CG2_TRIAL=$oldTrial; $env:CG2_VERIFY_TRIAL=$oldVerify; $env:CG2_COMPARE=$oldCompare }

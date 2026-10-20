"""Build an offline pack of all recovered OBJ models and their material textures."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile

REPOSITORY = Path(__file__).resolve().parents[2]
RESOURCES = REPOSITORY / "project/Resources"

INSTALLER = r'''param([Parameter(Mandatory=$true)][string]$RepositoryRoot)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
if (-not (Test-Path -LiteralPath (Join-Path $RepositoryRoot 'project\CG2_01.sln'))) {
    throw 'Select the CG2_01 repository root, not its project subfolder.'
}
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
# Verify the entire archive before adding anything. Existing files are never overwritten.
foreach ($entry in $manifest.files) {
    $source = Join-Path $PSScriptRoot $entry.path
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Archive integrity check failed: $($entry.path)"
    }
}
$added = 0
$kept = 0
foreach ($entry in $manifest.files) {
    $source = Join-Path $PSScriptRoot $entry.path
    $destination = Join-Path $RepositoryRoot $entry.path
    if (Test-Path -LiteralPath $destination) { $kept++; continue }
    New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Copy verification failed: $($entry.path)"
    }
    Write-Host "Added: $($entry.path)"
    $added++
}
$missing = @($manifest.files | Where-Object {
    $_.path.EndsWith('.obj') -and -not (Test-Path -LiteralPath (Join-Path $RepositoryRoot $_.path))
})
if ($missing.Count -gt 0) { throw 'Some OBJ models are still missing.' }
Write-Host "Finished. Added: $added. Existing files preserved: $kept. All 49 model paths exist."
Write-Host 'No Git, source, SDK or toolset settings were changed. Rebuild and test in Visual Studio.'
'''

README = r"""CG2_01 モデル復旧パック

以前の画像6枚だけのZIPとは別です。復旧用OBJ49個とMTL・参照テクスチャを収録しています。
元PCの未追跡OBJそのものを回収したものではなく、立方体・星・球・はしご・扉・鍵などの
代替形状と、日本語タイトル・開始案内・クリア文字のメッシュです。

1. ZIP全体を新しいフォルダへ展開します（ZIP内から直接実行しないでください）。
2. Visual Studioでゲームを停止します。
3. 展開したフォルダのアドレスバーへ powershell と入力してEnterを押します。
4. 次を実行します。RepositoryRootにはCG2_01リポジトリの実際の場所を指定します。

powershell -NoProfile -ExecutionPolicy Bypass -File .\InstallMissingModels.ps1 -RepositoryRoot "C:\Users\GSUser\source\repos\CG2_01"

不足ファイルだけを追加し、既存ファイルは一切上書きしません。
Git操作、コード、SDK・ツールセット設定の変更は行いません。
既存の素材が壊れている場合も保全するため、その場合は別途調査が必要です。
完了後はVisual Studioでビルド・起動確認してください。Windowsでの実表示は未確認です。
Git側ではscene_transition_recoveryにOBJを追跡済みで、モデルOBJのignoreも修正済みです。
"""


def build(output):
    models = sorted(RESOURCES.rglob("*.obj"))
    assert len(models) == 49, "Review the installer text if the model inventory changes"
    materials = sorted(RESOURCES.rglob("*.mtl"))
    payload = set(models + materials)
    for material in materials:
        for line in material.read_text(encoding="utf-8-sig").splitlines():
            words = line.strip().split(None, 1)
            if len(words) != 2 or words[0] != "map_Kd":
                continue
            name = words[1].strip().strip('"').replace("\\", "/")
            texture = material.parent / name
            if not texture.is_file():
                texture = material.parent / Path(name).name
            assert texture.is_file(), (material, name)
            payload.add(texture)
    payload.add(REPOSITORY / "project/Tools/TITLE_FONT_LICENSE.txt")
    entries = []
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(payload):
            relative = path.relative_to(REPOSITORY).as_posix()
            entries.append({"path": relative, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
            archive.write(path, relative)
        archive.writestr("manifest.json", json.dumps({"models": len(models), "files": entries}, indent=2))
        archive.writestr("InstallMissingModels.ps1", INSTALLER.encode("utf-8-sig"))
        archive.writestr("README.txt", README.encode("utf-8-sig"))
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        for entry in entries:
            assert hashlib.sha256(archive.read(entry["path"])).hexdigest() == entry["sha256"]
    print(f"Verified repair pack: {len(models)} OBJ, {len(materials)} MTL, {len(payload)} total payload files")
    print(f"{output} ({output.stat().st_size:,} bytes)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    build(parser.parse_args().output)

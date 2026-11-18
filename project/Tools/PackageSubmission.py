# 実行に必要なファイルを集め、過去の提出物を残して新しいZIPを作る。
"""Build a fresh, runnable submission folder and ZIP without changing earlier submissions."""
from pathlib import Path
from datetime import datetime
import shutil,zipfile,hashlib,json
repo=Path(__file__).resolve().parents[2]
name='LE3B_20_チハラ_シゴウ_回転の国の探検隊'
output=repo/'generated'/'submissions'/datetime.now().strftime('%Y%m%d-%H%M%S')
folder=output/name
folder.mkdir(parents=True,exist_ok=False)
shutil.copy2(repo/'generated/outputs/Release/CG2_01.exe',folder/'回転の国の探検隊.exe')
for dll in ('dxcompiler.dll','dxil.dll'):
    shutil.copy2(repo/'generated/outputs/Release'/dll,folder/dll)
crt=Path('C:/Program Files/Microsoft Visual Studio/18/Community/VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT')
if not (crt/'vcruntime140.dll').is_file() or not (crt/'msvcp140.dll').is_file():
    raise FileNotFoundError(f'Required v143 runtime not found: {crt}')
for dll in crt.glob('*.dll'):
    shutil.copy2(dll,folder/dll.name)
shutil.copytree(repo/'project/Resources',folder/'Resources',ignore=shutil.ignore_patterns('*.blend','*.blend1','*.fbx','Compiled','__pycache__'))
archive=output/(name+'.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for p in sorted(folder.rglob('*')):
        if p.is_file(): z.write(p,p.relative_to(output))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
manifest={'zip':str(archive),'folder':str(folder),'bytes':archive.stat().st_size,
          'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'files':sum(p.is_file() for p in folder.rglob('*'))}
(output/'package-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(manifest,ensure_ascii=False,indent=2))

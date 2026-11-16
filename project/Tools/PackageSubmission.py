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
for dll in crt.glob('*.dll'):
    shutil.copy2(dll,folder/dll.name)
shutil.copytree(repo/'project/Resources',folder/'Resources',ignore=shutil.ignore_patterns('*.blend','*.blend1','*.fbx','Compiled','__pycache__'))
(folder/'ゲーム起動.cmd').write_bytes('@echo off\r\npushd "%~dp0"\r\nstart "" "回転の国の探検隊.exe"\r\npopd\r\n'.encode('cp932'))
readme='''回転の国の探検隊
LE3B 20 チハラ シゴウ

【起動】
ZIPを右クリック→「すべて展開」してから「ゲーム起動.cmd」を開いてください。
実行ファイル・Resourcesフォルダ・DLLは同じフォルダに置いてください。
Visual StudioやBlenderのインストールは不要です。
Windows 10/11 64bit、DirectX 12対応GPUで実行してください。

【遊び方】
空に浮かぶ島を見回し、すべての鍵を集めてスターを目指します。
シャボン玉の中のパーツを回収し、道具袋から取り出して足場にできます。
本編は4ステージです。

【キーボード】
タイトル・選択の決定：Enter / Space
移動：WASD　　視点：矢印キー　　ジャンプ：Space
扉・スイッチの操作：F
道具袋：B　　パーツの直接選択：1～0
配置位置：WASD　　配置の高さ：Q/E　　パーツ回転：R
配置：左クリック / Enter　　配置中の視点：右ドラッグ
直前の配置を取り消す：Z　　配置から戻る：B
画面内の操作説明：F1　　ポーズ・戻る：Esc

【クリア後】
手帳にハンコを押す演出の後、Enter / Spaceでステージ選択、Escでタイトルへ。

制作ツール・ソースコード・デバッグ情報・録画用FFmpegは含みません。
収録画像・音声・モデル等は本作品の実行に必要なデータです。
'''
(folder/'はじめに.txt').write_text(readme,encoding='utf-8-sig')
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

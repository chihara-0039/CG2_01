# 本編マップの形状から、選択画面に表示する縮小模型を生成する。
"""Read the four curated maps; generate small, coloured selection dioramas and UI."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import recover_models

root=Path(__file__).resolve().parents[1]
names=['beginnerTrail.txt','mechanismGardens.txt','skylineRelay.txt','expeditionGrandTour.txt']
out=root/'Resources/Models/StagePreviews';out.mkdir(exist_ok=True)
colors=[(0,0,0),(113,163,79),(213,213,188),(161,102,57),(255,205,75),(148,231,255),
        (255,222,65),(77,215,190),(164,100,221),(255,168,61),(255,205,78),(160,106,64),
        (126,224,246),(119,165,210),(255,212,67),(208,153,44),(226,74,83),
        (201,102,136),(165,123,205),(220,122,72),(255,205,78),(106,231,189),
        (185,175,243),(232,113,75),(236,91,84),(78,141,231),(133,223,223)]
atlas=Image.new('RGB',(27*8,3*8));d=ImageDraw.Draw(atlas)
for i,color in enumerate(colors):
    for row,factor in enumerate((.65,.84,1.0)):
        d.rectangle((i*8,row*8,i*8+7,row*8+7),fill=tuple(int(c*factor) for c in color))
atlas.save(out/'palette.png')
(out/'preview.mtl').write_text('newmtl preview\nKd 1 1 1\nmap_Kd palette.png\n')
for index,name in enumerate(names):
    cells=[]
    for line in (root/'Resources/Stages'/name).read_text(encoding='utf-8-sig').splitlines()[1:]:
        tokens=line.split()
        if len(tokens)<7 or not all(v.lstrip('-').isdigit() for v in tokens[:4]): continue
        x,y,z,kind=map(int,tokens[:4])
        if kind and kind!=7: cells.append((x,y,z,kind))
    low=[min(c[a] for c in cells)-.5 for a in range(3)]
    high=[max(c[a] for c in cells)+.5 for a in range(3)]
    centre=[(a+b)/2 for a,b in zip(low,high)]
    scale=11/max(b-a for a,b in zip(low,high))
    mesh=recover_models.Mesh();mesh.lines += ['mtllib preview.mtl','usemtl preview']
    for x,y,z,kind in cells:
        first=len(mesh.lines)
        size=.42 if kind==5 else .65 if kind in (6,14,17,18,19,21,23) else 1
        mesh.box(((x-centre[0])*scale,(y-centre[1])*scale,-(z-centre[2])*scale),
                 (size*scale,size*scale,size*scale))
        face=0;vertex=0
        for n in range(first,len(mesh.lines)):
            if mesh.lines[n].startswith('vt '):
                row=2 if face==4 else 0 if face in (0,2,5) else 1
                mesh.lines[n]=f'vt {(kind+.5)/27} {1-(row+.5)/3}'
                vertex+=1
                if vertex%6==0: face+=1
    (out/f'course-{index}.obj').write_text('\n'.join(mesh.lines)+'\n')
    print(name,len(cells),'preview cells')

# A deterministic, quiet star field behind the miniature stage.
import random
rng=random.Random(39)
mesh=recover_models.Mesh()
for i in range(100):
    size=rng.uniform(.025,.07)
    mesh.box((rng.uniform(-17,17),rng.uniform(-9,9),-10),(size,size,.02))
(out/'stars.obj').write_text('\n'.join(mesh.lines)+'\n')

ui=root/'Resources/UI/campaign'
font=lambda size: ImageFont.truetype('C:/Windows/Fonts/YuGothB.ttc',size)
def strip(name,text,width=1150,height=48,size=23):
    im=Image.new('RGBA',(width,height),(10,20,38,232));draw=ImageDraw.Draw(im)
    draw.text((16,8),text,font=font(size),fill=(231,238,249));im.save(ui/name)
strip('select.png','← → ステージ選択　 Space / Enter：出発　 右ドラッグ：見回す　 Esc：戻る')
strip('select-pad.png','十字キー：ステージ選択　 A：出発　 右スティック：見回す　 B：戻る')
strip('play-minimal.png','WASD 移動　 矢印 視点　 B 所持品　 Esc ポーズ　 F1 操作一覧',1000,40,21)
strip('play-pad-minimal.png','左スティック 移動　 右スティック 視点　 LB 所持品　 START ポーズ',1000,40,21)
strip('place-minimal.png','WASD 位置　 Q/E 高さ　 R 回転　 左クリック 配置　 右ドラッグ 視点　 Z 取り消し　 B 戻る',1200,44,21)
strip('place-pad-minimal.png','左スティック 位置　 LB/RB 高さ　 X 回転　 A 配置　 右スティック 視点　 B 戻る',1200,44,21)

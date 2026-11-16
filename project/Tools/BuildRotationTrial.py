# 独立した回転試験ステージと操作案内を生成する。
"""Generate the isolated rotation trial. Does not change the curated campaign."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import recover_models
root=Path(__file__).resolve().parents[1]
parts=[line for line in (root/'Resources/Stages/expeditionGrandTour.txt').read_text().splitlines()
       if line.startswith(('PART ','PARTCELL '))]
cells={}
def put(x,y,z,t,v=0): cells[x,y,z]=(t,v)
def island(x0,x1,z0,z1):
    for x in range(x0,x1+1):
        for z in range(z0,z1+1):
            for y in (0,1):put(x,y,z,1)
island(3,9,9,15);island(21,25,9,15);island(13,19,2,6)
island(11,19,11,13);island(14,16,14,16)
put(5,2,12,7);put(8,2,12,23);put(7,2,12,21)
put(5,2,10,5,2+(1<<16));put(15,2,12,5,2+(2<<16));put(16,2,4,5,2+(3<<16))
put(23,2,12,6)
for x in range(3,10):put(x,1,12,2)
for x in range(11,20):put(x,1,12,2)
for x in range(21,26):put(x,1,12,2)
for z in range(2,7):put(16,1,z,2)
lines=['28 8 26','VISUAL_STYLE Miniature','ROTATION 11 8 9 8 2 12','PRESET "Sunny (Default)"',
       'ENVIRONMENT 0.48 0.67 0.8 1 0.85 1 0.96 0.88 0.5 -1 0.35']+parts
for (x,y,z),(t,v) in sorted(cells.items()):lines.append(f'{x} {y} {z} {t} 0 0 {v}')
(root/'Resources/Stages/rotationTrial.txt').write_text('\n'.join(lines)+'\n')
im=Image.new('RGBA',(1000,110),(14,30,46,235));d=ImageDraw.Draw(im)
font=lambda n:ImageFont.truetype('C:/Windows/Fonts/YuGothB.ttc',n)
d.text((16,5),'回転する島：スタートのスイッチ付近で F ／ パッドY',font=font(25),fill=(255,222,145))
d.text((16,43),'Shift+F ／ LB+Y：逆回転　 3個回収して東のゴールへ。北の島への道を回転で作ろう。',font=font(19),fill='white')
d.text((16,76),'配置は島をまたがず置けます。島の上のパーツは一緒に回り、Zで取り消せます。',font=font(19),fill='white')
im.save(root/'Resources/UI/campaign/rotation-trial.png')
# Original small explorer marker used on the sky selection screen.
palette=Image.new('RGB',(32,8));pd=ImageDraw.Draw(palette)
for i,color in enumerate([(154,102,54),(241,201,155),(53,111,130),(55,41,32)]):pd.rectangle((i*8,0,i*8+7,7),fill=color)
palette.save(root/'Resources/Models/StagePreviews/traveler.png')
(root/'Resources/Models/StagePreviews/traveler.mtl').write_text('newmtl traveler\nKd 1 1 1\nmap_Kd traveler.png\n')
mesh=recover_models.Mesh();mesh.lines+=['mtllib traveler.mtl','usemtl traveler']
def box(at,size,color):
    first=len(mesh.lines);mesh.box(at,size)
    for i in range(first,len(mesh.lines)):
        if mesh.lines[i].startswith('vt '):mesh.lines[i]=f'vt {(color+.5)/4} 0.5'
box((0,.1,0),(.55,.65,.35),2);box((0,.68,0),(.42,.42,.4),1)
box((0,.95,0),(.85,.08,.7),0);box((0,1.08,0),(.5,.25,.45),0)
box((0,.16,.3),(.4,.48,.25),0)
for x in (-.16,.16):box((x,-.44,0),(.17,.5,.2),3)
for x in (-.35,.35):box((x,.15,0),(.14,.45,.17),1)
(root/'Resources/Models/StagePreviews/traveler.obj').write_text('\n'.join(mesh.lines)+'\n')
print('rotationTrial.txt:',len(cells),'cells; three pickups; no campaign entries changed')

mesh=recover_models.Mesh();mesh.box((0,0,0),(6.4,7.7,.05))
(root/'Resources/Models/StagePreviews/menu-panel.obj').write_text('\n'.join(mesh.lines)+'\n')
mini=Image.new('RGBA',(1000,40),(14,30,46,235));d=ImageDraw.Draw(mini)
d.text((12,7),'F / Y 台座で90°回転　 Shift+F / LB+Y 逆回転　 B 所持品　 F1 説明',font=font(21),fill='white')
mini.save(root/'Resources/UI/campaign/rotation-minimal.png')

import AddCourseKeys
AddCourseKeys.add("rotationTrial.txt")

import BuildAdventureUI

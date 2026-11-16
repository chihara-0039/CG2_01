# 回転と配置を試す2ステージを生成し、既存の編集済みマップは保持する。
"""Create two editable rotation courses; never overwrites an existing course."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
parts=[s for s in (root/'Resources/Stages/expeditionGrandTour.txt').read_text().splitlines() if s.startswith(('PART ','PARTCELL '))]

# 固定足場と回転領域を組み、同名の既存マップがなければ保存する。
def build(name,relay):
    cells={}
    def put(x,y,z,t,v=0): cells[x,y,z]=(t,v)
    def floor(x0,x1,z0,z1):
        for x in range(x0,x1+1):
            for z in range(z0,z1+1):
                put(x,0,z,1);put(x,1,z,1)
    regions=[]
    if relay:
        floor(3,9,9,15)
        for x in (11,25,39):
            regions.append((x,8,9,x-3,2,12))
            floor(x+3,x+5,8,16) # Each north-south bridge must be turned east-west.
            put(x+4,2,12,14,1)
            floor(x+10,x+12,9,15)
            put(x-3,2,12,2) # Brass control pedestal, safe fixed ground.
            put(x-4,2,10,5,2+(2<<16))
        floor(49,55,9,15)
        put(22,2,14,21);put(36,2,14,21)
        put(53,2,12,6)
        width=59
    else:
        regions=[(11,8,9,8,2,12,16,2,4)]
        floor(3,9,9,15);floor(21,27,9,15);floor(13,19,2,6)
        floor(11,19,11,13);floor(14,16,14,16)
        # Side landing lets the player turn the island back after visiting the north key.
        put(8,2,12,2);put(16,2,4,2)
        put(5,2,10,14,1);put(15,2,12,14,1);put(17,2,4,14,1)
        put(7,2,14,5,2+(2<<16));put(23,2,10,5,2+(3<<16))
        put(25,2,12,6);width=31
    put(5,2,12,7)
    lines=[f'{width} 9 26','VISUAL_STYLE Miniature','PRESET "Sunny (Default)"']+parts
    lines += ['ROTATION '+' '.join(map(str,r)) for r in regions]
    lines += [f'{x} {y} {z} {t} 0 0 {v}' for (x,y,z),(t,v) in sorted(cells.items())]
    path=root/'Resources/Stages'/name
    if path.exists():
        print('Preserved existing',path.name);return
    path.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print('Created',path.name,len(cells),'cells')

build('rotationBridge.txt',False)
build('rotationRelay.txt',True)

from PIL import Image,ImageDraw,ImageFont
from AdventureTheme import font,sheet,INK,TEAL,MUTED
for slug,title,desc in [('bridge','風見の橋と寄り道の鍵','島を回して北の鍵へ。戻した橋を東へ渡ろう。配置で隙間を安全な道に。'),
                        ('relay','三つの橋をつなぐ空路','手前の台座で橋を90度ずつ回転。三つの鍵を集めて、雲の先の星へ。')]:
    im=Image.new('RGBA',(1000,110));d=sheet(im,(0,0,996,105))
    d.text((18,7),title,font=font(27),fill='#294c50')
    d.text((18,47),desc,font=font(20),fill='#35474b')
    d.text((18,79),'F / Y：回転　 Shift+F / LB+Y：逆回転　 B：道具袋　 F1：操作説明',font=font(18),fill='#655c48')
    im.save(root/f'Resources/UI/campaign/rotation-{slug}.png')

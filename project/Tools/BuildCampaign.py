# 本編ステージと紹介用の案内を生成する。
"""Three editor-native courses, campaign labels and Japanese navigation guides.
Run deliberately: replaces only the three generated campaign maps and UI assets.
"""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont
import recover_models
recover_models.FONT["H"]=[17,17,17,31,17,17,17]

ROOT = Path(__file__).resolve().parents[1]
STAGES = ROOT / 'Resources/Stages'
PARTS = []
for line in (STAGES / 'expeditionGrandTour.txt').read_text(encoding='utf-8-sig').splitlines():
    if line.startswith(('PART ', 'PARTCELL ')):
        PARTS.append(line)

class Course:
    def __init__(self, name, size):
        self.name, self.size, self.cells, self.doors = name, size, {}, {}
    def put(self, x, y, z, kind, variant=0, rotation=0):
        self.cells[x,y,z] = kind, variant, rotation
    def island(self, x0, x1, z0, z1, top):
        for x in range(x0,x1+1):
            for z in range(z0,z1+1):
                for y in range(top+1): self.put(x,y,z,1)
    def bubble(self, x,y,z,slot=0,kind=2): self.put(x,y,z,5,kind+(slot<<16))
    # 部屋の壁と入口を作り、ギミックを置く空間を用意する。
    def room(self,x0,x1,z0,z1,floor,gate,kind):
        for y in (floor+1,floor+2):
            for x in range(x0,x1+1):
                for z in (z0,z1): self.put(x,y,z,2)
            for z in range(z0,z1+1):
                for x in (x0,x1): self.put(x,y,z,2)
            self.put(gate[0],y,gate[1],kind,1)
    # 指定した点の間へ、通路用の地形セルを配置する。
    def path(self, points, y):
        for (x0,z0),(x1,z1) in zip(points,points[1:]):
            assert x0==x1 or z0==z1
            for x in range(min(x0,x1),max(x0,x1)+1):
                for z in range(min(z0,z1),max(z0,z1)+1):
                    if self.cells.get((x,y,z),(0,))[0]==1: self.put(x,y,z,2)
    # 生成した設定とセルを、ステージファイルへ書き出す。
    def save(self):
        lines=[' '.join(map(str,self.size)), 'VISUAL_STYLE Miniature',
               'PRESET "Sunny (Default)"',
               'ENVIRONMENT 0.48 0.67 0.8 1 0.85 1 0.96 0.88 0.5 -1 0.35']+PARTS
        for (x,y,z),(kind,variant,rotation) in sorted(self.cells.items(),key=lambda a:(a[0][1],a[0][2],a[0][0])):
            assert all(0<=a<b for a,b in zip((x,y,z),self.size))
            lines.append(f'{x} {y} {z} {kind} 0 {rotation} {variant}')
            if kind==8: lines.append(' '.join(map(str,self.doors[x,y,z])))
        (STAGES/self.name).write_text('\n'.join(lines)+'\n',encoding='utf-8')
        print(self.name, len(self.cells), 'cells', sum(t==5 for t,v,r in self.cells.values()),'bubbles')

# 1: broad safe garden, visible bridge-building lane, first climb and checkpoint.
c=Course('beginnerTrail.txt',(24,8,20))
c.island(2,10,2,16,0);c.island(15,21,3,16,2)
c.island(11,14,8,10,0)
for y in (1,2): c.put(14,y,9,3,rotation=math.pi/2)
c.put(4,1,5,7);c.bubble(8,1,6,1);c.bubble(18,3,10)
c.put(16,3,9,21);c.put(19,3,14,6)
for x in (4,6):
    for y in range(1,4): c.put(x,y,3,2)
c.put(5,3,3,2)
# Short side route: collected Z-part can widen this single-cell footbridge.
for x in range(11,15): c.put(x,0,13,11)
c.path([(4,5),(4,9),(14,9)],0)
c.path([(15,9),(19,9),(19,14)],2)
for x,z,base in [(2,2,0),(10,2,0),(21,3,2),(21,16,2)]:
    for y in (base+1,base+2): c.put(x,y,z,2)
c.save()

# 2: two enclosed pickup gardens require a key and a P-switch; broad approaches.
c=Course('mechanismGardens.txt',(32,9,24))
c.island(2,13,2,21,1);c.island(18,29,2,21,1)
c.island(14,17,9,11,1)
for x in (15,16): c.put(x,1,10,11)
c.put(4,2,5,7);c.put(7,2,5,14)
c.room(5,10,13,19,1,(7,13),15);c.bubble(7,2,16,2)
c.put(20,2,6,9,1)
c.room(21,27,13,19,1,(24,13),10);c.bubble(24,2,16,3)
c.bubble(12,2,9,kind=12);c.bubble(20,2,11,kind=3)
c.put(19,2,10,21);c.put(28,2,20,6)
for x in range(18,22): c.put(x,2,20,26)
c.path([(4,5),(7,5),(7,16)],1)
c.path([(7,10),(24,10),(24,16)],1)
c.path([(20,6),(20,10)],1)
c.path([(24,12),(28,12),(28,20)],1)
for x,z in [(2,2),(13,2),(29,2),(29,21)]:
    for y in (2,3): c.put(x,y,z,2)
c.save()

# 3: routes at two heights; dangerous shortcuts stay optional; summit is a ladder climb.
c=Course('skylineRelay.txt',(36,12,34))
c.island(3,14,3,27,2);c.island(21,32,3,27,2)
c.island(15,20,8,8,2)
for x in range(15,21):
    c.put(x,2,10,22,10+(x%3));c.put(x,2,22,11)
c.put(6,3,6,7);c.bubble(9,3,9,kind=11);c.bubble(10,3,20,4)
c.put(22,3,9,21);c.bubble(25,3,12,kind=13)
for x in range(25,31):
    for z in range(4,10): c.put(x,2,z,12)
c.put(23,3,6,16);c.put(28,3,16,17);c.put(30,4,19,18);c.put(23,3,19,19)
c.put(24,3,24,23)
for x in range(15,21): c.put(x,2,25,24 if x%2 else 25)
c.island(26,30,23,28,5)
for y in range(3,6): c.put(28,y,22,3)
c.bubble(28,6,25,5);c.put(28,6,27,6);c.put(27,6,24,21)
c.island(21,24,30,32,2);c.put(23,2,28,13,10+(10<<8)+(16<<16))
c.put(6,3,24,8);c.put(22,3,24,8)
c.doors[6,3,24]=(22,3,24);c.doors[22,3,24]=(6,3,24)
c.path([(6,6),(6,8),(22,8),(22,12),(32,12),(32,21),(28,21)],2)
c.path([(10,8),(10,20)],2)
c.path([(28,23),(28,27)],5)
for x,z in [(3,3),(14,3),(32,3),(3,27),(32,27)]:
    for y in (3,4): c.put(x,y,z,2)
c.save()

campaign=[('beginnerTrail.txt','01 FIRST EXPEDITION'),('mechanismGardens.txt','02 KEY AND SWITCH'),
          ('skylineRelay.txt','03 SKYLINE RELAY'),('expeditionGrandTour.txt','04 GRAND TOUR')]
legacy=[('gameStage01.txt','05 ORIGINAL 01'),('gameStage02.txt','06 ORIGINAL 02'),('stage04.txt','07 ORIGINAL 04'),
        ('new_stage_03.txt','08 ORIGINAL 03 A'),('stage_03 .txt','09 ORIGINAL 03 B'),
        ('dioramaRuins.txt','10 DIORAMA RUINS'),('tutorial.txt','11 ORIGINAL TUTORIAL')]
(STAGES/'sequence.txt').write_text(''.join(f+'\n' for f,label in campaign),encoding='utf-8')
for i,(file,label) in enumerate(campaign):
    mesh=recover_models.Mesh();mesh.text(label,.48)
    (ROOT/f'Resources/Models/Recovery/campaignLabel{i}.obj').write_text('\n'.join(mesh.lines)+'\n',encoding='utf-8')

out=ROOT/'Resources/UI/campaign';out.mkdir(exist_ok=True)
font=ImageFont.truetype('C:/Windows/Fonts/YuGothB.ttc',25)
small=ImageFont.truetype('C:/Windows/Fonts/YuGothM.ttc',19)
# 見出しと説明を描き、指定した大きさの案内画像へ保存する。
def guide(name,title,detail,size=(1150,64)):
    im=Image.new('RGBA',size,(13,26,39,235));d=ImageDraw.Draw(im)
    d.text((16,1),title,font=font,fill=(255,221,143))
    detail_font=small
    for point in range(19,12,-1):
        detail_font=ImageFont.truetype('C:/Windows/Fonts/YuGothM.ttc',point)
        if d.textbbox((0,0),detail,font=detail_font)[2] <= size[0]-32: break
    d.text((16,35),detail,font=detail_font,fill='white')
    im.save(out/name)
guide('select.png','ステージ選択　W/S・↑↓：選択　Space / Enter：決定　Esc：タイトル',
      '01 → 02 → 03 → 04 の順で要素を体験できます。旧ステージも自由に選べます。')
guide('select-pad.png','ステージ選択　十字キー：選択　A：決定　B：タイトル',
      '01 → 02 → 03 → 04 の順で要素を体験できます。旧ステージも自由に選べます。')
guide('clear.png','クリア！　Space / Enter：ステージ選択へ', 'Esc：タイトルへ戻る',(704,64))
guide('clear-pad.png','クリア！　A：ステージ選択へ', 'B：タイトルへ戻る',(704,64))
hints=[('01 はじめての探検','シャボン玉を2個集めてゴールへ。Bで所持品、1でパーツ選択。梯子はW/S。'),
       ('02 鍵とスイッチの庭','鍵はFで拾い、鍵の壁へ接触。PスイッチもF。シャボン玉は4個。'),
       ('03 空中回廊','細い道・時間足場・動く足場を見回そう。頂上へは梯子。回収は4個。'),
       ('04 総合探検','鍵・スイッチ・梯子・足場づくりを組み合わせ、10個回収して頂上へ。')]
for i,(title,detail) in enumerate(hints): guide(f'objective-{i}.png',title,detail,(960,72))

# Keep curated selection previews and compact guides consistent with the maps.
import AddCourseKeys
for course in AddCourseKeys.courses: AddCourseKeys.add(course)
import BuildStagePreviews
import BuildAdventureUI

# 梯子・仕掛け・配置を組み合わせた総合探検ステージを生成する。
"""Editor-native expedition: all 27 cell types, five 3x3x3 parts, optional building."""
from pathlib import Path
from collections import Counter
import json
ROOT=Path(__file__).resolve().parents[1]
cells={};doors={}
def put(x,y,z,t,v=0,r=0): cells[x,y,z]=(t,v,r)
def island(x0,x1,z0,z1,top):
 for x in range(x0,x1+1):
  for z in range(z0,z1+1):
   for y in range(top+1): put(x,y,z,1)
def bubble(x,y,z,slot):put(x,y,z,5,2+(slot<<16))
# 部屋の壁と入口を作り、ギミックを置く空間を用意する。
def room(x0,x1,z0,z1,floor,gate,kind):
 for y in range(floor+1,floor+4):
  for x in range(x0,x1+1):
   for z in (z0,z1):put(x,y,z,2)
  for z in range(z0,z1+1):
   for x in (x0,x1):put(x,y,z,2)
 gx,gz=gate
 for y in range(floor+1,floor+3):put(gx,y,gz,kind,1)
 # Open above the gate so the silhouette and headlight remain readable.
 cells.pop((gx,floor+3,gz),None)
# Four gardens with a visible central summit.
island(3,15,3,15,1);island(23,35,3,15,1)
island(23,35,23,35,3);island(3,15,23,35,3)
island(17,21,17,22,6)
put(6,2,6,7);bubble(11,2,8,1)
# East crossing: single-cell jumps on a narrow crumbling route, with room to build a broad deck.
for x in (16,18,20,22):
 for z in (7,8):put(x,1,z,11 if x in (18,20) else 1)
# Construction lane: Z-BRIDGE at (17,1,9) removes two jumps; one wall at (21,1,8) removes the last.
put(16,1,9,1);put(20,1,9,1)
bubble(25,2,8,2);put(24,2,9,21)
# Ice garden and spikes have a safe perimeter for observation and recovery.
for x in range(27,33):
 for z in range(6,11):put(x,1,z,12)
for x,z in ((28,7),(31,9),(33,11)):put(x,2,z,16)
put(29,2,12,17);put(33,3,6,18)
# Climb to the northern causeway; stable waiting pads alternate with timed pads.
for y in range(2,4):put(29,y,14,3)
island(28,30,15,16,3)
for z in (18,20,22):
 for x in (28,29):put(x,3,z,22,10+(z-18)//2)
# Side ride crosses the same channel. Its landing does not require a carried block.
put(25,3,17,13,(10)|((10)<<8)|((16)<<16))
island(24,26,15,16,3)
for y in range(2,4):put(25,y,14,3)
# North garden: enemies can be avoided on wide lanes; an ON/OFF side bridge is optional.
put(29,4,25,21);bubble(31,4,28,3)
put(27,4,29,19);put(32,5,32,18);put(25,4,25,17)
put(25,4,31,23)
for x in (16,18,20,22):
 for z in (28,29):put(x,3,z,11 if x==20 else 1)
for x in range(16,23):
 put(x,3,31,24 if x%2 else 25)
# Key and P-switch puzzles: walls make the keys/switch relevant, while no carried part is required.
put(13,4,27,14)
room(4,9,25,31,3,(6,25),15);bubble(6,4,28,4)
room(10,14,30,35,3,(12,30),10);put(12,4,27,9,1)
put(12,4,33,4) # Star is a solid golden object in the current engine, not a collectible.
# A glass gallery, checkpoint, and door loop let players examine camera/headlight/return travel.
for z in range(25,29):put(4,4,z,26)
put(14,4,24,21)
put(5,4,33,8);put(6,2,12,8);doors[5,4,33]=(6,2,12);doors[6,2,12]=(5,4,33)
# Fixed summit route: the carried stair/ladder is a shortcut, never a mandatory key.
island(15,19,23,25,3)
for y in range(4,7):put(18,y,23,3)
put(17,4,24,21);bubble(19,7,20,5);put(19,7,18,6)
# Appearing P-blocks form a second approach after the P-switch puzzle.
for x in (15,16):put(x,3,26,20,1)
# Standard inventory stock is also collected in bubbles, never granted invisibly.
for x,y,z,kind in [(10,2,6,2),(12,2,11,3),(25,2,10,12),(30,4,26,13),(12,4,32,11)]:
 put(x,y,z,5,kind)
# Quiet sandstone paths and edge landmarks give each garden a readable silhouette.
for x0,x1,z,y in [(5,15,8,1),(23,26,8,1),(23,34,28,3),(10,15,24,3)]:
 for x in range(x0,x1+1):
  if cells.get((x,y,z),(0,))[0]==1:put(x,y,z,2)
for x,z,base in [(3,3,1),(15,3,1),(35,23,3),(35,35,3),(3,35,3)]:
 for y in range(base+1,base+3):put(x,y,z,2)
# An original entrance arch; the centre remains high enough for the player collider.
for x in (5,7):
 for y in range(2,5):put(x,y,4,2)
put(6,4,4,2)
# Five connected editor-sized polycubes. Each coordinate is in the native 3x3x3 grid.
parts=[(1,'Z-BRIDGE',(1,.73,.34),[(x,0,0,2) for x in range(3)]+[(2,0,z,2) for z in (1,2)]),
 (2,'T-DECK',(.45,.8,.95),[(x,0,0,2) for x in range(3)]+[(1,0,z,2) for z in (1,2)]+[(1,1,2,3)]),
 (3,'STAIR-L',(.9,.73,.45),[(x,y,z,2) for x in range(2) for z in range(3) for y in range(z+1)]+[(2,y,2,3) for y in range(3)]),
 (4,'ARCHWAY',(.7,.86,.6),[(x,y,1,2) for x in (0,2) for y in range(3)]+[(1,2,1,2)]+[(1,2,0,2),(1,2,2,2)]),
 (5,'HOOK-LAD',(.9,.62,.8),[(0,y,1,3) for y in range(3)]+[(x,2,1,2) for x in (1,2)]+[(2,2,2,2),(2,1,2,2)])]
headers=['40 12 40','VISUAL_STYLE Miniature','PRESET "Sunny (Default)"','ENVIRONMENT 0.48 0.67 0.8 1 0.85 1 0.96 0.88 0.5 -1 0.35']
for slot,name,color,shape in parts:
 headers += [f'PART {slot} 2 {color[0]} {color[1]} {color[2]} {name}']
 headers += [f'PARTCELL {slot} {x} {y} {z} {t}' for x,y,z,t in shape]
lines=headers[:]
for (x,y,z),(t,v,r) in sorted(cells.items(),key=lambda item:(item[0][1],item[0][2],item[0][0])):
 lines.append(f'{x} {y} {z} {t} 0 {r} {v}')
 if t==8:lines.append(' '.join(map(str,doors[x,y,z])))
path=ROOT/'Resources/Stages/expeditionGrandTour.txt'
# Never overwrite an edited level unless explicitly running this generator again.
path.write_text('\n'.join(lines)+'\n',encoding='utf-8')
sequence=ROOT/'Resources/Stages/sequence.txt';text=sequence.read_text(encoding='utf-8-sig')
if path.name not in text.splitlines():sequence.write_text(text.rstrip()+'\n'+path.name+'\n',encoding='utf-8')
import recover_models
mesh=recover_models.Mesh();mesh.text('EXPEDITION GRAND TOUR',.48)
label_index=sequence.read_text(encoding='utf-8-sig').splitlines().index(path.name)
(ROOT/f'Resources/Models/Recovery/stageLabel{label_index}.obj').write_text('\n'.join(mesh.lines)+'\n',encoding='utf-8')
print(path,len(cells),'cells; types',dict(sorted(Counter(t for t,v,r in cells.values()).items())))

import AddCourseKeys
AddCourseKeys.add(path.name)
import BuildAdventureUI

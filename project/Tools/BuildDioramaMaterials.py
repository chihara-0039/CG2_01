"""Deterministic original grass/stone/wood atlas and per-face cube meshes."""
from pathlib import Path
from PIL import Image,ImageDraw
import random
from recover_models import Mesh
out=Path(__file__).resolve().parents[1]/'Resources/Models/Diorama'
out.mkdir(exist_ok=True)
rng=random.Random(41)
im=Image.new('RGB',(512,128));d=ImageDraw.Draw(im)
colors=[(86,137,66),(144,139,116),(104,112,105),(154,111,61)]
for tile,base in enumerate(colors):
 for y in range(0,128,4):
  for x in range(0,128,4):
   n=rng.randint(-9,9);d.rectangle((tile*128+x,y,tile*128+x+3,y+3),fill=tuple(max(0,min(255,c+n)) for c in base))
 if tile in (1,2):
  for y in range(0,128,32):
   d.line((tile*128,y,tile*128+127,y),fill=(77,83,75),width=2)
   for x in range((y//32%2)*32,128,64):d.line((tile*128+x,y,tile*128+x,y+31),fill=(77,83,75),width=2)
 if tile==3:
  for x in range(0,128,32):d.line((384+x,0,384+x,127),fill=(86,60,34),width=3)
im.save(out/'ruins.png')
points=[(-.5,-.5,-.5),(.5,-.5,-.5),(.5,.5,-.5),(-.5,.5,-.5),(-.5,-.5,.5),(.5,-.5,.5),(.5,.5,.5),(-.5,.5,.5)]
faces=((0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(3,7,6,2),(0,1,5,4))
for name in ('grass','stone','bridge'):
 mesh=Mesh();mesh.lines=['# Original diorama cube','mtllib '+name+'.mtl']
 for i,corners in enumerate(faces):
  tile=(0 if i==4 else 1) if name=='grass' else 2 if name=='stone' else 3
  a,b,c,e=[points[j] for j in corners]
  uv=[((tile+u*.96+.02)/4,v*.96+.02) for u,v in ((0,0),(1,0),(1,1),(0,1))]
  mesh.triangle(a,b,c,uv[:3]);mesh.triangle(a,c,e,(uv[0],uv[2],uv[3]))
 (out/(name+'.obj')).write_text('\n'.join(mesh.lines)+'\n')
 (out/(name+'.mtl')).write_text('newmtl material\nKd 1 1 1\nmap_Kd ruins.png\n')
print(out)

mesh=Mesh();mesh.sphere(1);mesh.lines[0]='# Original diorama cloud';mesh.lines.insert(1,'mtllib cloud.mtl')
(out/'cloud.obj').write_text('\n'.join(mesh.lines)+'\n')
(out/'cloud.mtl').write_text('newmtl material\nKd 1 1 1\nmap_Kd cloud.png\n')
Image.new('RGB',(16,16),(245,248,255)).save(out/'cloud.png')

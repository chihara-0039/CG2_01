# 面取りした箱庭ブロックの形状と材質を生成する。
"""Original miniature scenery: chamfered blocks and quiet grass/soil/stone/wood materials.

Only visual meshes change. Every vertex stays inside the original unit collider.
The original Diorama asset set remains available alongside this new set.
"""
from pathlib import Path
from itertools import product
import math
import random
from PIL import Image, ImageDraw
from recover_models import Mesh

OUT = Path(__file__).resolve().parents[1] / 'Resources/Models/DioramaGarden'
OUT.mkdir(exist_ok=True)
rng = random.Random(11010)
tile_size = 256
atlas = Image.new('RGB', (tile_size*4, tile_size))
draw = ImageDraw.Draw(atlas)
palette = [(113, 177, 63), (188, 143, 96), (213, 202, 167), (198, 145, 80)]
for tile, base in enumerate(palette):
    for y in range(tile_size):
        shade = 1.03 - .11*y/(tile_size-1)
        color = tuple(round(c*shade) for c in base)
        draw.line((tile*tile_size, y, (tile+1)*tile_size-1, y), fill=color)

# Small grouped marks rather than high-frequency noise across the whole surface.
for _ in range(150):
    x, y = rng.randrange(8,248), rng.randrange(8,248)
    draw.arc((x,y,x+5,y+7),180,300,fill=(133,191,76),width=2)
for y in (70,145,215):
    draw.line([(256+x,y+round(3*math.sin(x*.04))) for x in range(256)],fill=(174,127,83),width=2)
for _ in range(35):
    x,y=256+rng.randrange(12,244),rng.randrange(12,244)
    draw.ellipse((x,y,x+7,y+4),fill=(206,171,121))
# Broad warm sandstone blocks with soft bevel-like borders.
for row in range(4):
    for col in range(-1,3):
        x=512+col*128+(64 if row%2 else 0)
        y=row*64
        draw.rounded_rectangle((max(512,x+2),y+2,min(767,x+126),y+62),radius=7,fill=(221,210,176)) if x+126>512 and x+2<768 else None
        if 512<=x+10<768:
            draw.line((x+10,y+8,min(766,x+115),y+8),fill=(236,226,195),width=2)
# Long golden timber boards; no dark pixel grid.
for x in range(768,1024,64):
    draw.line((x,0,x,255),fill=(143,101,53),width=3)
    draw.line((x+5,0,x+5,255),fill=(223,174,108),width=2)
    for y in (42,110,195):
        draw.arc((x+18,y,x+44,y+30),65,275,fill=(176,122,61),width=1)
atlas.save(OUT/'garden.png')

# 面取りしたブロックを頂点と材質へ展開し、OBJで保存する。
def write_block(name):
    mesh=Mesh()
    mesh.lines=['# Original chamfered miniature block','mtllib '+name+'.mtl']
    triangles=[]
    half=.5
    inner=.46
    # Six inset faces, twelve chamfer strips and eight three-way corner caps.
    def quad(points):
        triangles.extend([(points[0],points[1],points[2]),(points[0],points[2],points[3])])
    for axis in range(3):
        others=[i for i in range(3) if i!=axis]
        for sign in (-1,1):
            points=[]
            for u,v in ((-1,-1),(-1,1),(1,1),(1,-1)):
                point=[0.,0.,0.]
                point[axis]=sign*half
                point[others[0]]=u*inner
                point[others[1]]=v*inner
                points.append(tuple(point))
            quad(points)
        for first,second in product((-1,1),repeat=2):
            points=[]
            for t,a,b in ((-inner,half,inner),(inner,half,inner),(inner,inner,half),(-inner,inner,half)):
                point=[0.,0.,0.]
                point[axis]=t
                point[others[0]]=first*a
                point[others[1]]=second*b
                points.append(tuple(point))
            quad(points)
    for signs in product((-1,1),repeat=3):
        triangles.append(tuple(tuple(signs[i]*(half if i==axis else inner) for i in range(3)) for axis in range(3)))
    for triangle in triangles:
        a,b,c=triangle
        ab=[b[i]-a[i] for i in range(3)]
        ac=[c[i]-a[i] for i in range(3)]
        normal=(ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0])
        center=tuple((a[i]+b[i]+c[i])/3 for i in range(3))
        if sum(normal[i]*center[i] for i in range(3))<0:
            b,c=c,b
            normal=tuple(-n for n in normal)
        tile=(0 if normal[1]>0 and center[1]>=inner else 1) if name=='grass' else 2 if name=='stone' else 3
        dominant=max(range(3),key=lambda i:abs(normal[i]))
        axes=(0,2) if dominant==1 else (2,1) if dominant==0 else (0,1)
        uv=tuple(((tile+.02+.96*(p[axes[0]]+.5))/4,.02+.96*(p[axes[1]]+.5)) for p in (a,b,c))
        mesh.triangle(a,b,c,uv)
    (OUT/(name+'.obj')).write_text('\n'.join(mesh.lines)+'\n',encoding='utf-8')
    (OUT/(name+'.mtl')).write_text('newmtl material\nKd 1 1 1\nmap_Kd garden.png\n',encoding='utf-8')
    assert len(triangles)==44
    print(name, len(triangles), 'triangles; bounds remain +/-0.5')

for name in ('grass','stone','bridge'):
    write_block(name)
cloud=Mesh()
cloud.sphere(1,rings=12,sectors=20)
cloud.lines.insert(1,'mtllib cloud.mtl')
position=(0,1,0)
for index,line in enumerate(cloud.lines):
    if line.startswith('v '):
        position=tuple(map(float,line.split()[1:]))
    elif line.startswith('vn '):
        length=math.sqrt(sum(value*value for value in position))
        cloud.lines[index]='vn '+' '.join(f'{value/length:.6f}' for value in position)
(OUT/'cloud.obj').write_text('\n'.join(cloud.lines)+'\n',encoding='utf-8')
(OUT/'cloud.mtl').write_text('newmtl material\nKd 1 1 1\nmap_Kd cloud.png\n',encoding='utf-8')
Image.new('RGB',(16,16),(250,248,239)).save(OUT/'cloud.png')

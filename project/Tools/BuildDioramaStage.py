"""Original miniature ruin: two three-cell gaps and an elevated lookout."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
cells = {}
def put(x,y,z,t,variant=0,rotation=0):
    cells[x,y,z]=(t,rotation,variant)
def island(x0,x1,z0,z1,top):
    for x in range(x0,x1+1):
        for z in range(z0,z1+1):
            for y in range(top+1): put(x,y,z,1)

# Three separate land masses: no permanent floor under either bridge.
island(2,6,2,6,2)
island(2,6,10,13,2)
island(10,13,10,13,2)
# Small side alcove and ancient pillars leave the main route unobstructed.
island(2,3,7,8,2)
for x,z in [(2,2),(6,2),(2,13),(13,10)]:
    for y in range(3,5): put(x,y,z,2)
# Lookout terrace. The ladder shaft is open through the roof and reaches above it.
island(11,13,12,13,5)
for y in range(3,8): put(11,y,12,3)
put(4,3,3,7)
put(4,3,5,5,65538)
put(5,3,12,5,65538)
put(13,6,13,6)

lines=['16 10 16','PRESET "Diorama"','ENVIRONMENT 0.48 0.67 0.80 1 0.85 1 0.96 0.88 0.5 -1 0.35',
       'PART 1 2 1 0.65 0.2 RUIN-BRIDGE']
lines += [f'PARTCELL 1 1 0 {z} 2' for z in range(3)]
lines += [f'{x} {y} {z} {t} 0 {r} {v}' for (x,y,z),(t,r,v) in sorted(cells.items(),key=lambda a:(a[0][1],a[0][2],a[0][0]))]
target=ROOT/'Resources/Stages/dioramaRuins.txt'
target.write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(f'{target}: {len(cells)} cells')
import recover_models
mesh=recover_models.Mesh();mesh.text('RUIN BRIDGES',.65)
(ROOT/'Resources/Models/Recovery/stageLabel6.obj').write_text('\n'.join(mesh.lines)+'\n',encoding='utf-8')

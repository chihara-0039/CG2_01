"""Build a playable route using an L deck and a carried stair/ladder assembly."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'Resources/Stages/dioramaRuins.txt').read_text(encoding='utf-8').splitlines()
headers=[line for line in source if not line[0].isdigit()]
cells={}
for line in source[1:]:
 if line[0].isdigit():
  values=list(map(int,line.split()));cells[tuple(values[:3])]=values[3:]
# Replace the fixed final lookout with the part the player collects.
for key in list(cells):
 x,y,z=key
 if x>=10 and y>=3:del cells[key]
# Bring the last island one square toward the centre of the diorama.
cells={(x,y,z-1 if x>=10 else z):value for (x,y,z),value in cells.items()}
parts=[(2,'L-DECK',[(0,0,0,2),(1,0,0,2),(2,0,0,2),(0,0,1,2),(0,0,2,2)]),
       (3,'STAIR-LADDER',[(x,y,z,2) for z in range(3) for y in range(z+1) for x in range(2)]+[(2,y,2,3) for y in range(3)])]
for slot,name,shape in parts:
 headers += [f'PART {slot} 2 0.75 0.85 1 {name}']+[f'PARTCELL {slot} {x} {y} {z} {t}' for x,y,z,t in shape]
cells[4,3,5]=[5,0,0,2+(2<<16)]
cells[11,3,9]=[5,0,0,2+(3<<16)]
cells[11,6,12]=[6,0,0,0]
lines=[source[0]]+headers+[' '.join(map(str,(*key,*value))) for key,value in sorted(cells.items(),key=lambda item:(item[0][1],item[0][2],item[0][0]))]
(root/'Resources/Stages/complexRoute.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8')

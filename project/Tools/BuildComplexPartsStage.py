"""Original concave, mixed-height and hollow 3x3x3 part fixtures."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
parts={2:('L-DECK',[(0,0,0,2),(1,0,0,2),(2,0,0,2),(0,0,1,2),(0,0,2,2)]),
       3:('STAIR-LADDER',[(x,y,z,2) for z in range(3) for y in range(z+1) for x in range(2)]+[(2,y,2,3) for y in range(3)]),
       4:('HOLLOW-FRAME',[(x,y,z,2) for y in range(3) for z in range(3) for x in range(3) if sum(v in (0,2) for v in (x,y,z))>=2])}
assert [len(p[1]) for p in parts.values()]==[5,15,20]
lines=['18 10 15','PRESET "Diorama"','ENVIRONMENT 0.48 0.67 0.8 1 0.85 1 0.96 0.88 0.5 -1 0.35','PART 1 2 1 0.65 0.2 RUIN-BRIDGE','PARTCELL 1 1 0 0 2','PARTCELL 1 1 0 1 2','PARTCELL 1 1 0 2 2']
for slot,(name,cells) in parts.items():
 lines += [f'PART {slot} 2 0.75 0.85 1 {name}']+[f'PARTCELL {slot} {x} {y} {z} {t}' for x,y,z,t in cells]
for y in range(3):
 for z in range(3,11):
  for x in range(1,16):lines.append(f'{x} {y} {z} 1 0 0 0')
lines += ['2 3 4 7 0 0 0']+[f'2 3 {z} 5 0 0 {2+(slot<<16)}' for slot,z in [(2,5),(3,7),(4,9)]]
(root/'Resources/Stages/complexParts.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Created L-deck 5, stair/ladder 15, hollow frame 20')

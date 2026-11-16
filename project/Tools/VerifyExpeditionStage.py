# 総合探検マップの通路とプレイヤーが通れる空間を検査する。
"""Static, placement-free accessibility audit; does not claim a human or timed-physics clear."""
from pathlib import Path
from collections import deque,Counter
import math,json
root=Path(__file__).resolve().parents[1]
lines=(root/'Resources/Stages/expeditionGrandTour.txt').read_text().splitlines()
cells={};parts={};doors={};i=1
while i<len(lines):
 a=lines[i].split();i+=1
 if not a:continue
 if a[0]=='PARTCELL':parts.setdefault(int(a[1]),{})[tuple(map(int,a[2:5]))]=int(a[5]);continue
 if not a[0].isdigit():continue
 x,y,z,t=map(int,a[:4]);cells[x,y,z]=t
 if t==8:doors[x,y,z]=tuple(map(int,lines[i].split()));i+=1
assert set(cells.values())==set(range(1,27))
assert len([t for t in cells.values() if t==5])==10
for slot,shape in parts.items():
 assert len(shape)>=5 and all(all(0<=v<3 for v in p) for p in shape)
 visited={next(iter(shape))};queue=deque(visited)
 while queue:
  x,y,z=queue.popleft()
  for dx,dy,dz in ((1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)):
   p=x+dx,y+dy,z+dz
   if p in shape and p not in visited:visited.add(p);queue.append(p)
 assert len(visited)==len(shape),(slot,'disconnected part')
# Key gate open, P switch activated; timed/crumbling surfaces available when crossed.
solid_types={1,2,4,11,12,20,22,24,26}
def solid(x,y,z):return cells.get((x,y,z),0) in solid_types
# プレイヤーの体積に重なるセルを調べ、通れる空間か判定する。
def clear(x,feet,z):
 for gy in range(math.floor(feet+.5),math.floor(feet+1.6+.5-.0001)+1):
  for gx in range(math.floor(x-.35+.5),math.floor(x+.35+.5-.0001)+1):
   for gz in range(math.floor(z-.35+.5),math.floor(z+.35+.5-.0001)+1):
    if solid(gx,gy,gz):return False
 return True
nodes={(x,y,z) for (x,y,z),t in cells.items() if solid(x,y,z) and clear(x,y+.5,z)}
adj={p:[] for p in nodes}
for x,y,z in nodes:
 for dx,dz in ((1,0),(-1,0),(0,1),(0,-1)):
  for distance in (1,2):
   q=x+dx*distance,y,z+dz*distance
   if q not in nodes:continue
   # Same-height hops have no ceiling in either mandatory crossing.
   good=all(clear(x+dx*distance*t,y+.5+(4*1.15*t*(1-t) if distance==2 else 0),z+dz*distance*t) for t in [v/24 for v in range(25)])
   if good:adj[x,y,z].append(q)
# Native vertical ladders link the lower supported standing point with an adjacent top landing.
for (x,y,z),t in cells.items():
 if t!=3 or cells.get((x,y-1,z))==3:continue
 top=y
 while cells.get((x,top+1,z))==3:top+=1
 bottom=(x,y-1,z)
 if bottom not in nodes:continue
 for dx,dz in ((1,0),(-1,0),(0,1),(0,-1)):
  q=x+dx,top,z+dz
  if q in nodes:adj[bottom].append(q);adj[q].append(bottom)
for (x,y,z),target in doors.items():
 a=x,y-1,z;b=target[0],target[1]-1,target[2]
 if a in nodes and b in nodes:adj[a].append(b)
spawn=next(p for p,t in cells.items() if t==7);start=spawn[0],spawn[1]-1,spawn[2]
seen={start};queue=deque([start]);previous={}
while queue:
 p=queue.popleft()
 for q in adj[p]:
  if q not in seen:seen.add(q);previous[q]=p;queue.append(q)
objectives=[p for p,t in cells.items() if t in (5,6,9,14,21,23)]
missing=[p for p in objectives if (p[0],p[1]-1,p[2]) not in seen]
# Verify the advertised building advantage on the actual level, not an empty test grid.
bridge={(17+x,1+y,9+z):t for (x,y,z),t in parts[1].items()}
assert not any(p in cells for p in bridge)
assert (21,1,8) not in cells
assisted=dict(cells);assisted.update(bridge);assisted[21,1,8]=2
walk_route=[(15,1,9),(16,1,9),(17,1,9),(18,1,9),(19,1,9),(20,1,9),(20,1,8),(21,1,8),(22,1,8),(23,1,8)]
assert all(assisted.get(p) in solid_types for p in walk_route)
assert all(abs(a[0]-b[0])+abs(a[2]-b[2])==1 for a,b in zip(walk_route,walk_route[1:]))
report={'success':not missing,'standingNodes':len(nodes),'reachableWithoutPlacement':len(seen),'missingObjectives':missing,'bubbleCount':10,'zBridgeAndOneWallMakeFirstCrossingWalkable':True,'partCells':{k:len(v) for k,v in parts.items()},'conditions':'key gate opened; P-switch active; timed/crumble traversed during active window; same-height gaps <=1 cell; no enemy/timing/input simulation'}
out=root.parent/'generated/expedition-review';out.mkdir(exist_ok=True,parents=True)
(out/'accessibility.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2));assert not missing

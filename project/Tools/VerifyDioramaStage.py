"""Check walking/climbing connectivity; this does not simulate human input."""
from pathlib import Path
from collections import deque
root=Path(__file__).resolve().parents[1]
lines=(root/'Resources/Stages/dioramaRuins.txt').read_text().splitlines()
cells={}
for line in lines[1:]:
    if line[0].isdigit():
        x,y,z,t,*_=line.split();cells[int(x),int(y),int(z)]=int(t)
def solid(x,y,z): return cells.get((x,y,z),0) in (1,2)
def ladder(x,h,z): return any(cells.get((x,y,z))==3 for y in (h,h+1))
def valid(x,h,z):
    return (0<=x<16 and 0<=h<9 and 0<=z<16 and
            not solid(x,h+1,z) and not solid(x,h+2,z) and
            (solid(x,h,z) or ladder(x,h,z)))
def reachable():
    seen={(4,2,3)};q=deque(seen)
    while q:
        x,h,z=q.popleft()
        candidates=[(x+1,h,z),(x-1,h,z),(x,h,z+1),(x,h,z-1)]
        if ladder(x,h,z): candidates += [(x,h+1,z),(x,h-1,z)]
        for n in candidates:
            if n not in seen and valid(*n):seen.add(n);q.append(n)
    return seen
assert (4,2,5) in reachable(), 'first pickup must be walkable'
assert (5,2,12) not in reachable(), 'first gap must separate the second pickup'
assert (13,5,13) not in reachable(), 'goal must initially be disconnected'
for z in range(7,10): cells[4,2,z]=2
assert (5,2,12) in reachable(), 'first bridge must connect the second pickup'
assert (13,5,13) not in reachable(), 'second bridge must still be needed'
for x in range(7,10): cells[x,2,11]=2
assert (13,5,13) in reachable(), 'rotated bridge and ladder must connect the goal'
assert sum(t==5 for t in cells.values())==2
assert sum(t==6 for t in cells.values())==1
sequence=(root/'Resources/Stages/sequence.txt').read_text().splitlines()
assert sequence[-1]=='dioramaRuins.txt' and len(sequence)==len(set(sequence))
print('PASS: two required walking connections, both pickups, open ladder shaft, elevated goal, stage sequence')

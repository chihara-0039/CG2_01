# ステージ内の回収物と通路を調べ、クリア条件用の鍵を配置する。
"""Add independent star-unlock keys without removing any block-storage bubbles."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'Resources/Stages'
courses={
 'beginnerTrail.txt':[(8,1,7),(18,3,13)],
 'mechanismGardens.txt':[(8,2,17),(25,2,17),(20,2,10)],
 'skylineRelay.txt':[(12,3,21),(25,3,13),(29,6,26)],
 'expeditionGrandTour.txt':[(10,2,8),(26,2,8),(7,4,28),(13,4,33),(20,7,20)],
 'rotationTrial.txt':[(5,2,11),(15,2,13),(17,2,4)]}
# 既存セルを調べ、ステージごとの鍵を追加して保存する。
def add(name):
    path=root/name
    if not path.exists(): return
    lines=path.read_text(encoding='utf-8-sig').splitlines();cells={}
    for line in lines[1:]:
        a=line.split()
        if len(a)>=7 and all(v.lstrip('-').isdigit() for v in a[:4]): cells[tuple(map(int,a[:3]))]=int(a[3])
    changed=False
    for p in courses[name]:
        if cells.get(p)==14: continue
        if cells.get(p,0)!=0: raise ValueError(f'Key position occupied: {name} {p}')
        lines.append(f'{p[0]} {p[1]} {p[2]} 14 0 0 1');changed=True
    if changed:path.write_text('\n'.join(lines)+'\n',encoding='utf-8')
if __name__=='__main__':
    for name in courses:add(name)

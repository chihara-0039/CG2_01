"""Presentation-only captions; engine recordings remain intact."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('--root',required=True);p.add_argument('--ffmpeg',required=True);a=p.parse_args()
root=Path(a.root);out=root/'presentation';out.mkdir(exist_ok=True)
font=ImageFont.truetype('C:/Windows/Fonts/YuGothB.ttc',25)
shots={
 'overview':[(0,10,'完成した2方向の橋を、別の角度から見回す')],
 'rotation':[(0,2,'① 重なる場所：設置できない'),(2,4,'② 置く前の形を、空いた場所へ動かす'),(4,6,'③ 90度回転：橋の向きを合わせる'),(6,7,'④ 見えた形のまま、3マスを置く'),(7,10,'⑤ 配置した橋を渡る')],
 'undo':[(0,2,'置く前の形を確認  ｜  足場の在庫：1個'),(2,4,'足場を置く  ｜  足場の在庫：0個'),(4,5,'設置を取り消す  ｜  足場の在庫：1個'),(5,6,'別の向きも確認  ｜  足場の在庫：1個'),(6,7,'橋になる向きへ戻す  ｜  足場の在庫：1個'),(7,10,'置き直して渡る  ｜  足場の在庫：0個')]
}
def run(*args):subprocess.run([a.ffmpeg,'-hide_banner','-loglevel','error','-y',*map(str,args)],check=True)
for name,phases in shots.items():
 args=['-i',root/f'diorama-{name}.mp4']
 for i,(_,_,caption) in enumerate(phases):
  im=Image.new('RGBA',(880,52),(24,29,36,235));d=ImageDraw.Draw(im);d.text((20,7),caption,font=font,fill=(241,245,249,255))
  im.save(out/f'{name}-{i}.png');args+=['-i',out/f'{name}-{i}.png']
 chain=[]
 if name=='overview':chain.append('[0:v]crop=720:405:120:50,scale=960:540[base]');last='base'
 else:last='0:v'
 for i,(start,end,_) in enumerate(phases):
  tag=f'v{i}';chain.append(f"[{last}][{i+1}:v]overlay=40:478:enable='gte(t,{start})*lt(t,{end})'[{tag}]");last=tag
 run(*args,'-filter_complex',';'.join(chain),'-map',f'[{last}]','-an','-c:v','libx264','-crf','19',out/f'{name}.mp4')
 run('-i',out/f'{name}.mp4','-filter_complex','fps=12,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer',out/f'{name}.gif')
print(out)

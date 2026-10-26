"""同じ操作の変更前後と、別場面の取り消し・ゲーム進行を発表素材へまとめる。"""
from pathlib import Path
import argparse, subprocess, shutil, json
from PIL import Image,ImageDraw,ImageFont
p=argparse.ArgumentParser();p.add_argument('--ffmpeg',required=True);p.add_argument('--root',required=True);p.add_argument('--output');a=p.parse_args()
root=Path(a.root).resolve();src=root/'media';out=Path(a.output).resolve() if a.output else root/'presentation-media';out.mkdir(exist_ok=True)
shutil.copy2(Path(__file__).parent/'References/reference-game.jpg',out/'reference-game.jpg')
def run(*args):subprocess.run([a.ffmpeg,'-hide_banner','-loglevel','error','-y',*map(str,args)],check=True)
font=ImageFont.truetype('C:/Windows/Fonts/YuGothB.ttc',24)
gap=Image.new('RGBA',(104,328),(32,32,32,255));gd=ImageDraw.Draw(gap)
gd.polygon([(15,134),(56,134),(56,110),(94,164),(56,218),(56,194),(15,194)],fill=(139,182,216,255))
gap.save(out/'old-new-arrow.png')
for i,lines in enumerate([['予定：3マス','在庫：1個'],['2マスだけ置かれた','在庫：0個'],['赤い表示：ここには置けない','在庫：1個'],['置かなかった：足場は0マス','在庫：1個のまま']]):
 im=Image.new('RGBA',(584,68),(16,28,43,255));d=ImageDraw.Draw(im)
 for j,line in enumerate(lines):d.text((14,2+31*j),line,font=font,fill=(241,245,249))
 im.save(out/f'phase-{i}.png')
run('-i',src/'before-collision.mp4','-i',src/'final-collision.mp4',
 '-i',out/'phase-0.png','-i',out/'phase-1.png','-i',out/'phase-2.png','-i',out/'phase-3.png','-loop','1','-i',out/'old-new-arrow.png',
 '-filter_complex',"[0:v]crop=960:412:0:128,scale=584:250,pad=584:328:0:0:color=0x101C2B[l];[1:v]crop=960:412:0:128,scale=584:250,pad=584:328:0:0:color=0x101C2B[r];[l][2:v]overlay=0:260:enable='lt(t,3)'[l0];[l0][3:v]overlay=0:260:enable='gte(t,3)'[l1];[r][4:v]overlay=0:260:enable='lt(t,3)'[r0];[r0][5:v]overlay=0:260:enable='gte(t,3)'[r1];[l1]scale=532:299,pad=532:328:0:14:color=0x202020[left];[r1]scale=532:299,pad=532:328:0:14:color=0x202020[right];[left][6:v][right]hstack=inputs=3:shortest=1[v]",
 '-map','[v]','-t','10','-an','-c:v','libx264','-crf','19',out/'collision-compare.mp4')
for name,source in [('collision-compare',out/'collision-compare.mp4'),('undo',src/'final-undo.mp4'),('route',src/'final-route.mp4')]:
 if name!='collision-compare':
  # Presentation copies omit the control-hint line; gameplay and stock count stay visible.
  clean=out/(name+'.mp4')
  run('-i',source,'-vf','drawbox=x=15:y=57:w=713:h=41:color=0x0C1623:t=fill','-an','-c:v','libx264','-crf','19',clean)
  source=clean
 run('-i',source,'-filter_complex','fps=12,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer',out/(name+'.gif'))
run('-ss','6','-i',src/'before-collision.mp4','-frames:v','1',out/'problem.png')
run('-ss','9','-i',src/'final-route.mp4','-frames:v','1',out/'hero.png')
run('-ss','1','-i',src/'final-undo.mp4','-frames:v','1',out/'preview.png')
print(out)

# Prefer the new stage recording when it has been captured and cropped.
diorama=root.parent/'diorama'
if (diorama/'diorama-route.gif').exists():
 for ext in ('gif','mp4'):shutil.copy2(diorama/('diorama-route.'+ext),out/('route.'+ext))
 run('-ss','14','-i',diorama/'diorama-route.mp4','-frames:v','1',out/'background.png')
for name in ('overview','rotation','undo','complex'):
 clip=diorama/'presentation'/(name+'.gif')
 if clip.exists():
  for ext in ('gif','mp4'):shutil.copy2(diorama/'presentation'/(name+'.'+ext),out/(name+'.'+ext))

"""同じ操作の変更前後と、別場面の取り消し・ゲーム進行を発表素材へまとめる。"""
from pathlib import Path
import argparse, subprocess, shutil, json
from PIL import Image,ImageDraw,ImageFont
p=argparse.ArgumentParser();p.add_argument('--ffmpeg',required=True);p.add_argument('--root',required=True);a=p.parse_args()
root=Path(a.root).resolve();src=root/'media';out=root/'presentation-media';out.mkdir(exist_ok=True)
def run(*args):subprocess.run([a.ffmpeg,'-hide_banner','-loglevel','error','-y',*map(str,args)],check=True)
font=ImageFont.truetype('C:/Windows/Fonts/YuGothB.ttc',24)
for i,lines in enumerate([['予定：3マス','在庫：1個'],['実体：2マスだけ設置','在庫：0個'],['赤い予定形：設置不可','在庫：1個'],['確定を拒否：実体は0マス','在庫：1個を維持']]):
 im=Image.new('RGBA',(584,68),(16,28,43,255));d=ImageDraw.Draw(im)
 for j,line in enumerate(lines):d.text((14,2+31*j),line,font=font,fill=(241,245,249))
 im.save(out/f'phase-{i}.png')
run('-i',src/'before-collision.mp4','-i',src/'final-collision.mp4',
 '-i',out/'phase-0.png','-i',out/'phase-1.png','-i',out/'phase-2.png','-i',out/'phase-3.png',
 '-filter_complex',"[0:v]crop=960:412:0:128,scale=584:250,pad=584:328:0:0:color=0x101C2B[l];[1:v]crop=960:412:0:128,scale=584:250,pad=584:328:0:0:color=0x101C2B[r];[l][2:v]overlay=0:260:enable='lt(t,3)'[l0];[l0][3:v]overlay=0:260:enable='gte(t,3)'[l1];[r][4:v]overlay=0:260:enable='lt(t,3)'[r0];[r0][5:v]overlay=0:260:enable='gte(t,3)'[r1];[l1][r1]hstack=shortest=1[v]",
 '-map','[v]','-t','10','-an','-c:v','libx264','-crf','19',out/'collision-compare.mp4')
for name,source in [('collision-compare',out/'collision-compare.mp4'),('undo',src/'final-undo.mp4'),('route',src/'final-route.mp4')]:
 if name!='collision-compare':shutil.copy2(source,out/(name+'.mp4'))
 run('-i',source,'-filter_complex','fps=12,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer',out/(name+'.gif'))
run('-ss','6','-i',src/'before-collision.mp4','-frames:v','1',out/'problem.png')
run('-ss','9','-i',src/'final-route.mp4','-frames:v','1',out/'hero.png')
run('-ss','1','-i',src/'final-undo.mp4','-frames:v','1',out/'preview.png')
print(out)

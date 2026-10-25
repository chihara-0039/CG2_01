"""エンジン録画の実映像を左右に並べ、発表用MP4/GIFを作る。"""
from pathlib import Path
import argparse, shutil, subprocess
from PIL import Image, ImageDraw, ImageFont

p=argparse.ArgumentParser()
p.add_argument('--ffmpeg', required=True)
p.add_argument('--workspace', required=True)
a=p.parse_args(); root=Path(a.workspace).resolve(); out=root/'v2-presentation-media'; out.mkdir(exist_ok=True)

def run(*args):
    subprocess.run([a.ffmpeg,'-hide_banner','-loglevel','error','-y',*map(str,args)],check=True)

def gif(source, name):
    run('-i',source,'-filter_complex','fps=12,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer',out/(name+'.gif'))

for name,left,right in [
    ('footpath-compare',root/'v2-dense/footpath-baseline.mp4',root/'v2-dense/footpath-bubble.mp4'),
    ('branch-compare',root/'v2-dense/branch-bubble.mp4',root/'v2-dense/branch-scan.mp4'),
    ('explorer-compare',root/'v2-clean-legacy/character-bubble.mp4',root/'v2-clean/character-bubble.mp4')]:
    dest=out/(name+'.mp4')
    run('-i',left,'-i',right,'-filter_complex','[0:v]scale=584:328,setsar=1[l];[1:v]scale=584:328,setsar=1[r];[l][r]hstack=shortest=1[v]',
        '-map','[v]','-t','10','-an','-c:v','libx264','-crf','20',dest)
    gif(dest,name)

# ラベルは再現配置の各段階を示す。ゲームの描画内容は加工しない。
font=ImageFont.truetype('C:/Windows/Fonts/YuGothB.ttc',25)
labels=['未回収：星に触れてもゴールは開かない','分岐でシャボン玉を回収','回収後：星に触れるとクリア画面へ']
for i,label in enumerate(labels):
    im=Image.new('RGBA',(960,54),(12,22,35,235));ImageDraw.Draw(im).text((18,8),label,font=font,fill='white');im.save(out/f'route-label-{i}.png')
run('-i',root/'v2-route-final/route-scan.mp4',
    '-i',out/'route-label-0.png','-i',out/'route-label-1.png','-i',out/'route-label-2.png',
    '-filter_complex',"[0:v][1:v]overlay=0:480:enable='lt(t,3)'[a];[a][2:v]overlay=0:480:enable='between(t,3,6)'[b];[b][3:v]overlay=0:480:enable='gte(t,6)'[v]",
    '-map','[v]','-t','12','-an','-c:v','libx264','-crf','20',out/'route-scan.mp4')
run('-i',out/'route-scan.mp4','-vf','scale=760:-2','-c:v','libx264','-an',out/'route-preview.mp4')
gif(out/'route-preview.mp4','route-scan')
run('-ss','7','-i',root/'v2-clean/character-bubble.mp4','-frames:v','1',out/'explorer-hero.png')
run('-ss','3','-i',root/'v2-dense/footpath-baseline.mp4','-frames:v','1',out/'problem.png')
for name in ['footpath-baseline','footpath-bubble','branch-scan']:
    shutil.copy2(root/'v2-dense'/(name+'.mp4.json'),out/(name+'.mp4.json'))
print(out)

"""ゲームオーバー画面の文字画像を生成する。実行時は生成済みPNGのみ使用。
再生成には Pillow と NotoSansCJK-Bold.ttc が必要。--font でフォントを指定可能。
"""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont

parser=argparse.ArgumentParser()
parser.add_argument('--font',default='/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc')
args=parser.parse_args()
out=Path(__file__).resolve().parents[1]/'Resources/UI/gameover'
out.mkdir(parents=True,exist_ok=True)
items=[('title','GAME OVER',(1200,200),146,(241,223,174,255)),
       ('subtitle','探検は、まだ終わらない。',(1000,90),48,(175,193,212,255)),
       ('retry','もう一度挑戦',(800,100),62,(255,249,231,255)),
       ('select','ステージ選択へ',(800,100),62,(255,249,231,255)),
       ('controls','↑↓ / W・S / 十字キー：選択　 SPACE / A：決定　 ESC / B：戻る',(1400,88),31,(180,199,219,255))]
for name,text,size,point,color in items:
    image=Image.new('RGBA',size)
    draw=ImageDraw.Draw(image)
    font=ImageFont.truetype(args.font,point)
    bbox=draw.textbbox((0,0),text,font=font,stroke_width=1)
    x=(size[0]-(bbox[2]-bbox[0]))/2-bbox[0]
    y=(size[1]-(bbox[3]-bbox[1]))/2-bbox[1]
    draw.text((x+2,y+5),text,font=font,fill=(0,0,0,160),stroke_width=1)
    draw.text((x,y),text,font=font,fill=color,stroke_width=1)
    image.save(out/(name+'.png'))

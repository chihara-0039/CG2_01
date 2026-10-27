"""提出用ゲームオーバーUIを生成。実行時は同梱PNGのみを使う。
再生成のみPillow・Noto CJK・DejaVu Serifが必要。フォントパスは引数で指定可能。
"""
from pathlib import Path
import argparse
import math
import random
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

parser=argparse.ArgumentParser()
parser.add_argument('--font',default='/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc')
parser.add_argument('--title-font',default='/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf')
args=parser.parse_args()
out=Path(__file__).resolve().parents[1]/'Resources/UI/gameover'
out.mkdir(parents=True,exist_ok=True)
# 一字ずつ動かすための等幅セル。C++側の144x192の切り出しと対応する。
mask=Image.new('L',(1152,192)); draw=ImageDraw.Draw(mask)
font=ImageFont.truetype(args.title_font,120)
for i,char in enumerate('GAMEOVER'):
    bounds=draw.textbbox((0,0),char,font=font)
    draw.text((i*144+72-(bounds[2]-bounds[0])/2-bounds[0],96-(bounds[3]-bounds[1])/2-bounds[1]),char,font=font,fill=255)
gradient=Image.new('RGBA',mask.size)
draw=ImageDraw.Draw(gradient)
stops=[(0,(255,251,226)),(.34,(247,223,155)),(.5,(161,119,55)),(.54,(255,241,190)),(1,(166,124,62))]
for y in range(192):
    t=y/191
    for (a,ca),(b,cb) in zip(stops,stops[1:]):
        if a<=t<=b:
            q=(t-a)/(b-a)
            color=tuple(round(ca[k]+(cb[k]-ca[k])*q) for k in range(3))
            draw.line((0,y,1152,y),fill=color+(255,)); break
image=Image.new('RGBA',mask.size)
shadow=ImageChops.offset(mask,2,5).filter(ImageFilter.GaussianBlur(2))
image.paste((2,5,12,255),(0,0),shadow)
outline=mask.filter(ImageFilter.MaxFilter(5))
image.paste((105,79,45,255),(0,0),outline)
image.paste(gradient,(0,0),mask)
image.save(out/'title.png')
items=[('subtitle','探検は、まだ終わらない。',(1000,90),44,(168,184,203,255)),
       ('retry','もう一度挑戦',(800,100),56,(255,248,225,255)),
       ('select','ステージ選択へ',(800,100),56,(232,238,246,255)),
       ('controls','↑↓ / W・S / 十字キー：選択　 SPACE / A：決定　 ESC / B：戻る',(1400,88),29,(154,176,198,255))]
for name,text,size,point,color in items:
    image=Image.new('RGBA',size); draw=ImageDraw.Draw(image)
    font=ImageFont.truetype(args.font,point)
    b=draw.textbbox((0,0),text,font=font)
    x=(size[0]-(b[2]-b[0]))/2-b[0]; y=(size[1]-(b[3]-b[1]))/2-b[1]
    draw.text((x+1,y+3),text,font=font,fill=(0,0,0,160))
    draw.text((x,y),text,font=font,fill=color)
    image.save(out/(name+'.png'))
# 穏やかな夜空。固定シードのため再生成で見た目が変わらない。
rng=random.Random(2039)
image=Image.new('RGBA',(1280,720)); pixels=image.load()
for y in range(720):
    for x in range(1280):
        r=math.hypot((x-640)/820,(y-250)/590)
        glow=max(0,1-r)**2
        grain=rng.randrange(3)
        pixels[x,y]=(5+round(9*glow)+grain,9+round(17*glow)+grain,17+round(28*glow)+grain,255)
draw=ImageDraw.Draw(image)
for i in range(95):
    x=rng.randrange(100,1180); y=rng.randrange(70,650)
    if 270<x<1010 and 150<y<575: continue
    c=rng.randrange(50,100)
    draw.ellipse((x,y,x+1,y+1),fill=(c,c+12,c+25,255))
# 外周の細い額縁と、四隅の短い目盛り。
draw.rounded_rectangle((44,40,1236,680),radius=12,outline=(38,53,65,255),width=1)
for x in [65,1215]:
    for y in [61,659]:
        sx=1 if x<640 else -1; sy=1 if y<360 else -1
        draw.line((x,y,x+30*sx,y),fill=(139,118,76,255),width=1)
        draw.line((x,y,x,y+21*sy),fill=(139,118,76,255),width=1)
image.save(out/'background.png')
# 回転する方位盤。文字を隠さない透過線画。
image=Image.new('RGBA',(512,512)); draw=ImageDraw.Draw(image)
for r in [176,195,224]:
    draw.ellipse((256-r,256-r,256+r,256+r),outline=(156,185,207,180),width=1)
for i in range(64):
    a=i*math.tau/64; r0=205 if i%8 else 197
    draw.line((256+math.cos(a)*r0,256+math.sin(a)*r0,256+math.cos(a)*218,256+math.sin(a)*218),fill=(180,199,212,200),width=1)
for i in range(8):
    a=i*math.tau/8
    points=[]
    for r,d in [(155,0),(36,.19),(17,0),(36,-.19)]: points.append((256+math.cos(a+d)*r,256+math.sin(a+d)*r))
    draw.polygon(points,fill=(137,165,191,50),outline=(198,216,226,210))
image.save(out/'sigil.png')
# 選択枠は角丸・二重の金縁と淡い内側グロー。
image=Image.new('RGBA',(680,88)); draw=ImageDraw.Draw(image)
draw.rounded_rectangle((3,3,677,85),radius=14,fill=(37,48,56,230),outline=(170,143,84,230),width=2)
draw.rounded_rectangle((9,9,671,79),radius=10,outline=(115,109,84,90),width=1)
draw.polygon([(27,44),(37,38),(47,44),(37,50)],fill=(236,211,151,255))
draw.polygon([(633,44),(643,38),(653,44),(643,50)],fill=(236,211,151,255))
image.save(out/'selection.png')

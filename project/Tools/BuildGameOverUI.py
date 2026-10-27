"""提出用ゲームオーバーUIを生成。実行時は同梱PNGのみを使う。
再生成のみPillow・Noto CJK・DejaVu Sansが必要。フォントパスは引数で指定可能。
"""
from pathlib import Path
import argparse
import math
import random
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

parser=argparse.ArgumentParser()
parser.add_argument('--font',default='/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc')
parser.add_argument('--title-font',default='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf')
args=parser.parse_args()
out=Path(__file__).resolve().parents[1]/'Resources/UI/gameover'
out.mkdir(parents=True,exist_ok=True)
# 一字ずつ動かすための等幅セル。C++側の144x192の切り出しと対応する。
mask=Image.new('L',(1152,192)); draw=ImageDraw.Draw(mask)
font=ImageFont.truetype(args.title_font,122)
for i,char in enumerate('GAMEOVER'):
    bounds=draw.textbbox((0,0),char,font=font)
    draw.text((i*144+72-(bounds[2]-bounds[0])/2-bounds[0],96-(bounds[3]-bounds[1])/2-bounds[1]),char,font=font,fill=255,stroke_width=2)
gradient=Image.new('RGBA',mask.size)
draw=ImageDraw.Draw(gradient)
stops=[(0,(255,251,201)),(.3,(255,194,99)),(.6,(255,100,77)),(1,(217,51,103))]
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
glow=mask.filter(ImageFilter.GaussianBlur(7))
image.paste((69,155,255,240),(0,0),glow)
outline=mask.filter(ImageFilter.MaxFilter(9))
image.paste((244,253,255,255),(0,0),outline)
image.paste(gradient,(0,0),mask)
image.save(out/'title.png')
items=[('subtitle','星の向こうへ、もう一度。',(1000,90),44,(168,184,203,255)),
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
        nebula=math.exp(-((y-350-80*math.sin(x/340))/150)**2)*max(0,1-r)
        pixels[x,y]=(2+round(9*glow+9*nebula)+grain,4+round(14*glow+12*nebula)+grain,14+round(35*glow+24*nebula)+grain,255)
draw=ImageDraw.Draw(image)
for i in range(145):
    x=rng.randrange(100,1180); y=rng.randrange(70,650)
    if 270<x<1010 and 150<y<575: continue
    c=rng.randrange(50,100)
    draw.ellipse((x,y,x+1,y+1),fill=(c,c+12,c+25,255))
image.save(out/'background.png')
# 宇宙をゆっくり流れる淡い渦。既存作品の画像は使わず数式で生成する。
image=Image.new('RGBA',(512,512)); draw=ImageDraw.Draw(image)
for arm in range(3):
    for j in range(180):
        t=j/180
        a=arm*math.tau/3+t*5.5
        r=10+210*t
        x=256+math.cos(a)*r; y=256+math.sin(a)*r*.7
        dot=2+3*(1-t)
        draw.ellipse((x-dot,y-dot,x+dot,y+dot),fill=(105,160,255,round((1-t)*100)))
image=image.filter(ImageFilter.GaussianBlur(4)); image.save(out/'sigil.png')
# 浮かぶ青いボタン。白い縁と丸い光で、硬い額縁から軽い宇宙のUIへ。
image=Image.new('RGBA',(680,88)); draw=ImageDraw.Draw(image)
draw.rounded_rectangle((5,4,675,84),radius=39,fill=(31,96,158,235),outline=(208,246,255,255),width=3)
draw.rounded_rectangle((12,10,668,78),radius=34,outline=(78,173,230,200),width=2)
draw.arc((35,13,645,65),180,360,fill=(143,215,255,140),width=2)
draw.polygon([(31,44),(44,33),(44,55)],fill=(255,251,209,255))
draw.polygon([(649,44),(636,33),(636,55)],fill=(255,251,209,255))
image.save(out/'selection.png')
# 小さな星の光芒。RGBAの柔らかい輪郭で画面上を漂わせる。
image=Image.new('RGBA',(64,64)); pixels=image.load()
for y in range(64):
    for x in range(64):
        dx=abs(x-31.5)/31.5; dy=abs(y-31.5)/31.5
        core=math.exp(-(dx*dx+dy*dy)*35)
        rays=max(0,1-(dx**.55+dy**.55))**2
        alpha=min(1,core*.65+rays)
        pixels[x,y]=(233,247,255,round(alpha*255))
image.save(out/'sparkle.png')

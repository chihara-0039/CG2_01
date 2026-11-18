# 空の旅に合わせた、丸いパネル・星・文字の共通設定。
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import math
INK='#173f70'
MUTED='#587d9d'
TEAL='#167cbe'
GOLD='#efab32'
PAPER=(238,250,255,246)

def font(n,serif=False):
    # 見出しは丸みのある教科書体、本文は読みやすいゴシック体。
    name='UDDigiKyokashoN-B.ttc' if serif else 'YuGothB.ttc'
    return ImageFont.truetype(str(Path('C:/Windows/Fonts')/name),n)

def sparkle(d,x,y,r=7,color='#ffffff'):
    d.polygon([(x,y-r),(x+r*.25,y-r*.25),(x+r,y),(x+r*.25,y+r*.25),
               (x,y+r),(x-r*.25,y+r*.25),(x-r,y),(x-r*.25,y-r*.25)],fill=color)

def badge_star(d,x,y,r=15):
    points=[]
    for i in range(10):
        a=-math.pi/2+i*math.pi/5
        radius=r if i%2==0 else r*.48
        points.append((x+math.cos(a)*radius,y+math.sin(a)*radius))
    d.polygon([(a,b+2) for a,b in points],fill='#c17a25')
    d.polygon(points,fill='#ffe571',outline='#dca637',width=2)
    sparkle(d,x-3,y-5,max(2,r*.2))

# 半透明の青い縁、白い反射、丸い角で空に浮くパネルを描く。
def sheet(im,box):
    x,y,r,b=map(int,box);w=r-x+1;h=b-y+1
    radius=min(24,max(10,h//3))
    d=ImageDraw.Draw(im)
    d.rounded_rectangle((x+2,y+5,r+2,b+5),radius=radius,fill=(15,50,100,65))
    d.rounded_rectangle(box,radius=radius,fill=(78,157,214,230),outline=(236,254,255,255),width=2)
    d.rounded_rectangle((x+4,y+4,r-4,b-4),radius=max(6,radius-4),fill=PAPER,outline=(138,208,241,255),width=1)
    d.arc((x+9,y+7,r-9,min(b-5,y+max(18,h//2))),185,350,fill=(255,255,255,240),width=2)
    if h>65:
        sparkle(d,r-22,y+20,6,'#9ad7f1')
        d.ellipse((r-37,y+30,r-33,y+34),fill='#b5e4f6')
    return d

def text(d,at,s,n=22,color=INK,serif=False):
    d.text(at,s,font=font(n,serif),fill=color)

def centered(d,box,s,n=22,color=INK,serif=False):
    f=font(n,serif);b=d.textbbox((0,0),s,font=f)
    d.text(((box[0]+box[2]-b[2]+b[0])/2-b[0],(box[1]+box[3]-b[3]+b[1])/2-b[1]),s,font=f,fill=color)

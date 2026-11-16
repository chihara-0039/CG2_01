# 紙面・文字・色の共通設定を提供し、ゲーム画面のUIをそろえる。
"""Shared field-journal palette, typography and panel treatment for every game screen."""
from pathlib import Path
from PIL import ImageDraw,ImageFont
INK='#243c43'
MUTED='#657476'
TEAL='#286d70'
GOLD='#b68a39'
PAPER=(244,238,220,245)

def font(n,serif=False):
    return ImageFont.truetype(str(Path('C:/Windows/Fonts')/('yumin.ttf' if serif else 'YuGothM.ttc')),n)

# 共通の紙色・罫線・影を描き、UIパネルの描画先を返す。
def sheet(im,box):
    d=ImageDraw.Draw(im);x,y,r,b=box
    d.rectangle((x+3,y+4,r+3,b+4),fill=(12,32,39,50))
    d.rectangle(box,fill=PAPER,outline=(160,146,110,210),width=1)
    d.line((x+8,y+8,r-8,y+8),fill=(211,201,172,190),width=1)
    d.line((x+8,b-8,r-8,b-8),fill=(211,201,172,160),width=1)
    return d

def text(d,at,s,n=22,color=INK,serif=False):
    d.text(at,s,font=font(n,serif),fill=color)

def centered(d,box,s,n=22,color=INK,serif=False):
    f=font(n,serif);b=d.textbbox((0,0),s,font=f)
    d.text(((box[0]+box[2]-b[2]+b[0])/2-b[0],(box[1]+box[3]-b[3]+b[1])/2-b[1]),s,font=f,fill=color)

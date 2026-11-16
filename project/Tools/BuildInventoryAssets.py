# 道具袋に表示するパーツのアイコン素材を生成する。
"""Original inventory voxel icon and compact native guide."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
out=Path(__file__).resolve().parents[1]/'Resources/UI/inventory'
im=Image.new('RGBA',(64,64));d=ImageDraw.Draw(im)
d.polygon([(32,2),(62,17),(32,33),(2,17)],fill=(255,255,255,255))
d.polygon([(2,17),(32,33),(32,62),(2,46)],fill=(155,165,177,255))
d.polygon([(32,33),(62,17),(62,46),(32,62)],fill=(204,217,229,255))
d.line([(2,17),(32,2),(62,17),(62,46),(32,62),(2,46),(2,17)],fill=(58,67,78,255),width=2)
im.save(out/'part-voxel.png')
im=Image.new('RGBA',(440,140),(18,32,45,235));d=ImageDraw.Draw(im)
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',25)
for y,text in [(5,'1-5 PARTS   6-0 STOCK'),(48,'I REUSE   Z UNDO'),(91,'B INVENTORY / CLICK EQUIP')]:d.text((8,y),text,font=font,fill=(225,246,255,255))
im.save(out/'quick-guide.png')

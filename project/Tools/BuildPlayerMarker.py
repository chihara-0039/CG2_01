# 壁越しのプレイヤー位置を示す足元マーカーを生成する。
"""Generate the native HUD foot marker; no external artwork or fonts."""
from pathlib import Path
from PIL import Image, ImageDraw
image=Image.new('RGBA',(256,128))
draw=ImageDraw.Draw(image)
draw.ellipse((6,6,249,121),outline=(7,23,34,240),width=18)
draw.ellipse((10,10,245,117),outline=(96,232,255,255),width=8)
draw.ellipse((111,47,145,81),fill=(7,23,34,255))
draw.ellipse((119,55,137,73),fill=(234,255,255,255))
image.save(Path(__file__).resolve().parents[1]/'Resources/UI/player-location.png')

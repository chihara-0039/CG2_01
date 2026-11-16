# 道具袋の名前と個数表示に使う文字アトラスを生成する。
"""Build the sprite label atlas used in both Release and Development inventory."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from AdventureTheme import font as theme_font
font = theme_font(16)
atlas = Image.new('RGBA', (256, 144))
draw = ImageDraw.Draw(atlas)
for glyph, code in enumerate(range(32, 127)):
    draw.text(((glyph % 16)*16, (glyph // 16)*24), chr(code), font=font,
              fill='white', stroke_width=0)
atlas.save(Path(__file__).resolve().parents[1] / 'Resources/UI/inventory/labels.png')

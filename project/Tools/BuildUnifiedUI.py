# 共通テーマを使って、ゲーム内のUI素材をまとめて生成する。
"""Rebuild the journal UI and replace remaining legacy screen assets consistently."""
from pathlib import Path
from PIL import Image,ImageDraw
import BuildAdventureUI
import BuildInventoryFont
from AdventureTheme import *
root=Path(__file__).resolve().parents[1]
out=root/'Resources/UI/adventure'

for pad in (False,True):
    im=Image.new('RGBA',(1000,44));d=sheet(im,(0,0,996,39))
    centered(d,(12,3,984,37),'A  探検に出発      B  終了' if pad else 'Enter / Space  探検に出発      Esc  終了',20)
    im.save(out/f'title-controls{"-pad" if pad else ""}.png')
    im=Image.new('RGBA',(1000,120));d=sheet(im,(0,0,996,115))
    text(d,(24,19),'道具袋から、道をつくる',27,INK,True)
    text(d,(24,66),'十字キーで選択 → Aで持ち出す　 Bで戻る' if pad else 'クリックで選んで配置へ　 1～0で直接選択　 B / Escで戻る',20,TEAL)
    im.save(out/f'inventory-help{"-pad" if pad else ""}.png')
    im=Image.new('RGBA',(1000,155));d=sheet(im,(0,0,996,150))
    text(d,(24,15),'探検の手帳',27,INK,True)
    text(d,(24,56),'すべての鍵を集めてスターへ。シャボン玉は足場をつくるための道具。',19)
    text(d,(24,88),'A  ジャンプ    X  扉・スイッチ    LB  道具袋' if pad else 'Space  ジャンプ    F  扉・スイッチ    B  道具袋',19,TEAL)
    text(d,(24,120),'足場を踏み外した直後もジャンプできます。　F1で手帳を閉じる',17,MUTED)
    im.save(out/f'help{"-pad" if pad else ""}.png')

names=BuildAdventureUI.names
details=['足場づくりを試しながら、2個の鍵を探そう。','扉とスイッチを見回して、4個の鍵へ。','細い道と動く足場を越えて、3個の鍵を探そう。','回収・配置・仕掛けを組み合わせ、6個の鍵と頂上の星へ。']
for i,(title,detail) in enumerate(zip(names,details)):
    im=Image.new('RGBA',(960,72));d=sheet(im,(0,0,956,67))
    text(d,(20,10),f'{i+1:02}  {title}',23,INK,True);text(d,(20,40),detail,17,MUTED)
    im.save(root/f'Resources/UI/campaign/objective-{i}.png')
im=Image.new('RGBA',(640,72));d=sheet(im,(0,0,636,67))
text(d,(20,12),'すべての鍵を集めて、星のもとへ',24,INK,True)
text(d,(20,43),'シャボン玉は配置用パーツ。道をつくって探検しよう。',16,MUTED)
im.save(out/'objective.png')

go=root/'Resources/UI/gameover'
for name,label,size,n,serif in [('title','探検は、ここから',(900,150),56,True),('subtitle','ひと息ついたら、もう一度。',(800,65),26,False),('retry','もう一度挑戦する',(600,70),30,False),('select','行き先を選びなおす',(600,70),30,False),('controls','↑ ↓ / 十字キー  選択     Enter / Space / A  決定     Esc / B  戻る',(900,56),19,False)]:
    im=Image.new('RGBA',size);d=ImageDraw.Draw(im);centered(d,(0,0,*size),label,n,INK if name!='subtitle' else MUTED,serif);im.save(go/(name+'.png'))
im=Image.new('RGBA',(1280,720),(14,34,39,150));sheet(im,(230,134,1050,665));im.save(go/'background.png')
im=Image.new('RGBA',(670,82));d=ImageDraw.Draw(im)
d.rectangle((5,4,664,77),fill=(40,109,112,26));d.line((6,5,6,76),fill=TEAL,width=4)
im.save(go/'selection.png')
print('Unified title, objectives, controls, inventory help and retry menu')

for name,follow in [('stage_overview_icon',False),('follow_player_icon',True)]:
    im=Image.new('RGBA',(64,64));d=sheet(im,(1,1,59,59))
    if follow:
        d.ellipse((24,15,38,29),outline=TEAL,width=2);d.arc((16,25,46,51),180,360,fill=TEAL,width=2)
    else:
        d.polygon([(15,25),(31,15),(47,25),(31,35)],outline=TEAL)
        d.line([(15,25),(15,39),(31,49),(47,39),(47,25)],fill=TEAL,width=2);d.line((31,35,31,49),fill=TEAL,width=2)
    im.save(root/f'Resources/UI/{name}.png')

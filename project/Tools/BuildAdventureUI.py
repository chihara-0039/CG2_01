# ステージ選択・鍵HUD・操作説明など、探検手帳風のUI画像を生成する。
"""Sky-travel UI: rounded blue panels, golden stars and friendly Japanese headings."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import math,random
import sys
root=Path(__file__).resolve().parents[1]
out=root/'Resources/UI/adventure';out.mkdir(exist_ok=True)
INK='#243c43';MUTED='#657476';TEAL='#286d70';GOLD='#b68a39';PAPER=(244,238,220,245)
def font(n,serif=False):
    files=['yumin.ttf','msmincho.ttc','YuGothM.ttc'] if serif else ['YuGothM.ttc']
    return ImageFont.truetype(next(str(Path('C:/Windows/Fonts')/f) for f in files if (Path('C:/Windows/Fonts')/f).exists()),n)
# 共通の紙色・罫線・影を描き、UIパネルの描画先を返す。
def sheet(im,box):
    d=ImageDraw.Draw(im);x,y,r,b=box
    d.rectangle((x+4,y+5,r+4,b+5),fill=(12,32,39,55));d.rectangle(box,fill=PAPER)
    d.line([(x+1,b),(x+1,y+1),(r-1,y+1),(r-1,b-1)],fill=(110,103,78,170),width=1)
    d.line((x+7,y+8,r-8,y+8),fill=(211,201,172,190),width=1)
    d.polygon([(r-15,y),(r,y+15),(r,y)],fill=(225,215,189,255))
    return d
def text(d,at,s,n=22,color=INK,serif=False):d.text(at,s,font=font(n,serif),fill=color)
# Override the legacy local helpers with the shared design system.
from AdventureTheme import INK,MUTED,TEAL,GOLD,PAPER,font,sheet,text,badge_star,sparkle

# 真鍮色の鍵の輪と軸を描く。
def key(d,x,y,scale=1,color=GOLD):
    s=scale;d.ellipse((x,y,x+18*s,y+18*s),outline=color,width=max(2,int(4*s)))
    d.line((x+15*s,y+15*s,x+35*s,y+35*s),fill=color,width=max(2,int(5*s)))
    d.line((x+26*s,y+26*s,x+33*s,y+19*s),fill=color,width=max(2,int(4*s)))
    d.line((x+32*s,y+32*s,x+39*s,y+25*s),fill=color,width=max(2,int(4*s)))
# 交互の半径から星の輪郭を作り、アイコンを描く。
def star(d,x,y,r=24):
    badge_star(d,x,y,r)
names=['はじめての探検','鍵とスイッチの庭','空中回廊','総合探検']

# 通常プレイの操作帯だけを再生成できるよう、他のUI生成から分離する。
def build_play_controls(pad):
    im=Image.new('RGBA',(1000,40));d=sheet(im,(0,0,996,35))
    text(d,(15,6),'左スティック  移動    右スティック  視点    LB  道具袋    START  休憩' if pad else 'WASD  移動    マウス / 矢印  視点    Alt  カーソル    B  道具袋    F1  手帳    Esc  休憩',18)
    im.save(out/f'play{"-pad" if pad else ""}.png')

# 配置中も同じフリールック操作を案内する。
def build_placement_controls(pad):
    im=Image.new('RGBA',(1200,44));d=sheet(im,(0,0,1196,39))
    text(d,(15,7),'左スティック  位置    LB/RB  高さ    X  回転    A  配置    右スティック  視点    B  戻る' if pad else 'WASD  位置    Q/E  高さ    R  回転    左クリック / Enter  配置    マウス  視点    Alt  カーソル    Z  取消    B  戻る',18)
    im.save(out/f'place{"-pad" if pad else ""}.png')

if __name__=='__main__' and ('--play-only' in sys.argv or '--controls-only' in sys.argv):
    for pad in (False,True):build_play_controls(pad)
    if '--controls-only' in sys.argv:
        for pad in (False,True):build_placement_controls(pad)
    raise SystemExit(0)

sub=['足場づくりの第一歩','仕掛けの先にある道','高い島へ、もう一歩','すべての経験をひとつに']
files=['beginnerTrail.txt','mechanismGardens.txt','skylineRelay.txt','expeditionGrandTour.txt']
counts=[]
for f in files:
    cells=[l.split() for l in (root/'Resources/Stages'/f).read_text().splitlines()[1:]]
    counts.append((sum(len(a)>=7 and a[3]=='14' for a in cells),sum(len(a)>=7 and a[3]=='5' for a in cells)))
for i in range(4):
    for pad in (False,True):
        for failed in (False,True):
            im=Image.new('RGBA',(1280,720));d=sheet(im,(28,150,350,570))
            badge_star(d,48,119,13);text(d,(70,103),'空の旅へ！',22,'#ffffff',True)
            text(d,(51,169),'どの島へ行こう？',25,INK,True);d.line((55,210,323,210),fill='#afdaef',width=2)
            for j,name in enumerate(names):
                y=232+j*76
                if i==j:
                    d.rounded_rectangle((39,y-8,339,y+60),radius=19,fill='#269cda',outline='#ffffff',width=2);sparkle(d,322,y+1,6)
                d.ellipse((51,y+5,84,y+38),fill='#ffdf73' if i==j else '#deeff8',outline='#ffffff',width=2)
                text(d,(56,y+10),f'{j+1:02}',14,INK if i==j else MUTED)
                text(d,(96,y),name,24 if i==j else 21,'#ffffff' if i==j else INK,True)
                text(d,(96,y+33),sub[j],12,'#e0f8ff' if i==j else MUTED)
            d=sheet(im,(485,63,1218,149));badge_star(d,516,103,18);text(d,(548,71),names[i],34,INK,True)
            text(d,(510,118),f'かぎ {counts[i][0]} 個   /   道具入りのシャボン玉 {counts[i][1]} 個',17,MUTED)
            if failed:text(d,(508,158),'この島を読み込めません。別の島を選んでください。',18,'#7b372e')
            d=sheet(im,(28,651,1251,695))
            text(d,(48,660),'十字キー  選択      A  出発      右スティック  島を見回す      B  戻る' if pad else '← →  選択      Enter / Space  出発      右ドラッグ  島を見回す      Esc  戻る',19)
            im.save(out/f'select-{"error-" if failed else ""}{i}{"-pad" if pad else ""}.png')
# 選択画面だけの修正時は、他のUI素材を再生成しない。
if __name__=='__main__' and '--selection-only' in sys.argv:
    print('Selection UI generated')
    raise SystemExit(0)

for total in range(17):
    for n in range(total+1):
        im=Image.new('RGBA',(244,76));d=sheet(im,(0,0,239,70));key(d,16,15,.85)
        text(d,(64,4),'かぎ',15,MUTED);text(d,(109,0),f'{n} / {total}',31,INK,True)
        text(d,(64,43),'スターが解放された' if n==total else 'すべて集めて、星のもとへ',13,TEAL if n==total else MUTED)
        im.save(out/f'keys-{n}-{total}.png')
im=Image.new('RGBA',(244,76));d=sheet(im,(0,0,239,70));key(d,16,15,.85);text(d,(64,20),'鍵を集めて、星へ',17);im.save(out/'keys-many.png')
for pad in (False,True):
    build_play_controls(pad)
    build_placement_controls(pad)
    im=Image.new('RGBA',(1000,155));d=sheet(im,(0,0,996,150));text(d,(22,12),'探検の手帳',27,INK,True)
    text(d,(23,54),'鍵をすべて集めると、スターを取れる。金色の鍵は触れると回収できる。',19)
    text(d,(23,86),'扉用の鍵とスイッチは F で操作。シャボン玉は道具袋に入る配置用パーツ。',19)
    text(d,(23,118),'B  道具袋     1〜0  パーツ選択     Z  直前の配置を戻す     F1  手帳を閉じる',17,TEAL)
    im.save(out/f'help{"-pad" if pad else ""}.png')
    im=Image.new('RGBA',(1280,720));d=sheet(im,(324,180,956,530));star(d,640,221,25)
    text(d,(500,264),'探 検 完 了',48,INK,True);text(d,(460,342),'すべての鍵を見つけ、星を手に入れた。',20,MUTED)
    d.line((370,399,910,399),fill='#afdaef',width=1)
    text(d,(414,428),'A   次の行き先を選ぶ' if pad else 'Enter / Space   次の行き先を選ぶ',24,TEAL)
    text(d,(539,478),'B   タイトルへ' if pad else 'Esc   タイトルへ',18,MUTED)
    im.save(out/f'clear{"-pad" if pad else ""}.png')
    im=Image.new('RGBA',(1280,720));d=sheet(im,(360,200,920,520));text(d,(485,239),'ひとやすみ',42,INK,True)
    for y,label in zip((335,391,447),('START / A / B   探検を続ける','Y   行き先を選ぶ','X   タイトルへ') if pad else ('Esc   探検を続ける','Enter   行き先を選ぶ','T   タイトルへ')):text(d,(439,y),label,24,TEAL)
    im.save(out/f'pause{"-pad" if pad else ""}.png')
im=Image.new('RGBA',(520,110));d=sheet(im,(35,10,484,93));star(d,86,50,23);text(d,(133,28),'星を手に入れた',32,INK,True);im.save(out/'star-get.png')
im=Image.new('RGBA',(420,720));d=sheet(im,(145,0,419,719));text(d,(173,28),'道具袋',33,INK,True);text(d,(174,65),'シャボン玉から回収したパーツ',13,MUTED);im.save(out/'inventory.png')
im=Image.new('RGBA',(220,70));d=ImageDraw.Draw(im);text(d,(0,0),'クリックで選んで配置へ',16,TEAL);text(d,(0,27),'B / Esc で閉じる',14,MUTED);im.save(out/'inventory-guide.png')
print('Expedition journal UI generated; four course sheets and key progress badges')

im=Image.new('RGBA',(48,48));d=ImageDraw.Draw(im);d.ellipse((2,2,45,45),outline=(90,112,112,130),width=1)
d.polygon([(24,3),(8,36),(24,29),(40,36)],fill=TEAL,outline=INK);im.save(out/'compass.png')
im=Image.new('RGBA',(1000,110));d=sheet(im,(0,0,996,105));text(d,(16,8),'回転する島の実験',25,INK,True)
text(d,(17,45),'台座の近くで F / Y：90度回転    Shift+F / LB+Y：逆回転',19,TEAL)
text(d,(17,78),'鍵を3個集めてスターへ。シャボン玉は配置用パーツ。島の上のパーツも一緒に回る。',18)
im.save(root/'Resources/UI/campaign/rotation-trial.png')
im=Image.new('RGBA',(1000,40));d=sheet(im,(0,0,996,35));text(d,(12,6),'F / Y  台座で回転    Shift+F / LB+Y  逆回転    B  道具袋    F1  手帳',18)
im.save(root/'Resources/UI/campaign/rotation-minimal.png')

# A real hollow-ring collectible key, matching the brass key used in the HUD.
import recover_models
modeldir=root/'Resources/Models/Adventure';modeldir.mkdir(exist_ok=True)
mesh=recover_models.Mesh()
for i in range(16):
    a=i*math.tau/16;b=(i+1)*math.tau/16
    def point(angle,r,z):return (math.cos(angle)*r,.16+math.sin(angle)*r,z)
    for depth in (-.055,.055):
        v=[point(a,.20,depth),point(b,.20,depth),point(b,.115,depth),point(a,.115,depth)]
        mesh.triangle(v[0],v[1],v[2]);mesh.triangle(v[0],v[2],v[3])
    for radius in (.20,.115):
        v=[point(a,radius,-.055),point(b,radius,-.055),point(b,radius,.055),point(a,radius,.055)]
        mesh.triangle(v[0],v[1],v[2]);mesh.triangle(v[0],v[2],v[3])
mesh.box((0,-.22,0),(.085,.47,.11))
mesh.box((.065,-.30,0),(.19,.08,.11));mesh.box((.065,-.42,0),(.19,.08,.11))
(modeldir/'key.obj').write_text('\n'.join(mesh.lines)+'\n')

im=Image.new('RGBA',(220,70));d=ImageDraw.Draw(im);text(d,(0,0),'十字キーで選択 / A 持ち出す',14,TEAL);text(d,(0,27),'B で閉じる',14,MUTED);im.save(out/'inventory-guide-pad.png')

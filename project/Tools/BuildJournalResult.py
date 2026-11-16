# クリア時の手帳・印刷面・ハンコ・印影の3D素材を生成する。
"""Original 3D field journal and rubber-stamp result props; no external assets."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageFilter,ImageChops
import math,random
import recover_models
root=Path(__file__).resolve().parents[1]
out=root/'Resources/Models/Journal';out.mkdir(exist_ok=True)
def font(n,serif=False):return ImageFont.truetype('C:/Windows/Fonts/yumin.ttf' if serif else 'C:/Windows/Fonts/YuGothM.ttc',n)
from AdventureTheme import INK,TEAL,GOLD,PAPER,font,sheet
ink=INK;teal=TEAL;brass=GOLD;paper=PAPER[:3]
# 共通の紙色と罫線から、手帳の印刷用ページを用意する。
def page():
 im=Image.new('RGB',(768,1024),paper);d=ImageDraw.Draw(im)
 for x in range(7,28,4):d.line((x,20,x,1000),fill='#c5b799',width=1)
 d.line((54,72,712,72),fill='#b6a987',width=2)
 for y in range(790,950,37):d.line((60,y,705,y),fill='#d0c5ab')
 return im,d
files=['beginnerTrail.txt','mechanismGardens.txt','skylineRelay.txt','expeditionGrandTour.txt','rotationTrial.txt','rotationBridge.txt','rotationRelay.txt']
names=['はじめての探検','鍵とスイッチの庭','空中回廊','総合探検','回転する島の実験','風見の橋と寄り道の鍵','三つの橋をつなぐ空路']
for i,(file,name) in enumerate(zip(files,names)):
 lines=(root/'Resources/Stages'/file).read_text(encoding='utf-8-sig').splitlines();cells=[]
 for line in lines[1:]:
  a=line.split()
  if len(a)>=7 and all(v.lstrip('-').isdigit() for v in a[:4]):cells.append(tuple(map(int,a[:4])))
 keys=sum(c[3]==14 for c in cells)
 im,d=page();d.text((58,98),'探検報告',font=font(55,True),fill=ink)
 d.text((60,188),name,font=font(36,True),fill=ink);d.line((60,257,705,257),fill=brass,width=3)
 d.text((65,290),'鍵の回収',font=font(27),fill=ink);d.text((478,278),f'{keys} / {keys}',font=font(43,True),fill=teal)
 d.text((65,370),'すべての鍵を見つけ、',font=font(26),fill=ink)
 d.text((65,414),'この島の星を手に入れた。',font=font(26),fill=ink)
 d.text((61,936),'回転の国の探検隊  /  踏破記録',font=font(20),fill='#7e775f');im.save(out/f'report-{i}.png')
 im,d=page();d.text((58,98),'旅の見取り図',font=font(47,True),fill=ink)
 # Real footprint from the completed course, printed as a small field sketch.
 blocks={(x,z) for x,y,z,t in cells if t in (1,2,3,11,12,13,22,24,25)}
 minx=min(x for x,z in blocks);maxx=max(x for x,z in blocks);minz=min(z for x,z in blocks);maxz=max(z for x,z in blocks)
 scale=min(610/(maxx-minx+1),510/(maxz-minz+1));ox=(768-(maxx-minx+1)*scale)/2;oy=265
 for x,z in sorted(blocks):
  px=ox+(x-minx)*scale;py=oy+(z-minz)*scale
  d.rectangle((px,py,px+scale-.7,py+scale-.7),fill='#a6b49c',outline='#6e8979')
 d.text((61,824),'雲の海を越えた先で、',font=font(26,True),fill=ink)
 d.text((61,866),'またひとつ、道を見つけた。',font=font(26,True),fill=ink)
 d.text((61,936),f'FIELD NOTE   {i+1:02}',font=font(20),fill='#7e775f');im.save(out/f'map-{i}.png')
im=Image.new('RGB',(768,1024),'#24484a');d=ImageDraw.Draw(im)
for pad in (24,36):d.rectangle((pad,pad,767-pad,1023-pad),outline=brass,width=3)
d.text((151,340),'探検手帳',font=font(105,True),fill='#d9bd79');d.text((160,485),'回転の国の探検隊',font=font(39),fill='#c5b174')
d.line((161,568,606,568),fill=brass,width=3);d.text((240,816),'FIELD JOURNAL',font=font(27),fill='#b6a16e');im.save(out/'cover.png')
im=Image.new('RGBA',(512,512));d=ImageDraw.Draw(im)
d.ellipse((24,24,487,487),outline='#a34432',width=14);d.ellipse((44,44,467,467),outline='#a34432',width=4)
d.text((104,142),'踏破',font=font(142,True),fill='#a34432');d.text((125,330),'探 検 隊 証 明',font=font(30),fill='#a34432')
noise=Image.effect_noise(im.size,43).point(lambda p:180 if p<100 else 255)
im.putalpha(ImageChops.multiply(im.getchannel('A'),noise));im.save(out/'ink.png')
shadow=Image.new('RGBA',(256,256));d=ImageDraw.Draw(shadow);d.ellipse((34,45,222,211),fill=(21,28,22,105));shadow=shadow.filter(ImageFilter.GaussianBlur(15));shadow.save(out/'shadow.png')
palette=Image.new('RGB',(40,24));d=ImageDraw.Draw(palette)
for i,c in enumerate([(36,72,74),(225,215,184),(112,73,41),(35,34,30),(172,137,69)]):
 for j,f in enumerate((.67,.83,1)):
  d.rectangle((i*8,j*8,i*8+7,j*8+7),fill=tuple(int(v*f) for v in c))
palette.save(out/'palette.png')
def material(name,texture):
 (out/f'{name}.mtl').write_text(f'newmtl {name}\nKd 1 1 1\nmap_Kd {texture}\n')
# 複数の箱を指定色で組み合わせ、手帳やハンコのモデルを保存する。
def box_model(name,boxes):
 mesh=recover_models.Mesh();mesh.lines+=['mtllib palette.mtl','usemtl palette']
 for at,size,color in boxes:
  at=(at[0],at[1],-at[2]);first=len(mesh.lines);mesh.box(at,size);n=0
  for j in range(first,len(mesh.lines)):
   if mesh.lines[j].startswith('vt '):
    face=n//6;row=2 if face in (1,4) else 0 if face==5 else 1
    mesh.lines[j]=f'vt {(color+.5)/5} {1-(row+.5)/3}';n+=1
 (out/f'{name}.obj').write_text('\n'.join(mesh.lines)+'\n')
material('palette','palette.png')
box_model('cover', [((3.2,0,0),(6.4,8.5,.24),0)])
box_model('back', [((3.2,0,.42),(6.4,8.5,.18),0),((0,0,.20),(.22,8.55,.60),0)])
box_model('pages',[((3.16,0,.18),(6.08,8.12,.32),1)]+[((3.16,y,-.005),(6.08,.012,.018),1) for y in (-3.95,-3.9,-3.85)])
box_model('stamp', [((0,0,.22),(2.55,2.55,.22),3),((0,0,.03),(2.65,2.65,.18),4),
                    ((0,0,-.74),(.50,.50,1.42),2),((0,0,-1.52),(1.0,.85,.35),2)])
# ページ画像と対応するUVを持つ板を生成し、OBJと材質を保存する。
def quad(name,texture,x0,y0,x1,y1,z,reverse=False):
 material(name,texture);m=recover_models.Mesh();m.lines += [f'mtllib {name}.mtl',f'usemtl {name}']
 p=[(x0,y0,-z),(x1,y0,-z),(x1,y1,-z),(x0,y1,-z)]
 u=[(1 if reverse else 0,0),(0 if reverse else 1,0),(0 if reverse else 1,1),(1 if reverse else 0,1)]
 m.triangle(p[0],p[1],p[2],(u[0],u[1],u[2]));m.triangle(p[0],p[2],p[3],(u[0],u[2],u[3]))
 (out/f'{name}.obj').write_text('\n'.join(m.lines)+'\n')
quad('cover-art','cover.png',.04,-4.20,6.36,4.20,-.126)
for i in range(len(files)):
 quad(f'report-{i}',f'report-{i}.png',.14,-4.02,6.18,4.02,-.025)
 quad(f'map-{i}',f'map-{i}.png',.14,-4.02,6.18,4.02,.126,True)
quad('ink','ink.png',-1.48,-1.48,1.48,1.48,0)
quad('shadow','shadow.png',-1.65,-1.65,1.65,1.65,0)
for pad in (False,True):
 im=Image.new('RGBA',(1280,720));d=sheet(im,(256,653,1024,695))
 d.text((281,662),'A  次の行き先へ       B  タイトルへ' if pad else 'Enter / Space  次の行き先へ       Esc  タイトルへ',font=font(21),fill=ink)
 im.save(out/f'guide{"-pad" if pad else ""}.png')
print('3D journal covers, pages, seven course reports, stamp and ink generated')

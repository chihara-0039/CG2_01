from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
import math
OUT=Path(__file__).resolve().parent
FONT='C:/Windows/Fonts/YuGothM.ttc'
BOLD='C:/Windows/Fonts/YuGothB.ttc'
INK='#243849'; BLUE='#235d82'
def font(n=25,b=False): return ImageFont.truetype(BOLD if b else FONT,n)
def canvas(title,sub):
 im=Image.new('RGB',(2400,1380),'white');d=ImageDraw.Draw(im)
 d.text((85,50),title,font=font(44,True),fill=INK)
 d.text((85,118),sub,font=font(25),fill=INK)
 d.line((85,172,2315,172),fill=BLUE,width=3)
 return im,d
def box(d,x,y,w,title,fields,methods,abstract=False):
 h1=65;h2=max(55,20+len(fields)*37);h3=max(55,20+len(methods)*37)
 d.rectangle((x,y,x+w,y+h1+h2+h3),fill='white',outline=INK,width=3)
 d.rectangle((x,y,x+w,y+h1),fill='#e7f0f5',outline=INK,width=3)
 d.text((x+18,y+14),title,font=font(27,True),fill=INK)
 d.line((x,y+h1+h2,x+w,y+h1+h2),fill=INK,width=2)
 for i,t in enumerate(fields): d.text((x+18,y+h1+10+i*37),t,font=font(23),fill=INK)
 for i,t in enumerate(methods): d.text((x+18,y+h1+h2+10+i*37),t,font=font(23),fill=INK)
 return (x,y,w,h1+h2+h3)
def edge(d,pts,kind='assoc',label=None,at=None):
 if kind=='dep':
  for a,b in zip(pts,pts[1:]):
   dist=math.dist(a,b)
   for n in range(0,int(dist),20):
    q=min(n+11,dist); d.line((a[0]+(b[0]-a[0])*n/dist,a[1]+(b[1]-a[1])*n/dist,a[0]+(b[0]-a[0])*q/dist,a[1]+(b[1]-a[1])*q/dist),fill=INK,width=3)
 else: d.line(pts,fill=INK,width=3)
 if kind=='comp':
  a,b=pts[0],pts[1];dx,dy=b[0]-a[0],b[1]-a[1];n=math.hypot(dx,dy);u,v=dx/n,dy/n
  d.polygon([a,(a[0]+u*18-v*10,a[1]+v*18+u*10),(a[0]+u*36,a[1]+v*36),(a[0]+u*18+v*10,a[1]+v*18-u*10)],fill=INK)
 else:
  a,b=pts[-2],pts[-1];dx,dy=b[0]-a[0],b[1]-a[1];n=math.hypot(dx,dy);u,v=dx/n,dy/n
  left=(b[0]-u*25-v*13,b[1]-v*25+u*13);right=(b[0]-u*25+v*13,b[1]-v*25-u*13)
  if kind=='inherit': d.polygon([b,left,right],fill='white',outline=INK,width=3)
  else: d.line([left,b,right],fill=INK,width=3)
 if label:
  x,y=at;bounds=d.textbbox((x,y),label,font=font(22));d.rectangle((bounds[0]-7,bounds[1]-4,bounds[2]+7,bounds[3]+4),fill='white');d.text((x,y),label,font=font(22),fill=INK)
im,d=canvas('ブロック配置・取消のクラス図','Command：配置と取消')
box(d,100,250,880,'BlockPlacementController',['- lastCommand_ : unique_ptr<PlaceBlocksCommand>','- stageMap_ : StageMap*','- inventory_ : BlockInventory*'],['+ TryPlace(index : const Int3&, rotationY : float) : bool','+ TryUndo() : bool','+ HasUndo() : bool'])
box(d,1210,250,1060,'PlaceBlocksCommand',['- stageMap_ : StageMap*','- inventory_ : BlockInventory*','- plan_ : Placement::Plan','- cells_ : vector<SavedCell>','- applied_ : bool'],['+ Execute() : bool','+ Undo(player : const Vector3*) : bool','+ CanUndo() : bool'])
box(d,220,1020,770,'StageMap',[],['+ GetCell(index : const Int3&) : const MapCell*','+ SetCell(index : const Int3&, cell : const MapCell&) : bool'])
box(d,1300,1020,840,'BlockInventory',['- wallCount_ : int','- customCounts_ : int[5]'],['+ ConsumeBlock(type : BlockType, count : int, customId : int) : bool','+ AddBlock(type : BlockType, count : int, customId : int) : void'])
edge(d,[(980,380),(1210,380)],'comp','所有 0..1',(1000,330))
edge(d,[(400,577),(400,1020)],label='参照（非所有）',at=(410,825))
edge(d,[(650,577),(650,760),(1090,760),(1090,1300),(1280,1300),(1280,1170),(1300,1170)],label='参照（非所有）',at=(760,725))
edge(d,[(1440,651),(1440,890),(850,890),(850,1020)],label='地形を変更・復元',at=(960,850))
edge(d,[(1900,651),(1900,1020)],label='在庫を消費・返却',at=(1915,835))
im.save(OUT/'01_Command.png')
im,d=canvas('パーティクル更新のクラス図','Strategy：種類ごとの動きを共通の基底クラスで扱う')
box(d,100,240,900,'ParticleManager',['- particleGroups_ : unordered_map<string, ParticleGroup>'],['+ Update(deltaTime : float, viewMatrix : const Matrix4x4&,','    projectionMatrix : const Matrix4x4&, playerPos : const Vector3&,','    stageMap : StageMap*) : void'])
box(d,1340,240,880,'ParticleMotion  <<abstract>>',[],['+ ~ParticleMotion()  {virtual}','+ Update(particle : Particle&, deltaTime : float) : void','    {abstract, const}'])
# Inheritance arrows point from each derived class to the abstract base.
for x,name in [(80,'LinearMotion'),(545,'AccentMotion'),(1010,'DustMotion'),(1475,'AdventureMotion'),(1940,'FireworkMotion')]:
 box(d,x,1010,380,name,[],['+ Update(p : Particle&,','    deltaTime : float) : void','    {override, const}'])
 edge(d,[(x+190,1010),(x+190,920),(1780,920),(1780,490)],'inherit')
edge(d,[(1000,360),(1340,360)],'dep','基底参照でUpdateを呼ぶ',(1020,315))
box(d,120,620,650,'ParticleGroup',['+ particles : list<Particle>'],[])
box(d,940,620,600,'Particle',['+ transform : Transform','+ velocity : Vector3','+ lifeTime : float','+ maxTime : float'],[])
edge(d,[(420,493),(420,620)],'comp','0..*',(440,525))
edge(d,[(770,715),(940,715)],'comp','0..*',(810,665))
edge(d,[(1780,490),(1780,595),(1540,595),(1540,685)],'dep','引数を更新',(1800,535))
im.save(OUT/'02_Particle_Strategy.png')

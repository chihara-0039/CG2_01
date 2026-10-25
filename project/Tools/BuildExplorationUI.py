"""探索ステージの操作案内を生成する。描画中の文字はゲームに同梱するPNGを使う。"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
out=Path(__file__).resolve().parents[1]/'Resources/UI/PR1_01'
out.mkdir(parents=True,exist_ok=True)
font=ImageFont.truetype('C:/Windows/Fonts/YuGothB.ttc',38)
small=ImageFont.truetype('C:/Windows/Fonts/YuGothM.ttc',23)
labels={
 'baseline':('比較 1 / 一様な距離フォグ','F4：方式切替　 G：スキャン（方式3）　 WASD：移動　 矢印：視点　 F8：録画'),
 'bubble':('比較 2 / プレイヤー周辺を晴らす','近くは見えるが、分岐の先までは見えない　 F4：方式切替　 F8：録画'),
 'scan':('霧の探索路 / 視線スキャン','G：前方をスキャン（再使用6秒）　 左の分岐でシャボン玉を探し、奥の星へ　 F4：比較'),
 'trial_hint':('F7：足場づくりの箱庭','回収した足場をインベントリから持ち出し、設置と取り消しを試す　 Z：取り消し'),
}
for name,(title,hint) in labels.items():
 im=Image.new('RGBA',(1200,130),(12,22,35,210)); d=ImageDraw.Draw(im)
 d.text((22,8),title,font=font,fill=(190,240,255,255))
 d.text((22,69),hint,font=small,fill=(245,248,255,255))
 im.save(out/(name+'.png'))
im=Image.new('RGBA',(320,65),(20,22,30,225)); d=ImageDraw.Draw(im)
d.text((14,3),'REC   F8 : STOP',font=small,fill=(255,100,100,255))
im.save(out/'recording.png')
print(out)

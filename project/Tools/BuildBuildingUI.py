from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'Resources/UI/PR1_01'
f=ImageFont.truetype('C:/Windows/Fonts/YuGothB.ttc',27);sm=ImageFont.truetype('C:/Windows/Fonts/YuGothM.ttc',19)
labels=[('足場を拾って、道を作る','F7：箱庭　B：インベントリ　R：回転　Enter：設置　Z：取り消し'),('設置する前に形を確認','水色のブロックに、３マスの足場が重なっている'),('設置不可：ブロックと重なっています','形も在庫も変えず、カーソルを動かして置き直せる'),('設置を確定','確定後の形と残りの所持数を確認する'),('直前の設置を取り消し、在庫へ戻した','別の位置や向きを試せる'),('回収した足場を、谷を渡る道に使う','インベントリで持ち出し、向きと設置先を選ぶ'),('設置可能：形全体が空いています','Enter / A：設置　R / X：回転　Z / Back：直前の設置を取り消す'),('取り消す前に、別の置き方を試す','最初の配置を戻してから、向きを変えてもう一度置く'),('設置不可：ステージの外に出ています','形全体がステージ内に収まる場所へ動かす'),('操作不可：プレイヤーと重なっています','身体と重なる場所への設置や、乗っている足場の取り消しを防ぐ'),('在庫がありません','Bでインベントリを確認　Zで直前の設置を戻す'),('操作できません：対象の状態を確認してください','取り消し対象がない、または設置後にブロックが変化しています')]
for i,(a,b)in enumerate(labels):
 im=Image.new('RGBA',(950,110),(12,22,35,235));d=ImageDraw.Draw(im);d.text((16,8),a,font=f,fill='white');d.text((16,64),b,font=sm,fill=(180,220,235));im.save(p/f'build-{i}.png')
for i in range(2):
 im=Image.new('RGBA',(270,65),(12,22,35,235));ImageDraw.Draw(im).text((10,14),f'足場の在庫：{i} 個',font=f,fill='white');im.save(p/f'build-count-{i}.png')

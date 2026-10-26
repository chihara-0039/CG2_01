// Usage: node BuildBuildingSlides.mjs <mediaDir> <buildDir> <artifact-tool module path>
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const [mediaDir,buildDir,modulePath]=process.argv.slice(2);
const {Presentation,PresentationFile}=await import(pathToFileURL(modulePath).href);
await fs.mkdir(buildDir,{recursive:true});
const C={bg:'#202020',text:'#F1F1F1',muted:'#BBBBBB',accent:'#8BB6D8'};
const spec=[];
function slide(title,notes){const s={title,notes,nodes:[]};spec.push(s);return s;}
function text(s,value,x,y,w,h,size=28,color=C.text,bold=false){s.nodes.push({kind:'text',value,x,y,w,h,size,color,bold});}
function image(s,file,x,y,w,h){s.nodes.push({kind:'image',file,x,y,w,h});}
function table(s,values,x,y,w,h,widths,grid=false){s.nodes.push({kind:'table',values,x,y,w,h,widths,grid});}
const capture='映像は同じ箱庭・同じカメラ条件の再現撮影。位置・操作時刻を撮影用に自動化している。実入力の連続プレイや人による操作性評価ではない。';
let s=slide('足場配置の全マス検証と一手取り消し','発表の軸は回収したブロックを使って道を作る操作の改善。録画機能・フォグ・モデル変更は独立した発表項目に含めない。'+capture);
s.cover=true;
text(s,'足場配置の全マス検証と一手取り消し',172,264,936,86,40,C.text,false);
s.nodes.at(-1).align='center';
text(s,'LE3B_20_チハラ_シゴウ',240,390,800,64,30,C.text,false);
s.nodes.at(-1).align='center';

s=slide('参考ゲームと、目指す遊び','参考：任天堂『進め！キノピオ隊長』。箱庭を見回し、仕掛けを使って道を探す遊びを参考にしている。本作独自の回収・在庫・配置を改善する。画像出典：https://www.nintendo.com/jp/switch/ajh9a/course/index.html © Nintendo');
image(s,'reference-game.jpg',56,160,760,428);
text(s,'進め！キノピオ隊長',862,170,350,45,28,C.accent,true);
text(s,'箱庭を見回して道を探し\n仕掛けを使って先へ進む',862,225,350,108,25);
text(s,'本作で追求すること',862,378,350,45,28,C.accent,true);
text(s,'回収した足場を持ち出し\n自分で道を作る\nその配置を安心して試せる',862,433,350,140,25);
text(s,'参考画像：任天堂『進め！キノピオ隊長』公式サイト　© Nintendo',56,617,1168,36,16,C.muted);

s=slide('課題：持ち出した足場が、一部だけ置かれる','変更前のBlockPlacementController.cppと実行ファイルをgenerated/PR1_01/build/baselineに保存してから撮影。3セルの複合パーツの1セルが既存のIceBlockと重なる条件。既存処理では空いている2セルだけ置かれ、パーツ在庫が1から0に減った。before-collision.jsonに実体セル数2、在庫0を記録。'+capture);
image(s,'problem.png',56,160,760,428);
text(s,'やりたいこと',862,170,350,45,28,C.accent,true);
text(s,'回収した3マスの足場を\n使って、谷に道を作る',862,225,350,108,25);
text(s,'起きていた問題',862,378,350,45,28,C.accent,true);
text(s,'1マスが重なると\n2マスだけ置かれる\n在庫は1個減ってしまう',862,433,350,140,25);
text(s,'変更前の実行ファイルで再現した、形状と在庫の不一致',56,617,1168,36,22,C.muted);

s=slide('改善①：形全体を確認してから確定する','左は変更前、右は改善後。どちらも同じ3セルのパーツを同じ位置へ3秒で確定。右は配置計画を全セル検証するため、実体0セル、在庫1のまま拒否した。比較映像の注記は実行ログと一致させ、UIを除いた実際の描画を左右に並べた。赤い形は予定形で、設置された実体ではない。'+capture);
text(s,'変更前：一部だけ置いて消費',56,161,560,45,28,C.muted);
text(s,'改善後：形も在庫も変えず拒否',665,161,560,45,28,C.accent,true);
image(s,'collision-compare.gif',56,218,1168,328);
text(s,'置けない理由を表示し、確定前から赤い予定形で知らせる',56,583,1168,72,29);

s=slide('仕組み：プレビューと確定で、同じ計画を使う','Placement::Makeが3×3×3パーツの非空セルを列挙し、90度単位で座標を回転する。全セルの範囲・既存ブロック・プレイヤーAABBを検証。BlockPlacementController::Evaluateで在庫も確認し、その同じPlanのセル列をTryPlaceの確定とStagePlacementPreviewBuilderの表示へ渡す。下図は今回の直線3セルを上から見た例。確定失敗時にはマップを書き換えず在庫も消費しない。');
text(s,'上から見た3×3の形',56,153,490,43,29,C.accent,true);
table(s,[['','足場',''],['','足場',''],['','足場','']],66,220,225,180,[75,75,75],true);
text(s,'90度回転',337,278,190,45,28,C.muted);
table(s,[['','',''],['足場','足場',''],['','','']],555,220,225,180,[75,75,75],true);
// 直線3セルの回転結果を表す。
s.nodes[s.nodes.length-1].values[1][2]='足場';
text(s,'回転後の全セルを検証',851,224,365,47,28,C.accent,true);
text(s,'ステージ内に収まるか\n既存ブロックと重ならないか\n身体と重ならないか\n必要な在庫があるか',851,287,365,175,24);
text(s,'同じセル一覧を、予定形の表示と設置の確定で共有する',56,495,1168,62,30,C.accent,true);
text(s,'1セルでも置けなければ全体を止め、途中までの配置を作らない',56,582,1168,65,28);

s=slide('改善②：置いてから、別の置き方を試せるようにする','3秒で3セルを設置して在庫0、5秒でTryUndoを実行して3セルを戻し在庫1、6秒から向きを変え8秒で再配置する。通常ゲームでも直前の1回を取り消せる。セルの変更前と確定後、消費した種類・カスタムIDを保存し、同じセルがそのまま残っている場合だけ一括復元・返却する。二重返却、身体に重なる操作、乗っている足場の削除を防ぐ。'+capture);
image(s,'undo.gif',56,160,760,428);
text(s,'直前の配置を保存',862,171,350,45,27,C.accent,true);
text(s,'直前の配置を元に戻し\n同じ種類を1個返却\n向きを変えてもう一度置く',862,229,350,133,25);
text(s,'戻してよい場合だけ',862,409,350,45,27,C.accent,true);
text(s,'乗っている足場は消さない\n変化したセルは上書きしない\n返却は1回だけ',862,467,350,128,24);
text(s,'操作を取り消せる範囲は、直前に成功した設置1回',56,619,1168,34,22,C.muted);

s=slide('ゲームへの接続：ふたつの谷をつなぐ遺跡','通常ステージとして追加した独自の箱庭。回収、1本目の橋、取り消し、再設置、2個目の回収、90度回転した橋、はしご、高台の星へ接続。位置と時刻は自動化し、回収・配置・在庫・取り消し・ゴール判定は実処理。手動通しプレイや入力・登攀の操作性の検証ではない。generated/PR1_01/diorama/verification.jsonの7項目がtrue。高台に接地し、はしごを離れて星に触れるまでクリアしないことも確認。撮影時のHUDとクリア文字は描画しない。空・雲海・芝と石積みと木の専用素材は通常プレイにも適用。');
image(s,'route.gif',56,160,760,428);
text(s,'回収して持ち出す',862,172,350,45,28,C.accent,true);
text(s,'島ごとに足場を1個回収\n回収した足場を道に使う',862,229,350,108,25);
text(s,'置いて道を作る',862,390,350,45,28,C.accent,true);
text(s,'2本目は90度回して設置\nはしごを登り、高台の星へ',862,447,350,108,25);
text(s,'位置と操作時刻は自動化。回収・在庫・配置・クリアはゲームの実処理',56,619,1168,34,21,C.muted);

s=slide('検証：失敗しても、形と在庫を変えない','ゲーム内のVerifyBuildingで14項目を確認。成功・失敗後の実際のStageMapセルとBlockInventoryを検証した。範囲外、既存ブロック、身体との重なり、全セル配置、1個だけ消費、在庫なし、足場上の取り消し禁止、復元と返却、二重取り消し禁止、4回転、設置後の変化、空パーツ、初期化時の履歴消去を確認。verification.jsonが全true。別に回収からゴールまでの再現撮影が成功。人による操作性・操作回数の改善率は未測定。');
table(s,[['確認した条件','期待する動作','結果'],['既存ブロックとの重なり','全体を拒否・在庫を維持','成功'],['範囲外／身体との重なり','確定前に止める','成功'],['4方向の回転','表示と同じ3マスへ設置','成功'],['在庫なし／空のパーツ','形も在庫も変えない','成功'],['取り消し／二重取り消し','全体を戻す・返却は1回','成功'],['足場上／設置後の変化','危険な取り消しを拒否','成功']],56,162,1168,378,[430,560,178]);
text(s,'14項目の配置検証と、2本の橋・回収・ゴールの接続を確認',56,577,1168,48,29,C.accent,true);
text(s,'操作性の改善率は、今後プレイヤーが操作して確かめる',56,638,1168,32,21,C.muted);

s=slide('次の追求：迷わず置ける操作に近づける','次に検証する対象もブロック配置に限定する。初期カーソル位置や高さ変更の案内、回転の分かりやすさを調整し、足場を置くまでの操作回数、誤確定回数、取り消し後の復帰を測りたい。現段階は配置と在庫の整合性の改善であり、人による使いやすさの評価結果ではない。');
text(s,'今回できたこと',56,163,540,50,32,C.accent,true);
text(s,'予定形と確定結果をそろえる\n失敗時に形と在庫を守る\n直前の設置を戻して試し直す',56,245,550,171,28);
text(s,'次に確かめたいこと',693,163,530,50,32,C.accent,true);
text(s,'初期カーソルの位置は適切か\n高さと向きを迷わず変えられるか\n設置までの操作回数を減らせるか',693,245,535,171,27);

// Narrative: motivation -> existing implementation -> two improvements -> results -> next steps.
const narrativeEdits=[{"index":2,"from":"課題：持ち出した足場が、一部だけ置かれる","to":"最初の実装：回収した足場を持ち出して置く"},{"index":2,"from":"やりたいこと","to":"できていたこと"},{"index":2,"from":"回収した3マスの足場を\n使って、谷に道を作る","to":"回収した足場を在庫に保存\n選んだ場所へ配置する"},{"index":2,"from":"起きていた問題","to":"使って分かった課題"},{"index":2,"from":"変更前の実行ファイルで再現した、形状と在庫の不一致","to":"1マスずつ配置する処理では、形状と在庫に不一致が発生した"},{"index":4,"from":"仕組み：プレビューと確定で、同じ計画を使う","to":"改良①：形全体を検証し、まとめて配置する"},{"index":4,"from":"同じセル一覧を、予定形の表示と設置の確定で共有する","to":"先に全マスを検証し、その結果を予定形の表示と確定で共有する"},{"index":3,"from":"改善①：形全体を確認してから確定する","to":"改良①の結果：失敗しても形と在庫が残る"},{"index":5,"from":"改善②：置いてから、別の置き方を試せるようにする","to":"改良②：配置を取り消して、別の置き方を試す"},{"index":5,"from":"直前の配置を保存","to":"次に解決したいこと"},{"index":5,"from":"直前の配置を元に戻し\n同じ種類を1個返却\n向きを変えてもう一度置く","to":"別の向きも試したい\n直前の設置を元に戻し\n同じ種類を1個返却"},{"index":6,"from":"ゲームへの接続：ふたつの谷をつなぐ遺跡","to":"改良した結果：回収した足場で2つの谷を渡る"},{"index":7,"from":"検証：失敗しても、形と在庫を変えない","to":"結果の検証：配置と在庫の整合性を確認"},{"index":8,"from":"次の追求：迷わず置ける操作に近づける","to":"今後：設置までの操作回数を減らす"},{"index":8,"from":"今回できたこと","to":"今回の到達点"},{"index":8,"from":"次に確かめたいこと","to":"次の改良と評価"}];
for(const edit of narrativeEdits){
 const s=spec[edit.index];
 if(s.title===edit.from)s.title=edit.to;
 for(const node of s.nodes)if(node.kind==='text'&&node.value===edit.from)node.value=edit.to;
}
for(const node of spec[8].nodes)if(node.kind==='text' && node.value.startsWith('初期カーソルの位置は適切か'))node.value='初期カーソルを足元へ寄せる\n高さと回転の表示を整理する\n設置までの操作回数を比べる';
text(spec[8],'同じ課題で、操作回数・置き間違い・取り消し後の復帰を記録する',56,520,1168,80,24,C.accent);
const background=structuredClone(spec[2]);
background.title='実装背景：足場を置き直して道を考えたい';
background.notes='足場の向きや位置を試して道を作るため、配置結果の予測、失敗時の在庫保全、設置の取り消しを実装する。画像は完成した検証用ステージであり変更前の映像ではない。基本の回収・在庫・配置は既存機能。';
background.nodes=[
 {kind:'image',file:'background.png',x:56,y:160,w:760,h:428},
 {kind:'text',value:'目指す遊び',x:862,y:170,w:350,h:45,size:28,color:C.accent,bold:true},
 {kind:'text',value:'橋の向きや位置を変え\n道を考えて星へ進む',x:862,y:225,w:350,h:108,size:25,color:C.text},
 {kind:'text',value:'そのために必要なこと',x:862,y:378,w:350,h:45,size:28,color:C.accent,bold:true},
 {kind:'text',value:'置ける場所を先に知る\n失敗で足場を失わない\n置いた後も試し直せる',x:862,y:433,w:350,h:140,size:25,color:C.text},
 {kind:'text',value:'今回の実装：全マスを確認して配置／直前の設置を取り消す',x:56,y:617,w:1168,h:36,size:22,color:C.muted}
];
const sequence=[spec[0],spec[1],background,spec[2],spec[4],spec[3],spec[5],spec[6],spec[7],spec[8]];
spec.splice(0,spec.length,...sequence);
const deck=Presentation.create({slideSize:{width:1280,height:720}});
for(let i=0;i<spec.length;i++){
 const src=spec[i],dst=deck.slides.add();dst.background.fill=C.bg;dst.speakerNotes.text=src.notes;
 const texts=src.cover?src.nodes:[{kind:'text',value:src.title,x:56,y:42,w:1168,h:90,size:40,color:C.text,bold:false},...src.nodes,{kind:'text',value:String(i+1).padStart(2,'0'),x:1180,y:678,w:48,h:28,size:18,color:C.muted}];
 if(!src.cover)dst.shapes.add({geometry:'rect',position:{left:56,top:124,width:1168,height:1},fill:'#68717A',line:{fill:'none',width:0}});
 for(const n of texts){
  if(n.kind==='text'){const sh=dst.shapes.add({geometry:'textbox',position:{left:n.x,top:n.y,width:n.w,height:n.h},fill:'none',line:{fill:'none',width:0}});sh.text=n.value;sh.text.style={typeface:'Yu Gothic',fontSize:n.size,color:n.color,bold:n.bold||false,alignment:n.align||'left',verticalAlignment:'top',insets:0,autoFit:'none'};}
  if(n.kind==='image')dst.images.add({blob:new Uint8Array(await fs.readFile(path.join(mediaDir,n.file))),contentType:n.file.endsWith('.gif')?'image/gif':n.file.endsWith('.jpg')?'image/jpeg':'image/png',fit:'contain',position:{left:n.x,top:n.y,width:n.w,height:n.h}});
  if(n.kind==='table'){
   const t=dst.tables.add({rows:n.values.length,columns:n.values[0].length,left:n.x,top:n.y,width:n.w,height:n.h,columnWidths:n.widths,values:n.values});
   for(let r=0;r<n.values.length;r++)for(let c=0;c<n.values[0].length;c++){const cell=t.getCell(r,c);cell.fill=n.grid?(n.values[r][c]?'#365B77':'#353535'):(r===0?'#353535':r%2?'#292929':'#252525');cell.text.style={typeface:'Yu Gothic',fontSize:n.grid?23:23,color:r===0&&!n.grid?C.accent:C.text,bold:r===0&&!n.grid};}
  }
 }
}
await fs.writeFile(path.join(buildDir,'spec.json'),JSON.stringify(spec,null,2));
await (await PresentationFile.exportPptx(deck)).save(path.join(buildDir,'candidate.pptx'));
for(let i=0;i<deck.slides.items.length;i++){const png=await deck.export({slide:deck.slides.items[i],format:'png',scale:1});await fs.writeFile(path.join(buildDir,`slide-${i+1}.png`),new Uint8Array(await png.arrayBuffer()));}
console.log(JSON.stringify({slides:spec.length,candidate:path.join(buildDir,'candidate.pptx')}));

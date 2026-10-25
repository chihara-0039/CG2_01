// Usage: node BuildBuildingSlides.mjs <mediaDir> <buildDir> <artifact-tool module path>
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const [mediaDir,buildDir,modulePath]=process.argv.slice(2);
const {Presentation,PresentationFile}=await import(pathToFileURL(modulePath).href);
await fs.mkdir(buildDir,{recursive:true});
const C={bg:'#101C2B',text:'#F1F5F9',muted:'#B5C5D6',accent:'#77E5E0'};
const spec=[];
function slide(title,notes){const s={title,notes,nodes:[]};spec.push(s);return s;}
function text(s,value,x,y,w,h,size=28,color=C.text,bold=false){s.nodes.push({kind:'text',value,x,y,w,h,size,color,bold});}
function image(s,file,x,y,w,h){s.nodes.push({kind:'image',file,x,y,w,h});}
function table(s,values,x,y,w,h,widths,grid=false){s.nodes.push({kind:'table',values,x,y,w,h,widths,grid});}
const capture='映像は同じ箱庭・同じカメラ条件の再現撮影。位置・操作時刻を撮影用に自動化している。実入力の連続プレイや人による操作性評価ではない。';
let s=slide('足場づくりを、安心して試せる操作へ','発表の軸は回収したブロックを使って道を作る操作の改善。録画機能・フォグ・モデル変更は独立した発表項目に含めない。'+capture);
text(s,'回収したブロックで\n道を作る操作の改善',56,178,430,134,36,C.accent,true);
text(s,'回転の国の探検隊\n技術進捗 / PR1_01\n2026.10.26',56,411,410,124,26,C.muted);
image(s,'hero.png',506,172,720,405);

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

s=slide('改善②：置いてから、別の置き方を試せるようにする','3秒で3セルを設置して在庫0、5秒でTryUndoを実行して3セルを戻し在庫1、6秒から向きを変え8秒で再配置する。通常ゲームでもZ／パッドBackで直前の1回を取り消せる。セルの変更前と確定後、消費した種類・カスタムIDを保存し、同じセルがそのまま残っている場合だけ一括復元・返却する。二重返却、身体に重なる操作、乗っている足場の削除を防ぐ。'+capture);
image(s,'undo.gif',56,160,760,428);
text(s,'直前の配置を保存',862,171,350,45,27,C.accent,true);
text(s,'Z / Backで形を戻し\n同じ種類を1個返却\n向きを変えてもう一度置く',862,229,350,133,25);
text(s,'戻してよい場合だけ',862,409,350,45,27,C.accent,true);
text(s,'乗っている足場は消さない\n変化したセルは上書きしない\n返却は1回だけ',862,467,350,128,24);
text(s,'操作を取り消せる範囲は、直前に成功した設置1回',56,619,1168,34,22,C.muted);

s=slide('ゲームへの接続：回収した足場で谷を渡る','F7の実験用箱庭。BubblePickupControllerでカスタム足場を回収し、実際のインベントリUIを開いてConfirmSelection、通常の使用要求から配置モードに入る。重なる予定形を確認してから別の場所へ3セルを設置し、対岸の星へ進む。回収・在庫・設置・ゴール判定はゲームの実処理。位置・カメラ・選択タイミングは撮影用スクリプトで制御している。final-route.jsonでgoalReached=trueを確認。既存ステージの回収・在庫・配置にも同じ改善が適用される。');
image(s,'route.gif',56,160,760,428);
text(s,'回収して持ち出す',862,172,350,45,28,C.accent,true);
text(s,'シャボン玉から足場を回収\nインベントリで選んで使用',862,229,350,108,25);
text(s,'置いて道を作る',862,390,350,45,28,C.accent,true);
text(s,'予定形で場所を確認\n橋を置き、対岸の星へ進む',862,447,350,108,25);
text(s,'位置と操作時刻は自動化。回収・在庫・配置・クリアはゲームの実処理',56,619,1168,34,21,C.muted);

s=slide('検証：失敗しても、形と在庫を変えない','ゲーム内のVerifyBuildingで14項目を確認。成功・失敗後の実際のStageMapセルとBlockInventoryを検証した。範囲外、既存ブロック、身体との重なり、全セル配置、1個だけ消費、在庫なし、足場上の取り消し禁止、復元と返却、二重取り消し禁止、4回転、設置後の変化、空パーツ、初期化時の履歴消去を確認。verification.jsonが全true。別に回収からゴールまでの再現撮影が成功。人による操作性・操作回数の改善率は未測定。');
table(s,[['確認した条件','期待する動作','結果'],['既存ブロックとの重なり','全体を拒否・在庫を維持','成功'],['範囲外／身体との重なり','確定前に止める','成功'],['4方向の回転','表示と同じ3マスへ設置','成功'],['在庫なし／空のパーツ','形も在庫も変えない','成功'],['取り消し／二重取り消し','全体を戻す・返却は1回','成功'],['足場上／設置後の変化','危険な取り消しを拒否','成功']],56,162,1168,378,[430,560,178]);
text(s,'14項目の状態検証と、回収からゴールまでの接続を確認',56,577,1168,48,29,C.accent,true);
text(s,'操作性の改善率は、今後プレイヤーが操作して確かめる',56,638,1168,32,21,C.muted);

s=slide('次の追求：迷わず置ける操作に近づける','次に検証する対象もブロック配置に限定する。初期カーソル位置や高さ変更の案内、回転の分かりやすさを調整し、足場を置くまでの操作回数、誤確定回数、取り消し後の復帰を測りたい。現段階は配置と在庫の整合性の改善であり、人による使いやすさの評価結果ではない。実演はF7で箱庭、シャボン玉を回収、Bから足場を選びEnter、WASDとQ/Eで位置・高さ、Rで回転、Enterで確定、Zで取り消し。');
text(s,'今回できたこと',56,163,540,50,32,C.accent,true);
text(s,'予定形と確定結果をそろえる\n失敗時に形と在庫を守る\n直前の設置を戻して試し直す',56,245,550,171,28);
text(s,'次に確かめたいこと',693,163,530,50,32,C.accent,true);
text(s,'初期カーソルの位置は適切か\n高さと向きを迷わず変えられるか\n設置までの操作回数を減らせるか',693,245,535,171,27);
text(s,'実演：F7で箱庭を開き、回収した足場を置いて取り消す',56,520,1168,90,30,C.accent,true);
text(s,'B：持ち出す　WASD／Q・E：位置と高さ　R：回転　Enter：設置　Z：取り消し',56,638,1168,35,20,C.muted);

const deck=Presentation.create({slideSize:{width:1280,height:720}});
for(let i=0;i<spec.length;i++){
 const src=spec[i],dst=deck.slides.add();dst.background.fill=C.bg;dst.speakerNotes.text=src.notes;
 const texts=[{kind:'text',value:src.title,x:56,y:42,w:1168,h:90,size:40,color:C.text,bold:true},...src.nodes,{kind:'text',value:String(i+1).padStart(2,'0'),x:1180,y:678,w:48,h:28,size:18,color:C.muted}];
 for(const n of texts){
  if(n.kind==='text'){const sh=dst.shapes.add({geometry:'textbox',position:{left:n.x,top:n.y,width:n.w,height:n.h},fill:'none',line:{fill:'none',width:0}});sh.text=n.value;sh.text.style={typeface:'Yu Gothic',fontSize:n.size,color:n.color,bold:n.bold||false,verticalAlignment:'top',insets:0,autoFit:'none'};}
  if(n.kind==='image')dst.images.add({blob:new Uint8Array(await fs.readFile(path.join(mediaDir,n.file))),contentType:n.file.endsWith('.gif')?'image/gif':'image/png',fit:'contain',position:{left:n.x,top:n.y,width:n.w,height:n.h}});
  if(n.kind==='table'){
   const t=dst.tables.add({rows:n.values.length,columns:n.values[0].length,left:n.x,top:n.y,width:n.w,height:n.h,columnWidths:n.widths,values:n.values});
   for(let r=0;r<n.values.length;r++)for(let c=0;c<n.values[0].length;c++){const cell=t.getCell(r,c);cell.fill=n.grid?(n.values[r][c]?'#256E71':'#23394D'):(r===0?'#23394D':r%2?'#15283A':'#102234');cell.text.style={typeface:'Yu Gothic',fontSize:n.grid?23:23,color:r===0&&!n.grid?C.accent:C.text,bold:r===0&&!n.grid};}
  }
 }
}
await fs.writeFile(path.join(buildDir,'spec.json'),JSON.stringify(spec,null,2));
await (await PresentationFile.exportPptx(deck)).save(path.join(buildDir,'candidate.pptx'));
for(let i=0;i<deck.slides.items.length;i++){const png=await deck.export({slide:deck.slides.items[i],format:'png',scale:1});await fs.writeFile(path.join(buildDir,`slide-${i+1}.png`),new Uint8Array(await png.arrayBuffer()));}
console.log(JSON.stringify({slides:spec.length,candidate:path.join(buildDir,'candidate.pptx')}));

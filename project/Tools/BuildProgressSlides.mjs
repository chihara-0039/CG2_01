// Usage: node BuildProgressSlides.mjs <mediaDir> <buildDir> <artifact-tool module path>
// 映像はエンジンのCaptureComparisons.ps1で撮影した実データを使う。
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const [mediaDir,buildDir,modulePath]=process.argv.slice(2);
if(!mediaDir || !buildDir || !modulePath) throw new Error('mediaDir / buildDir / artifact-tool module path required');
const {Presentation,PresentationFile}=await import(pathToFileURL(modulePath).href);
await fs.mkdir(buildDir,{recursive:true});
const deck=Presentation.create({slideSize:{width:1280,height:720}});
const font='Yu Gothic';
const C={bg:'#101C2B',text:'#F1F5F9',muted:'#B5C5D6',accent:'#77E5E0'};
function text(slide,value,x,y,w,h,size=28,color=C.text,bold=false){
 const s=slide.shapes.add({geometry:'textbox',position:{left:x,top:y,width:w,height:h},fill:'none',line:{fill:'none',width:0}});
 s.text=value; s.text.style={typeface:font,fontSize:size,color,bold,verticalAlignment:'top',insets:0,autoFit:'none'};
 return s;
}
function slide(title,notes){
 const s=deck.slides.add(); s.background.fill=C.bg;
 text(s,title,56,42,1168,82,43,C.text,true);
 text(s,String(deck.slides.items.length).padStart(2,'0'),1180,665,48,28,18,C.muted);
 s.speakerNotes.text=notes;
 return s;
}
async function image(slide,name,x,y,w,h){
 const blob=new Uint8Array(await fs.readFile(path.join(mediaDir,name)));
 return slide.images.add({blob,contentType:name.endsWith('.gif')?'image/gif':'image/png',alt:name,fit:'contain',position:{left:x,top:y,width:w,height:h}});
}
function callout(s,head,body,y){text(s,head,862,y,350,50,28,C.accent,true);text(s,body,862,y+54,350,112,25);}
const notesBase='映像は過去の録画ではなく、PR1_01の専用ステージを同じ位置・カメラ経路・時間設定で再現撮影した方式比較。カメラとプレイヤー位置はスクリプト制御し、実入力のプレイ映像とは区別する。';
let s=slide('霧を「探索」に変える', '今回の進捗は視線方向に連動するフォグと、比較を記録するエンジン録画機能です。見た目だけでなく、分岐を調べ、シャボン玉を集めてゴールへ進む専用ステージへ組み込みました。'+notesBase);
text(s,'視線スキャンと\n比較録画の実装',58,173,440,145,44,C.accent,true);
text(s,'回転の国の探検隊\n進捗発表 / PR1_01\n2026.10.25',58,395,390,150,27,C.muted);
await image(s,'scan-6s.png',506,166,720,405);

s=slide('課題：霧で分岐の先が読みにくい',notesBase+' ここでの課題は専用の霧ステージで設定した開発上の課題です。操作性が向上したというユーザーテストの結果ではありません。基準方式は距離8〜55のsmoothstep、最大濃度0.94で、後の方式も同じ霧密度を使います。');
await image(s,'baseline.gif',56,158,760,428);
callout(s,'基準：一様な距離フォグ','遠くほど霧を濃くする\n分岐や柱も一緒に霞む',176);
callout(s,'改善したいこと','雰囲気を残しつつ\n進む道を確認したい',390);
text(s,'同一条件での方式比較 / 10秒ループ',56,610,760,40,22,C.muted);

s=slide('改善①：プレイヤー周辺を晴らす',notesBase+' 第1段階はプレイヤーを中心とした円形の視界マスクです。半径4以内を晴らし、8までを滑らかに減衰させます。近くの視界を保てますが、少し離れた分岐を確認できる範囲は広がりません。');
await image(s,'bubble.gif',56,158,760,428);
callout(s,'近くの足場を確保','プレイヤーからの距離で\n霧の量を減らす',176);
callout(s,'残った課題','周囲だけでは\n見たい方向を選べない',390);
text(s,'半径4〜8で滑らかに切り替える',56,610,760,40,22,C.muted);

s=slide('改善②：視線方向をスキャンする',notesBase+' 第2段階ではカメラの水平回転から視線方向を求め、プレイヤーから前方28の範囲にカプセル形状の視界マスクを作ります。Gで波を広げ、約3.5秒で元に戻します。再使用は6秒。スキャン中も視点を回して見たい分岐へ向けられます。');
await image(s,'scan.gif',56,158,760,428);
callout(s,'Gで探索スキャン','カメラの向きに合わせて\n前方の霧を一時的に晴らす',176);
callout(s,'常時は全体を見せない','波の展開 → 視界保持 → 復帰\n再使用まで6秒',390);
text(s,'視点回転に追従 / ポーズ中は時間停止',56,610,760,40,22,C.muted);

s=slide('ゲームへの組み込み', '専用ステージ「霧の探索路」はF6から開始できます。WASDで移動、矢印でカメラ回転、Gでスキャン。左の分岐でシャボン玉を回収し、奥の星に触れるとクリアします。既存のBubblePickupControllerとGoalの判定を接続しました。統合テストでは検証地点へプレイヤーを配置し、実際の物理・回収・ゴール処理を通しています。人による遊びやすさ評価は未実施です。');
await image(s,'scan-route.png',56,161,760,428);
callout(s,'見る → 探す → 進む','視点を回して道を調べる\n左の分岐でシャボン玉を回収',168);
callout(s,'既存ルールと接続','未回収：ゴールはロック\n回収後：奥の星でクリア',390);
text(s,'新ステージだけに適用し、既存ステージの霧を維持',56,611,1060,40,22,C.muted);

s=slide('シェーダーとゲーム状態の接続', 'FogExplorer.cppが位置・視線方向・スキャンの経過時間・クールダウンを管理します。定数バッファの追加領域を通常描画とインスタンシング描画の両方へ渡し、同じHLSLの計算で足場とモデルの霧を制御します。実装はワールド座標を使う表面フォグで、ボリューメトリックフォグではありません。色の合成と密度制御の式は説明用に簡略化しています。');
text(s,'CPU：ゲームの状態',56,156,540,52,32,C.accent,true);
text(s,'位置・カメラ方向\nスキャン時間・再使用時間\nポーズ／シーン遷移で停止・解除',56,231,560,162,29);
text(s,'GPU：各ピクセルの霧',680,156,540,52,32,C.accent,true);
text(s,'距離フォグ ×（1 − 視界マスク）\n近距離の円 ＋ 前方カプセル\n高さの差で作用範囲を制限',680,231,540,162,29);
text(s,'境界と時間をsmoothstepで補間',56,457,1136,60,35,C.accent,true);
text(s,'通常モデル／インスタンシングで同じ計算を使い、足場だけ霧が濃くなる差を防ぐ',56,539,1136,88,28);

s=slide('比較映像を残すエンジン録画機能', 'FrameRecorder.cppはDX12のCopyTextureRegionでRGBA8バックバッファをreadback heapへコピーします。既存フレームフェンスを待った後にMapし、RowPitchの余白を除去して密なRGBAへ変換します。最大8フレームのキューを通じてワーカーのFFmpegへ送り、H.264 MP4へ圧縮します。停止後にパレット生成付きGIFへ変換。既存エンジンが毎フレームGPUを待つ設計なので、この実装はGPUを完全非同期化したものではありません。参考: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion と https://ffmpeg.org/ffmpeg-filters.html#palettegen-1');
text(s,'描画結果 → GPUコピー → CPU回収 → 圧縮ワーカー',56,164,1168,70,34,C.accent,true);
text(s,'描画側',56,298,300,54,31,C.accent,true);
text(s,'既存フェンス後に画像を回収\n最大8フレームのキュー\n満杯時は描画を待たせず取得を省略',56,371,550,170,28);
text(s,'保存側',686,298,300,54,31,C.accent,true);
text(s,'MP4：960×540 / 30fps\nGIF：640×360 / 12fps\n保存が遅れた時刻は直前画像で補完',686,371,540,170,28);
text(s,'F8：開始・停止 / DevelopmentではFPS・解像度・保存先を設定',56,601,1136,48,24,C.muted);

const stats=await Promise.all(['baseline','bubble','scan'].map(async name=>JSON.parse(await fs.readFile(path.join(mediaDir,name+'.mp4.json'),'utf8'))));
s=slide('実機検証の結果', '録画の数値はエンジンが生成した各mp4.jsonの値です。画像回収CPU時間はメモリー確保・Map・行コピー・Unmapの平均で、GPUコピーやエンコードを含む総負荷ではありません。今回の条件は1920×1080のバックバッファから960×540へ出力、30fpsで約10秒です。統合検証: gameplay-verification.json。数学的境界の検証: FogExplorerTests.cpp。処理速度の改善率や他PCでの性能を示す測定ではありません。');
const values=[['方式','保存フレーム','補完フレーム','CPU画像回収 / 枚'],...stats.map((a,i)=>[['一様な霧','周辺のみ','視線スキャン'][i],String(a.encodedFrames),String(a.duplicateFrames),a.meanReadbackCpuMs.toFixed(2)+' ms'])];
const table=s.tables.add({rows:4,columns:4,left:56,top:162,width:1168,height:240,columnWidths:[300,260,260,348],values});
for(let r=0;r<4;r++)for(let c=0;c<4;c++){const cell=table.getCell(r,c);cell.fill=r===0?'#23394D':(r%2?'#15283A':'#102234');cell.text.style={typeface:font,fontSize:26,color:r===0?C.accent:C.text,bold:r===0};}
text(s,'実際のゲーム処理を通した確認',56,450,1168,52,32,C.accent,true);
text(s,'着地 / スキャン / ポーズ中の時計停止\n未回収時のゴール制限 / シャボン玉回収 / 回収後のクリア',56,521,1168,96,27);
text(s,'CPU時間は画像回収部分のみ。総描画負荷・操作性の改善率は未計測',56,636,1100,32,20,C.muted);

s=slide('次の改善と実演', '発表ではF6で専用ステージを開き、F4で3方式を切り替え、矢印キーで視点を動かしてGでスキャンします。F8で録画開始・停止。人による評価では、分岐の見落とし、回収までの時間、スキャン待ちの長さを確認したいです。録画の残課題は音声、より低いGPU同期コスト、長時間録画と保存容量の管理です。現時点は無音・最大120秒、GIFは先頭10秒が既定、15秒まで設定可能です。');
text(s,'遊びの検証',56,170,500,54,34,C.accent,true);
text(s,'分岐の見落としを減らせるか\n晴れる長さ・持続時間は適切か\nスキャン待ちが負担にならないか',56,255,560,170,29);
text(s,'録画機能の拡張',700,170,500,54,34,C.accent,true);
text(s,'ゲーム音声の保存\nGPUとCPUの同期設計の見直し\n撮影プリセットと比較素材の管理',700,255,530,170,29);
text(s,'実演：F6で探索路 → F4で比較 → Gでスキャン → F8で録画',56,527,1168,88,30,C.accent,true);
text(s,'現在の制限：無音・録画120秒まで / GIFは先頭10秒が既定',56,636,1168,32,21,C.muted);

await (await PresentationFile.exportPptx(deck)).save(path.join(buildDir,'candidate.pptx'));
for(let i=0;i<deck.slides.items.length;i++){
 const png=await deck.export({slide:deck.slides.items[i],format:'png',scale:1});
 await fs.writeFile(path.join(buildDir,`slide-${String(i+1).padStart(2,'0')}.png`),new Uint8Array(await png.arrayBuffer()));
}
await fs.writeFile(path.join(buildDir,'deck-summary.json'),JSON.stringify({slides:deck.slides.items.length,font,stats},null,2));
console.log(JSON.stringify({slides:deck.slides.items.length,candidate:path.join(buildDir,'candidate.pptx')}));

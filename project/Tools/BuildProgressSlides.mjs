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
text(s,'回転の国の探検隊\n進捗発表 / PR1_01\n2026.10.26',58,395,390,150,27,C.muted);
await image(s,'explorer-hero.png',506,166,720,405);

s=slide('課題：霧で分岐の先が読みにくい',notesBase+' ここでの課題は専用の霧ステージで設定した開発上の課題です。操作性が向上したというユーザーテストの結果ではありません。基準方式は距離4〜28のsmoothstep、最大濃度0.94で、後の方式も同じ霧密度を使います。');
await image(s,'problem.png',56,158,760,428);
callout(s,'基準：一様な距離フォグ','遠くほど霧を濃くする\n分岐や柱も一緒に霞む',176);
callout(s,'改善したいこと','雰囲気を残しつつ\n進む道を確認したい',390);
text(s,'比較用ステージで設定した課題',56,610,760,40,22,C.muted);

s=slide('改善①：足元の見やすさを比較する',notesBase+' 足元に寄ったカメラで同じ道を前進する比較。左は距離フォグのみ、右は半径4〜8の周辺マスク。次のスライドとは別の撮影経路を使う。');
text(s,'変更前：距離フォグのみ',56,161,560,45,29,C.muted);
text(s,'試作：プレイヤー周辺を晴らす',665,161,560,45,29,C.accent,true);
await image(s,'footpath-compare.gif',56,218,1168,329);
text(s,'近くの道は見やすくなるが、遠い分岐を調べるには範囲が足りない',56,579,1168,74,29);

s=slide('改善②：分岐を向いてスキャンする',notesBase+' 分岐の手前で立ち止まり、カメラを左右へ回す比較。左は周辺のみ、右はGの視線スキャン。カメラ方向のカプセルマスクにより見たい方向を選ぶ。1.2秒で発動し、時間経過で戻る。');
text(s,'試作：周囲だけが見える',56,161,560,45,29,C.muted);
text(s,'改善後：見たい方向を晴らす',665,161,560,45,29,C.accent,true);
await image(s,'branch-compare.gif',56,218,1168,329);
text(s,'視点回転に追従。約3.5秒で霧が戻り、6秒で再使用できる',56,579,1168,74,29);

s=slide('ゲームへの組み込み', '専用ステージ「霧の探索路」はF6から開始できます。WASDで移動、矢印でカメラ回転、Gでスキャン。左の分岐でシャボン玉を回収し、奥の星に触れるとクリアします。既存のBubblePickupControllerとGoalの判定を接続しました。映像は撮影用に位置を切り替え、通常の回収・ゴール処理を通す。統合テストでも検証地点へプレイヤーを配置し、実際の物理・回収・ゴール処理を通しています。人による遊びやすさ評価は未実施です。');
await image(s,'route-scan.gif',56,161,760,428);
callout(s,'回収とゴール条件','未回収の星に接触\n分岐で回収後、再び星へ',168);
callout(s,'既存ルールと接続','未回収：ゴールはロック\n回収後：奥の星でクリア',390);
text(s,'撮影用に位置を切替。回収・ゴール判定は通常のゲーム処理を使用',56,611,1060,40,22,C.muted);

s=slide('探検家らしい外見へ更新する', '元のwalk.gltfの人体、骨格、モーションを残し、頂点カラーによる衣服と、Headに追従するサファリハット、Spine2に追従するリュック・肩紐・寝袋を追加した。左右は同じカメラ周回。比較用環境変数CG2_LEGACY_MODEL=1で変更前の外見を再現し、通常起動では新しい外見を使う。当たり判定と移動速度は変更しない。');
text(s,'変更前：素体モデル',56,161,560,45,29,C.muted);
text(s,'変更後：帽子と探索装備',665,161,560,45,29,C.accent,true);
await image(s,'explorer-compare.gif',56,218,1168,329);
text(s,'帽子は頭、リュックは背骨に追従。歩行アニメーションを引き継ぐ',56,579,1168,74,29);

s=slide('比較映像を残すエンジン録画機能', 'FrameRecorder.cppはDX12のCopyTextureRegionでRGBA8バックバッファをreadback heapへコピーします。既存フレームフェンスを待った後にMapし、RowPitchの余白を除去して密なRGBAへ変換します。最大8フレームのキューを通じてワーカーのFFmpegへ送り、H.264 MP4へ圧縮します。停止後にパレット生成付きGIFへ変換。既存エンジンが毎フレームGPUを待つ設計なので、この実装はGPUを完全非同期化したものではありません。参考: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion と https://ffmpeg.org/ffmpeg-filters.html#palettegen-1');
text(s,'描画結果 → GPUコピー → CPU回収 → 圧縮ワーカー',56,164,1168,70,34,C.accent,true);
text(s,'描画側',56,298,300,54,31,C.accent,true);
text(s,'既存フェンス後に画像を回収\n最大8フレームのキュー\n満杯時は描画を待たせず取得を省略',56,371,550,170,28);
text(s,'保存側',686,298,300,54,31,C.accent,true);
text(s,'MP4：960×540 / 30fps\nGIF：640×360 / 12fps\n保存が遅れた時刻は直前画像で補完',686,371,540,170,28);
text(s,'F8：開始・停止 / DevelopmentではFPS・解像度・保存先を設定',56,601,1136,48,24,C.muted);

const stats=await Promise.all(['footpath-baseline','footpath-bubble','branch-scan'].map(async name=>JSON.parse(await fs.readFile(path.join(mediaDir,name+'.mp4.json'),'utf8'))));
s=slide('実機検証の結果', '録画の数値はエンジンが生成した各mp4.jsonの値です。画像回収CPU時間はメモリー確保・Map・行コピー・Unmapの平均で、GPUコピーやエンコードを含む総負荷ではありません。今回の条件は1920×1080のバックバッファから960×540へ出力、30fpsで約10秒です。統合検証: gameplay-verification.json。数学的境界の検証: FogExplorerTests.cpp。処理速度の改善率や他PCでの性能を示す測定ではありません。表は足元のbaseline/bubbleと分岐のscanのログで、異なる場面の速度比較ではありません。回収映像は12秒、360フレーム、画面遷移時に15フレームを補完しました。');
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

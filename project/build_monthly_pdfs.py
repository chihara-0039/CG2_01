from pathlib import Path
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak, Image, ListFlowable, ListItem, KeepTogether

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'monthly_submission_docs'; ASSETS=OUT/'assets_current'
ACCOUNT='LE3C_15_チハラ_シゴウ'
pdfmetrics.registerFont(TTFont('JP', r'C:\Windows\Fonts\meiryo.ttc'))
pdfmetrics.registerFont(TTFont('JPB', r'C:\Windows\Fonts\meiryob.ttc'))
BLUE=colors.HexColor('#1F4E79'); CYAN=colors.HexColor('#1991AA'); INK=colors.HexColor('#202A35'); MUTED=colors.HexColor('#5A6470')

styles=getSampleStyleSheet()
title=ParagraphStyle('title',fontName='JPB',fontSize=27,leading=36,textColor=BLUE,spaceAfter=14)
sub=ParagraphStyle('sub',fontName='JP',fontSize=13,leading=21,textColor=MUTED)
h1=ParagraphStyle('h1',fontName='JPB',fontSize=21,leading=28,textColor=BLUE,spaceAfter=10,borderColor=BLUE,borderWidth=0,borderPadding=5)
h2=ParagraphStyle('h2',fontName='JPB',fontSize=13,leading=19,textColor=CYAN,spaceBefore=8,spaceAfter=4)
body=ParagraphStyle('body',fontName='JP',fontSize=10.2,leading=17,textColor=INK,spaceAfter=6)
cap=ParagraphStyle('cap',fontName='JP',fontSize=8.5,leading=13,textColor=MUTED,alignment=TA_CENTER)

def img(name,w=165*mm,caption=''):
    p=ASSETS/name
    out=[Image(str(p),width=w,height=w*0.516)]
    if caption: out += [Spacer(1,2*mm),Paragraph(caption,cap)]
    return out

def bullets(items):
    return ListFlowable([ListItem(Paragraph(x,body),leftIndent=4*mm) for x in items],bulletType='bullet',leftIndent=7*mm,bulletFontName='JP',bulletFontSize=8,spaceAfter=4*mm)

def header_footer(canvas,doc):
    canvas.saveState(); canvas.setFont('JP',7.5); canvas.setFillColor(MUTED)
    canvas.drawRightString(A4[0]-18*mm,10*mm,f'{ACCOUNT}  |  {doc.page}'); canvas.restoreState()

def build(path, cover_title, cover_sub, pages):
    story=[Spacer(1,28*mm),Paragraph('MONTHLY SUBMISSION 2026',h2),Paragraph(cover_title,title),Paragraph(cover_sub,sub),Spacer(1,90*mm),Paragraph('日本工学院専門学校　LE3C 15<br/>チハラ シゴウ<br/>2026年8月',body)]
    for heading,lead,parts in pages:
        story += [PageBreak(),Paragraph(heading,h1)]
        if lead: story += [Paragraph(lead,sub),Spacer(1,4*mm)]
        for kind,value in parts:
            if kind=='h': story.append(Paragraph(value,h2))
            elif kind=='p': story.append(Paragraph(value,body))
            elif kind=='b': story.append(bullets(value))
            elif kind=='i': story += img(*value)
    SimpleDocTemplate(str(path),pagesize=A4,rightMargin=18*mm,leftMargin=18*mm,topMargin=17*mm,bottomMargin=17*mm,title=cover_title,author='チハラ シゴウ').build(story,onFirstPage=header_footer,onLaterPages=header_footer)

portfolio=[
('作品概要','移動・ジャンプ・カメラ操作で立体ステージを攻略し、星のゴールを目指す3D探索アクションです。',[('i',('frame_05.png',165*mm,'雷と照明が変化するステージ。星がゴールの目印。')),('h','コアループ'),('b',['タイトル画面からステージセレクトへ進む','移動とジャンプで足場を攻略し、カメラでルートを探す','星を取得し、専用演出とクリアシーンを経てステージセレクトへ戻る']),('h','操作'),('p','キーボード／マウスとXboxコントローラーに対応。プレイヤー追従カメラと全体カメラを切り替えられます。')]),
('ゲームを支える表現','天候と照明を遊びの空間へ統合しています。',[('i',('frame_03.png',165*mm,'Tempest Storm適用時の暗天候。')),('b',['通常・雨・雪・Tempest Stormの天候プリセット','雨・雪の地面衝突エフェクト、雲、雷のランダム化','天候に連動する天球色・明るさ・複数ライト','GPUパーティクル、ポストエフェクト、プレイヤー発光'])]),
('制作ツールとワークフロー','ゲーム本体だけでなく、制作と確認のためのツールも自作しています。',[('b',['ステージエディタ：配置、保存、一覧表示、天候設定','Blender／外部JSONの読込とファイル更新時のホットリロード確認','エフェクトエディタと天候プリセット連携','スキニングエディタ：GPUスキニング、補間、骨表示、武器装着、左手GPUパーティクル']),('i',('frame_01.png',145*mm,'ゲーム内の操作説明オブジェクト。'))]),
('現在地と今後','機能を増やす段階から、遊びへ結び付けて磨く段階へ進めています。',[('h','現在できていること'),('b',['BaseSceneとSceneManagerによるタイトル／ゲーム／クリアのシーン管理','移動・ジャンプ・カメラ・落下復帰・スター取得判定','スター取得演出、CONGRATULATION表示、次シーンまで継続する花火','外部ステージ読込、天候、照明、GPUパーティクル、ポストエフェクト']),('h','次に強化すること'),('b',['ステージ攻略のバリエーションとギミック追加','スター取得からクリアまでのカメラワーク調整','操作感、当たり判定、カメラ挙動の継続調整','短時間で作品の魅力が伝わる企業向けデモ導線'])])]

program=[
('1. 全体設計','巨大化していたMyGame.cppを整理し、ライフサイクルの窓口へ縮小しました。',[('b',['MyGame：Initialize / Update / Draw / Finalizeの窓口','GameRuntime：ゲーム全体の更新と描画を統括','Controller群：ステージ、天候、カメラ、UI、シーン遷移を担当','Manager群：テクスチャ、モデル、パーティクル、ライトを管理']),('p','役割を分離することで、機能追加時の影響範囲を狭め、処理の所在を追いやすくしています。')]),
('2. プレイヤーとカメラ','探索ゲームの操作性を支える基本機能です。',[('b',['移動・ジャンプ・落下時のスポーン／中継地点復帰','追従カメラと全体カメラの切替','マウスとXboxコントローラーによる回転・ズーム','スターとの接触判定、取得演出、クリアシーンへの進行']),('i',('frame_05.png',160*mm,'ゲームプレイ画面。'))]),
('3. ステージ制作と外部連携','短い反復でステージを調整できる制作環境を構築しました。',[('b',['ImGuiステージエディタで配置・削除・回転・保存','保存済みステージと外部レベルの一覧表示','Blenderから出力したJSONをゲームとエディタで読込','起動中のファイル変更を検知し、確認後に再読込'])]),
('4. 天候・照明・エフェクト','環境表現をプリセットとしてゲームシーンへ適用します。',[('b',['空色、明るさ、ライト、パーティクルを天候ごとに変更','雨・雪の衝突地点で専用パーティクルを生成','Tempest Stormの雲・風雨・雷を統合','プレイヤーライトと雷光を複数光源として同時使用','シーン遷移時に天候とポストエフェクトをNormalへ復帰']),('i',('frame_03.png',160*mm,'Tempest Stormのステージ表示。'))]),
('5. GPUスキニングとアニメーション','頂点変形をCompute Shaderへ渡し、アニメーションと装備を連携します。',[('b',['Compute Shaderで頂点スキニング','複数メッシュ／複数マテリアル対応','アニメーション補間と急なTポーズ復帰の抑制','ボーン・ジョイント・名前・軸のデバッグ表示','右手ジョイントへ武器、左手ジョイントへGPUパーティクルを追従'])]),
('6. シーン管理・描画・安定性','ゲーム進行と描画状態をシーン単位で管理します。',[('b',['BaseSceneを継承したTitleScene／GameClearScene','SceneManagerとSceneFactoryによる生成・更新・描画・終了処理','タイトル、ステージセレクト、ゲーム、クリアの遷移','クリア文字の表示後、次シーンまで画面奥で継続する花火','GPUパーティクル、ポストエフェクト、複数ライトの状態管理']),('h','今後'),('p','GPU負荷の計測、シーン切替の回帰テスト、ステージデータ検証、ゲーム側の完了定義に沿ったチェックを追加します。')])]

build(OUT/f'{ACCOUNT}_ポートフォリオ.pdf','自作エンジンで制作する<br/>3D探索アクション','ゲームと制作ツールを一体化した、制作中の就職作品',portfolio)
build(OUT/f'{ACCOUNT}_プログラム説明資料.pdf','自作ゲームエンジン<br/>プログラム説明資料','DirectX 12ベースのゲーム／ツール統合設計',program)

"""制作スクリプトへ説明を補い、Pythonの構文木が変化しないことを検証する。"""
from pathlib import Path
from datetime import datetime
import ast,shutil
ROOT=Path(__file__).resolve().parents[2]
HEADERS={
 'AddCourseKeys':'ステージ内の回収物と通路を調べ、クリア条件用の鍵を配置する。',
 'AdventureTheme':'紙面・文字・色の共通設定を提供し、ゲーム画面のUIをそろえる。',
 'AnnotateAuthoredSources':'説明一覧を照合し、未記載のC++・シェーダー関数へコメントを補う。',
 'AuditSourceStyle':'外部ライブラリを除いたソースから、波括弧と説明の不足を検出する。',
 'FormatAuthoredSources':'変更前を退避し、処理のトークンを照合しながら書式と波括弧をそろえる。',
 'BuildAdventureUI':'ステージ選択・鍵HUD・操作説明など、探検手帳風のUI画像を生成する。',
 'BuildBuildingMedia':'足場づくりの紹介用に、撮影素材から映像を組み立てる。',
 'BuildBuildingUI':'足場づくりの操作・進行を伝えるUI画像を生成する。',
 'BuildCampaign':'本編ステージと紹介用の案内を生成する。',
 'BuildComparisonMedia':'比較撮影の素材を読み、確認用の映像を作る。',
 'BuildComplexPartsStage':'複雑なパーツの回収・配置を確認する試験ステージを生成する。',
 'BuildComplexRouteStage':'複雑なパーツを使う経路検証用ステージを生成する。',
 'BuildDioramaMaterials':'箱庭表現の地形に使う色・模様のテクスチャを生成する。',
 'BuildDioramaStage':'箱庭の見た目と経路を確認するステージを生成する。',
 'BuildExpeditionStage':'梯子・仕掛け・配置を組み合わせた総合探検ステージを生成する。',
 'BuildExplorationUI':'探索とスキャンの比較に使う案内画像を生成する。',
 'BuildGameOverUI':'失敗・再挑戦画面で使用する文字と背景の素材を生成する。',
 'BuildGardenStyle':'面取りした箱庭ブロックの形状と材質を生成する。',
 'BuildInventoryAssets':'道具袋に表示するパーツのアイコン素材を生成する。',
 'BuildInventoryFont':'道具袋の名前と個数表示に使う文字アトラスを生成する。',
 'BuildJournalResult':'クリア時の手帳・印刷面・ハンコ・印影の3D素材を生成する。',
 'BuildModelRepairPack':'不足モデルと依存素材を収集し、モデル復元用の配布物を作る。',
 'BuildPlayerMarker':'壁越しのプレイヤー位置を示す足元マーカーを生成する。',
 'BuildPresentationClips':'紹介映像に使う撮影素材を、用途ごとのクリップへまとめる。',
 'BuildRotationCourses':'回転と配置を試す2ステージを生成し、既存の編集済みマップは保持する。',
 'BuildRotationTrial':'独立した回転試験ステージと操作案内を生成する。',
 'BuildStagePreviews':'本編マップの形状から、選択画面に表示する縮小模型を生成する。',
 'BuildTitleMesh':'日本語タイトルの輪郭から、面取りと厚みを持つ3D文字を生成する。',
 'BuildUnifiedUI':'共通テーマを使って、ゲーム内のUI素材をまとめて生成する。',
 'PackageSubmission':'実行に必要なファイルを集め、過去の提出物を残して新しいZIPを作る。',
 'recover_models':'不足するモデルとテクスチャを調べ、必要な代替形状を復元する。',
 'VerifyDioramaStage':'箱庭マップの足場と経路を検査する。',
 'VerifyExpeditionStage':'総合探検マップの通路とプレイヤーが通れる空間を検査する。',
 'level_editor':'Blender上でレベルの配置・衝突形状を編集し、JSONで読み書きする。',
 'BuildRelease':'プロジェクト指定の環境を使い、Release/x64をビルドする。',
 'CaptureBuilding':'足場づくりの試験モードを起動し、確認用映像を録画する。',
 'CaptureCinematic':'指定された演出を自動起動し、録画の出力先を設定する。',
 'CaptureComparisons':'比較対象を順に起動し、同じ条件で映像を記録する。',
 'MeasureTutorial':'チュートリアルを起動し、フレーム時間の計測結果を収集する。',
 'OpenRotationCourse':'指定した回転ステージをプレイまたは編集用に開き、起動用の環境設定を復元する。',
 'StartBuilding':'足場づくりの試験モードを設定してゲームを起動する。',
 'StartDiorama':'箱庭表現の検証モードを設定してゲームを起動する。',
 'StartEditor':'編集用構成をビルドし、ステージエディタを開く。',
 'StartExpedition':'総合探検ステージ用の起動設定を適用し、終了後に設定を復元する。',
 'StartExploration':'探索霧の比較モードを設定してゲームを起動する。',
 'StartGame':'必要なプレビューを更新し、Release版をproject作業ディレクトリで起動する。',
 'StartRotationTrial':'回転試験用の起動設定を適用し、開始後に環境変数を復元する。',
}
FUNCTIONS={
 'add':'既存セルを調べ、ステージごとの鍵を追加して保存する。',
 'sheet':'共通の紙色・罫線・影を描き、UIパネルの描画先を返す。',
 'notes':'説明一覧を読み、関数名からコメントを引ける辞書を作る。',
 'describe':'関数と所有クラスを照合し、その処理に対応する説明を選ぶ。',
 'inspect':'ソースの構文木を走査し、波括弧と関数コメントの不足を記録する。',
 'key':'真鍮色の鍵の輪と軸を描く。',
 'star':'交互の半径から星の輪郭を作り、アイコンを描く。',
 'guide':'見出しと説明を描き、指定した大きさの案内画像へ保存する。',
 'room':'部屋の壁と入口を作り、ギミックを置く空間を用意する。',
 'path':'指定した点の間へ、通路用の地形セルを配置する。',
 'save':'生成した設定とセルを、ステージファイルへ書き出す。',
 'write_block':'面取りしたブロックを頂点と材質へ展開し、OBJで保存する。',
 'page':'共通の紙色と罫線から、手帳の印刷用ページを用意する。',
 'box_model':'複数の箱を指定色で組み合わせ、手帳やハンコのモデルを保存する。',
 'quad':'ページ画像と対応するUVを持つ板を生成し、OBJと材質を保存する。',
 'surface':'文字の輪郭を三角形へ分割し、表裏と面取りの面を作る。',
 'make_model':'モデル名に応じて代替形状を選び、必要なメッシュを生成する。',
 'targets':'材質と既知の参照先から、復元対象のモデル一覧を集める。',
 'validate':'OBJの頂点・UV・法線・面を数え、参照の整合性を調べる。',
 'main':'復元対象を検証し、書き込み指定時だけ不足する素材を生成する。',
 'triangle':'3点から法線を求め、三角形の頂点と参照を追加する。',
 'box':'中心と大きさから6面の箱を作り、三角形へ分割する。',
 'text':'文字のビットマップを小さな箱へ展開し、立体文字を作る。',
 'japanese_title':'保存した日本語タイトルの三角形を読み、文字モデルへ追加する。',
 'sphere':'緯度と経度で球面を分割し、三角形のメッシュを作る。',
 'point':'緯度と経度の番号から、球面上の座標を求める。',
 'reachable':'移動可能な足場を探索し、指定地点まで到達できるか調べる。',
 'clear':'プレイヤーの体積に重なるセルを調べ、通れる空間か判定する。',
 'register':'Blenderの操作クラスと衝突形状の描画コールバックを登録する。',
 'unregister':'描画コールバックと操作クラスの登録を解除する。',
 'invoke':'前回の保存先を初期値にして、ファイル選択を開始する。',
}
SPECIAL={
 'BuildModelRepairPack.py:build':'OBJ・材質・依存画像を集め、復元パックとして出力する。',
 'BuildRotationCourses.py:build':'固定足場と回転領域を組み、同名の既存マップがなければ保存する。',
 'BuildTitleMesh.py:build':'日本語フォントの輪郭から表面・側面・面取りを作り、三角形データを保存する。',
 'recover_models.py:Mesh.star':'星の輪郭と厚みから、表裏と側面の三角形を追加する。',
 'level_editor.py:CG2_OT_import_level.execute':'レベルJSONから親子構造・変換・衝突形状をBlenderへ読み込む。',
 'level_editor.py:CG2_OT_export_level.execute':'配置オブジェクトと開始位置を、エンジンが読めるレベルJSONへ書き出す。',
 'level_editor.py:CG2_OT_add_collider.execute':'選択オブジェクトへ箱の衝突設定と初期サイズを追加する。',
 'level_editor.py:CG2_OT_initialize_object_properties.execute':'選択オブジェクトへ、レベル出力に必要なプロパティの既定値を補う。',
 'level_editor.py:_ColliderDrawer.draw':'シーンの衝突形状を線分へ展開し、3Dビューへ重ねて描く。',
 'level_editor.py:CG2_PT_level_editor.draw':'レベルの読み書きと選択オブジェクトの設定をサイドパネルへ表示する。',
}

# コメントだけを挿入し、構文木が変わった場合はファイルを書き換えない。
snapshot=ROOT/'generated/style-review'/('tools-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
changed=0;count=0
for path in sorted((ROOT/'project/Tools').rglob('*')):
    if path.suffix not in ('.py','.ps1') or path==Path(__file__): continue
    data=path.read_bytes();text=data.decode('utf-8-sig');lines=text.splitlines(keepends=True);edits=[]
    header=HEADERS.get(path.stem)
    if header and not text.startswith('# '+header): edits.append((0,'# '+header+'\n'))
    if path.suffix=='.py':
        before=ast.parse(text);owners={}
        for cls in ast.walk(before):
            if isinstance(cls,ast.ClassDef):
                for child in cls.body:
                    if isinstance(child,(ast.FunctionDef,ast.AsyncFunctionDef)): owners[id(child)]=cls.name+'.'
        for node in ast.walk(before):
            if not isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef)) or ast.get_docstring(node) or node.end_lineno-node.lineno<4: continue
            previous=lines[node.lineno-2].lstrip() if node.lineno>1 else ''
            if previous.startswith('#'): continue
            key=path.name+':'+owners.get(id(node),'')+node.name
            note=SPECIAL.get(key) or FUNCTIONS.get(node.name)
            if not note: raise RuntimeError('説明の未確認: '+key)
            indent=lines[node.lineno-1][:len(lines[node.lineno-1])-len(lines[node.lineno-1].lstrip())]
            edits.append((node.lineno-1,indent+'# '+note+'\n'));count+=1
    if not edits: continue
    for index,note in sorted(edits,reverse=True): lines.insert(index,note)
    after=''.join(lines)
    if path.suffix=='.py' and ast.dump(before)!=ast.dump(ast.parse(after)): raise RuntimeError('構文変更: '+str(path))
    backup=snapshot/path.relative_to(ROOT);backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,backup)
    eol='\r\n' if b'\r\n' in data else '\n'
    encoded=after.replace('\r\n','\n').replace('\n',eol).encode('utf-8')
    if data.startswith(b'\xef\xbb\xbf'): encoded=b'\xef\xbb\xbf'+encoded
    path.write_bytes(encoded);changed+=1
print(f'制作スクリプト {changed} ファイル、処理コメント {count} 件を更新')

# 説明一覧を照合し、未記載のC++・シェーダー関数へコメントを補う。
"""Apply reviewed Japanese intent notes to undocumented processing units."""
from pathlib import Path
from datetime import datetime
import json,re,shutil
from AuditSourceStyle import ROOT,PARSER,sources,walk

DOMAINS={
 'DirectXCommon':'DirectXのデバイス・描画先・同期', 'Input':'キーボード・マウス・パッドの入力',
 'WinApp':'ゲーム用ウィンドウ', 'Sprite':'2Dスプライト', 'SpriteCommon':'2Dスプライトの共有描画設定',
 'TextureManager':'画像とSRVの管理', 'Model':'3Dモデル', 'ModelManager':'モデルの共有管理',
 'Object3d':'3Dオブジェクト', 'Object3dCommon':'3Dオブジェクトの共有描画設定',
 'SkinnedModel':'関節と重みを持つモデル', 'SkinnedObject':'スキニング付きオブジェクト',
 'SkinnedObjectCommon':'スキニング付きモデルの共有描画設定', 'ShadowMap':'影の深度マップ',
 'SrvManager':'シェーダー参照用ディスクリプタ', 'ParticleManager':'粒子の発生・更新・描画',
 'Camera':'表示用カメラ', 'LightCamera':'影を描く光源カメラ', 'FrameRecorder':'動画・GIFの録画',
 'BlockInventory':'配置パーツの在庫', 'BlockPlacementController':'配置と取り消し',
 'BubblePickupController':'シャボン玉の回収', 'GameplayCameraController':'プレイ用の追従カメラ',
 'MapCursor':'セル選択カーソル', 'FogExplorer':'探索範囲の霧', 'WeatherRuntimeController':'天候の演出',
 'BlenderRuntimeLevel':'外部レベル', 'GameBgmController':'シーン別BGM', 'GameClearScene':'クリア手帳',
 'GameOver':'失敗と再挑戦の演出', 'GameRuntime':'ゲーム全体', 'MyGame':'ゲーム本体',
 'EffectShowcaseController':'エフェクトの展示', 'PostEffectShowcaseController':'画面効果の展示',
 'RecoveryTransition':'シーン切り替えの幕', 'SceneManager':'現在のシーン',
 'SkinningEditorController':'モデル・関節・配置のエディタ', 'Skybox':'空の背景',
 'StageSelect':'ステージ選択', 'StartIntro':'ステージ到着の演出', 'TitleScene':'タイトルの3D文字と空の演出',
 'StormEffectEditorController':'嵐のプリセット編集', 'StageEditorController':'ステージ編集',
 'StageMap':'ステージのセルとギミック', 'BlockInventoryUI':'道具袋の表示と選択',
 'GameplayUIManager':'操作案内と対象プロンプト', 'Sound':'音声の読み込みと再生',
}
UPDATES={
 'Input':'前フレームの状態を保存し、各入力機器の最新状態と使用中の機器を取得する。',
 'Sprite':'位置・大きさ・UV・色から、2D描画に使う頂点と変換定数を更新する。',
 'SkinnedModel':'再生中モーションから関節姿勢とスキニング用の行列を更新する。',
 'SkinnedObject':'モデルの姿勢とワールド変換を、描画・影・スキニングの定数へ反映する。',
 'ParticleManager':'寿命と速度を進め、天候・衝撃などの粒子を更新して描画データへまとめる。',
 'Camera':'現在の姿勢と投影設定から、ビュー行列と投影行列を再計算する。',
 'LightCamera':'光源の位置と向きから、影の描画に使うビュー・投影行列を求める。',
 'BubblePickupController':'プレイヤー周囲のセルを調べ、接触したシャボン玉の中身を在庫へ移す。',
 'GameplayCameraController':'追従設定と視点入力から、目標の注視点・角度・距離を更新する。',
 'MapCursor':'選択セルからカーソルのワールド位置と描画変換を更新する。',
 'FogExplorer':'移動と経過時間に合わせて、探索の進み具合と表示用状態を更新する。',
 'WeatherRuntimeController':'選択中の天候と地形に合わせ、雨・雪・嵐の発生条件を更新する。',
 'BlenderRuntimeLevel':'外部レベルの各モデルへ、現在のカメラと光源の行列を渡す。',
 'EffectShowcaseController':'プリセット切り替えの入力と自動再生の時刻を進める。',
 'GameBgmController':'シーンに合う曲を選び、再生と音量遷移を更新する。',
 'GameClearScene':'手帳を開いて打刻する時刻・姿勢・効果音を進め、完了後の入力を受け付ける。',
 'GameOver':'失敗演出を進め、再挑戦・選択画面への移動・入力解禁の状態を更新する。',
 'GameRuntime':'入力と演出を進め、現在のシーン・カメラ・光源・UIを順に更新する。',
 'PostEffectShowcaseController':'表示する画面効果を切り替え、ディゾルブなどの時間変化を進める。',
 'RecoveryTransition':'遷移時刻から、幕と装飾スプライトの変換・透明度を更新する。',
 'SceneManager':'現在のシーンに、そのフレームの更新処理を委譲する。',
 'Skybox':'カメラと背景色を反映し、空を描くための変換定数を更新する。',
 'StageSelect':'ステージとプレビューの操作を受け付け、選択状態と表示を更新する。',
 'StartIntro':'到着時刻に合わせて飛行・雲・カメラを進め、通常プレイへ引き渡す。',
 'TitleScene':'到着・待機・出発の段階に合わせ、タイトルと星・空・カメラを動かす。',
 'StageMap':'移動床・敵・崩れる床など、セルに属するギミックの時間と状態を進める。',
 'BlockInventoryUI':'開閉・直接選択・カーソル・決定の入力を処理し、在庫表示へ反映する。',
 'GameplayUIManager':'プレイヤー周囲の操作対象を調べ、位置に合うプロンプトを更新する。',
}
SHADERS={
 'Bloom.PS.hlsl':'周辺の高輝度色を集め、元画像へブルームの光を重ねる。',
 'BoxFilter3x3.PS.hlsl':'3×3の近傍画素を平均し、画像をぼかす。',
 'BoxFilter5x5.PS.hlsl':'5×5の近傍画素を平均し、画像をぼかす。',
 'CloudSea.PS.hlsl':'視線に沿って雲の密度を積算し、光と透過を持つ雲海の色を求める。',
 'CopyImage.PS.hlsl':'入力テクスチャの色を、そのまま画面出力へ渡す。',
 'DepthBasedOutline.PS.hlsl':'隣接する深度の差から、形状の輪郭を強調する。',
 'Dissolve.PS.hlsl':'マスクとしきい値を比較し、消去範囲と境界の色を合成する。',
 'EmitParticle.CS.hlsl':'空きリストから粒子を取り出し、発生位置・速度・寿命を設定する。',
 'Fullscreen.VS.hlsl':'画面全体を覆う頂点とUVを作り、画面効果の入力へ渡す。',
 'GaussianFilter.PS.hlsl':'周辺画素へガウス重みを付け、滑らかなぼかしを求める。',
 'GPUParticle.VS.hlsl':'GPU粒子の状態から頂点を変換し、描画用の座標と色を求める。',
 'Grayscale.PS.hlsl':'入力色から輝度を求め、グレースケールへ変換する。',
 'InitializeParticle.CS.hlsl':'粒子の状態と空きリストを初期値へ設定する。',
 'LuminanceBasedOutline.PS.hlsl':'隣接画素の輝度差を使い、明暗の境界を強調する。',
 'Object3d.PS.hlsl':'材質・光源・影・環境効果を合成し、3Dモデルの画素色を決める。',
 'Object3dInstanced.PS.hlsl':'インスタンスごとの色へ材質・光源・影を適用し、画素色を決める。',
 'Object3d.VS.hlsl':'モデル頂点をカメラと光源の空間へ変換し、法線とUVを渡す。',
 'Particle.PS.hlsl':'粒子のテクスチャと密度・色を合成し、半透明の画素を出力する。',
 'Particle.VS.hlsl':'粒子ごとの変換を頂点へ適用し、UVと色を渡す。',
 'ParticlePrimitive.PS.hlsl':'粒子の形状・材質に合わせて色と透明度を求める。',
 'RadialBlur.PS.hlsl':'中心からの方向へ複数回サンプリングし、放射状のぼかしを作る。',
 'Random.PS.hlsl':'画面座標と時刻からノイズを作り、入力画像へ適用する。',
 'Sepia.PS.hlsl':'入力色をセピア調の配色へ変換する。',
 'ShadowMap.PS.hlsl':'影の深度パスに必要な出力を返す。',
 'ShadowMap.VS.hlsl':'頂点を光源の投影空間へ変換し、影の深度を記録できるようにする。',
 'ShadowMapInstanced.VS.hlsl':'インスタンスの変換を適用し、頂点を光源の投影空間へ送る。',
 'SkinnedObject.VS.hlsl':'変形済みの頂点をカメラと光源の空間へ変換する。',
 'Skinning.CS.hlsl':'関節の重み付き行列から、変形後の頂点位置と法線を計算する。',
 'Skybox.PS.hlsl':'視線方向に対応するキューブマップを読み、背景色を出力する。',
 'Skybox.VS.hlsl':'空の頂点をカメラへ合わせ、キューブマップの参照方向を渡す。',
 'Skydome.VS.hlsl':'空のドームをカメラ空間へ変換し、背景用の位置とUVを渡す。',
 'Sprite.PS.hlsl':'テクスチャとスプライト色を掛け合わせ、2D表示の画素色を決める。',
 'Sprite.VS.hlsl':'2D頂点へ変換を適用し、画面位置とUVを求める。',
 'UpdateParticle.CS.hlsl':'GPU粒子の移動と寿命を進め、消えた粒子を空きリストへ戻す。',
 'Vignette.PS.hlsl':'画面中心からの距離で明るさを調整し、周辺を落とす。',
}

# 説明一覧を読み、関数名からコメントを引ける辞書を作る。
def notes():
    result={}
    for line in Path(__file__).with_name('SourceCommentNotes.txt').read_text(encoding='utf-8').splitlines():
        if line and not line.startswith('#'):
            key,value=line.split(' = ',1);result[key]=value
    return result

# 関数と所有クラスを照合し、その処理に対応する説明を選ぶ。
def describe(path,header,lookup):
    match=re.search(r'([~\w:]+)\s*\(',header)
    if not match: return None
    qualified=match.group(1);name=qualified.split('::')[-1]
    owner=qualified.split('::')[0] if '::' in qualified else path.stem
    domain=DOMAINS.get(owner)
    exact=lookup.get(path.name+':'+name) or lookup.get(qualified) or lookup.get(name)
    if exact: return exact
    if name=='main':
        return SHADERS.get(path.name,{'StageEditorTests.cpp':'マップ保存・拡張・回転・選択などの回帰テストを実行する。',
            'RecoveryTests.cpp':'復帰と画面遷移の時刻・入力受付を検証する。',
            'FogExplorerTests.cpp':'探索範囲の記録と霧の更新結果を検証する。'}.get(path.name))
    if name.startswith('~'):
        return '保持するGPU資源と参照を解放する。' if owner!='D3DResourceLeakChecker' else '終了時のDXGIデバッグ情報から、解放されていないGPU資源を報告する。'
    if domain:
        if name=='Initialize': return domain+'を準備し、初期値と必要なリソースを設定する。'
        if name=='Finalize': return domain+'が保持するオブジェクトとリソースを解放する。'
        if name=='Draw': return domain+'の表示対象を選び、描画コマンドを記録する。'
        if name=='PreDraw': return domain+'に使うパイプラインと参照先をコマンドリストへ設定する。'
        if name=='DrawImGui': return domain+'の設定と状態をGUIへ表示し、編集入力を反映する。'
        if name=='Update': return UPDATES.get(owner)
    return None

if __name__=='__main__':
    import argparse
    ap=argparse.ArgumentParser();ap.add_argument('--apply',action='store_true');args=ap.parse_args()
    lookup=notes();pending=[];missing=[]
    for path in sources():
        data=path.read_bytes();edits=[]
        for node in walk(PARSER.parse(data).root_node):
            if node.type!='function_definition' or node.end_point.row-node.start_point.row<4: continue
            body=node.child_by_field_name('body')
            if not body: continue
            previous=node.prev_named_sibling
            if previous and previous.type=='comment' and node.start_point.row-previous.end_point.row<=2: continue
            start=data.rfind(b'\n',0,node.start_byte)+1
            indentation=data[start:node.start_byte]
            if indentation.strip(): continue
            header=data[node.start_byte:body.start_byte].decode('utf-8-sig')
            comment=describe(path,header,lookup)
            if comment:
                eol=b'\r\n' if b'\r\n' in data else b'\n'
                edits.append((start,indentation+('// '+comment).encode('utf-8')+eol))
            else: missing.append((str(path.relative_to(ROOT)),header.split('(')[0].strip()))
        if edits: pending.append((path,data,edits))
    report=ROOT/'generated/style-review/missing-comments.json'
    report.write_text(json.dumps(missing,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'documentable':sum(len(p[2]) for p in pending),'missing':len(missing)}))
    if args.apply:
        if missing: raise RuntimeError('Review missing descriptions before applying comments')
        backup=ROOT/'generated/style-review'/('comments-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
        for path,data,edits in pending:
            dest=backup/path.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
            for start,comment in sorted(edits,reverse=True): data=data[:start]+comment+data[start:]
            path.write_bytes(data)

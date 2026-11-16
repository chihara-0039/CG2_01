# 壁越しプレイヤー表示（2026-11-10）

- 現在のGPUスキニング済み頂点を、壁の深度より奥の部分だけ半透明の水色で描画。
- 補助パスを通常プレイヤーより前に描き、見えている本体は通常描画で上書きする。自己遮蔽による全身の着色を防ぐ。
- マテリアル・スケール・GPU定数バッファを書き換えず、プレイヤー1回の追加描画。旧処理の元モデル3回描画は廃止。
- 隠れている間は足元平面へ投影した水色の輪と中心点を表示。黒い縁で明るい壁でも識別できる。ネイティブSpriteで描くためReleaseでも表示。
- 足元マーカーの隠れ判定は足元・腰・頭の3本のレイ。整数中心のセル座標に合わせ、Blender使用時は外部コライダーも見る。深度による本体透過判定とは独立。細い遮蔽物・非コライダーメッシュ等ではマーカー判定に差が出る可能性あり。
- 一人称・開始演出・ゲームオーバー中の補助表示は抑止。

Debug/x64・Release/x64ビルド成功。Releaseの実機録画で、完全遮蔽時のシルエットと足元の輪、遮蔽物がなくなると通常表示へ戻ることを視認。手操作による全ステージ確認は未実施。

確認素材：generated/occlusion-review/verified/hidden.png、clear.png、cinematic-occlusion.mp4 / .gif。
再撮影：project/Tools/CaptureCinematic.ps1 -Shot occlusion（確認用の位置・床変更は撮影中だけで、ステージファイルは変更しない）。
新規ピクセルシェーダ：project/Resources/shaders/hlsl/PlayerOccluded.PS.hlsl。
足元マーカー生成元：project/Tools/BuildPlayerMarker.py。

性能確認：Releaseチュートリアル静止600フレーム（デバッガ・録画なし）は平均60.00FPS。CSVはgenerated/performance/tutorial-occlusion-release.csv。遮蔽状態の負荷測定は別途未実施。

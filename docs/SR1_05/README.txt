SR1 01_05 UMLクラス図 / CG2_01

提出用画像（2400 x 1380 px）
  01_Command.png             01_04の配置・取消と所有関係
  02_Particle_Strategy.png   01_02の継承・仮想関数呼び出しと粒の所有関係
一枚のみ提出する場合、継承とコンポジションを両方示す02を推奨。

実コードを確認して主要メンバーを抜粋。確認時のmaster: d8ae133。
今回ゲームの実装には変更を加えていない。

編集用
  .mmd: Mermaid記法の対応するクラス図。型・引数は読みやすさのため一部簡略化。
  render_diagrams.py: PNGの配置・文字・UML記号を描画するPythonソース。
  リポジトリ直下で .venv/Scripts/python.exe docs/SR1_05/render_diagrams.py
  PillowとWindowsの游ゴシックを使用。PNGはこのスクリプトから生成したもの。

記号
  白い三角: 継承。先端は基底クラス。
  黒い菱形: コンポジション。菱形は所有側。
  実線矢印: 継続的な参照。対象の寿命は所有しない。
  破線矢印: 呼び出しや関数引数による依存。
  + 公開 / - 非公開。図のクラスは全メンバーを網羅していない。

実装対応
  project/Game/Block/BlockPlacementController.h / .cpp
  project/Game/Block/PlaceBlocksCommand.h / .cpp
  project/Game/Block/BlockInventory.h
  project/Game/Stage/StageMap.h
  project/Engine/Graphics/Particle/ParticleMotion.h / .cpp
  project/Engine/Graphics/Particle/ParticleManager.h / .cpp
  project/Engine/Graphics/Particle/Particle.h

補足
  ParticleGroupはParticleManager内の公開struct。
  派生MotionクラスはParticleMotion.cpp内の無名名前空間に定義。
  SelectParticleMotionは自由関数で、共有staticの実装を基底const参照で返す。
  ParticleManagerはMotionをメンバー所有しないため、ここには菱形を使わない。
  Commandの取消履歴は直前の一操作のみ（0..1）。粒とグループは0個以上（0..*）。

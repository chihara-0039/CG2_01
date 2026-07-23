#pragma once

class Input;
class ParticleManager;
class PostProcessRenderer;

/// CG5評価課題用のポストエフェクト閲覧モードを管理する。
/// キー割り当てと説明UIを同じクラスに置き、表示と操作の不一致を防ぐ。
class PostEffectShowcaseController {
public:
    /// エフェクト切り替えを処理する。TABが押された場合はtrueを返す。
    bool Update(Input& input, ParticleManager* particleManager, PostProcessRenderer& postProcess);

    /// 現在のエフェクト名とRelease用操作ガイドを描画する。
    void DrawImGui(const PostProcessRenderer& postProcess) const;
};

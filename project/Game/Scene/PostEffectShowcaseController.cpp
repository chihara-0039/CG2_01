#include "PostEffectShowcaseController.h"

#include "Input.h"
#include "ParticleManager.h"
#include "PostProcessRenderer.h"
#include "externals/imgui/imgui.h"
#include <iterator>

namespace {
/// 入力キー、PostProcess内部番号、画面表示名の対応表。
struct PostEffectBinding {
    BYTE key;
    int mode;
    const char* keyLabel;
    const char* effectName;
};

constexpr PostEffectBinding kBindings[] = {
    {DIK_1, 1, "1", "Grayscale"},
    {DIK_2, 3, "2", "Vignetting"},
    {DIK_3, 6, "3", "GaussianFilter / Smoothing"},
    {DIK_4, 4, "4", "BoxFilter 3x3"},
    {DIK_5, 5, "5", "BoxFilter 5x5"},
    {DIK_6, 7, "6", "LuminanceBasedOutline"},
    {DIK_7, 8, "7", "DepthBasedOutline"},
    {DIK_8, 9, "8", "RadialBlur"},
    {DIK_9, 10, "9", "Dissolve"},
    {DIK_0, 11, "0", "Random"},
};

const char* GetEffectName(int mode) {
    for (const PostEffectBinding& binding : kBindings) {
        if (binding.mode == mode) {
            return binding.effectName;
        }
    }
    return mode == 2 ? "Sepia" : "Normal";
}
} // namespace

bool PostEffectShowcaseController::Update(
    Input& input, ParticleManager* particleManager, PostProcessRenderer& postProcess) {
    postProcess.SetEnabled(true);

    // 画面空間エフェクトを評価しやすくするため、パーティクル演出を停止する。
    if (particleManager) {
        particleManager->SetStormActive(false);
        particleManager->SetDrawGPUParticleSphere(false);
        particleManager->ClearParticles();
    }

    for (const PostEffectBinding& binding : kBindings) {
        if (!input.TriggerKey(binding.key)) {
            continue;
        }
        postProcess.SetPostEffectMode(binding.mode);
        if (binding.mode == 10) {
            postProcess.SetDissolveThreshold(0.35f);
        } else if (binding.mode == 11) {
            postProcess.SetRandomMode(0);
            postProcess.SetRandomStrength(0.55f);
        }
    }
    return input.TriggerKey(DIK_TAB);
}

void PostEffectShowcaseController::DrawImGui(const PostProcessRenderer& postProcess) const {
    const ImGuiIO& io = ImGui::GetIO();
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;

#ifdef NDEBUG
    const float headerX = 24.0f;
    const float headerWidth = io.DisplaySize.x - 48.0f;
#else
    const float headerX = 344.0f;
    const float headerWidth = io.DisplaySize.x - headerX - 24.0f;
#endif

    ImGui::SetNextWindowPos(ImVec2(headerX, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(headerWidth, 92.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("PostEffect Showcase Header", nullptr, flags);
    ImGui::SetWindowFontScale(1.45f);
    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.36f, 1.0f), "POST EFFECT SHOWCASE");
    ImGui::SetWindowFontScale(1.15f);
    ImGui::Text("Current : %s", GetEffectName(postProcess.GetPostEffectMode()));
    ImGui::TextUnformatted("CG5 Evaluation Task 1 dedicated mode");
    ImGui::End();

#ifdef NDEBUG
    const float margin = 24.0f;
    const float height = 214.0f;
    ImGui::SetNextWindowPos(ImVec2(margin, io.DisplaySize.y - height - margin), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - margin * 2.0f, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("PostEffect Showcase Controls", nullptr, flags);
    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.36f, 1.0f), "PostEffect Controls");
    ImGui::Separator();
    ImGui::TextUnformatted("Particle effects are disabled so each screen-space effect is easy to inspect.");
    ImGui::Columns(2, "PostEffectOnlyKeyColumns", false);
    for (int i = 0; i < static_cast<int>(std::size(kBindings)); ++i) {
        ImGui::Text("%s : %s", kBindings[i].keyLabel, kBindings[i].effectName);
        if (i == 4) {
            ImGui::NextColumn();
        }
    }
    ImGui::Columns(1);
    ImGui::TextUnformatted("TAB : Back to Stage Select     MMB : Orbit     Shift+MMB : Pan     Wheel : Zoom");
    ImGui::End();
#endif
}

#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"

// ==============================================================
//  DirectionalLight 讒矩菴・
//
//  GPU (HLSL 繧ｷ繧ｧ繝ｼ繝繝ｼ) 縺ｫ貂｡縺吝ｹｳ陦悟・貅舌ョ繝ｼ繧ｿ縲・
//  縲悟､ｪ髯ｽ蜈峨阪・繧医≧縺ｫ譁ｹ蜷代・豎ｺ縺ｾ縺｣縺ｦ縺・ｋ縺悟・貅舌′辟｡髯宣□縺ｫ縺ゅｋ辣ｧ譏弱・
//
//  縲食LSL 蛛ｴ縺ｮ蟇ｾ蠢懊ヰ繝・ヵ繧｡縲・
//    cbuffer LightBuffer : register(b1) { DirectionalLight light; }
//
//  縲仙推繝輔ぅ繝ｼ繝ｫ繝峨・諢丞袖縲・
//    color         : 蜈峨・濶ｲ (RGBA)縲ら區 (1,1,1,1) 縺瑚・辟ｶ縺ｪ譏ｼ蜈峨・
//    direction     : 蜈峨′髯阪▲縺ｦ縺上ｋ譁ｹ蜷・(豁｣隕丞喧繝吶け繝医Ν)縲・
//                    萓・ (0,-1,0) 竊・逵滉ｸ翫°繧蛾剄繧句・縲・
//                    SetLightDirection() 縺ｧ閾ｪ蜍墓ｭ｣隕丞喧縺輔ｌ繧九・
//    intensity     : 蜈峨・蠑ｷ縺・(0.0:證励＞ ~ 1.0:騾壼ｸｸ ~ 2.0莉･荳・譏弱ｋ縺・
//    cameraPosition: 繧ｹ繝壹く繝･繝ｩ繝ｼ (髀｡髱｢蜿榊ｰ・ 縺ｨ繝ｪ繝繝ｩ繧､繝医・險育ｮ励↓
//                    繧ｫ繝｡繝ｩ菴咲ｽｮ縺悟ｿ・ｦ√↑縺溘ａ霑ｽ蜉縲・
//    paddingLight  : HLSL 縺ｮ 16 繝舌う繝医い繝ｩ繧､繝｡繝ｳ繝郁ｦ∽ｻｶ繧呈ｺ縺溘☆縺溘ａ縺ｮ
//                    遨ｺ縺阪ヱ繝・ぅ繝ｳ繧ｰ縲ょｿ・★菫晄戟縺吶ｋ縺薙→縲・
// ==============================================================
struct DirectionalLight {
    Vector4 color;           // 蜈峨・濶ｲ RGBA
    Vector3 direction;       // 蜈峨・譁ｹ蜷・(豁｣隕丞喧貂医∩)
    float   intensity;       // 蜈峨・蠑ｷ縺・
    Vector3 cameraPosition;  // 繧ｹ繝壹く繝･繝ｩ繝ｼ險育ｮ礼畑繧ｫ繝｡繝ｩ菴咲ｽｮ
    float   paddingLight;
    Vector3 pointLightPosition;
    float   pointLightIntensity;
    Vector4 pointLightColor;
};

// ==============================================================
//  Object3dCommon
//
//  繧ｷ繝ｼ繝ｳ蜀・・蜈ｨ 3D 繧ｪ繝悶ず繧ｧ繧ｯ繝医′蜈ｱ譛峨☆繧区緒逕ｻ繝ｪ繧ｽ繝ｼ繧ｹ繧堤ｮ｡逅・☆繧九け繝ｩ繧ｹ縲・
//
//  笏笏笏 荳ｻ縺ｪ蠖ｹ蜑ｲ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
//  1. RootSignature 縺ｮ逕滓・繝ｻ菫晄戟
//     竊・繧ｷ繧ｧ繝ｼ繝繝ｼ縺ｫ菴輔ｒ貂｡縺吶° (螳壽焚繝舌ャ繝輔ぃ繝ｻ繝・け繧ｹ繝√Ε縺ｪ縺ｩ) 繧貞ｮ夂ｾｩ縺吶ｋ
//
//  2. PSO (Pipeline State Object) 縺ｮ逕滓・繝ｻ菫晄戟
//     竊・鬆らせ繧ｷ繧ｧ繝ｼ繝繝ｼ繝ｻ繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繝ｼ繝ｻ繝悶Ξ繝ｳ繝峨・繝ｩ繧ｹ繧ｿ繝ｩ繧､繧ｶ繝ｼ繧・
//        縺ｾ縺ｨ繧√※縺ｲ縺ｨ縺､縺ｮ縲梧緒逕ｻ險ｭ螳壹そ繝・ヨ縲阪→縺励※ GPU 縺ｫ逋ｻ骭ｲ縺励◆繧ゅ・縲・
//        謠冗判縺吶ｋ遞ｮ鬘槭＃縺ｨ縺ｫ PSO 縺悟ｭ伜惠縺吶ｋ (騾壼ｸｸ / 蠖ｱ / 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ 縺ｪ縺ｩ)
//
//  3. 蟷ｳ陦悟・貅・(DirectionalLight) 縺ｮ螳壽焚繝舌ャ繝輔ぃ邂｡逅・
//     竊・繧ｲ繝ｼ繝繝ｯ繝ｼ繝ｫ繝牙・菴薙〒蜈ｱ譛峨☆繧句・縺ｮ譁ｹ蜷代・濶ｲ繝ｻ蠑ｷ蠎ｦ繧・GPU 縺ｫ騾√ｋ
//
//  笏笏笏 菴ｿ縺・婿 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
//  Object3d::Draw() 縺ｮ逶ｴ蜑阪↓ PreDraw() 繧貞他縺ｶ縺ｨ縲・
//  驕ｩ蛻・↑ RootSignature 縺ｨ PSO 縺・CommandList 縺ｫ繝舌う繝ｳ繝峨＆繧後ｋ縲・
//
//  笏笏笏 PSO 縺ｮ遞ｮ鬘・笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
//  騾壼ｸｸ PSO             : 繝ｩ繧､繝・ぅ繝ｳ繧ｰ繝ｻ繝・け繧ｹ繝√Ε縺ゅｊ縲ゆｸ闊ｬ逧・↑ 3D 繧ｪ繝悶ず繧ｧ繧ｯ繝育畑縲・
//  蠖ｱ PSO               : 豺ｱ蠎ｦ蛟､縺ｮ縺ｿ譖ｸ縺崎ｾｼ繧縲４hadowMap 逕滓・逕ｨ縲・
//  繝励Ξ繧､繝､繝ｼ蠑ｷ隱ｿ PSO   : 螢√↓髫繧後◆繝励Ξ繧､繝､繝ｼ繧偵す繝ｫ繧ｨ繝・ヨ縺ｧ陦ｨ遉ｺ縺吶ｋ縺溘ａ縺ｮ迚ｹ谿・PSO縲・
//  繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ PSO : 蜷後§繝｢繝・Ν繧定､・焚縺ｾ縺ｨ繧√※ 1 繝峨Ο繝ｼ繧ｳ繝ｼ繝ｫ縺ｧ謠上￥縺溘ａ縺ｮ PSO縲・
//  蜊企乗・繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ PSO : 繝悶Ο繝・け縺ｮ蜊企乗・陦ｨ遉ｺ逕ｨ縲・
// ==============================================================
class Object3dCommon {
public:
    // -------------------------------------------------------
    //  蛻晄悄蛹悶３ootSignature / PSO / LightBuffer 繧堤函謌舌☆繧九・
    //  TextureManager 縺ｯ SetTextureManager() 縺ｧ莠句燕縺ｫ繧ｻ繝・ヨ縺励※縺翫￥縺薙→縲・
    // -------------------------------------------------------
    void Initialize(DirectXCommon* dxCommon);

    // -------------------------------------------------------
    //  PreDraw : 謠冗判蜑阪↓ CommandList 縺ｸ謠冗判險ｭ螳壹ｒ繝舌う繝ｳ繝峨☆繧九・
    //  蜈ｷ菴鍋噪縺ｫ縺ｯ莉･荳九ｒ陦後≧:
    //    繝ｻRootSignature 繧偵そ繝・ヨ (縺ｩ縺ｮ繧ｹ繝ｭ繝・ヨ縺ｫ菴輔ｒ貂｡縺吶°)
    //    繝ｻPSO 繧偵そ繝・ヨ (繧ｷ繧ｧ繝ｼ繝繝ｼ繝ｻ繝ｩ繧ｹ繧ｿ繝ｩ繧､繧ｶ繝ｼ險ｭ螳・
    //    繝ｻ繝励Μ繝溘ユ繧｣繝悶ヨ繝昴Ο繧ｸ繝ｼ繧偵そ繝・ヨ (TRIANGLELIST)
    //    繝ｻ蟷ｳ陦悟・貅舌・螳壽焚繝舌ャ繝輔ぃ繧・b1 繧ｹ繝ｭ繝・ヨ縺ｫ繝舌う繝ｳ繝・
    //  蜷・が繝悶ず繧ｧ繧ｯ繝医・ Draw() 蜑阪↓縺薙ｌ縺悟他縺ｰ繧後※縺・ｋ縺薙→繧貞燕謠舌→縺吶ｋ縲・
    // -------------------------------------------------------
    void PreDraw();

    // 笏笏 繧ｲ繝・ち繝ｼ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏

    /// <summary>DirectXCommon 縺ｸ縺ｮ繧｢繧ｯ繧ｻ繧ｹ (繝・け繧ｹ繝√Ε繝ｭ繝ｼ繝画凾縺ｪ縺ｩ縺ｫ菴ｿ逕ｨ)</summary>
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    /// <summary>
    /// 騾壼ｸｸ謠冗判逕ｨ縺ｮ RootSignature縲・
    /// 繧ｷ繝｣繝峨え繝槭ャ繝玲緒逕ｻ蜑阪↓ commandList->SetGraphicsRootSignature() 縺ｫ貂｡縺吶・
    /// </summary>
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

    /// <summary>
    /// 騾壼ｸｸ謠冗判逕ｨ縺ｮ PSO (繝ｩ繧､繝・ぅ繝ｳ繧ｰ繝ｻ繝・け繧ｹ繝√Ε縺ゅｊ)縲・
    /// 縺薙％縺ｧ null 縺瑚ｿ斐ｋ蝣ｴ蜷医・ Initialize() 縺ｧ縺ｮ PSO 逕滓・縺ｫ螟ｱ謨励＠縺ｦ縺・ｋ縲・
    /// </summary>
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }
    ID3D12PipelineState* GetSkinnedPipelineState() const { return skinnedPipelineState_.Get(); }

    /// <summary>
    /// 蠖ｱ謠冗判逕ｨ縺ｮ PSO (豺ｱ蠎ｦ蛟､縺ｮ縺ｿ譖ｸ縺崎ｾｼ繧繝ｻ繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繝ｼ縺ｪ縺・縲・
    /// ShadowMap::PreDraw() 蠕後↓縺薙ｌ繧偵そ繝・ヨ縺励※ DrawShadow() 繧貞他縺ｶ縲・
    /// </summary>
    ID3D12PipelineState* GetShadowPipelineState() const { return shadowPipelineState_.Get(); }

    // 笏笏 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ逕ｨ繧ｲ繝・ち繝ｼ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    // 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ = 蜷後§繝｢繝・Ν繧貞､ｧ驥上↓ 1 繝峨Ο繝ｼ繧ｳ繝ｼ繝ｫ縺ｧ謠上￥謚豕輔・
    // 繝悶Ο繝・け縺ｮ繧医≧縺ｫ縺溘￥縺輔ｓ荳ｦ縺ｶ繧ｪ繝悶ず繧ｧ繧ｯ繝医↓譛牙柑縲・

    /// <summary>繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ逕ｨ RootSignature</summary>
    ID3D12RootSignature* GetInstancedRootSignature() const { return instancedRootSignature_.Get(); }

    /// <summary>繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ騾壼ｸｸ謠冗判 PSO</summary>
    ID3D12PipelineState* GetInstancedPipelineState() const { return instancedPipelineState_.Get(); }

    /// <summary>繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ蠖ｱ謠冗判 PSO</summary>
    ID3D12PipelineState* GetInstancedShadowPipelineState() const { return instancedShadowPipelineState_.Get(); }

    /// <summary>繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ蜊企乗・謠冗判 PSO (螢√・騾乗・蛹悶↑縺ｩ縺ｫ菴ｿ逕ｨ)</summary>
    ID3D12PipelineState* GetInstancedAlphaPipelineState() const { return instancedAlphaPipelineState_.Get(); }

    // 笏笏 繝ｩ繧､繝亥宛蠕｡ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    // 繧ｹ繝・・繧ｸ縺斐→縺ｫ蜈峨・譁ｹ蜷代・濶ｲ繝ｻ蠑ｷ縺輔ｒ螟峨∴繧九％縺ｨ縺ｧ髮ｰ蝗ｲ豌励′螟峨ｏ繧九・
    // StageMap 縺九ｉ隱ｭ縺ｿ蜿悶▲縺溷､繧呈ｯ弱ヵ繝ｬ繝ｼ繝 Update() 縺ｧ縺薙％縺ｫ險ｭ螳壹☆繧九・

    /// <summary>繝・ヵ繧ｩ繝ｫ繝医・繝ｩ繧､繝郁ｨｭ螳壹↓謌ｻ縺・(逋ｽ縺・・繝ｻ譁懊ａ荳翫°繧臥・繧峨☆)</summary>
    void SetDefaultLight();

    /// <summary>蟷ｳ陦悟・貅舌・譁ｹ蜷代ｒ繧ｻ繝・ヨ縲ょ・驛ｨ縺ｧ閾ｪ蜍墓ｭ｣隕丞喧縺輔ｌ繧九・/summary>
    void SetLightDirection(const Vector3& direction) {
        if (lightData_) lightData_->direction = Math::Normalize(direction);
    }

    /// <summary>蟷ｳ陦悟・貅舌・濶ｲ繧偵そ繝・ヨ (RGBA)</summary>
    void SetLightColor(const Vector4& color) {
        if (lightData_) lightData_->color = color;
    }

    /// <summary>蟷ｳ陦悟・貅舌・蠑ｷ縺輔ｒ繧ｻ繝・ヨ (0.0 縲・2.0 遞句ｺｦ縺悟ｮ溽畑逧・</summary>
    void SetLightIntensity(float intensity) { lightData_->intensity = intensity; }
    void SetPointLight(const Vector3& pos, float intensity, const Vector4& color) {
        if(lightData_) {
            lightData_->pointLightPosition = pos;
            lightData_->pointLightIntensity = intensity;
            lightData_->pointLightColor = color;
        }
    }

    /// <summary>
    /// 繧ｹ繝壹く繝･繝ｩ繝ｼ險育ｮ礼畑縺ｮ繧ｫ繝｡繝ｩ菴咲ｽｮ繧偵そ繝・ヨ縲・
    /// MyGame::Update() 縺ｧ豈弱ヵ繝ｬ繝ｼ繝 camera->GetPosition() 繧呈ｸ｡縺吶％縺ｨ縲・
    /// </summary>
    void SetCameraPosition(const Vector3& cameraPosition) {
        if (lightData_) lightData_->cameraPosition = cameraPosition;
    }

    /// <summary>GPU 荳翫・蟷ｳ陦悟・貅舌ヰ繝・ヵ繧｡縺ｮ莉ｮ諠ｳ繧｢繝峨Ξ繧ｹ (螳壽焚繝舌ャ繝輔ぃ縺ｮ繝舌う繝ｳ繝峨↓菴ｿ逕ｨ)</summary>
    D3D12_GPU_VIRTUAL_ADDRESS GetLightGPUVirtualAddress() const {
        return lightResource_->GetGPUVirtualAddress();
    }

    // 笏笏 TextureManager 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏

    /// <summary>
    /// TextureManager 繧偵そ繝・ヨ縺吶ｋ縲・nitialize() 繧医ｊ蜑阪↓蠢・★蜻ｼ縺ｶ縺薙→縲・
    /// 繝・け繧ｹ繝√Ε縺ｮ SRV 繝偵・繝怜盾辣ｧ縺ｫ菴ｿ縺・・
    /// </summary>
    void SetTextureManager(TextureManager* textureManager) { textureManager_ = textureManager; }

    /// <summary>TextureManager 縺ｸ縺ｮ繝昴う繝ｳ繧ｿ繧定ｿ斐☆</summary>
    TextureManager* GetTextureManager() const { return textureManager_; }

    void SetEnvironmentTextureHandle(uint32_t textureHandle) { environmentTextureHandle_ = textureHandle; }
    uint32_t GetEnvironmentTextureHandle() const { return environmentTextureHandle_; }

    // -------------------------------------------------------
    //  PreDrawPlayerHighlight
    //  繧ｫ繝｡繝ｩ縺ｨ繝励Ξ繧､繝､繝ｼ縺ｮ髢薙↓螢√′縺ゅｋ蝣ｴ蜷医↓
    //  繝励Ξ繧､繝､繝ｼ繧貞｣∬ｶ翫＠縺ｧ繧ゅす繝ｫ繧ｨ繝・ヨ陦ｨ遉ｺ縺吶ｋ縺溘ａ縺ｮ迚ｹ谿・PSO 縺ｫ蛻・ｊ譖ｿ縺医ｋ縲・
    //  縺薙・蠕・Player::DrawHighlight() 繧貞他縺ｳ縲∫ｵゅｏ縺｣縺溘ｉ PreDraw() 縺ｧ謌ｻ縺吶・
    // -------------------------------------------------------
    void PreDrawPlayerHighlight();

private:
    // 笏笏 蛻晄悄蛹門・驛ｨ髢｢謨ｰ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    // 蜷・Μ繧ｽ繝ｼ繧ｹ縺ｮ逕滓・繧貞ｽｹ蜑ｲ縺斐→縺ｫ蛻・牡縲る・分縺ｫ萓晏ｭ倬未菫ゅ′縺ゅｋ縺溘ａ豕ｨ諢上・

    void CreateRootSignature();             // 騾壼ｸｸ謠冗判逕ｨ RootSignature
    void CreateGraphicsPipeline();
    void CreateSkinnedPipeline();          // 騾壼ｸｸ謠冗判逕ｨ PSO
    void CreateLightBuffer();              // 蟷ｳ陦悟・貅舌・螳壽焚繝舌ャ繝輔ぃ
    void CreateShadowPipeline();           // 蠖ｱ謠冗判逕ｨ PSO
    void CreateInstancedRootSignature();   // 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ逕ｨ RootSignature
    void CreateInstancedGraphicsPipeline();// 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ騾壼ｸｸ PSO
    void CreateInstancedShadowPipeline();  // 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ蠖ｱ PSO
    void CreatePlayerHighlightPipeline();  // 繝励Ξ繧､繝､繝ｼ繧ｷ繝ｫ繧ｨ繝・ヨ逕ｨ PSO
    void CreateInstancedAlphaPipeline();   // 蜊企乗・繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ PSO

private:
    DirectXCommon*  dxCommon_       = nullptr;
    TextureManager* textureManager_ = nullptr;
    uint32_t environmentTextureHandle_ = 0;

    // 笏笏 RootSignature / PSO 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;     // 騾壼ｸｸ謠冗判逕ｨ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skinnedPipelineState_;     // 騾壼ｸｸ謠冗判逕ｨ

    // 笏笏 蟷ｳ陦悟・貅仙ｮ壽焚繝舌ャ繝輔ぃ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    // GPU 荳翫↓遒ｺ菫昴＆繧後◆繝舌ャ繝輔ぃ縺ｫ lightData_ 繝昴う繝ｳ繧ｿ邨檎罰縺ｧ CPU 縺九ｉ譖ｸ縺崎ｾｼ繧縲・
    // 繝槭ャ繝玲ｸ医∩縺ｪ縺ｮ縺ｧ豈弱ヵ繝ｬ繝ｼ繝譖ｸ縺崎ｾｼ繧薙□蜀・ｮｹ縺後◎縺ｮ縺ｾ縺ｾ GPU 縺ｫ蜿肴丐縺輔ｌ繧九・
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr; // GPU 繝舌ャ繝輔ぃ縺ｸ縺ｮ CPU 蛛ｴ繝昴う繝ｳ繧ｿ

    // 笏笏 迚ｹ谿・PSO 鄒､ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;          // 蠖ｱ逕ｨ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> playerHighlightPipelineState_; // 繧ｷ繝ｫ繧ｨ繝・ヨ逕ｨ

    // 笏笏 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ逕ｨ繝ｪ繧ｽ繝ｼ繧ｹ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    Microsoft::WRL::ComPtr<ID3D12RootSignature> instancedRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedShadowPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedAlphaPipelineState_; // 蜊企乗・逕ｨ
};



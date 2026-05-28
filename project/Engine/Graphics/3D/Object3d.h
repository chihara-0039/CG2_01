#pragma once
#include "Object3dCommon.h"
#include "Model.h"
#include "MyMath.h"


// ==============================================================
//  TransformationMatrix 讒矩菴・
//
//  GPU (鬆らせ繧ｷ繧ｧ繝ｼ繝繝ｼ) 縺ｫ貂｡縺呵｡悟・縺ｮ螳壽焚繝舌ャ繝輔ぃ縲・
//  HLSL 蛛ｴ縺ｧ縺ｯ "cbuffer TransformationBuffer : register(b0)" 縺ｫ蟇ｾ蠢懊・
//
//  縲仙推陦悟・縺ｮ諢丞袖縲・
//  WVP   : World ﾃ・View ﾃ・Projection 繧呈寺縺大粋繧上○縺溷粋謌占｡悟・縲・
//          鬆らせ繧偵せ繧ｯ繝ｪ繝ｼ繝ｳ蠎ｧ讓吶↓螟画鋤縺吶ｋ縺溘ａ縺ｫ菴ｿ縺・・
//          譛邨ら噪縺ｫ "float4 pos = mul(float4(v.pos,1), WVP);" 縺ｧ菴ｿ逕ｨ縺吶ｋ縲・
//
//  World : 繝ｯ繝ｼ繝ｫ繝臥ｩｺ髢薙∈縺ｮ螟画鋤陦悟・ (遘ｻ蜍輔・蝗櫁ｻ｢繝ｻ諡｡邵ｮ)縲・
//          繝ｩ繧､繝・ぅ繝ｳ繧ｰ險育ｮ励・繝ｯ繝ｼ繝ｫ繝臥ｩｺ髢薙〒陦後≧縺溘ａ蛻･騾疲ｸ｡縺吶・
//
//  lightViewProjection : 繝ｩ繧､繝医き繝｡繝ｩ隕也せ縺ｮ VP 陦悟・縲・
//          繧ｷ繝｣繝峨え繝槭ャ繝励・繧ｵ繝ｳ繝励Μ繝ｳ繧ｰ菴咲ｽｮ繧呈ｱゅａ繧九◆繧√↓菴ｿ縺・・
//          繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繝ｼ蜀・〒縲瑚・蛻・′蠖ｱ縺ｮ荳ｭ縺ｫ縺・ｋ縺九阪ｒ蛻､螳壹☆繧九・
// ==============================================================
struct TransformationMatrix {
    Matrix4x4 WVP;                 // 繝ｯ繝ｼ繝ｫ繝嘉励ン繝･繝ｼﾃ励・繝ｭ繧ｸ繧ｧ繧ｯ繧ｷ繝ｧ繝ｳ蜷域・陦悟・
    Matrix4x4 World;               // 繝ｯ繝ｼ繝ｫ繝牙､画鋤陦悟・ (繝ｩ繧､繝・ぅ繝ｳ繧ｰ險育ｮ礼畑)
    Matrix4x4 lightViewProjection; // 繝ｩ繧､繝医き繝｡繝ｩ縺ｮ VP 陦悟・ (繧ｷ繝｣繝峨え繝槭ャ繝怜盾辣ｧ逕ｨ)
};

// ==============================================================
//  Material 讒矩菴・
//
//  GPU (繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繝ｼ) 縺ｫ貂｡縺吶・繝・Μ繧｢繝ｫ諠・ｱ縺ｮ螳壽焚繝舌ャ繝輔ぃ縲・
//  HLSL 蛛ｴ縺ｧ縺ｯ "cbuffer MaterialBuffer : register(b2)" 縺ｫ蟇ｾ蠢懊・
//
//  縲仙推繝輔ぅ繝ｼ繝ｫ繝峨・諢丞袖縲・
//  color         : RGBA縲ゅい繝ｫ繝輔ぃ < 1.0 縺ｧ蜊企乗・縲・
//  enableLighting: 0 = 繝ｩ繧､繝・ぅ繝ｳ繧ｰ縺ｪ縺・(繧ｹ繧ｫ繧､繝峨・繝繝ｻ繧ｰ繝ｪ繝・ラ邱壹↑縺ｩ閾ｪ逋ｺ蜈臥黄菴・
//                  1 = 繝ｩ繧､繝・ぅ繝ｳ繧ｰ縺ゅｊ (騾壼ｸｸ縺ｮ 3D 繧ｪ繝悶ず繧ｧ繧ｯ繝・
//  shininess     : 髀｡髱｢蜿榊ｰ・・驪ｭ縺輔・.0 = 繝槭ャ繝医・.0 = 繝斐き繝斐き縲・
//  metallic      : 驥大ｱ樊─縲・.0 = 繝励Λ繧ｹ繝√ャ繧ｯ縲・.0 = 驩・・驥大ｱ槭・
//  emissive      : 閾ｪ蟾ｱ逋ｺ蜈蛾㍼縲・.0 = 逋ｺ蜈峨↑縺励・.0 莉･荳・= 證鈴裸縺ｧ繧ょ・繧九・
//  uvTransform   : UV 繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ繝ｻ繧ｿ繧､繝ｪ繝ｳ繧ｰ逕ｨ縺ｮ陦悟・縲・
//                  繝・ヵ繧ｩ繝ｫ繝医・蜊倅ｽ崎｡悟・ (1蛟阪・繧ｪ繝輔そ繝・ヨ縺ｪ縺・縲・
// ==============================================================
struct Material {
    Vector4   color;           // 濶ｲ RGBA (繧｢繝ｫ繝輔ぃ縺ｧ騾乗・蠎ｦ蛻ｶ蠕｡)
    int32_t   enableLighting;  // 繝ｩ繧､繝・ぅ繝ｳ繧ｰ譛牙柑繝輔Λ繧ｰ (0 or 1)
    float     shininess;       // 髀｡髱｢蜿榊ｰ・・蠑ｷ縺輔・驪ｭ縺・(0.0 縲・1.0)
    float     metallic;        // 驥大ｱ樊─ (0.0:髱樣≡螻・縲・1.0:驥大ｱ・
    float     emissive;        // 閾ｪ蟾ｱ逋ｺ蜈蛾㍼ (0.0:逋ｺ蜈峨↑縺・縲・1.0莉･荳・逋ｺ蜈・
    Matrix4x4 uvTransform;     // UV 螟画鋤陦悟・ (繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ繝ｻ繧ｿ繧､繝ｪ繝ｳ繧ｰ縺ｫ菴ｿ逕ｨ)
};

// ==============================================================
//  Object3d
//
//  繧ｷ繝ｼ繝ｳ縺ｫ驟咲ｽｮ縺吶ｋ 3D 繧ｪ繝悶ず繧ｧ繧ｯ繝・1 蛟九ｒ陦ｨ縺吶け繝ｩ繧ｹ縲・
//
//  笏笏笏 險ｭ險域ｦりｦ・笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
//  Object3d 縺ｯ縲瑚ｦ九◆逶ｮ縺ｮ險ｭ螳壼､縲阪ｒ謖√▽縺縺代〒縲・
//  螳滄圀縺ｮ繝｡繝・す繝･繝・・繧ｿ (鬆らせ繝ｻ繧､繝ｳ繝・ャ繧ｯ繧ｹ) 縺ｯ Model 縺梧戟縺｣縺ｦ縺・ｋ縲・
//  Object3d 縺ｨ Model 繧貞・縺代ｋ縺薙→縺ｧ縲・
//  縲悟酔縺・Model 繧定､・焚縺ｮ Object3d 縺ｫ蜈ｱ譛峨＆縺帙ｋ縲阪％縺ｨ縺後〒縺阪ｋ縲・
//
//    Model  笏笏笏 鬆らせ繝舌ャ繝輔ぃ / 繝・け繧ｹ繝√Ε / 繝｡繝・す繝･諠・ｱ (驥阪＞繝ｻ1蛟九□縺台ｿ晄戟)
//    Object3d 笏 菴咲ｽｮ繝ｻ蝗櫁ｻ｢繝ｻ繧ｹ繧ｱ繝ｼ繝ｫ / 繝槭ユ繝ｪ繧｢繝ｫ濶ｲ (霆ｽ縺・・縺溘￥縺輔ｓ菴懊ｌ繧・
//
//  笏笏笏 謠冗判縺ｮ豬√ｌ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
//  1. Initialize()  : 螳壽焚繝舌ャ繝輔ぃ 2 譫・(TransformationMatrix / Material) 繧・GPU 縺ｫ遒ｺ菫昴・
//  2. SetModel()    : 謠冗判縺ｫ菴ｿ縺・Model 繧定ｨｭ螳壹・
//  3. SetCamera()   : View / Projection 陦悟・繧貞女縺大叙縺｣縺ｦ菫晏ｭ倥・
//  4. Update()      : World 陦悟・繧定ｨ育ｮ励＠縲ゝransformationMatrix 繝舌ャ繝輔ぃ縺ｫ譖ｸ縺崎ｾｼ繧縲・
//  5. Draw()        : Model->Draw() 縺ｧ繝｡繝・す繝･繧呈緒逕ｻ縲・
//
//  笏笏笏 螳壽焚繝舌ャ繝輔ぃ縺ｮ繧ｹ繝ｭ繝・ヨ蜑ｲ繧雁ｽ薙※ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
//  b0 : TransformationMatrix (WVP / World / lightVP)
//  b1 : DirectionalLight      (Object3dCommon 縺檎ｮ｡逅・
//  b2 : Material              (濶ｲ繝ｻ繝ｩ繧､繝・ぅ繝ｳ繧ｰ險ｭ螳・
//  t0 : 繝・け繧ｹ繝√Ε SRV        (Model 縺梧戟縺､繝・け繧ｹ繝√Ε)
//  t1 : ShadowMap SRV         (蠖ｱ蛻､螳夂畑豺ｱ蠎ｦ繝・け繧ｹ繝√Ε)
// ==============================================================
class Object3d {
public:
    // -------------------------------------------------------
    //  Initialize : 螳壽焚繝舌ャ繝輔ぃ繧・GPU 荳翫↓遒ｺ菫昴＠縲・
    //  TransformationMatrix 縺ｨ Material 縺ｮ繝・ヵ繧ｩ繝ｫ繝亥､繧呈嶌縺崎ｾｼ繧縲・
    //  繝｢繝・Ν縺梧ｱｺ縺ｾ縺｣縺ｦ縺・↑縺上※繧ゅ％縺薙〒蜻ｼ繧薙〒繧医＞縲・
    // -------------------------------------------------------
    void Initialize(Object3dCommon* object3dCommon);

    // -------------------------------------------------------
    //  Update : 豈弱ヵ繝ｬ繝ｼ繝蜻ｼ縺ｶ縲・
    //  transform_ 縺九ｉ World 陦悟・繧定ｨ育ｮ励＠縲・
    //  View / Projection 縺ｨ邨・∩蜷医ｏ縺帙※ WVP 繧剃ｽ懈・縲・
    //  TransformationMatrix 螳壽焚繝舌ャ繝輔ぃ繧・GPU 縺ｫ譖ｸ縺崎ｾｼ繧縲・
    //  lightVP 縺ｯ繧ｷ繝｣繝峨え繝槭ャ繝怜盾辣ｧ縺ｮ縺溘ａ縺ｫ lightViewProjection 縺ｫ蜈･繧後ｋ縲・
    // -------------------------------------------------------
    void Update(const Matrix4x4& lightVP);

    // -------------------------------------------------------
    //  Draw : 繧ｪ繝悶ず繧ｧ繧ｯ繝医ｒ謠冗判縺吶ｋ縲・
    //  Object3dCommon::PreDraw() 縺御ｺ句燕縺ｫ蜻ｼ縺ｰ繧後※縺・ｋ蜑肴署縺ｧ蜍穂ｽ懊☆繧九・
    //  蜀・Κ縺ｧ TransformationMatrix繝ｻMaterial 縺ｮ螳壽焚繝舌ャ繝輔ぃ繧偵ヰ繧､繝ｳ繝峨＠縲・
    //  Model::Draw() 繧貞他繧薙〒繝｡繝・す繝･繧呈緒逕ｻ縺吶ｋ縲・
    // -------------------------------------------------------
    void Draw();

    // 笏笏 繝｢繝・Ν繝ｻ繝医Λ繝ｳ繧ｹ繝輔か繝ｼ繝縺ｮ險ｭ螳・笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏

    /// <summary>
    /// 謠冗判縺ｫ菴ｿ縺・Δ繝・Ν繧偵そ繝・ヨ縲よ園譛画ｨｩ縺ｯ貂｡縺輔↑縺・(Model 縺ｯ蜻ｼ縺ｳ蜃ｺ縺怜・縺檎ｮ｡逅・縲・
    /// </summary>
    void SetModel(Model* model) { model_ = model; }

    /// <summary>繝ｯ繝ｼ繝ｫ繝臥ｩｺ髢薙〒縺ｮ菴咲ｽｮ繧偵そ繝・ヨ (繝｡繝ｼ繝医Ν蜊倅ｽ・</summary>
    void SetPosition(const Vector3& position) { transform_.translate = position; }

    /// <summary>蝗櫁ｻ｢隗偵ｒ繧ｪ繧､繝ｩ繝ｼ隗・(繝ｩ繧ｸ繧｢繝ｳ) 縺ｧ繧ｻ繝・ヨ縲９YZ 縺ｮ鬆・↓驕ｩ逕ｨ縲・/summary>
    void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }

    /// <summary>諡｡螟ｧ邵ｮ蟆上ｒ繧ｻ繝・ヨ縲・.0 = 遲牙・/ 2.0 = 2蛟・/ 0.5 = 蜊雁・縲・/summary>
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    // -------------------------------------------------------
    //  DrawShadow : 繧ｷ繝｣繝峨え繝槭ャ繝励∈縺ｮ譖ｸ縺崎ｾｼ縺ｿ謠冗判縲・
    //  繝ｩ繧､繝医き繝｡繝ｩ隕也せ縺ｮ VP 陦悟・ (lightVP) 繧剃ｽｿ縺｣縺ｦ
    //  豺ｱ蠎ｦ蛟､縺ｮ縺ｿ繧・ShadowMap 繝ｪ繧ｽ繝ｼ繧ｹ縺ｫ譖ｸ縺崎ｾｼ繧縲・
    //  ShadowMap::PreDraw() 蠕後↓蜻ｼ縺ｶ縺薙→縲・
    // -------------------------------------------------------
    void DrawShadow(const Matrix4x4& lightViewProjection);

    // -------------------------------------------------------
    //  SetCamera : 繝薙Η繝ｼ陦悟・縺ｨ繝励Ο繧ｸ繧ｧ繧ｯ繧ｷ繝ｧ繝ｳ陦悟・繧偵そ繝・ヨ縲・
    //  Camera::GetViewMatrix() / GetProjectionMatrix() 縺ｮ謌ｻ繧雁､繧呈ｸ｡縺吶・
    //  Update() 繧医ｊ蜑阪↓豈弱ヵ繝ｬ繝ｼ繝蜻ｼ縺ｶ縺薙→縲・
    // -------------------------------------------------------
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_       = view;
        projectionMatrix_ = projection;
    }

    // 笏笏 繝槭ユ繝ｪ繧｢繝ｫ蛻ｶ蠕｡ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    // 縺薙ｌ繧峨・繧ｻ繝・ち繝ｼ縺ｯ蜀・Κ縺ｮ Material 螳壽焚繝舌ャ繝輔ぃ繧堤峩謗･譖ｸ縺肴鋤縺医ｋ縲・
    // 豈弱ヵ繝ｬ繝ｼ繝蜻ｼ繧薙〒繧・GPU 霆｢騾√・繧ｳ繧ｹ繝医・縺九°繧峨↑縺・(譌｢縺ｫ繝槭ャ繝玲ｸ医∩縺ｮ縺溘ａ)縲・

    /// <summary>陦ｨ遉ｺ濶ｲ繧・RGBA 縺ｧ險ｭ螳壹Ｂlpha < 1.0 縺ｧ蜊企乗・縲・/summary>
    void SetColor(const Vector4& color) { if (materialData_) materialData_->color = color; }

    /// <summary>繝ｩ繧､繝・ぅ繝ｳ繧ｰ繧呈怏蜉ｹ/辟｡蜉ｹ縺ｫ縺吶ｋ縲ゅせ繧ｫ繧､繝峨・繝縺ｪ縺ｩ閾ｪ逋ｺ蜈臥黄菴薙・ false 縺ｫ縲・/summary>
    void SetEnableLighting(bool enable) { if (materialData_) materialData_->enableLighting = (enable ? 1 : 0); }

    /// <summary>髀｡髱｢蜿榊ｰ・・蠑ｷ縺・(0.0:繝槭ャ繝・縲・1.0:繝斐き繝斐き)</summary>
    void SetShininess(float shininess) { if (materialData_) materialData_->shininess = shininess; }

    /// <summary>驥大ｱ樊─ (0.0:繝励Λ繧ｹ繝√ャ繧ｯ 縲・1.0:驥大ｱ・</summary>
    void SetMetallic(float metallic) { if (materialData_) materialData_->metallic = metallic; }

    /// <summary>閾ｪ蟾ｱ逋ｺ蜈蛾㍼縲・.0 雜・〒繝ｩ繧､繝医′縺ｪ縺上※繧り｡ｨ遉ｺ縺輔ｌ繧九・/summary>
    void SetEmissive(float emissive) { if (materialData_) materialData_->emissive = emissive; }

    /// <summary>UV 螟画鋤陦悟・繧偵そ繝・ヨ (繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ繝ｻ繧ｿ繧､繝ｪ繝ｳ繧ｰ逕ｨ)</summary>
    void SetUVTransform(const Transform& uvTransform);

    // 笏笏 繧ｲ繝・ち繝ｼ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏

    /// <summary>險ｭ螳壻ｸｭ縺ｮ Model 繝昴う繝ｳ繧ｿ (螟画峩繝ｻ遒ｺ隱阪↓菴ｿ逕ｨ)</summary>
    Model* GetModel() const { return model_; }

    /// <summary>繝医Λ繝ｳ繧ｹ繝輔か繝ｼ繝讒矩菴・(菴咲ｽｮ繝ｻ蝗櫁ｻ｢繝ｻ繧ｹ繧ｱ繝ｼ繝ｫ繧剃ｸ諡ｬ蜿門ｾ・</summary>
    const Transform& GetTransform() const { return transform_; }

    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

    /// <summary>繝槭ユ繝ｪ繧｢繝ｫ繝・・繧ｿ縺ｮ繧ｳ繝斐・繧貞叙蠕・(繝・ヰ繝・げ逕ｨ)</summary>
    const Material& GetMaterial() const { return *materialData_; }

    /// <summary>繝ｯ繝ｼ繝ｫ繝我ｽ咲ｽｮ繧貞叙蠕・(蠖薙◆繧雁愛螳壹・UI 霑ｽ蠕薙↑縺ｩ縺ｫ菴ｿ逕ｨ)</summary>
    const Vector3& GetPosition() const { return transform_.translate; }

private:
    // 笏笏 蜿ら・ (謇譛峨＠縺ｪ縺・ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    Object3dCommon* object3dCommon_ = nullptr; // PreDraw() 縺ｪ縺ｩ繧貞他縺ｶ縺溘ａ縺ｮ蜿ら・
    Model*          model_          = nullptr;  // 繝｡繝・す繝･繝・・繧ｿ縺ｮ蜿ら・ (謇譛画ｨｩ縺ｪ縺・

    // 笏笏 繝医Λ繝ｳ繧ｹ繝輔か繝ｼ繝 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    // scale / rotate / translate 繧偵・縺ｨ縺､縺ｮ讒矩菴薙↓縺ｾ縺ｨ繧√※縺・ｋ縲・
    // Update() 縺ｧ縺薙ｌ繧峨°繧・World 陦悟・ (繧｢繝輔ぅ繝ｳ螟画鋤陦悟・) 繧堤函謌舌☆繧九・
    Transform transform_ = { {1,1,1}, {0,0,0}, {0,0,0} }; // 繝・ヵ繧ｩ繝ｫ繝・ 遲牙阪・蝗櫁ｻ｢縺ｪ縺励・蜴溽せ

    // 笏笏 繧ｫ繝｡繝ｩ陦悟・ (豈弱ヵ繝ｬ繝ｼ繝 SetCamera() 縺ｧ譖ｴ譁ｰ) 笏笏笏笏笏笏笏笏笏笏
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    // 笏笏 GPU 螳壽焚繝舌ャ繝輔ぃ 笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏
    // GPU 荳翫↓遒ｺ菫昴＆繧後◆繝舌ャ繝輔ぃ縲・ap 縺ｧ CPU 蛛ｴ繝昴う繝ｳ繧ｿ繧貞叙蠕励＠縲・
    // 繝・・繧ｿ繧呈嶌縺崎ｾｼ繧縺縺代〒 GPU 縺ｫ蜊ｳ蜿肴丐縺輔ｌ繧・(繧｢繝・・繝ｭ繝ｼ繝峨ヲ繝ｼ繝励・縺溘ａ)縲・

    /// <summary>WVP / World / lightVP 陦悟・繧呈ｼ邏阪☆繧句ｮ壽焚繝舌ャ繝輔ぃ (register b0)</summary>
    public:
    ID3D12Resource* GetTransformationResource() const { return transformationResource_.Get(); }
    ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
    Object3dCommon* GetObject3dCommon() const { return object3dCommon_; }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
    TransformationMatrix* transformationData_ = nullptr; // CPU 蛛ｴ譖ｸ縺崎ｾｼ縺ｿ繝昴う繝ｳ繧ｿ

    /// <summary>濶ｲ繝ｻ繝ｩ繧､繝・ぅ繝ｳ繧ｰ險ｭ螳壹ｒ譬ｼ邏阪☆繧九・繝・Μ繧｢繝ｫ繝舌ャ繝輔ぃ (register b2)</summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr; // CPU 蛛ｴ譖ｸ縺崎ｾｼ縺ｿ繝昴う繝ｳ繧ｿ
};




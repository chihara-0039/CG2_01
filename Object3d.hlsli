// マテリアル情報（色・ライティング有効フラグ・UV変換など）
struct Material
{
    float4 color; // 色（RGBA）
    bool enableLighting; // ライティングを使うかどうか
    float3 padding; // パディング（bool の後ろを16バイトに揃える）

    // UV変換行列（必要であれば）※使わないなら削除可
    float4x4 uvTransform;
};

// ディレクショナルライトの情報（方向・強度・色）
struct DirectionalLight
{
    float3 direction; // ライトの方向
    float intensity; // 強度
    float3 color; // ライトの色
    float pad; // パディング（16バイト揃え）
};

// 行列データ（ワールド行列・WVP行列）
struct TransformationMatrix
{
    float4x4 WVP; // World-View-Projection
    float4x4 World; // ワールド行列（法線変換などに使用）
};

// 頂点シェーダー出力構造体（ピクセルシェーダー入力と一致）
struct VertexShaderOutput
{
    float4 position : SV_POSITION; // 最終的なスクリーン座標
    float2 texcoord : TEXCOORD0; // テクスチャ座標
    float3 normal : NORMAL0; // 法線ベクトル（照明計算に使用）
};

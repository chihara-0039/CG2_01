struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    nointerpolation float4 lightColor : COLOR1;
    nointerpolation float2 lightDirection : TEXCOORD2;
    nointerpolation float ambientLight : TEXCOORD3;
    nointerpolation float shape : TEXCOORD1;
};

struct VertexShaderInput
{
    // --- 頂点データ (Slot 0) ---
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;

    // --- インスタンスデータ (Slot 1) ---
    // 行列を4つのベクトルとして明示的に受け取る
    float4 wvpRow0 : INSTANCE_WVP0; // Index 0
    float4 wvpRow1 : INSTANCE_WVP1; // Index 1
    float4 wvpRow2 : INSTANCE_WVP2; // Index 2
    float4 wvpRow3 : INSTANCE_WVP3; // Index 3
    
    float4 color : INSTANCE_COLOR;
    float4 lightColor : INSTANCE_LIGHT_COLOR;
    float2 lightDirection : INSTANCE_LIGHT_DIRECTION;
    float ambientLight : INSTANCE_AMBIENT_LIGHT;
    float shape : INSTANCE_SHAPE;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // 4つのベクトルを1つの行列に復元する
    float4x4 WVP;
    WVP[0] = input.wvpRow0;
    WVP[1] = input.wvpRow1;
    WVP[2] = input.wvpRow2;
    WVP[3] = input.wvpRow3;
    
    // 座標変換
    output.position = mul(input.position, WVP);
    output.texcoord = input.texcoord;
    output.color = input.color;
    output.lightColor = input.lightColor;
    output.lightDirection = input.lightDirection;
    output.ambientLight = input.ambientLight;
    output.shape = input.shape;
    
    return output;
}

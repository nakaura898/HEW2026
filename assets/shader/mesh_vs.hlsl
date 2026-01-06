//----------------------------------------------------------------------------
// mesh_vs.hlsl
// 3D Mesh vertex shader
//----------------------------------------------------------------------------

//============================================================================
// 定数バッファ
//============================================================================

// b0: フレーム定数
cbuffer PerFrame : register(b0)
{
    matrix viewProjection;
    float4 cameraPosition;
};

// b1: オブジェクト定数
cbuffer PerObject : register(b1)
{
    matrix world;
    matrix worldInvTranspose;
};

//============================================================================
// 入出力構造体
//============================================================================

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR0;
};

struct VSOutput
{
    float4 position     : SV_POSITION;
    float3 worldPos     : TEXCOORD0;
    float3 worldNormal  : TEXCOORD1;
    float3 worldTangent : TEXCOORD2;
    float3 worldBinorm  : TEXCOORD3;
    float2 texCoord     : TEXCOORD4;
    float4 color        : COLOR0;
};

//============================================================================
// メイン
//============================================================================

VSOutput main(VSInput input)
{
    VSOutput output;

    // ワールド座標
    float4 worldPosition = mul(float4(input.position, 1.0), world);
    output.worldPos = worldPosition.xyz;

    // クリップ座標
    output.position = mul(worldPosition, viewProjection);

    // 法線をワールド空間に変換（逆転置行列を使用）
    output.worldNormal = normalize(mul(float4(input.normal, 0.0), worldInvTranspose).xyz);

    // タンジェントをワールド空間に変換
    output.worldTangent = normalize(mul(float4(input.tangent.xyz, 0.0), world).xyz);

    // バイノーマルを計算（タンジェントのw成分でハンドネス）
    output.worldBinorm = cross(output.worldNormal, output.worldTangent) * input.tangent.w;

    // テクスチャ座標
    output.texCoord = input.texCoord;

    // 頂点カラー
    output.color = input.color;

    return output;
}

//----------------------------------------------------------------------------
// shadow_vs.hlsl
// シャドウパス用頂点シェーダー
//----------------------------------------------------------------------------

//============================================================================
// 定数バッファ
//============================================================================

// b0: ライトビュー・プロジェクション
cbuffer ShadowPass : register(b0)
{
    matrix lightViewProjection;
};

// b1: オブジェクト定数
cbuffer PerObject : register(b1)
{
    matrix world;
    matrix worldInvTranspose;  // 未使用だがレイアウト互換性のため
};

//============================================================================
// 入出力構造体
//============================================================================

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;      // 未使用
    float4 tangent  : TANGENT;     // 未使用
    float2 texCoord : TEXCOORD0;   // 未使用
    float4 color    : COLOR0;      // 未使用
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

//============================================================================
// メイン
//============================================================================

VSOutput main(VSInput input)
{
    VSOutput output;

    // ワールド座標
    float4 worldPosition = mul(float4(input.position, 1.0), world);

    // ライト空間座標
    output.position = mul(worldPosition, lightViewProjection);

    return output;
}

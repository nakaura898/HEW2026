//----------------------------------------------------------------------------
// ground_normalize_ps.hlsl
// Normalize accumulated ground tiles by dividing RGB by alpha
// Blend with base color at edges for seamless transition
//----------------------------------------------------------------------------

Texture2D tex : register(t0);
SamplerState samp : register(s0);

// ベースカラー（ground simple.jpg: #4C8447）
static const float3 BASE_COLOR = float3(0.30, 0.52, 0.28);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float4 accumulated = tex.Sample(samp, input.texCoord);

    // アルファが非常に小さい場合はベースカラーを返す
    if (accumulated.a < 0.001) {
        return float4(BASE_COLOR, 1.0);
    }

    // 正規化: RGB / A で加重平均を復元
    float3 normalizedRGB = accumulated.rgb / accumulated.a;

    // アルファに基づいてベースカラーとブレンド
    float blendFactor = saturate(accumulated.a);
    float3 blendedRGB = lerp(BASE_COLOR, normalizedRGB, blendFactor);

    // 結果を返す（常に不透明）
    return float4(blendedRGB, 1.0);
}

//----------------------------------------------------------------------------
// ground_fix_ps.hlsl
// Post-process shader to fix overly bright pixels in ground
//----------------------------------------------------------------------------

Texture2D tex : register(t0);
SamplerState samp : register(s0);

// ベースカラー（#4C8447）
static const float3 BASE_COLOR = float3(0.30, 0.52, 0.28);
static const float BRIGHTNESS_THRESHOLD = 0.50;  // この明るさを超えたら置換

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float4 texColor = tex.Sample(samp, input.texCoord);

    // 明るさを計算（輝度）
    float brightness = dot(texColor.rgb, float3(0.299, 0.587, 0.114));

    // 明るすぎる場合、ベースカラーで置き換え（違和感をなくす）
    if (brightness > BRIGHTNESS_THRESHOLD) {
        texColor.rgb = BASE_COLOR;
    }

    return texColor;
}

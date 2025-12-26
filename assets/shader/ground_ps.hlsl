//----------------------------------------------------------------------------
// ground_ps.hlsl
// Ground tile pixel shader with edge fade for seamless tiling
//----------------------------------------------------------------------------

Texture2D tex : register(t0);
SamplerState samp : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float4 texColor = tex.Sample(samp, input.texCoord);

    // Edge fade: 端に近いほどアルファを下げる
    // fadeWidth: フェード領域の幅（0.0〜0.5、大きいほど広くぼかす）
    float fadeWidth = 0.15;

    // 各辺からの距離でフェード計算
    float fadeLeft   = smoothstep(0.0, fadeWidth, input.texCoord.x);
    float fadeRight  = smoothstep(0.0, fadeWidth, 1.0 - input.texCoord.x);
    float fadeTop    = smoothstep(0.0, fadeWidth, input.texCoord.y);
    float fadeBottom = smoothstep(0.0, fadeWidth, 1.0 - input.texCoord.y);

    // 全方向のフェードを掛け合わせ
    float edgeFade = fadeLeft * fadeRight * fadeTop * fadeBottom;

    float4 result = texColor * input.color;
    result.a *= edgeFade;

    // プリマルチプライドアルファ: RGBにアルファを事前乗算
    result.rgb *= result.a;

    return result;
}

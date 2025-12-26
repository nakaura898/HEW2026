//----------------------------------------------------------------------------
// ground_accumulate_ps.hlsl
// Ground tile accumulation shader for weighted average calculation
// Output: (RGB * weight, weight) where weight = edge fade
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
    
    // X方向とY方向を別々にフェード
    float fadeWidth = 0.30;

    float distX = min(input.texCoord.x, 1.0 - input.texCoord.x);
    float distY = min(input.texCoord.y, 1.0 - input.texCoord.y);

    float fadeX = smoothstep(0.0, fadeWidth, distX);
    float fadeY = smoothstep(0.0, fadeWidth, distY);
    float weight = fadeX * fadeY;
    
    float3 color = texColor.rgb * input.color.rgb;
    return float4(color * weight, weight);
}
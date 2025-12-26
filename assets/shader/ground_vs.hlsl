//----------------------------------------------------------------------------
// ground_vs.hlsl
// Ground tile vertex shader (same as sprite)
//----------------------------------------------------------------------------

cbuffer CBuffer : register(b0) {
    matrix viewProjection;
};

struct VSInput {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR0;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), viewProjection);
    output.texCoord = input.texCoord;
    output.color = input.color;
    return output;
}
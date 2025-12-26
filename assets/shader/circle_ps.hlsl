//----------------------------------------------------------------------------
// circle_ps.hlsl
// 円描画用ピクセルシェーダー（UV座標で円形にクリップ）
//----------------------------------------------------------------------------

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    // UV座標の中心からの距離を計算
    float2 center = float2(0.5, 0.5);
    float2 diff = input.texCoord - center;
    float distSq = dot(diff, diff);

    // 半径0.5の円の外側は破棄
    if (distSq > 0.25) {  // 0.5 * 0.5 = 0.25
        discard;
    }

    return input.color;
}

//----------------------------------------------------------------------------
// mesh_ps.hlsl
// 3D Mesh PBR pixel shader with shadow mapping
//----------------------------------------------------------------------------

//============================================================================
// 定数
//============================================================================

static const float PI = 3.14159265359;
static const uint MAX_LIGHTS = 8;

// ライトタイプ
static const uint LIGHT_DIRECTIONAL = 0;
static const uint LIGHT_POINT = 1;
static const uint LIGHT_SPOT = 2;

//============================================================================
// 定数バッファ
//============================================================================

// b0: フレーム定数
cbuffer PerFrame : register(b0)
{
    matrix viewProjection;
    float4 cameraPosition;
};

// b2: マテリアル定数
cbuffer Material : register(b2)
{
    float4 albedoColor;         // rgb = albedo, a = alpha
    float metallic;
    float roughness;
    float ao;
    float emissiveStrength;
    float4 emissiveColor;
    uint useAlbedoMap;
    uint useNormalMap;
    uint useMetallicMap;
    uint useRoughnessMap;
};

// b3: ライティング定数
cbuffer Lighting : register(b3)
{
    float4 lightCameraPosition;  // カメラ位置（重複、互換性のため）
    float4 ambientColor;         // 環境光
    uint numLights;
    uint3 lightPad;

    // ライト配列（各64バイト）
    struct
    {
        float4 position;     // xyz = pos, w = type
        float4 direction;    // xyz = dir, w = range
        float4 color;        // rgb = color, a = intensity
        float4 spotParams;   // x = innerCos, y = outerCos, z = falloff
    } lights[MAX_LIGHTS];
};

// b4: シャドウ定数
cbuffer Shadow : register(b4)
{
    matrix lightViewProjection;  // ライト空間変換行列
    float4 shadowParams;         // x = depthBias, y = normalBias, z = shadowStrength, w = enabled
};

//============================================================================
// テクスチャ・サンプラー
//============================================================================

Texture2D albedoTexture    : register(t0);
Texture2D normalTexture    : register(t1);
Texture2D metallicTexture  : register(t2);
Texture2D roughnessTexture : register(t3);
Texture2D aoTexture        : register(t4);
Texture2D shadowMap        : register(t5);

SamplerState linearSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

//============================================================================
// 入力構造体
//============================================================================

struct PSInput
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
// PBR関数
//============================================================================

// フレネル（Schlick近似）
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// 法線分布関数（GGX/Trowbridge-Reitz）
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

// ジオメトリ関数（Schlick-GGX）
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}

// ジオメトリ関数（Smith）
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// ライトの減衰計算
float CalculateAttenuation(float distance, float range)
{
    // 距離による減衰（inverse square falloff）
    float attenuation = 1.0 / (1.0 + distance * distance / (range * range));

    // 範囲外で0になるようにフェードアウト
    float falloff = saturate(1.0 - distance / range);
    return attenuation * falloff * falloff;
}

// スポットライトの減衰
float CalculateSpotFalloff(float3 lightDir, float3 spotDir, float innerCos, float outerCos)
{
    float theta = dot(lightDir, -spotDir);
    float epsilon = innerCos - outerCos;
    return saturate((theta - outerCos) / max(epsilon, 0.0001));
}

//============================================================================
// シャドウ関数
//============================================================================

// シャドウ係数計算（PCF）
float CalculateShadow(float3 worldPos, float3 normal)
{
    // シャドウが無効の場合
    if (shadowParams.w < 0.5)
    {
        return 1.0;
    }

    // ライト空間座標に変換
    float4 lightSpacePos = mul(float4(worldPos, 1.0), lightViewProjection);

    // NDC座標に変換
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // [-1,1] を [0,1] に変換
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = projCoords.y * -0.5 + 0.5;  // Y軸反転

    // シャドウマップ範囲外チェック
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    // バイアス適用
    float depthBias = shadowParams.x;
    float normalBias = shadowParams.y;

    // 法線バイアス（シャドウアクネ対策）
    float3 lightDir = lights[0].direction.xyz;
    float NdotL = max(dot(normal, -lightDir), 0.0);
    float bias = depthBias + normalBias * (1.0 - NdotL);

    float currentDepth = projCoords.z - bias;

    // PCFフィルタリング（3x3）
    float shadow = 0.0;
    float2 texelSize = float2(1.0 / 2048.0, 1.0 / 2048.0);  // シャドウマップ解像度

    [unroll]
    for (int x = -1; x <= 1; x++)
    {
        [unroll]
        for (int y = -1; y <= 1; y++)
        {
            float2 offset = float2(x, y) * texelSize;
            float pcfDepth = shadowMap.Sample(linearSampler, projCoords.xy + offset).r;
            shadow += currentDepth > pcfDepth ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;

    // シャドウ強度を適用
    float shadowStrength = shadowParams.z;
    return lerp(1.0, shadow, shadowStrength);
}

//============================================================================
// メイン
//============================================================================

float4 main(PSInput input) : SV_TARGET
{
    // マテリアルパラメータ取得
    float3 albedo = albedoColor.rgb * input.color.rgb;
    float alpha = albedoColor.a * input.color.a;
    float met = metallic;
    float rough = roughness;
    float aoValue = ao;

    // テクスチャサンプリング
    if (useAlbedoMap)
    {
        float4 texColor = albedoTexture.Sample(linearSampler, input.texCoord);
        albedo *= texColor.rgb;
        alpha *= texColor.a;
    }

    if (useMetallicMap)
    {
        met *= metallicTexture.Sample(linearSampler, input.texCoord).r;
    }

    if (useRoughnessMap)
    {
        rough *= roughnessTexture.Sample(linearSampler, input.texCoord).r;
    }

    // 法線計算
    float3 N = normalize(input.worldNormal);
    if (useNormalMap)
    {
        float3 tangentNormal = normalTexture.Sample(linearSampler, input.texCoord).xyz * 2.0 - 1.0;
        float3x3 TBN = float3x3(
            normalize(input.worldTangent),
            normalize(input.worldBinorm),
            N
        );
        N = normalize(mul(tangentNormal, TBN));
    }

    // ビュー方向
    float3 V = normalize(cameraPosition.xyz - input.worldPos);

    // F0（反射率）
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, met);

    // シャドウ係数計算（ディレクショナルライト用）
    float shadowFactor = CalculateShadow(input.worldPos, N);

    // ライティング計算
    float3 Lo = float3(0.0, 0.0, 0.0);

    for (uint i = 0; i < numLights && i < MAX_LIGHTS; ++i)
    {
        float3 lightPos = lights[i].position.xyz;
        uint lightType = (uint)lights[i].position.w;
        float3 lightDir = lights[i].direction.xyz;
        float lightRange = lights[i].direction.w;
        float3 lightColor = lights[i].color.rgb;
        float lightIntensity = lights[i].color.a;

        float3 L;
        float attenuation = 1.0;
        float shadow = 1.0;

        if (lightType == LIGHT_DIRECTIONAL)
        {
            // 平行光源
            L = -normalize(lightDir);
            // 最初のディレクショナルライトにシャドウを適用
            if (i == 0)
            {
                shadow = shadowFactor;
            }
        }
        else if (lightType == LIGHT_POINT)
        {
            // 点光源
            float3 toLight = lightPos - input.worldPos;
            float distance = length(toLight);
            L = toLight / distance;
            attenuation = CalculateAttenuation(distance, lightRange);
        }
        else // LIGHT_SPOT
        {
            // スポットライト
            float3 toLight = lightPos - input.worldPos;
            float distance = length(toLight);
            L = toLight / distance;
            attenuation = CalculateAttenuation(distance, lightRange);

            float innerCos = lights[i].spotParams.x;
            float outerCos = lights[i].spotParams.y;
            attenuation *= CalculateSpotFalloff(L, lightDir, innerCos, outerCos);
        }

        // ハーフベクトル
        float3 H = normalize(V + L);

        // PBR BRDF
        float NDF = DistributionGGX(N, H, rough);
        float G = GeometrySmith(N, V, L, rough);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        float3 specular = numerator / max(denominator, 0.001);

        // 拡散反射と鏡面反射の比率
        float3 kS = F;
        float3 kD = (1.0 - kS) * (1.0 - met);

        // ライト寄与（シャドウ適用）
        float NdotL = max(dot(N, L), 0.0);
        float3 radiance = lightColor * lightIntensity * attenuation * shadow;
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // 環境光
    float3 ambient = ambientColor.rgb * albedo * aoValue;

    // エミッシブ
    float3 emissive = emissiveColor.rgb * emissiveStrength;

    // 最終カラー
    float3 color = ambient + Lo + emissive;

    // HDRトーンマッピング（簡易Reinhard）
    color = color / (color + 1.0);

    // ガンマ補正
    color = pow(color, 1.0 / 2.2);

    return float4(color, alpha);
}

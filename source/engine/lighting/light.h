//----------------------------------------------------------------------------
//! @file   light.h
//! @brief  ライト構造体定義
//----------------------------------------------------------------------------
#pragma once

#include "engine/math/math_types.h"
#include "engine/math/color.h"
#include <cstdint>

//============================================================================
//! @brief ライトタイプ
//============================================================================
enum class LightType : uint32_t
{
    Directional = 0,  //!< 平行光源（太陽光）
    Point = 1,        //!< 点光源
    Spot = 2          //!< スポットライト
};

//============================================================================
//! @brief ライトデータ（GPU定数バッファ用）
//!
//! @note 64バイト固定（16バイトアラインメント）
//============================================================================
struct alignas(16) LightData
{
    Vector4 position;     //!< xyz = 位置, w = ライトタイプ (16 bytes)
    Vector4 direction;    //!< xyz = 方向, w = 有効範囲 (16 bytes)
    Color color;          //!< rgb = 色, a = 強度 (16 bytes)
    Vector4 spotParams;   //!< x = 内角cos, y = 外角cos, z = 減衰, w = 未使用 (16 bytes)
};  // Total: 64 bytes

static_assert(sizeof(LightData) == 64, "LightData must be 64 bytes");

//============================================================================
//! @brief 最大ライト数
//============================================================================
constexpr uint32_t kMaxLights = 8;

//============================================================================
//! @brief ライティング定数バッファ
//!
//! @note シェーダーのcbuffer Lightingに対応
//============================================================================
struct alignas(16) LightingConstants
{
    Vector4 cameraPosition;                //!< カメラ位置 (16 bytes)
    Color ambientColor;                    //!< 環境光色 (16 bytes)
    uint32_t numLights;                    //!< 有効ライト数 (4 bytes)
    uint32_t pad[3];                       //!< パディング (12 bytes)
    LightData lights[kMaxLights];          //!< ライト配列 (512 bytes)
};  // Total: 560 bytes

static_assert(sizeof(LightingConstants) == 560, "LightingConstants size mismatch");

//============================================================================
//! @brief ライトデータビルダー
//============================================================================
namespace LightBuilder
{

//! @brief 平行光源を作成
//! @param direction 光の方向（正規化済み）
//! @param color 光の色
//! @param intensity 強度
inline LightData Directional(const Vector3& direction, const Color& color, float intensity)
{
    LightData data = {};
    data.position = Vector4(0, 0, 0, static_cast<float>(LightType::Directional));
    data.direction = Vector4(direction.x, direction.y, direction.z, 0.0f);
    data.color = Color(color.R(), color.G(), color.B(), intensity);
    data.spotParams = Vector4(0, 0, 0, 0);
    return data;
}

//! @brief 点光源を作成
//! @param position 光源位置
//! @param color 光の色
//! @param intensity 強度
//! @param range 有効範囲
inline LightData Point(const Vector3& position, const Color& color, float intensity, float range)
{
    LightData data = {};
    data.position = Vector4(position.x, position.y, position.z, static_cast<float>(LightType::Point));
    data.direction = Vector4(0, 0, 0, range);
    data.color = Color(color.R(), color.G(), color.B(), intensity);
    data.spotParams = Vector4(0, 0, 1.0f, 0);  // z = 減衰係数
    return data;
}

//! @brief スポットライトを作成
//! @param position 光源位置
//! @param direction 光の方向（正規化済み）
//! @param color 光の色
//! @param intensity 強度
//! @param range 有効範囲
//! @param innerAngleDegrees 内角（度）
//! @param outerAngleDegrees 外角（度）
inline LightData Spot(const Vector3& position, const Vector3& direction,
                      const Color& color, float intensity, float range,
                      float innerAngleDegrees, float outerAngleDegrees)
{
    LightData data = {};
    data.position = Vector4(position.x, position.y, position.z, static_cast<float>(LightType::Spot));
    data.direction = Vector4(direction.x, direction.y, direction.z, range);
    data.color = Color(color.R(), color.G(), color.B(), intensity);

    // cos値に変換（度→ラジアン→cos）
    float innerCos = std::cos(ToRadians(innerAngleDegrees * 0.5f));
    float outerCos = std::cos(ToRadians(outerAngleDegrees * 0.5f));
    data.spotParams = Vector4(innerCos, outerCos, 1.0f, 0);

    return data;
}

} // namespace LightBuilder

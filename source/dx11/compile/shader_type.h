//----------------------------------------------------------------------------
//! @file   shader_type.h
//! @brief  シェーダータイプ列挙型
//----------------------------------------------------------------------------
#pragma once

#include <cstdint>

//===========================================================================
//! シェーダータイプ
//===========================================================================
enum class ShaderType : uint8_t
{
    Vertex,     //!< 頂点シェーダー
    Pixel,      //!< ピクセルシェーダー
    Geometry,   //!< ジオメトリシェーダー
    Hull,       //!< ハルシェーダー
    Domain,     //!< ドメインシェーダー
    Compute,    //!< コンピュートシェーダー

    Count       //!< シェーダータイプ数
};

//! シェーダープロファイル文字列を取得
//! @param [in] type シェーダータイプ
//! @return プロファイル文字列（例: "vs_5_0"）、無効な場合nullptr
[[nodiscard]] inline constexpr const char* GetShaderProfile(ShaderType type) noexcept
{
    switch (type) {
    case ShaderType::Vertex:   return "vs_5_0";
    case ShaderType::Pixel:    return "ps_5_0";
    case ShaderType::Geometry: return "gs_5_0";
    case ShaderType::Hull:     return "hs_5_0";
    case ShaderType::Domain:   return "ds_5_0";
    case ShaderType::Compute:  return "cs_5_0";
    default:                   return nullptr;
    }
}

//! シェーダータイプ名を取得
//! @param [in] type シェーダータイプ
//! @return タイプ名（例: "Vertex"）
[[nodiscard]] inline constexpr const char* GetShaderTypeName(ShaderType type) noexcept
{
    switch (type) {
    case ShaderType::Vertex:   return "Vertex";
    case ShaderType::Pixel:    return "Pixel";
    case ShaderType::Geometry: return "Geometry";
    case ShaderType::Hull:     return "Hull";
    case ShaderType::Domain:   return "Domain";
    case ShaderType::Compute:  return "Compute";
    default:                   return "Unknown";
    }
}

//! シェーダーエントリーポイント名を取得
//! @param [in] type シェーダータイプ
//! @return エントリーポイント名（例: "VSMain"）、無効な場合nullptr
[[nodiscard]] inline constexpr const char* GetShaderEntryPoint(ShaderType type) noexcept
{
    switch (type) {
    case ShaderType::Vertex:   return "VSMain";
    case ShaderType::Pixel:    return "PSMain";
    case ShaderType::Geometry: return "GSMain";
    case ShaderType::Hull:     return "HSMain";
    case ShaderType::Domain:   return "DSMain";
    case ShaderType::Compute:  return "CSMain";
    default:                   return nullptr;
    }
}

//! グラフィクスパイプラインのシェーダーかどうか
//! @param [in] type シェーダータイプ
//! @return グラフィクスパイプライン用ならtrue、Computeならfalse
[[nodiscard]] inline constexpr bool IsGraphicsShader(ShaderType type) noexcept
{
    return type != ShaderType::Compute && type != ShaderType::Count;
}

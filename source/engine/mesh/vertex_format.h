//----------------------------------------------------------------------------
//! @file   vertex_format.h
//! @brief  3Dメッシュ頂点フォーマット定義
//----------------------------------------------------------------------------
#pragma once

#include "engine/math/math_types.h"
#include "engine/math/color.h"
#include "dx11/gpu_common.h"

//============================================================================
//! @brief 3Dメッシュ頂点フォーマット
//!
//! @details PBRレンダリングに必要な全属性を持つ汎用頂点
//!          Position + Normal + Tangent + UV + Color = 64 bytes
//!
//! @note Tangentのw成分はbitangentの向き（±1）を格納
//============================================================================
struct MeshVertex
{
    Vector3 position;   //!< 頂点座標 (12 bytes)
    Vector3 normal;     //!< 法線ベクトル (12 bytes)
    Vector4 tangent;    //!< 接線ベクトル + bitangent符号 (16 bytes)
    Vector2 texCoord;   //!< テクスチャ座標 (8 bytes)
    Color color;        //!< 頂点カラー (16 bytes)
};  // Total: 64 bytes

static_assert(sizeof(MeshVertex) == 64, "MeshVertex size mismatch");

//============================================================================
//! @brief スキンメッシュ頂点フォーマット（将来用）
//!
//! @details MeshVertex + ボーンインデックス + ボーンウェイト
//============================================================================
struct SkinnedMeshVertex
{
    Vector3 position;       //!< 頂点座標 (12 bytes)
    Vector3 normal;         //!< 法線ベクトル (12 bytes)
    Vector4 tangent;        //!< 接線ベクトル (16 bytes)
    Vector2 texCoord;       //!< テクスチャ座標 (8 bytes)
    Color color;            //!< 頂点カラー (16 bytes)
    uint32_t boneIndices;   //!< 4つのボーンインデックス (packed) (4 bytes)
    Vector4 boneWeights;    //!< 4つのボーンウェイト (16 bytes)
};  // Total: 84 bytes

static_assert(sizeof(SkinnedMeshVertex) == 84, "SkinnedMeshVertex size mismatch");

//============================================================================
//! @brief InputLayout記述子
//============================================================================
namespace MeshInputLayouts
{

//! MeshVertex用InputLayout
inline constexpr D3D11_INPUT_ELEMENT_DESC MeshVertexLayout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};
inline constexpr uint32_t MeshVertexLayoutCount = static_cast<uint32_t>(std::size(MeshVertexLayout));

//! SkinnedMeshVertex用InputLayout
inline constexpr D3D11_INPUT_ELEMENT_DESC SkinnedMeshVertexLayout[] = {
    { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 68, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};
inline constexpr uint32_t SkinnedMeshVertexLayoutCount = static_cast<uint32_t>(std::size(SkinnedMeshVertexLayout));

} // namespace MeshInputLayouts

//============================================================================
//! @brief 頂点ストライド取得
//============================================================================
inline constexpr uint32_t GetMeshVertexStride() noexcept {
    return sizeof(MeshVertex);
}

inline constexpr uint32_t GetSkinnedMeshVertexStride() noexcept {
    return sizeof(SkinnedMeshVertex);
}

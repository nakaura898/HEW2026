//----------------------------------------------------------------------------
//! @file   mesh_loader_gltf.h
//! @brief  glTF/GLBメッシュローダー（tinygltf使用）
//----------------------------------------------------------------------------
#pragma once

#include "mesh_loader.h"

//============================================================================
//! @brief glTF/GLBローダー
//!
//! @details tinygltfを使用してglTF 2.0形式をロード
//!          - .gltf (JSON + 外部バイナリ/テクスチャ)
//!          - .glb (バイナリ形式、全て埋め込み)
//============================================================================
class MeshLoaderGltf final : public IMeshLoader
{
public:
    MeshLoaderGltf() = default;
    ~MeshLoaderGltf() override = default;

    //! @brief サポートする拡張子
    [[nodiscard]] bool SupportsExtension(const std::string& extension) const override;

    //! @brief ファイルからロード
    [[nodiscard]] MeshLoadResult Load(
        const std::string& filePath,
        const MeshLoadOptions& options) override;

    //! @brief メモリからロード
    [[nodiscard]] MeshLoadResult LoadFromMemory(
        const void* data,
        size_t size,
        const std::string& hint,
        const MeshLoadOptions& options) override;

    //! @brief ローダー名
    [[nodiscard]] const char* GetName() const override { return "tinygltf"; }
};


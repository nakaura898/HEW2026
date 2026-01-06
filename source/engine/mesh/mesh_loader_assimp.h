//----------------------------------------------------------------------------
//! @file   mesh_loader_assimp.h
//! @brief  Assimpメッシュローダー（OBJ/FBX/その他対応）
//----------------------------------------------------------------------------
#pragma once

#include "mesh_loader.h"

//============================================================================
//! @brief Assimpローダー
//!
//! @details Open Asset Import Libraryを使用して多様なフォーマットをロード
//!          - .obj (Wavefront OBJ)
//!          - .fbx (Autodesk FBX)
//!          - .dae (Collada)
//!          - .3ds (3D Studio)
//!          - その他Assimpがサポートするフォーマット
//============================================================================
class MeshLoaderAssimp final : public IMeshLoader
{
public:
    MeshLoaderAssimp() = default;
    ~MeshLoaderAssimp() override = default;

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
    [[nodiscard]] const char* GetName() const override { return "Assimp"; }
};


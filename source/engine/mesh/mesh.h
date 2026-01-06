//----------------------------------------------------------------------------
//! @file   mesh.h
//! @brief  3Dメッシュクラス
//----------------------------------------------------------------------------
#pragma once

#include "vertex_format.h"
#include "dx11/gpu/buffer.h"
#include "common/utility/non_copyable.h"
#include <vector>
#include <string>
#include <memory>

//============================================================================
//! @brief バウンディングボックス
//============================================================================
struct BoundingBox
{
    Vector3 min = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    //! @brief 中心座標取得
    [[nodiscard]] Vector3 Center() const noexcept {
        return (min + max) * 0.5f;
    }

    //! @brief 半径（各軸の半分のサイズ）取得
    [[nodiscard]] Vector3 Extents() const noexcept {
        return (max - min) * 0.5f;
    }

    //! @brief 頂点を含むよう拡張
    void Expand(const Vector3& point) noexcept {
        min = Vector3::Min(min, point);
        max = Vector3::Max(max, point);
    }

    //! @brief 有効なバウンディングボックスか
    [[nodiscard]] bool IsValid() const noexcept {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
};

//============================================================================
//! @brief サブメッシュ（マルチマテリアル対応）
//!
//! @details 1つのメッシュ内で異なるマテリアルを使う部分を表す
//============================================================================
struct SubMesh
{
    uint32_t indexOffset = 0;     //!< インデックスバッファ内の開始位置
    uint32_t indexCount = 0;      //!< インデックス数
    uint32_t materialIndex = 0;   //!< マテリアルスロット番号
    std::string name;             //!< サブメッシュ名（デバッグ用）
};

//============================================================================
//! @brief メッシュ記述子
//============================================================================
struct MeshDesc
{
    std::vector<MeshVertex> vertices;   //!< 頂点データ
    std::vector<uint32_t> indices;      //!< インデックスデータ
    std::vector<SubMesh> subMeshes;     //!< サブメッシュ配列
    BoundingBox bounds;                 //!< バウンディングボックス
    std::string name;                   //!< メッシュ名（デバッグ用）
};

//============================================================================
//! @brief 3Dメッシュクラス
//!
//! @details GPUリソース（頂点/インデックスバッファ）を保持
//!          MeshManagerが所有し、MeshHandleで参照
//!
//! @note スレッドセーフ性: 読み取り専用操作はスレッドセーフ
//============================================================================
class Mesh final : private NonCopyable
{
public:
    //! @brief メッシュ生成
    //! @param desc メッシュ記述子
    //! @return 生成されたメッシュ（失敗時nullptr）
    [[nodiscard]] static std::shared_ptr<Mesh> Create(const MeshDesc& desc);

    ~Mesh() = default;

    //----------------------------------------------------------
    //! @name バッファアクセス
    //----------------------------------------------------------
    //!@{

    //! @brief 頂点バッファ取得
    [[nodiscard]] Buffer* GetVertexBuffer() const noexcept { return vertexBuffer_.get(); }

    //! @brief インデックスバッファ取得
    [[nodiscard]] Buffer* GetIndexBuffer() const noexcept { return indexBuffer_.get(); }

    //!@}
    //----------------------------------------------------------
    //! @name メッシュ情報
    //----------------------------------------------------------
    //!@{

    //! @brief 頂点数取得
    [[nodiscard]] uint32_t GetVertexCount() const noexcept { return vertexCount_; }

    //! @brief インデックス数取得
    [[nodiscard]] uint32_t GetIndexCount() const noexcept { return indexCount_; }

    //! @brief サブメッシュ数取得
    [[nodiscard]] uint32_t GetSubMeshCount() const noexcept {
        return static_cast<uint32_t>(subMeshes_.size());
    }

    //! @brief サブメッシュ取得
    //! @param index サブメッシュインデックス
    //! @return サブメッシュ参照
    [[nodiscard]] const SubMesh& GetSubMesh(uint32_t index) const {
        assert(index < subMeshes_.size() && "SubMesh index out of range");
        return subMeshes_[index];
    }

    //! @brief 全サブメッシュ取得
    [[nodiscard]] const std::vector<SubMesh>& GetSubMeshes() const noexcept {
        return subMeshes_;
    }

    //! @brief バウンディングボックス取得
    [[nodiscard]] const BoundingBox& GetBounds() const noexcept { return bounds_; }

    //! @brief メッシュ名取得
    [[nodiscard]] const std::string& GetName() const noexcept { return name_; }

    //! @brief GPUメモリサイズ推定（bytes）
    [[nodiscard]] size_t GpuSize() const noexcept {
        return vertexCount_ * sizeof(MeshVertex) + indexCount_ * sizeof(uint32_t);
    }

    //!@}

private:
    Mesh() = default;

    BufferPtr vertexBuffer_;            //!< 頂点バッファ
    BufferPtr indexBuffer_;             //!< インデックスバッファ
    uint32_t vertexCount_ = 0;          //!< 頂点数
    uint32_t indexCount_ = 0;           //!< インデックス数
    std::vector<SubMesh> subMeshes_;    //!< サブメッシュ配列
    BoundingBox bounds_;                //!< バウンディングボックス
    std::string name_;                  //!< メッシュ名
};

using MeshPtr = std::shared_ptr<Mesh>;

//----------------------------------------------------------------------------
//! @file   mesh.cpp
//! @brief  3Dメッシュクラス実装
//----------------------------------------------------------------------------
#include "mesh.h"
#include "common/logging/logging.h"

std::shared_ptr<Mesh> Mesh::Create(const MeshDesc& desc)
{
    // バリデーション
    if (desc.vertices.empty()) {
        LOG_ERROR("[Mesh::Create] vertices is empty");
        return nullptr;
    }

    if (desc.indices.empty()) {
        LOG_ERROR("[Mesh::Create] indices is empty");
        return nullptr;
    }

    // 頂点バッファ作成
    auto vertexBuffer = Buffer::CreateVertex(
        static_cast<uint32_t>(desc.vertices.size() * sizeof(MeshVertex)),
        sizeof(MeshVertex),
        false,  // dynamic = false（静的メッシュ）
        desc.vertices.data()
    );

    if (!vertexBuffer) {
        LOG_ERROR("[Mesh::Create] Failed to create vertex buffer");
        return nullptr;
    }

    // インデックスバッファ作成
    auto indexBuffer = Buffer::CreateIndex(
        static_cast<uint32_t>(desc.indices.size() * sizeof(uint32_t)),
        false,  // dynamic = false
        desc.indices.data()
    );

    if (!indexBuffer) {
        LOG_ERROR("[Mesh::Create] Failed to create index buffer");
        return nullptr;
    }

    // メッシュ構築
    auto mesh = std::shared_ptr<Mesh>(new Mesh());
    mesh->vertexBuffer_ = std::move(vertexBuffer);
    mesh->indexBuffer_ = std::move(indexBuffer);
    mesh->vertexCount_ = static_cast<uint32_t>(desc.vertices.size());
    mesh->indexCount_ = static_cast<uint32_t>(desc.indices.size());
    mesh->name_ = desc.name;

    // サブメッシュ設定
    if (desc.subMeshes.empty()) {
        // サブメッシュ未指定の場合、全体を1つのサブメッシュとして扱う
        SubMesh defaultSubMesh;
        defaultSubMesh.indexOffset = 0;
        defaultSubMesh.indexCount = mesh->indexCount_;
        defaultSubMesh.materialIndex = 0;
        defaultSubMesh.name = desc.name;
        mesh->subMeshes_.push_back(defaultSubMesh);
    } else {
        mesh->subMeshes_ = desc.subMeshes;
    }

    // バウンディングボックス
    if (desc.bounds.IsValid()) {
        mesh->bounds_ = desc.bounds;
    } else {
        // 頂点から計算
        BoundingBox bounds;
        for (const auto& vertex : desc.vertices) {
            bounds.Expand(vertex.position);
        }
        mesh->bounds_ = bounds;
    }

    LOG_INFO("[Mesh::Create] Created mesh '" + mesh->name_ +
             "' (vertices: " + std::to_string(mesh->vertexCount_) +
             ", indices: " + std::to_string(mesh->indexCount_) +
             ", submeshes: " + std::to_string(mesh->subMeshes_.size()) + ")");

    return mesh;
}

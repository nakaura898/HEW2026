//----------------------------------------------------------------------------
//! @file   mesh_loader.cpp
//! @brief  メッシュローダー共通実装
//----------------------------------------------------------------------------
#include "mesh_loader.h"
#include "common/logging/logging.h"
#include <algorithm>
#include <cctype>

//============================================================================
// MeshLoaderRegistry
//============================================================================

MeshLoaderRegistry& MeshLoaderRegistry::Get()
{
    static MeshLoaderRegistry instance;
    return instance;
}

void MeshLoaderRegistry::Register(std::unique_ptr<IMeshLoader> loader)
{
    if (!loader) {
        LOG_WARN("[MeshLoaderRegistry] Attempted to register null loader");
        return;
    }

    LOG_INFO("[MeshLoaderRegistry] Registered loader: " + std::string(loader->GetName()));
    loaders_.push_back(std::move(loader));
}

IMeshLoader* MeshLoaderRegistry::GetLoaderForExtension(const std::string& extension) const
{
    for (const auto& loader : loaders_) {
        if (loader->SupportsExtension(extension)) {
            return loader.get();
        }
    }
    return nullptr;
}

IMeshLoader* MeshLoaderRegistry::GetLoaderForFile(const std::string& filePath) const
{
    std::string ext = MeshLoaderUtils::GetExtension(filePath);
    return GetLoaderForExtension(ext);
}

MeshLoadResult MeshLoaderRegistry::Load(const std::string& filePath, const MeshLoadOptions& options)
{
    IMeshLoader* loader = GetLoaderForFile(filePath);
    if (!loader) {
        MeshLoadResult result;
        result.success = false;
        result.errorMessage = "No loader found for file: " + filePath;
        LOG_ERROR("[MeshLoaderRegistry] " + result.errorMessage);
        return result;
    }

    LOG_INFO("[MeshLoaderRegistry] Loading '" + filePath + "' with " + loader->GetName());
    return loader->Load(filePath, options);
}

std::vector<std::string> MeshLoaderRegistry::GetSupportedExtensions() const
{
    std::vector<std::string> extensions;
    // 一般的な拡張子リスト
    const char* commonExtensions[] = {
        ".gltf", ".glb", ".obj", ".fbx", ".dae", ".3ds", ".blend"
    };

    for (const char* ext : commonExtensions) {
        for (const auto& loader : loaders_) {
            if (loader->SupportsExtension(ext)) {
                extensions.push_back(ext);
                break;
            }
        }
    }
    return extensions;
}

void MeshLoaderRegistry::Clear()
{
    loaders_.clear();
    LOG_INFO("[MeshLoaderRegistry] Cleared all loaders");
}

//============================================================================
// MeshLoaderUtils
//============================================================================

namespace MeshLoaderUtils
{

std::string GetExtension(const std::string& filePath)
{
    size_t dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos) {
        return "";
    }

    std::string ext = filePath.substr(dotPos);
    // 小文字に変換
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

void CalculateNormals(std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices)
{
    // 全ての法線をゼロにリセット
    for (auto& v : vertices) {
        v.normal = Vector3(0, 0, 0);
    }

    // 各三角形の法線を計算し、頂点に加算
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            continue;
        }

        const Vector3& p0 = vertices[i0].position;
        const Vector3& p1 = vertices[i1].position;
        const Vector3& p2 = vertices[i2].position;

        Vector3 edge1 = p1 - p0;
        Vector3 edge2 = p2 - p0;
        Vector3 normal = edge1.Cross(edge2);

        // 面積による重み付け（外積の長さ = 平行四辺形の面積）
        vertices[i0].normal = vertices[i0].normal + normal;
        vertices[i1].normal = vertices[i1].normal + normal;
        vertices[i2].normal = vertices[i2].normal + normal;
    }

    // 正規化
    for (auto& v : vertices) {
        v.normal.Normalize();
    }
}

void CalculateTangents(std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices)
{
    // タンジェント計算用のバッファ
    std::vector<Vector3> tan1(vertices.size(), Vector3(0, 0, 0));
    std::vector<Vector3> tan2(vertices.size(), Vector3(0, 0, 0));

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            continue;
        }

        const MeshVertex& v0 = vertices[i0];
        const MeshVertex& v1 = vertices[i1];
        const MeshVertex& v2 = vertices[i2];

        Vector3 edge1 = v1.position - v0.position;
        Vector3 edge2 = v2.position - v0.position;

        Vector2 deltaUV1 = v1.texCoord - v0.texCoord;
        Vector2 deltaUV2 = v2.texCoord - v0.texCoord;

        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (std::abs(f) < 1e-8f) {
            continue;  // 縮退三角形をスキップ
        }
        f = 1.0f / f;

        Vector3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        Vector3 bitangent;
        bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

        tan1[i0] = tan1[i0] + tangent;
        tan1[i1] = tan1[i1] + tangent;
        tan1[i2] = tan1[i2] + tangent;

        tan2[i0] = tan2[i0] + bitangent;
        tan2[i1] = tan2[i1] + bitangent;
        tan2[i2] = tan2[i2] + bitangent;
    }

    // グラム・シュミット直交化
    for (size_t i = 0; i < vertices.size(); ++i) {
        const Vector3& n = vertices[i].normal;
        const Vector3& t = tan1[i];

        // Gram-Schmidt orthogonalize
        Vector3 tangent = t - n * n.Dot(t);
        tangent.Normalize();

        // Handedness
        float w = (n.Cross(t).Dot(tan2[i]) < 0.0f) ? -1.0f : 1.0f;

        vertices[i].tangent = Vector4(tangent.x, tangent.y, tangent.z, w);
    }
}

} // namespace MeshLoaderUtils


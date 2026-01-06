//----------------------------------------------------------------------------
//! @file   mesh_loader_gltf.cpp
//! @brief  glTF/GLBメッシュローダー実装
//!
//! @note   tinygltfを使用（ヘッダーオンリーライブラリ）
//!         external/tinygltf/tiny_gltf.h を配置すること
//!         HAS_TINYGLTFマクロが定義されていない場合はスタブ実装
//----------------------------------------------------------------------------

#include "mesh_loader_gltf.h"

// tinygltfがインストールされているかチェック
#if __has_include("tiny_gltf.h")
#define HAS_TINYGLTF 1
#else
#define HAS_TINYGLTF 0
#endif

#if HAS_TINYGLTF

// tinygltf設定（実装を1回だけインクルード）
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// stb_image_writeがsprintfを使用するため
#define _CRT_SECURE_NO_WARNINGS

// 警告を無視（外部ライブラリ）
#pragma warning(push)
#pragma warning(disable: 4018)  // signed/unsigned mismatch
#pragma warning(disable: 4267)  // size_t -> int conversion
#pragma warning(disable: 4244)  // double -> float conversion
#pragma warning(disable: 4100)  // unreferenced formal parameter
#pragma warning(disable: 4996)  // deprecated functions (sprintf)

#include "tiny_gltf.h"

#pragma warning(pop)
#include "engine/fs/file_system_manager.h"
#include "common/logging/logging.h"
#include <algorithm>

namespace
{

//! @brief glTFアクセサからデータを取得
template<typename T>
const T* GetAccessorData(const tinygltf::Model& model, int accessorIndex, size_t& count)
{
    if (accessorIndex < 0) {
        count = 0;
        return nullptr;
    }

    const auto& accessor = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    count = accessor.count;
    return reinterpret_cast<const T*>(
        buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
}

//! @brief プリミティブからメッシュ頂点を構築
bool BuildMeshVertices(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    std::vector<MeshVertex>& vertices,
    std::vector<uint32_t>& indices,
    const MeshLoadOptions& options)
{
    // 位置データは必須
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) {
        LOG_ERROR("[MeshLoaderGltf] Primitive has no POSITION attribute");
        return false;
    }

    size_t vertexCount = 0;
    const float* positions = GetAccessorData<float>(model, posIt->second, vertexCount);
    if (!positions || vertexCount == 0) {
        LOG_ERROR("[MeshLoaderGltf] Failed to get position data");
        return false;
    }

    // 頂点データを構築
    vertices.resize(vertexCount);

    // 位置
    for (size_t i = 0; i < vertexCount; ++i) {
        vertices[i].position = Vector3(
            positions[i * 3 + 0] * options.scale,
            positions[i * 3 + 1] * options.scale,
            positions[i * 3 + 2] * options.scale
        );
        // デフォルト値
        vertices[i].normal = Vector3(0, 1, 0);
        vertices[i].tangent = Vector4(1, 0, 0, 1);
        vertices[i].texCoord = Vector2(0, 0);
        vertices[i].color = Colors::White;
    }

    // 法線
    auto normIt = primitive.attributes.find("NORMAL");
    if (normIt != primitive.attributes.end()) {
        size_t count = 0;
        const float* normals = GetAccessorData<float>(model, normIt->second, count);
        if (normals && count == vertexCount) {
            for (size_t i = 0; i < vertexCount; ++i) {
                vertices[i].normal = Vector3(
                    normals[i * 3 + 0],
                    normals[i * 3 + 1],
                    normals[i * 3 + 2]
                );
            }
        }
    }

    // テクスチャ座標
    auto texIt = primitive.attributes.find("TEXCOORD_0");
    if (texIt != primitive.attributes.end()) {
        size_t count = 0;
        const float* texCoords = GetAccessorData<float>(model, texIt->second, count);
        if (texCoords && count == vertexCount) {
            for (size_t i = 0; i < vertexCount; ++i) {
                float u = texCoords[i * 2 + 0];
                float v = texCoords[i * 2 + 1];
                if (options.flipUVs) {
                    v = 1.0f - v;
                }
                vertices[i].texCoord = Vector2(u, v);
            }
        }
    }

    // タンジェント
    auto tanIt = primitive.attributes.find("TANGENT");
    if (tanIt != primitive.attributes.end()) {
        size_t count = 0;
        const float* tangents = GetAccessorData<float>(model, tanIt->second, count);
        if (tangents && count == vertexCount) {
            for (size_t i = 0; i < vertexCount; ++i) {
                vertices[i].tangent = Vector4(
                    tangents[i * 4 + 0],
                    tangents[i * 4 + 1],
                    tangents[i * 4 + 2],
                    tangents[i * 4 + 3]
                );
            }
        }
    }

    // 頂点カラー
    auto colorIt = primitive.attributes.find("COLOR_0");
    if (colorIt != primitive.attributes.end()) {
        size_t count = 0;
        const auto& accessor = model.accessors[colorIt->second];

        if (accessor.type == TINYGLTF_TYPE_VEC4) {
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                const float* colors = GetAccessorData<float>(model, colorIt->second, count);
                if (colors && count == vertexCount) {
                    for (size_t i = 0; i < vertexCount; ++i) {
                        vertices[i].color = Color(
                            colors[i * 4 + 0],
                            colors[i * 4 + 1],
                            colors[i * 4 + 2],
                            colors[i * 4 + 3]
                        );
                    }
                }
            }
            else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                const uint8_t* colors = GetAccessorData<uint8_t>(model, colorIt->second, count);
                if (colors && count == vertexCount) {
                    for (size_t i = 0; i < vertexCount; ++i) {
                        vertices[i].color = Color(
                            colors[i * 4 + 0] / 255.0f,
                            colors[i * 4 + 1] / 255.0f,
                            colors[i * 4 + 2] / 255.0f,
                            colors[i * 4 + 3] / 255.0f
                        );
                    }
                }
            }
        }
    }

    // インデックス
    if (primitive.indices >= 0) {
        const auto& accessor = model.accessors[primitive.indices];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];

        const uint8_t* indexData = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

        indices.resize(accessor.count);

        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                for (size_t i = 0; i < accessor.count; ++i) {
                    indices[i] = static_cast<uint32_t>(indexData[i]);
                }
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                for (size_t i = 0; i < accessor.count; ++i) {
                    indices[i] = static_cast<uint32_t>(reinterpret_cast<const uint16_t*>(indexData)[i]);
                }
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                for (size_t i = 0; i < accessor.count; ++i) {
                    indices[i] = reinterpret_cast<const uint32_t*>(indexData)[i];
                }
                break;
            default:
                LOG_ERROR("[MeshLoaderGltf] Unsupported index component type");
                return false;
        }

        // ワインディングオーダー反転
        if (options.flipWindingOrder) {
            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                std::swap(indices[i + 1], indices[i + 2]);
            }
        }
    }
    else {
        // インデックスなし - 連番で生成
        indices.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) {
            indices[i] = static_cast<uint32_t>(i);
        }
    }

    // 法線計算（オプション）
    if (options.calculateNormals && normIt == primitive.attributes.end()) {
        MeshLoaderUtils::CalculateNormals(vertices, indices);
    }

    // タンジェント計算（オプション）
    if (options.calculateTangents && tanIt == primitive.attributes.end()) {
        MeshLoaderUtils::CalculateTangents(vertices, indices);
    }

    return true;
}

//! @brief マテリアルを変換
MaterialDesc ConvertMaterial(const tinygltf::Model& model, int materialIndex, [[maybe_unused]] const std::string& basePath)
{
    MaterialDesc desc;

    if (materialIndex < 0 || materialIndex >= static_cast<int>(model.materials.size())) {
        desc.name = "Default";
        return desc;
    }

    const auto& mat = model.materials[materialIndex];
    desc.name = mat.name.empty() ? ("Material_" + std::to_string(materialIndex)) : mat.name;

    // PBR Metallic-Roughness
    const auto& pbr = mat.pbrMetallicRoughness;

    // ベースカラー
    if (pbr.baseColorFactor.size() >= 4) {
        desc.params.albedoColor = Color(
            static_cast<float>(pbr.baseColorFactor[0]),
            static_cast<float>(pbr.baseColorFactor[1]),
            static_cast<float>(pbr.baseColorFactor[2]),
            static_cast<float>(pbr.baseColorFactor[3])
        );
    }

    desc.params.metallic = static_cast<float>(pbr.metallicFactor);
    desc.params.roughness = static_cast<float>(pbr.roughnessFactor);

    // エミッシブ
    if (mat.emissiveFactor.size() >= 3) {
        float r = static_cast<float>(mat.emissiveFactor[0]);
        float g = static_cast<float>(mat.emissiveFactor[1]);
        float b = static_cast<float>(mat.emissiveFactor[2]);
        if (r > 0 || g > 0 || b > 0) {
            desc.params.emissiveColor = Color(r, g, b, 1.0f);
            desc.params.emissiveStrength = 1.0f;
        }
    }

    // テクスチャパスは後でTextureManagerで解決
    // ここではマテリアル記述子のみ返す

    return desc;
}

} // anonymous namespace

//============================================================================
// MeshLoaderGltf 実装
//============================================================================

bool MeshLoaderGltf::SupportsExtension(const std::string& extension) const
{
    return extension == ".gltf" || extension == ".glb";
}

MeshLoadResult MeshLoaderGltf::Load(const std::string& filePath, const MeshLoadOptions& options)
{
    MeshLoadResult result;

    // ファイル読み込み
    auto& fsm = FileSystemManager::Get();
    auto fileResult = fsm.ReadFile(filePath);

    if (!fileResult.success) {
        result.errorMessage = "Failed to read file: " + filePath + " (" + fileResult.errorMessage() + ")";
        LOG_ERROR("[MeshLoaderGltf] " + result.errorMessage);
        return result;
    }

    // メモリからロード
    std::string ext = MeshLoaderUtils::GetExtension(filePath);
    return LoadFromMemory(fileResult.bytes.data(), fileResult.bytes.size(), ext, options);
}

MeshLoadResult MeshLoaderGltf::LoadFromMemory(
    const void* data,
    size_t size,
    const std::string& hint,
    const MeshLoadOptions& options)
{
    MeshLoadResult result;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool success = false;

    // バイナリかASCIIかを拡張子で判定
    std::string ext = hint;
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".glb") {
        success = loader.LoadBinaryFromMemory(
            &model,
            &err,
            &warn,
            reinterpret_cast<const unsigned char*>(data),
            static_cast<unsigned int>(size)
        );
    }
    else {
        success = loader.LoadASCIIFromString(
            &model,
            &err,
            &warn,
            reinterpret_cast<const char*>(data),
            static_cast<unsigned int>(size),
            ""  // base dir
        );
    }

    if (!warn.empty()) {
        LOG_WARN("[MeshLoaderGltf] " + warn);
    }

    if (!success) {
        result.errorMessage = "Failed to parse glTF: " + err;
        LOG_ERROR("[MeshLoaderGltf] " + result.errorMessage);
        return result;
    }

    // マテリアル変換
    if (options.loadMaterials) {
        for (size_t i = 0; i < model.materials.size(); ++i) {
            result.materialDescs.push_back(ConvertMaterial(model, static_cast<int>(i), ""));
        }
    }

    // メッシュ変換
    for (size_t meshIdx = 0; meshIdx < model.meshes.size(); ++meshIdx) {
        const auto& gltfMesh = model.meshes[meshIdx];

        MeshDesc meshDesc;
        meshDesc.name = gltfMesh.name.empty() ?
            ("Mesh_" + std::to_string(meshIdx)) : gltfMesh.name;

        uint32_t baseVertex = 0;
        uint32_t baseIndex = 0;

        for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); ++primIdx) {
            const auto& primitive = gltfMesh.primitives[primIdx];

            // 三角形のみサポート
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                LOG_WARN("[MeshLoaderGltf] Skipping non-triangle primitive");
                continue;
            }

            std::vector<MeshVertex> primVertices;
            std::vector<uint32_t> primIndices;

            if (!BuildMeshVertices(model, primitive, primVertices, primIndices, options)) {
                continue;
            }

            // インデックスオフセットを適用
            for (auto& idx : primIndices) {
                idx += baseVertex;
            }

            // サブメッシュ追加
            SubMesh subMesh;
            subMesh.indexOffset = baseIndex;
            subMesh.indexCount = static_cast<uint32_t>(primIndices.size());
            subMesh.materialIndex = (primitive.material >= 0) ?
                static_cast<uint32_t>(primitive.material) : 0;
            subMesh.name = meshDesc.name + "_Prim" + std::to_string(primIdx);

            meshDesc.subMeshes.push_back(subMesh);

            // 頂点・インデックスを結合
            meshDesc.vertices.insert(meshDesc.vertices.end(),
                                     primVertices.begin(), primVertices.end());
            meshDesc.indices.insert(meshDesc.indices.end(),
                                    primIndices.begin(), primIndices.end());

            baseVertex += static_cast<uint32_t>(primVertices.size());
            baseIndex += static_cast<uint32_t>(primIndices.size());
        }

        if (meshDesc.vertices.empty()) {
            continue;
        }

        // メッシュ作成
        auto mesh = Mesh::Create(meshDesc);
        if (mesh) {
            result.meshes.push_back(mesh);
        }
    }

    result.success = !result.meshes.empty();

    if (result.success) {
        LOG_INFO("[MeshLoaderGltf] Loaded " + std::to_string(result.meshes.size()) +
                 " meshes, " + std::to_string(result.materialDescs.size()) + " materials");
    }

    return result;
}

#else // !HAS_TINYGLTF

//============================================================================
// スタブ実装（tinygltf未インストール時）
//============================================================================

#include "common/logging/logging.h"

bool MeshLoaderGltf::SupportsExtension(const std::string& extension) const
{
    (void)extension;
    return false;  // tinygltfがないので対応しない
}

MeshLoadResult MeshLoaderGltf::Load(const std::string& filePath, const MeshLoadOptions& options)
{
    (void)filePath;
    (void)options;
    MeshLoadResult result;
    result.errorMessage = "glTF loader not available (tinygltf not installed)";
    LOG_WARN("[MeshLoaderGltf] " + result.errorMessage);
    return result;
}

MeshLoadResult MeshLoaderGltf::LoadFromMemory(
    const void* data,
    size_t size,
    const std::string& hint,
    const MeshLoadOptions& options)
{
    (void)data;
    (void)size;
    (void)hint;
    (void)options;
    MeshLoadResult result;
    result.errorMessage = "glTF loader not available (tinygltf not installed)";
    LOG_WARN("[MeshLoaderGltf] " + result.errorMessage);
    return result;
}

#endif // HAS_TINYGLTF

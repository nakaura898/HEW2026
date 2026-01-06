//----------------------------------------------------------------------------
//! @file   mesh_loader_assimp.cpp
//! @brief  Assimpメッシュローダー実装
//!
//! @note   AssimpをNuGetパッケージで追加すること
//!         例: Assimp.Native または assimp
//!         HAS_ASSIMPマクロが定義されていない場合はスタブ実装
//----------------------------------------------------------------------------

#include "mesh_loader_assimp.h"

// Assimpがインストールされているかチェック
#if __has_include(<assimp/Importer.hpp>)
#define HAS_ASSIMP 1
#else
#define HAS_ASSIMP 0
#endif

#if HAS_ASSIMP

// 警告を無視（外部ライブラリ）
#pragma warning(push)
#pragma warning(disable: 4244)  // conversion warnings
#pragma warning(disable: 4267)  // size_t conversion

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

#pragma warning(pop)
#include "engine/fs/file_system_manager.h"
#include "common/logging/logging.h"
#include <algorithm>
#include <unordered_set>

namespace
{

//! @brief サポートする拡張子
const std::unordered_set<std::string> kSupportedExtensions = {
    ".obj",
    ".fbx",
    ".dae",
    ".3ds",
    ".blend",
    ".ply",
    ".stl",
    ".x",
    ".x3d"
};

//! @brief AssimpのaiVector3DをVector3に変換
inline Vector3 ToVector3(const aiVector3D& v)
{
    return Vector3(v.x, v.y, v.z);
}

//! @brief Assimpのカラーをカラーに変換
inline Color ToColor(const aiColor4D& c)
{
    return Color(c.r, c.g, c.b, c.a);
}

//! @brief Assimpポストプロセスフラグを構築
unsigned int GetPostProcessFlags(const MeshLoadOptions& options)
{
    unsigned int flags = aiProcess_Triangulate          // 三角形化
                       | aiProcess_JoinIdenticalVertices // 重複頂点を結合
                       | aiProcess_SortByPType          // プリミティブタイプでソート
                       | aiProcess_ValidateDataStructure;

    if (options.calculateNormals) {
        flags |= aiProcess_GenSmoothNormals;
    }
    else {
        flags |= aiProcess_GenNormals;  // 法線がない場合のみ生成
    }

    if (options.calculateTangents) {
        flags |= aiProcess_CalcTangentSpace;
    }

    if (options.flipUVs) {
        flags |= aiProcess_FlipUVs;
    }

    if (options.flipWindingOrder) {
        flags |= aiProcess_FlipWindingOrder;
    }

    return flags;
}

//! @brief aiMeshからMeshDescを構築
bool BuildMeshFromAiMesh(
    const aiMesh* aiMesh,
    MeshDesc& desc,
    uint32_t& baseVertex,
    uint32_t& baseIndex,
    const MeshLoadOptions& options)
{
    if (!aiMesh->HasPositions()) {
        LOG_WARN("[MeshLoaderAssimp] Mesh has no positions, skipping");
        return false;
    }

    const uint32_t vertexCount = aiMesh->mNumVertices;
    const uint32_t startVertex = static_cast<uint32_t>(desc.vertices.size());
    const uint32_t startIndex = static_cast<uint32_t>(desc.indices.size());

    // 頂点データを追加
    desc.vertices.reserve(desc.vertices.size() + vertexCount);

    for (uint32_t i = 0; i < vertexCount; ++i) {
        MeshVertex vertex;

        // 位置
        vertex.position = ToVector3(aiMesh->mVertices[i]) * options.scale;

        // 法線
        if (aiMesh->HasNormals()) {
            vertex.normal = ToVector3(aiMesh->mNormals[i]);
        }
        else {
            vertex.normal = Vector3(0, 1, 0);
        }

        // タンジェント
        if (aiMesh->HasTangentsAndBitangents()) {
            const aiVector3D& t = aiMesh->mTangents[i];
            const aiVector3D& b = aiMesh->mBitangents[i];
            const aiVector3D& n = aiMesh->mNormals[i];

            // Handedness計算
            aiVector3D cross;
            cross.x = n.y * t.z - n.z * t.y;
            cross.y = n.z * t.x - n.x * t.z;
            cross.z = n.x * t.y - n.y * t.x;

            float dot = cross.x * b.x + cross.y * b.y + cross.z * b.z;
            float w = (dot < 0.0f) ? -1.0f : 1.0f;

            vertex.tangent = Vector4(t.x, t.y, t.z, w);
        }
        else {
            vertex.tangent = Vector4(1, 0, 0, 1);
        }

        // テクスチャ座標（最初のUVチャンネルを使用）
        if (aiMesh->HasTextureCoords(0)) {
            vertex.texCoord = Vector2(
                aiMesh->mTextureCoords[0][i].x,
                aiMesh->mTextureCoords[0][i].y
            );
        }
        else {
            vertex.texCoord = Vector2(0, 0);
        }

        // 頂点カラー（最初のカラーチャンネルを使用）
        if (aiMesh->HasVertexColors(0)) {
            vertex.color = ToColor(aiMesh->mColors[0][i]);
        }
        else {
            vertex.color = Colors::White;
        }

        desc.vertices.push_back(vertex);
    }

    // インデックスデータを追加
    uint32_t indexCount = 0;
    for (uint32_t f = 0; f < aiMesh->mNumFaces; ++f) {
        indexCount += aiMesh->mFaces[f].mNumIndices;
    }

    desc.indices.reserve(desc.indices.size() + indexCount);

    for (uint32_t f = 0; f < aiMesh->mNumFaces; ++f) {
        const aiFace& face = aiMesh->mFaces[f];
        for (uint32_t j = 0; j < face.mNumIndices; ++j) {
            desc.indices.push_back(startVertex + face.mIndices[j]);
        }
    }

    // サブメッシュ追加
    SubMesh subMesh;
    subMesh.indexOffset = startIndex;
    subMesh.indexCount = static_cast<uint32_t>(desc.indices.size()) - startIndex;
    subMesh.materialIndex = aiMesh->mMaterialIndex;
    subMesh.name = aiMesh->mName.C_Str();

    desc.subMeshes.push_back(subMesh);

    baseVertex += vertexCount;
    baseIndex += subMesh.indexCount;

    return true;
}

//! @brief aiMaterialからMaterialDescを構築
MaterialDesc ConvertMaterial(const aiMaterial* aiMat, [[maybe_unused]] const std::string& basePath)
{
    MaterialDesc desc;

    // 名前
    aiString name;
    if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
        desc.name = name.C_Str();
    }

    // ベースカラー/ディフューズ
    aiColor4D diffuse;
    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
        desc.params.albedoColor = ToColor(diffuse);
    }

    // メタリック（PBR）
    float metallic = 0.0f;
    if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
        desc.params.metallic = metallic;
    }

    // ラフネス（PBR）
    float roughness = 0.5f;
    if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
        desc.params.roughness = roughness;
    }
    else {
        // Shininess からラフネスを推定
        float shininess = 0.0f;
        if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
            // 高いshininessは低いroughness
            desc.params.roughness = 1.0f - std::min(shininess / 128.0f, 1.0f);
        }
    }

    // エミッシブ
    aiColor4D emissive;
    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
        if (emissive.r > 0 || emissive.g > 0 || emissive.b > 0) {
            desc.params.emissiveColor = ToColor(emissive);
            desc.params.emissiveStrength = 1.0f;
        }
    }

    // テクスチャパスは後でTextureManagerで解決
    // ここではマテリアル記述子のみ返す

    return desc;
}

//! @brief ノードを再帰的に処理してメッシュを収集
void ProcessNode(
    const aiNode* node,
    const aiScene* scene,
    MeshDesc& desc,
    uint32_t& baseVertex,
    uint32_t& baseIndex,
    const MeshLoadOptions& options)
{
    // このノードのメッシュを処理
    for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        BuildMeshFromAiMesh(mesh, desc, baseVertex, baseIndex, options);
    }

    // 子ノードを再帰的に処理
    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], scene, desc, baseVertex, baseIndex, options);
    }
}

} // anonymous namespace

//============================================================================
// MeshLoaderAssimp 実装
//============================================================================

bool MeshLoaderAssimp::SupportsExtension(const std::string& extension) const
{
    return kSupportedExtensions.find(extension) != kSupportedExtensions.end();
}

MeshLoadResult MeshLoaderAssimp::Load(const std::string& filePath, const MeshLoadOptions& options)
{
    MeshLoadResult result;

    // ファイル読み込み
    auto& fsm = FileSystemManager::Get();
    auto fileResult = fsm.ReadFile(filePath);

    if (!fileResult.success) {
        result.errorMessage = "Failed to read file: " + filePath + " (" + fileResult.errorMessage() + ")";
        LOG_ERROR("[MeshLoaderAssimp] " + result.errorMessage);
        return result;
    }

    // 拡張子を取得
    std::string ext = MeshLoaderUtils::GetExtension(filePath);

    return LoadFromMemory(fileResult.bytes.data(), fileResult.bytes.size(), ext, options);
}

MeshLoadResult MeshLoaderAssimp::LoadFromMemory(
    const void* data,
    size_t size,
    const std::string& hint,
    const MeshLoadOptions& options)
{
    MeshLoadResult result;

    Assimp::Importer importer;

    // ポストプロセスフラグを設定
    unsigned int flags = GetPostProcessFlags(options);

    // 拡張子ヒントを使用してフォーマットを特定
    const aiScene* scene = importer.ReadFileFromMemory(
        data,
        size,
        flags,
        hint.c_str()
    );

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        result.errorMessage = "Assimp error: " + std::string(importer.GetErrorString());
        LOG_ERROR("[MeshLoaderAssimp] " + result.errorMessage);
        return result;
    }

    // マテリアル変換
    if (options.loadMaterials) {
        for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
            result.materialDescs.push_back(ConvertMaterial(scene->mMaterials[i], ""));
        }
    }

    // シーン全体を1つのMeshDescにまとめる
    MeshDesc meshDesc;
    meshDesc.name = "AssimpMesh";

    uint32_t baseVertex = 0;
    uint32_t baseIndex = 0;

    ProcessNode(scene->mRootNode, scene, meshDesc, baseVertex, baseIndex, options);

    if (meshDesc.vertices.empty()) {
        result.errorMessage = "No valid mesh data found";
        LOG_ERROR("[MeshLoaderAssimp] " + result.errorMessage);
        return result;
    }

    // メッシュ作成
    auto mesh = Mesh::Create(meshDesc);
    if (mesh) {
        result.meshes.push_back(mesh);
        result.success = true;

        LOG_INFO("[MeshLoaderAssimp] Loaded mesh with " +
                 std::to_string(meshDesc.vertices.size()) + " vertices, " +
                 std::to_string(meshDesc.indices.size()) + " indices, " +
                 std::to_string(meshDesc.subMeshes.size()) + " submeshes");
    }
    else {
        result.errorMessage = "Failed to create mesh";
        LOG_ERROR("[MeshLoaderAssimp] " + result.errorMessage);
    }

    return result;
}

#else // !HAS_ASSIMP

//============================================================================
// スタブ実装（Assimp未インストール時）
//============================================================================

#include "common/logging/logging.h"

bool MeshLoaderAssimp::SupportsExtension(const std::string& extension) const
{
    (void)extension;
    return false;  // Assimpがないので対応しない
}

MeshLoadResult MeshLoaderAssimp::Load(const std::string& filePath, const MeshLoadOptions& options)
{
    (void)filePath;
    (void)options;
    MeshLoadResult result;
    result.errorMessage = "Assimp loader not available (Assimp not installed)";
    LOG_WARN("[MeshLoaderAssimp] " + result.errorMessage);
    return result;
}

MeshLoadResult MeshLoaderAssimp::LoadFromMemory(
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
    result.errorMessage = "Assimp loader not available (Assimp not installed)";
    LOG_WARN("[MeshLoaderAssimp] " + result.errorMessage);
    return result;
}

#endif // HAS_ASSIMP

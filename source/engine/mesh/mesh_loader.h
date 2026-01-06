//----------------------------------------------------------------------------
//! @file   mesh_loader.h
//! @brief  メッシュローダーインターフェース
//----------------------------------------------------------------------------
#pragma once

#include "mesh.h"
#include "engine/material/material.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>

//============================================================================
//! @brief メッシュロード結果
//!
//! @details 1つのファイルから複数のメッシュとマテリアルを読み込む場合に使用
//============================================================================
struct MeshLoadResult
{
    std::vector<MeshPtr> meshes;                    //!< 読み込んだメッシュ
    std::vector<MaterialDesc> materialDescs;       //!< マテリアル記述子
    std::vector<std::string> texturePathsToLoad;   //!< 読み込むべきテクスチャパス
    bool success = false;                          //!< 成功フラグ
    std::string errorMessage;                      //!< エラーメッセージ

    //! @brief 成功チェック
    [[nodiscard]] bool IsValid() const noexcept { return success && !meshes.empty(); }
};

//============================================================================
//! @brief ローダーオプション
//============================================================================
struct MeshLoadOptions
{
    bool calculateNormals = false;      //!< 法線がない場合に計算
    bool calculateTangents = true;      //!< タンジェントを計算
    bool flipUVs = false;               //!< UV座標のY軸を反転
    bool flipWindingOrder = false;      //!< ワインディングオーダーを反転
    float scale = 1.0f;                 //!< スケール係数
    bool loadMaterials = true;          //!< マテリアル情報を読み込む
    bool loadTextures = false;          //!< 埋め込みテクスチャを読み込む（glTF用）
};

//============================================================================
//! @brief メッシュローダーインターフェース
//!
//! @details 各フォーマット用のローダーが実装する
//============================================================================
class IMeshLoader
{
public:
    virtual ~IMeshLoader() = default;

    //! @brief サポートする拡張子かチェック
    //! @param extension 拡張子（".gltf", ".obj"など、ドット付き小文字）
    //! @return サポートしている場合true
    [[nodiscard]] virtual bool SupportsExtension(const std::string& extension) const = 0;

    //! @brief ファイルからメッシュを読み込む
    //! @param filePath ファイルパス（assets:/形式またはフルパス）
    //! @param options ロードオプション
    //! @return ロード結果
    [[nodiscard]] virtual MeshLoadResult Load(
        const std::string& filePath,
        const MeshLoadOptions& options = {}) = 0;

    //! @brief メモリからメッシュを読み込む
    //! @param data バイナリデータ
    //! @param size データサイズ
    //! @param hint ファイル名ヒント（拡張子判定用）
    //! @param options ロードオプション
    //! @return ロード結果
    [[nodiscard]] virtual MeshLoadResult LoadFromMemory(
        const void* data,
        size_t size,
        const std::string& hint,
        const MeshLoadOptions& options = {}) = 0;

    //! @brief ローダー名を取得
    [[nodiscard]] virtual const char* GetName() const = 0;
};

//============================================================================
//! @brief メッシュローダーレジストリ
//!
//! @details 複数のローダーを管理し、拡張子に応じて適切なローダーを選択
//============================================================================
class MeshLoaderRegistry
{
public:
    //! @brief シングルトン取得
    [[nodiscard]] static MeshLoaderRegistry& Get();

    //! @brief ローダーを登録
    //! @param loader ローダーインスタンス
    void Register(std::unique_ptr<IMeshLoader> loader);

    //! @brief 拡張子に対応するローダーを取得
    //! @param extension 拡張子（".gltf"など）
    //! @return ローダー（見つからない場合nullptr）
    [[nodiscard]] IMeshLoader* GetLoaderForExtension(const std::string& extension) const;

    //! @brief ファイルパスからローダーを取得
    //! @param filePath ファイルパス
    //! @return ローダー（見つからない場合nullptr）
    [[nodiscard]] IMeshLoader* GetLoaderForFile(const std::string& filePath) const;

    //! @brief ファイルを読み込む（適切なローダーを自動選択）
    //! @param filePath ファイルパス
    //! @param options ロードオプション
    //! @return ロード結果
    [[nodiscard]] MeshLoadResult Load(
        const std::string& filePath,
        const MeshLoadOptions& options = {});

    //! @brief サポートする拡張子一覧を取得
    [[nodiscard]] std::vector<std::string> GetSupportedExtensions() const;

    //! @brief 全ローダーをクリア
    void Clear();

private:
    MeshLoaderRegistry() = default;
    ~MeshLoaderRegistry() = default;
    MeshLoaderRegistry(const MeshLoaderRegistry&) = delete;
    MeshLoaderRegistry& operator=(const MeshLoaderRegistry&) = delete;

    std::vector<std::unique_ptr<IMeshLoader>> loaders_;
};

//============================================================================
//! @brief ユーティリティ関数
//============================================================================
namespace MeshLoaderUtils
{
    //! @brief ファイルパスから拡張子を取得（小文字、ドット付き）
    //! @param filePath ファイルパス
    //! @return 拡張子（例: ".gltf"）
    [[nodiscard]] std::string GetExtension(const std::string& filePath);

    //! @brief 法線を計算
    //! @param vertices 頂点配列
    //! @param indices インデックス配列
    void CalculateNormals(std::vector<MeshVertex>& vertices,
                          const std::vector<uint32_t>& indices);

    //! @brief タンジェントを計算（MikkTSpace簡易版）
    //! @param vertices 頂点配列
    //! @param indices インデックス配列
    void CalculateTangents(std::vector<MeshVertex>& vertices,
                           const std::vector<uint32_t>& indices);

} // namespace MeshLoaderUtils


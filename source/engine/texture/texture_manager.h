//----------------------------------------------------------------------------
//! @file   texture_manager.h
//! @brief  テクスチャマネージャー
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/gpu/gpu.h"
#include <memory>
#include <string>

class IReadableFileSystem;
class ITextureLoader;
class ITextureCache;

//===========================================================================
//! キャッシュ統計情報
//===========================================================================
struct TextureCacheStats
{
    size_t textureCount = 0;          //!< テクスチャキャッシュ数
    size_t hitCount = 0;              //!< キャッシュヒット回数
    size_t missCount = 0;             //!< キャッシュミス回数
    size_t totalMemoryBytes = 0;      //!< 総メモリ使用量

    [[nodiscard]] double HitRate() const noexcept {
        size_t total = hitCount + missCount;
        return total > 0 ? static_cast<double>(hitCount) / total : 0.0;
    }
};

//===========================================================================
//! テクスチャマネージャー（シングルトン）
//!
//! テクスチャのロード、キャッシュを一元管理する。
//!
//! @note 使用例:
//! @code
//!   // 初期化（内部でWIC+DDS+LRUキャッシュを自動構成）
//!   TextureManager::Get().Initialize(fileSystem);
//!
//!   // 2Dテクスチャ読み込み（デフォルト: sRGB, mipmap生成）
//!   auto diffuse = TextureManager::Get().LoadTexture2D("textures:/diffuse.png");
//!
//!   // 法線マップ（Linear）
//!   auto normal = TextureManager::Get().LoadTexture2D("textures:/normal.png", false);
//!
//!   // キューブマップ
//!   auto skybox = TextureManager::Get().LoadTextureCube("textures:/skybox.dds");
//!
//!   // 終了
//!   TextureManager::Get().Shutdown();
//! @endcode
//===========================================================================
class TextureManager final : private NonCopyableNonMovable
{
public:
    //! シングルトンインスタンスを取得
    static TextureManager& Get() noexcept;

    //----------------------------------------------------------
    //! @name   初期化・終了
    //----------------------------------------------------------
    //!@{

    //! 初期化（内部でWIC+DDS+LRUキャッシュを自動構成）
    //! @param [in] fileSystem ファイルシステム（必須）
    void Initialize(IReadableFileSystem* fileSystem);

    //! 終了
    void Shutdown();

    //! 初期化済みかどうか
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }

    //!@}
    //----------------------------------------------------------
    //! @name   テクスチャ読み込み
    //----------------------------------------------------------
    //!@{

    //! 2Dテクスチャを読み込み
    //! @param [in] path マウントパス
    //! @param [in] sRGB sRGBフォーマットとして扱う
    //! @param [in] generateMips mipmap自動生成
    //! @return テクスチャ（失敗時nullptr）
    [[nodiscard]] TexturePtr LoadTexture2D(
        const std::string& path,
        bool sRGB = true,
        bool generateMips = true);

    //! キューブマップを読み込み（単一DDSファイル）
    //! @param [in] path マウントパス
    //! @param [in] sRGB sRGBフォーマットとして扱う
    //! @param [in] generateMips mipmap自動生成
    //! @return テクスチャ（失敗時nullptr）
    [[nodiscard]] TexturePtr LoadTextureCube(
        const std::string& path,
        bool sRGB = true,
        bool generateMips = true);

    //!@}
    //----------------------------------------------------------
    //! @name   テクスチャ作成（ファイルなし）
    //----------------------------------------------------------
    //!@{

    //! 空の2Dテクスチャを作成
    [[nodiscard]] TexturePtr Create2D(
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
        uint32_t bindFlags = D3D11_BIND_SHADER_RESOURCE,
        const void* initialData = nullptr,
        uint32_t rowPitch = 0);

    //! レンダーターゲットを作成
    [[nodiscard]] TexturePtr CreateRenderTarget(
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

    //! 深度ステンシルを作成
    [[nodiscard]] TexturePtr CreateDepthStencil(
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_D24_UNORM_S8_UINT);

    //!@}
    //----------------------------------------------------------
    //! @name   キャッシュ管理
    //----------------------------------------------------------
    //!@{

    //! キャッシュをクリア
    void ClearCache();

    //! キャッシュ統計を取得
    [[nodiscard]] TextureCacheStats GetCacheStats() const;

    //!@}

private:
    TextureManager() = default;
    ~TextureManager() = default;

    //! 拡張子に対応するローダーを取得
    [[nodiscard]] ITextureLoader* GetLoaderForExtension(const std::string& path) const;

    //! キャッシュキーを計算
    [[nodiscard]] uint64_t ComputeCacheKey(
        const std::string& path,
        bool sRGB,
        bool generateMips) const;

    bool initialized_ = false;
    IReadableFileSystem* fileSystem_ = nullptr;
    std::unique_ptr<ITextureLoader> ddsLoader_;
    std::unique_ptr<ITextureLoader> wicLoader_;
    std::unique_ptr<ITextureCache> cache_;

    // 統計情報
    mutable TextureCacheStats stats_;
};

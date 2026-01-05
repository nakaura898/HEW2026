//----------------------------------------------------------------------------
//! @file   texture_manager.h
//! @brief  テクスチャマネージャー
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/gpu/gpu.h"
#include "engine/texture/texture_handle.h"
#include <memory>
#include <string>
#include <cassert>
#include <vector>
#include <queue>
#include <unordered_map>

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
//! Handle + RefCount + GC方式でテクスチャのライフサイクルを自動管理。
//!
//! @note 使用例:
//! @code
//!   // 初期化
//!   TextureManager::Get().Initialize(fileSystem);
//!
//!   // シーン開始時にスコープ開始
//!   auto scopeId = TextureManager::Get().BeginScope();
//!
//!   // テクスチャ読み込み（ハンドルを返す、所有権なし）
//!   TextureHandle tex = TextureManager::Get().Load("textures:/sprite.png");
//!
//!   // 使用時
//!   if (Texture* ptr = TextureManager::Get().Get(tex)) {
//!       sprite->SetTexture(ptr);
//!   }
//!
//!   // シーン終了時にスコープ終了 → 自動GC
//!   TextureManager::Get().EndScope(scopeId);
//!
//!   // 終了
//!   TextureManager::Get().Shutdown();
//! @endcode
//===========================================================================
class TextureManager final : private NonCopyableNonMovable
{
public:
    //------------------------------------------------------------------------
    //! @name スコープ管理型
    //------------------------------------------------------------------------
    //!@{
    using ScopeId = uint32_t;
    static constexpr ScopeId kGlobalScope = 0;  //!< 永続スコープ（シーン終了で解放されない）
    //!@}

    //! シングルトンインスタンス取得
    static TextureManager& Get()
    {
        assert(instance_ && "TextureManager::Create() must be called first");
        return *instance_;
    }

    //! インスタンス生成
    static void Create();

    //! インスタンス破棄
    static void Destroy();

    //! デストラクタ
    ~TextureManager();

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
    //! @name   スコープ管理（Handle + RefCount + GC）
    //----------------------------------------------------------
    //!@{

    //! @brief スコープ開始（シーン開始時に呼ぶ）
    //! @return 新しいスコープID
    //! @note このスコープ内でロードされたテクスチャはEndScope()時に自動解放される
    [[nodiscard]] ScopeId BeginScope();

    //! @brief スコープ終了（シーン終了時に呼ぶ）
    //! @param scopeId BeginScope()で取得したスコープID
    //! @note このスコープでロードされた全テクスチャのrefcountを減らし、GCを実行
    void EndScope(ScopeId scopeId);

    //! @brief 現在のスコープIDを取得
    [[nodiscard]] ScopeId GetCurrentScope() const noexcept { return currentScope_; }

    //!@}
    //----------------------------------------------------------
    //! @name   ハンドルベースAPI（推奨）
    //----------------------------------------------------------
    //!@{

    //! @brief 2Dテクスチャをロード（現在スコープに紐付け）
    //! @param path マウントパス
    //! @param sRGB sRGBフォーマットとして扱う
    //! @return テクスチャハンドル（失敗時Invalid）
    //! @warning OnLoadAsync()内で使用禁止。OnEnter()またはOnLoadComplete()で使用すること。
    //!          非同期ロード中はスコープが確定していないため、正しいスコープに紐付けられない。
    [[nodiscard]] TextureHandle Load(const std::string& path, bool sRGB = true);

    //! @brief 2Dテクスチャをグローバルスコープでロード（永続）
    //! @param path マウントパス
    //! @param sRGB sRGBフォーマットとして扱う
    //! @return テクスチャハンドル（失敗時Invalid）
    //! @note シーン終了時も解放されない。Shutdown()まで生存。
    [[nodiscard]] TextureHandle LoadGlobal(const std::string& path, bool sRGB = true);

    //! @brief ハンドルからTexture*を取得
    //! @param handle テクスチャハンドル
    //! @return Texture*（無効なハンドルの場合nullptr）
    [[nodiscard]] Texture* Get(TextureHandle handle) const noexcept;

    //! @brief refcount=0のテクスチャを解放
    //! @note EndScope()内で自動呼び出しされるため、通常は手動呼び出し不要
    void GarbageCollect();

    //!@}
    //----------------------------------------------------------
    //! @name   テクスチャ読み込み（レガシーAPI）
    //----------------------------------------------------------
    //!@{

    //! 2Dテクスチャを読み込み
    //! @param [in] path マウントパス
    //! @param [in] sRGB sRGBフォーマットとして扱う
    //! @param [in] generateMips mipmap自動生成（2Dゲームでは通常不要）
    //! @return テクスチャ（失敗時nullptr）
    //! @note generateMips=trueはD3D11_RESOURCE_MISC_GENERATE_MIPSを使用するため、
    //!       シャットダウン時にリファレンスカウントが残る既知の問題があります。
    [[nodiscard]] TexturePtr LoadTexture2D(
        const std::string& path,
        bool sRGB = true,
        bool generateMips = false);

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
    //! @name   テクスチャ圧縮
    //----------------------------------------------------------
    //!@{

    //! テクスチャをBC1形式に圧縮
    //! @param source 圧縮元テクスチャ（レンダーターゲット等）
    //! @return BC1圧縮されたテクスチャ（失敗時nullptr）
    //! @note 8倍のVRAM削減（例: 59MB → 7MB）、アルファなし
    [[nodiscard]] TexturePtr CompressToBC1(Texture* source);

    //! テクスチャをBC3形式に圧縮
    //! @param source 圧縮元テクスチャ（レンダーターゲット等）
    //! @return BC3圧縮されたテクスチャ（失敗時nullptr）
    //! @note 4倍のVRAM削減、フルアルファ対応
    [[nodiscard]] TexturePtr CompressToBC3(Texture* source);

    //! テクスチャをBC7形式に圧縮
    //! @param source 圧縮元テクスチャ（レンダーターゲット等）
    //! @return BC7圧縮されたテクスチャ（失敗時nullptr）
    //! @note 4倍のVRAM削減、高品質、アルファ対応
    //! @warning 大きいテクスチャは圧縮に時間がかかる
    [[nodiscard]] TexturePtr CompressToBC7(Texture* source);

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
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    static inline std::unique_ptr<TextureManager> instance_ = nullptr;

    //------------------------------------------------------------------------
    //! @name 内部構造体
    //------------------------------------------------------------------------
    //!@{

    //! @brief テクスチャスロット（内部ストレージ）
    struct TextureSlot
    {
        TexturePtr texture;           //!< 実際のテクスチャ
        uint32_t refCount = 0;        //!< 参照カウント
        uint16_t generation = 0;      //!< 世代番号（古いハンドル検出用）
        bool inUse = false;           //!< スロット使用中フラグ
    };

    //! @brief スコープデータ（スコープごとのテクスチャ追跡）
    struct ScopeData
    {
        std::vector<TextureHandle> textures;  //!< このスコープで使用中のテクスチャハンドル
    };

    //!@}
    //------------------------------------------------------------------------
    //! @name 内部ヘルパー
    //------------------------------------------------------------------------
    //!@{

    //! 拡張子に対応するローダーを取得
    [[nodiscard]] ITextureLoader* GetLoaderForExtension(const std::string& path) const;

    //! キャッシュキーを計算
    [[nodiscard]] uint64_t ComputeCacheKey(
        const std::string& path,
        bool sRGB,
        bool generateMips) const;

    //! @brief スロットを割り当て
    [[nodiscard]] TextureHandle AllocateSlot(TexturePtr texture);

    //! @brief 指定スコープにテクスチャを追加
    void AddToScope(TextureHandle handle, ScopeId scope);

    //! @brief 参照カウントを増加
    void IncrementRefCount(TextureHandle handle);

    //! @brief 参照カウントを減少
    void DecrementRefCount(TextureHandle handle);

    //! @brief 指定スコープでテクスチャをロード
    [[nodiscard]] TextureHandle LoadInScope(const std::string& path, bool sRGB, ScopeId scope);

    //!@}
    //------------------------------------------------------------------------
    //! @name メンバ変数
    //------------------------------------------------------------------------

    bool initialized_ = false;
    IReadableFileSystem* fileSystem_ = nullptr;
    std::unique_ptr<ITextureLoader> ddsLoader_;
    std::unique_ptr<ITextureLoader> wicLoader_;
    std::unique_ptr<ITextureCache> cache_;

    // 統計情報
    mutable TextureCacheStats stats_;

    //--- スロットベースストレージ ---
    std::vector<TextureSlot> slots_;                          //!< テクスチャスロット配列
    std::queue<uint16_t> freeIndices_;                        //!< 空きスロットインデックス
    std::unordered_map<uint64_t, TextureHandle> handleCache_; //!< パス→ハンドルキャッシュ

    //--- スコープ管理 ---
    ScopeId currentScope_ = kGlobalScope;                     //!< 現在のスコープ
    ScopeId nextScopeId_ = 1;                                 //!< 次のスコープID
    std::unordered_map<ScopeId, ScopeData> scopes_;           //!< スコープデータ
};

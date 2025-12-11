//----------------------------------------------------------------------------
//! @file   texture_cache.h
//! @brief  テクスチャキャッシュ
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/gpu/gpu.h"
#include <unordered_map>
#include <list>
#include <string>

//! テクスチャ弱参照
using TextureWeakPtr = std::weak_ptr<Texture>;

//===========================================================================
//! テクスチャキャッシュインターフェース
//===========================================================================
class ITextureCache
{
public:
    virtual ~ITextureCache() = default;

    //! キャッシュからテクスチャを取得
    //! @param [in] key キャッシュキー
    //! @return テクスチャ（キャッシュミス時nullptr）
    [[nodiscard]] virtual TexturePtr Get(uint64_t key) = 0;

    //! テクスチャをキャッシュに追加
    //! @param [in] key キャッシュキー
    //! @param [in] texture テクスチャ
    virtual void Put(uint64_t key, TexturePtr texture) = 0;

    //! キャッシュをクリア
    virtual void Clear() = 0;

    //! キャッシュエントリ数を取得
    [[nodiscard]] virtual size_t Count() const = 0;

    //! 使用メモリサイズを取得（バイト）
    [[nodiscard]] virtual size_t MemoryUsage() const = 0;
};

//===========================================================================
//! LRUテクスチャキャッシュ
//!
//! @brief 最近使用されていないテクスチャを自動的に破棄するキャッシュ
//!
//! @details
//! - メモリ上限を超えるとLRU順で古いエントリを削除
//! - 弱参照を使用し、外部で解放されたテクスチャは自動的に除去
//! - スレッドセーフではない（シングルスレッド使用を想定）
//!
//! @code
//!   LRUTextureCache cache(256 * 1024 * 1024);  // 256MB
//!
//!   // キャッシュに追加
//!   cache.Put(key, texture);
//!
//!   // キャッシュから取得（LRU順更新）
//!   auto tex = cache.Get(key);
//!
//!   // メモリ使用量確認
//!   size_t usage = cache.MemoryUsage();
//! @endcode
//===========================================================================
class LRUTextureCache final : public ITextureCache
{
public:
    //! コンストラクタ
    //! @param [in] maxMemoryBytes メモリ上限（バイト、デフォルト256MB）
    explicit LRUTextureCache(size_t maxMemoryBytes = 256 * 1024 * 1024);

    ~LRUTextureCache() override = default;

    // コピー禁止
    LRUTextureCache(const LRUTextureCache&) = delete;
    LRUTextureCache& operator=(const LRUTextureCache&) = delete;

    // ムーブ許可
    LRUTextureCache(LRUTextureCache&&) noexcept = default;
    LRUTextureCache& operator=(LRUTextureCache&&) noexcept = default;

    //----------------------------------------------------------
    //! @name   ITextureCache実装
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] TexturePtr Get(uint64_t key) override;
    void Put(uint64_t key, TexturePtr texture) override;
    void Clear() override;
    [[nodiscard]] size_t Count() const override;
    [[nodiscard]] size_t MemoryUsage() const override;

    //!@}
    //----------------------------------------------------------
    //! @name   LRU固有
    //----------------------------------------------------------
    //!@{

    //! メモリ上限を設定
    void SetMaxMemory(size_t maxMemoryBytes);

    //! メモリ上限を取得
    [[nodiscard]] size_t GetMaxMemory() const noexcept { return maxMemoryBytes_; }

    //! 期限切れエントリを削除（弱参照が無効になったもの）
    void PurgeExpired();

    //! メモリ上限まで古いエントリを削除
    void Evict();

    //!@}

private:
    //! キャッシュエントリ
    struct CacheEntry
    {
        uint64_t key;
        TexturePtr texture;  //!< 強参照
        size_t memorySize;          //!< テクスチャのメモリサイズ
    };

    using LruList = std::list<CacheEntry>;
    using LruIterator = LruList::iterator;

    //! LRUリスト（先頭が最新、末尾が最古）
    LruList lruList_;

    //! キーからイテレータへのマップ
    std::unordered_map<uint64_t, LruIterator> cacheMap_;

    //! メモリ上限
    size_t maxMemoryBytes_;

    //! 現在のメモリ使用量
    size_t currentMemoryBytes_ = 0;

    //! エントリを最新に移動
    void TouchEntry(LruIterator it);

    //! 最古のエントリを削除
    void EvictOldest();
};

//===========================================================================
//! 弱参照テクスチャキャッシュ
//!
//! @brief 弱参照を使用し、外部で解放されたテクスチャを自動的に除去するキャッシュ
//!
//! @details
//! - テクスチャの生存を延長しない（弱参照のみ保持）
//! - 外部で全ての強参照が解放されると、キャッシュエントリは無効になる
//! - Get時に無効なエントリは自動削除
//! - メモリ上限なし（テクスチャ自体は保持しないため）
//! - スレッドセーフではない（シングルスレッド使用を想定）
//!
//! @code
//!   WeakTextureCache cache;
//!
//!   // キャッシュに追加（弱参照として保持）
//!   cache.Put(key, texture);
//!
//!   // キャッシュから取得（外部で生存していればshared_ptrを返す）
//!   auto tex = cache.Get(key);  // 解放済みならnullptr
//!
//!   // 無効なエントリを一括削除
//!   cache.PurgeExpired();
//! @endcode
//===========================================================================
class WeakTextureCache final : public ITextureCache
{
public:
    WeakTextureCache() = default;
    ~WeakTextureCache() override = default;

    // コピー禁止
    WeakTextureCache(const WeakTextureCache&) = delete;
    WeakTextureCache& operator=(const WeakTextureCache&) = delete;

    // ムーブ許可
    WeakTextureCache(WeakTextureCache&&) noexcept = default;
    WeakTextureCache& operator=(WeakTextureCache&&) noexcept = default;

    //----------------------------------------------------------
    //! @name   ITextureCache実装
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] TexturePtr Get(uint64_t key) override;
    void Put(uint64_t key, TexturePtr texture) override;
    void Clear() override;
    [[nodiscard]] size_t Count() const override;
    [[nodiscard]] size_t MemoryUsage() const override;

    //!@}
    //----------------------------------------------------------
    //! @name   WeakTextureCache固有
    //----------------------------------------------------------
    //!@{

    //! 期限切れエントリを削除（弱参照が無効になったもの）
    //! @return 削除されたエントリ数
    size_t PurgeExpired();

    //! 有効なエントリ数を取得（期限切れを除く）
    [[nodiscard]] size_t ValidCount() const;

    //!@}

private:
    //! キャッシュエントリ
    struct WeakEntry
    {
        TextureWeakPtr texture;     //!< 弱参照
        size_t memorySizeHint;      //!< メモリサイズヒント（参考値）
    };

    //! キーからエントリへのマップ
    std::unordered_map<uint64_t, WeakEntry> cacheMap_;
};

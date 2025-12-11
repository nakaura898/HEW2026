//----------------------------------------------------------------------------
//! @file   texture_cache.cpp
//! @brief  テクスチャキャッシュ実装
//----------------------------------------------------------------------------
#include "texture_cache.h"
#include "common/logging/logging.h"

//============================================================================
// LRUTextureCache 実装
//============================================================================

LRUTextureCache::LRUTextureCache(size_t maxMemoryBytes)
    : maxMemoryBytes_(maxMemoryBytes)
{
}

TexturePtr LRUTextureCache::Get(uint64_t key)
{
    auto it = cacheMap_.find(key);
    if (it == cacheMap_.end()) {
        return nullptr;
    }

    // LRU順を更新（最新に移動）
    TouchEntry(it->second);

    return it->second->texture;
}

void LRUTextureCache::Put(uint64_t key, TexturePtr texture)
{
    if (!texture) {
        return;
    }

    // 既存エントリがあれば削除
    auto existingIt = cacheMap_.find(key);
    if (existingIt != cacheMap_.end()) {
        currentMemoryBytes_ -= existingIt->second->memorySize;
        lruList_.erase(existingIt->second);
        cacheMap_.erase(existingIt);
    }

    // メモリサイズ計算
    size_t memSize = texture->GpuSize();

    // メモリ上限を超える場合は古いエントリを削除
    while (currentMemoryBytes_ + memSize > maxMemoryBytes_ && !lruList_.empty()) {
        EvictOldest();
    }

    // 新しいエントリを先頭に追加
    CacheEntry entry;
    entry.key = key;
    entry.texture = std::move(texture);
    entry.memorySize = memSize;

    lruList_.push_front(std::move(entry));
    cacheMap_[key] = lruList_.begin();
    currentMemoryBytes_ += memSize;
}

void LRUTextureCache::Clear()
{
    lruList_.clear();
    cacheMap_.clear();
    currentMemoryBytes_ = 0;
}

size_t LRUTextureCache::Count() const
{
    return cacheMap_.size();
}

size_t LRUTextureCache::MemoryUsage() const
{
    return currentMemoryBytes_;
}

void LRUTextureCache::SetMaxMemory(size_t maxMemoryBytes)
{
    maxMemoryBytes_ = maxMemoryBytes;

    // 新しい上限を超えている場合は削除
    while (currentMemoryBytes_ > maxMemoryBytes_ && !lruList_.empty()) {
        EvictOldest();
    }
}

void LRUTextureCache::PurgeExpired()
{
    // IntrusivePtrは強参照なので、外部で参照がなくなっても
    // キャッシュが持っている限り解放されない
    // この実装では明示的な期限切れ処理は不要
    // （弱参照を使う場合はここで無効なエントリを削除する）
}

void LRUTextureCache::Evict()
{
    while (currentMemoryBytes_ > maxMemoryBytes_ && !lruList_.empty()) {
        EvictOldest();
    }
}

void LRUTextureCache::TouchEntry(LruIterator it)
{
    if (it != lruList_.begin()) {
        // 先頭に移動
        lruList_.splice(lruList_.begin(), lruList_, it);
    }
}

void LRUTextureCache::EvictOldest()
{
    if (lruList_.empty()) {
        return;
    }

    // 末尾（最古）のエントリを削除
    auto& oldest = lruList_.back();
    currentMemoryBytes_ -= oldest.memorySize;
    cacheMap_.erase(oldest.key);
    lruList_.pop_back();
}

//============================================================================
// WeakTextureCache 実装
//============================================================================

TexturePtr WeakTextureCache::Get(uint64_t key)
{
    auto it = cacheMap_.find(key);
    if (it == cacheMap_.end()) {
        return nullptr;
    }

    // 弱参照から強参照を取得
    TexturePtr texture = it->second.texture.lock();
    if (!texture) {
        // 期限切れエントリを削除
        cacheMap_.erase(it);
        return nullptr;
    }

    return texture;
}

void WeakTextureCache::Put(uint64_t key, TexturePtr texture)
{
    if (!texture) {
        return;
    }

    // メモリサイズを記録（参考値として）
    WeakEntry entry;
    entry.texture = texture;  // shared_ptr → weak_ptr に暗黙変換
    entry.memorySizeHint = texture->GpuSize();

    cacheMap_[key] = std::move(entry);
}

void WeakTextureCache::Clear()
{
    cacheMap_.clear();
}

size_t WeakTextureCache::Count() const
{
    return cacheMap_.size();
}

size_t WeakTextureCache::MemoryUsage() const
{
    // 有効なエントリのメモリサイズ合計を返す
    size_t total = 0;
    for (const auto& [key, entry] : cacheMap_) {
        if (!entry.texture.expired()) {
            total += entry.memorySizeHint;
        }
    }
    return total;
}

size_t WeakTextureCache::PurgeExpired()
{
    size_t purged = 0;
    for (auto it = cacheMap_.begin(); it != cacheMap_.end(); ) {
        if (it->second.texture.expired()) {
            it = cacheMap_.erase(it);
            ++purged;
        } else {
            ++it;
        }
    }
    return purged;
}

size_t WeakTextureCache::ValidCount() const
{
    size_t count = 0;
    for (const auto& [key, entry] : cacheMap_) {
        if (!entry.texture.expired()) {
            ++count;
        }
    }
    return count;
}

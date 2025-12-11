//----------------------------------------------------------------------------
//! @file   shader_cache.cpp
//! @brief  シェーダーキャッシュ実装
//----------------------------------------------------------------------------
#include "shader_cache.h"

ID3DBlob* ShaderCache::find(uint64_t key) const noexcept
{
    std::shared_lock lock(mutex_);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        ++hitCount_;
        return it->second.Get();
    }

    ++missCount_;
    return nullptr;
}

void ShaderCache::store(uint64_t key, ID3DBlob* bytecode)
{
    if (!bytecode) return;

    std::unique_lock lock(mutex_);
    cache_[key] = bytecode;
}

void ShaderCache::clear()
{
    std::unique_lock lock(mutex_);
    cache_.clear();
}

bool ShaderCache::isEmpty() const noexcept
{
    std::shared_lock lock(mutex_);
    return cache_.empty();
}

size_t ShaderCache::size() const noexcept
{
    std::shared_lock lock(mutex_);
    return cache_.size();
}

ShaderCacheStats ShaderCache::getStats() const noexcept
{
    std::shared_lock lock(mutex_);

    ShaderCacheStats stats;
    stats.hitCount = hitCount_;
    stats.missCount = missCount_;
    stats.entryCount = cache_.size();

    for (const auto& [key, bytecode] : cache_) {
        if (bytecode) {
            stats.totalSize += bytecode->GetBufferSize();
        }
    }

    return stats;
}

void ShaderCache::resetStats() noexcept
{
    std::unique_lock lock(mutex_);
    hitCount_ = 0;
    missCount_ = 0;
}

//============================================================================
// ShaderResourceCache 実装
//============================================================================

ShaderPtr ShaderResourceCache::Get(uint64_t key)
{
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        ++hitCount_;
        return it->second;
    }

    ++missCount_;
    return nullptr;
}

void ShaderResourceCache::Put(uint64_t key, ShaderPtr shader)
{
    if (!shader) return;
    cache_[key] = std::move(shader);
}

void ShaderResourceCache::Clear()
{
    cache_.clear();
}

size_t ShaderResourceCache::Count() const noexcept
{
    return cache_.size();
}

ShaderCacheStats ShaderResourceCache::GetStats() const noexcept
{
    ShaderCacheStats stats;
    stats.hitCount = hitCount_;
    stats.missCount = missCount_;
    stats.entryCount = cache_.size();
    stats.totalSize = 0;  // Shaderオブジェクトのサイズ計算は省略
    return stats;
}

void ShaderResourceCache::ResetStats() noexcept
{
    hitCount_ = 0;
    missCount_ = 0;
}

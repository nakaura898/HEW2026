//----------------------------------------------------------------------------
//! @file   hash.h
//! @brief  ハッシュユーティリティ
//----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace HashUtil
{

//----------------------------------------------------------------------------
//! FNV-1aハッシュ（64bit）
//! @param [in] data データポインタ
//! @param [in] size データサイズ
//! @param [in] hash 初期ハッシュ値（チェーン用）
//! @return ハッシュ値
//----------------------------------------------------------------------------
inline uint64_t Fnv1a(const void* data, size_t size, uint64_t hash = 14695981039346656037ULL) noexcept
{
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

//----------------------------------------------------------------------------
//! 文字列のFNV-1aハッシュ
//! @param [in] str 文字列
//! @param [in] hash 初期ハッシュ値（チェーン用）
//! @return ハッシュ値
//----------------------------------------------------------------------------
inline uint64_t Fnv1aString(const std::string& str, uint64_t hash = 14695981039346656037ULL) noexcept
{
    return Fnv1a(str.data(), str.size(), hash);
}

} // namespace HashUtil

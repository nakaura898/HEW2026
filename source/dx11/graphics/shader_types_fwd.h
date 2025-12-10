//----------------------------------------------------------------------------
//! @file   shader_types_fwd.h
//! @brief  シェーダー関連の前方宣言
//----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// コンパイラ・キャッシュ
class IShaderCompiler;
class IShaderCache;
class IShaderResourceCache;

// ファイルシステム
class IReadableFileSystem;

//----------------------------------------------------------------------------
//! シェーダーマクロ定義
//----------------------------------------------------------------------------
struct ShaderDefine {
    std::string name;   //!< マクロ名
    std::string value;  //!< マクロ値（空文字列でも有効）

    ShaderDefine() = default;
    ShaderDefine(const std::string& n, const std::string& v = "1")
        : name(n), value(v) {}
};

//----------------------------------------------------------------------------
//! シェーダーキャッシュ統計情報
//----------------------------------------------------------------------------
struct ShaderCacheStats {
    size_t hitCount = 0;    //!< キャッシュヒット回数
    size_t missCount = 0;   //!< キャッシュミス回数
    size_t entryCount = 0;  //!< エントリ数
    size_t totalSize = 0;   //!< 総バイトコードサイズ

    [[nodiscard]] double HitRate() const noexcept {
        size_t total = hitCount + missCount;
        return total > 0 ? static_cast<double>(hitCount) / total : 0.0;
    }
};

//----------------------------------------------------------------------------
//! @file   shader_cache.h
//! @brief  シェーダーキャッシュ
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/graphics/shader_types_fwd.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

//----------------------------------------------------------------------------
//! シェーダーキャッシュインターフェース
//----------------------------------------------------------------------------
class IShaderCache {
public:
    virtual ~IShaderCache() = default;

    //! キャッシュを検索
    //! @param [in] key キャッシュキー（64bitハッシュ）
    //! @return バイトコード（見つからない場合はnullptr）
    [[nodiscard]] virtual ID3DBlob* find(uint64_t key) const noexcept = 0;

    //! キャッシュに保存
    //! @param [in] key キャッシュキー（64bitハッシュ）
    //! @param [in] bytecode バイトコード
    virtual void store(uint64_t key, ID3DBlob* bytecode) = 0;

    //! キャッシュをクリア
    virtual void clear() = 0;

    //! 統計情報を取得
    [[nodiscard]] virtual ShaderCacheStats getStats() const noexcept = 0;
};

//----------------------------------------------------------------------------
//! シェーダーバイトコードキャッシュ（メモリキャッシュ実装）
//!
//! @note スレッドセーフ:
//!   - find(): 読み取りロック（複数同時アクセス可能）
//!   - store(): 書き込みロック（排他）
//!   - clear(): 書き込みロック（排他）
//----------------------------------------------------------------------------
class ShaderCache : public IShaderCache {
public:
    ShaderCache() = default;
    ~ShaderCache() override = default;

    [[nodiscard]] ID3DBlob* find(uint64_t key) const noexcept override;
    void store(uint64_t key, ID3DBlob* bytecode) override;
    void clear() override;
    [[nodiscard]] ShaderCacheStats getStats() const noexcept override;

    //! キャッシュが空か確認
    [[nodiscard]] bool isEmpty() const noexcept;

    //! エントリ数を取得
    [[nodiscard]] size_t size() const noexcept;

    //! 統計情報をリセット
    void resetStats() noexcept;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint64_t, ComPtr<ID3DBlob>> cache_;

    mutable size_t hitCount_ = 0;
    mutable size_t missCount_ = 0;
};

//----------------------------------------------------------------------------
//! キャッシュなし実装（テスト・デバッグ用）
//----------------------------------------------------------------------------
class NullShaderCache : public IShaderCache {
public:
    [[nodiscard]] ID3DBlob* find(uint64_t) const noexcept override {
        return nullptr;
    }
    void store(uint64_t, ID3DBlob*) override {}
    void clear() override {}
    [[nodiscard]] ShaderCacheStats getStats() const noexcept override { return {}; }
};

//============================================================================
// シェーダーリソースキャッシュ（Shaderオブジェクト用）
//============================================================================

// 前方宣言
class Shader;
using ShaderPtr = std::shared_ptr<Shader>;

//----------------------------------------------------------------------------
//! シェーダーリソースキャッシュインターフェース
//!
//! コンパイル済みShaderオブジェクトをキャッシュするためのインターフェース。
//! IShaderCache（バイトコード用）とは別に、GPU上のシェーダーオブジェクトを管理。
//----------------------------------------------------------------------------
class IShaderResourceCache {
public:
    virtual ~IShaderResourceCache() = default;

    //! キャッシュを検索
    //! @param [in] key キャッシュキー（64bitハッシュ）
    //! @return シェーダー（見つからない場合はnullptr）
    [[nodiscard]] virtual ShaderPtr Get(uint64_t key) = 0;

    //! キャッシュに保存
    //! @param [in] key キャッシュキー（64bitハッシュ）
    //! @param [in] shader シェーダー
    virtual void Put(uint64_t key, ShaderPtr shader) = 0;

    //! キャッシュをクリア
    virtual void Clear() = 0;

    //! エントリ数を取得
    [[nodiscard]] virtual size_t Count() const noexcept = 0;

    //! 統計情報を取得
    [[nodiscard]] virtual ShaderCacheStats GetStats() const noexcept = 0;
};

//----------------------------------------------------------------------------
//! シェーダーリソースキャッシュ（メモリキャッシュ実装）
//!
//! @note スレッドセーフではない（シングルスレッド使用を想定）
//----------------------------------------------------------------------------
class ShaderResourceCache : public IShaderResourceCache {
public:
    ShaderResourceCache() = default;
    ~ShaderResourceCache() override = default;

    // コピー禁止
    ShaderResourceCache(const ShaderResourceCache&) = delete;
    ShaderResourceCache& operator=(const ShaderResourceCache&) = delete;

    [[nodiscard]] ShaderPtr Get(uint64_t key) override;
    void Put(uint64_t key, ShaderPtr shader) override;
    void Clear() override;
    [[nodiscard]] size_t Count() const noexcept override;
    [[nodiscard]] ShaderCacheStats GetStats() const noexcept override;

    //! 統計情報をリセット
    void ResetStats() noexcept;

private:
    std::unordered_map<uint64_t, ShaderPtr> cache_;
    mutable size_t hitCount_ = 0;
    mutable size_t missCount_ = 0;
};

//----------------------------------------------------------------------------
//! キャッシュなし実装（テスト・デバッグ用）
//----------------------------------------------------------------------------
class NullShaderResourceCache : public IShaderResourceCache {
public:
    [[nodiscard]] ShaderPtr Get(uint64_t) override { return nullptr; }
    void Put(uint64_t, ShaderPtr) override {}
    void Clear() override {}
    [[nodiscard]] size_t Count() const noexcept override { return 0; }
    [[nodiscard]] ShaderCacheStats GetStats() const noexcept override { return {}; }
};

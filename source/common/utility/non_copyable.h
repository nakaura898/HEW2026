//----------------------------------------------------------------------------
//! @file   non_copyable.h
//! @brief  コピー禁止基底クラス
//!
//! @details
//! リソース管理クラスで使用するコピー/ムーブ制御用の基底クラスを提供します。
//!
//! 使用例:
//! @code
//! // シングルトン等、コピーもムーブも禁止
//! class Manager final : private NonCopyableNonMovable { ... };
//!
//! // リソースクラス、コピー禁止・ムーブ許可
//! class Buffer : private NonCopyable { ... };
//! @endcode
//----------------------------------------------------------------------------
#pragma once

//! コピー禁止・ムーブ禁止の基底クラス
//! @details シングルトンや一意リソースの管理に使用
class NonCopyableNonMovable
{
protected:
    NonCopyableNonMovable() = default;
    ~NonCopyableNonMovable() = default;

    // コピー禁止
    NonCopyableNonMovable(const NonCopyableNonMovable&) = delete;
    NonCopyableNonMovable& operator=(const NonCopyableNonMovable&) = delete;

    // ムーブ禁止
    NonCopyableNonMovable(NonCopyableNonMovable&&) = delete;
    NonCopyableNonMovable& operator=(NonCopyableNonMovable&&) = delete;
};

//! コピー禁止・ムーブ許可の基底クラス
//! @details GPUリソース等、所有権の移動が必要なクラスに使用
class NonCopyable
{
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;

    // コピー禁止
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

    // ムーブ許可
    NonCopyable(NonCopyable&&) noexcept = default;
    NonCopyable& operator=(NonCopyable&&) noexcept = default;
};

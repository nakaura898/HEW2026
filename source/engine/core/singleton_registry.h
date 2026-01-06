//----------------------------------------------------------------------------
//! @file   singleton_registry.h
//! @brief  シングルトン初期化順序の検証
//!
//! @note スレッドセーフではない。メインスレッド初期化時のみ使用。
//!
//! @details シングルトンの依存関係を追跡し、初期化順序の問題を早期検出する。
//!          - 依存関係が満たされていない場合はDebugビルドでassert
//!          - 各シングルトンのCreate()/Destroy()でSINGLETON_REGISTER/UNREGISTER使用
//!
//! @code
//! // TextureManager::Create()内で
//! SINGLETON_REGISTER(TextureManager,
//!     SingletonId::GraphicsDevice | SingletonId::GraphicsContext);
//!
//! // TextureManager::Destroy()内で
//! SINGLETON_UNREGISTER(TextureManager);
//! @endcode
//----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <cassert>
#include <string_view>
#include "common/logging/logging.h"

//============================================================================
//! @brief シングルトンID（ビットフラグ）
//============================================================================
enum class SingletonId : uint32_t {
    None = 0,

    //! @name DX11層
    //! @{
    GraphicsDevice  = 1 << 0,
    GraphicsContext = 1 << 1,
    //! @}

    //! @name Engine層
    //! @{
    JobSystem           = 1 << 2,
    FileSystemManager   = 1 << 3,
    TextureManager      = 1 << 4,
    ShaderManager       = 1 << 5,
    InputManager        = 1 << 6,
    RenderStateManager  = 1 << 7,
    SpriteBatch         = 1 << 8,
    CollisionManager    = 1 << 9,
    SceneManager        = 1 << 10,
    Renderer            = 1 << 11,
    //! @}

    //! @name Debug
    //! @{
    DebugDraw       = 1 << 12,
    CircleRenderer  = 1 << 13,
    //! @}
};

//! @brief SingletonIdのビット演算サポート
inline constexpr SingletonId operator|(SingletonId a, SingletonId b) noexcept {
    return static_cast<SingletonId>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr SingletonId operator&(SingletonId a, SingletonId b) noexcept {
    return static_cast<SingletonId>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline constexpr SingletonId operator~(SingletonId a) noexcept {
    return static_cast<SingletonId>(~static_cast<uint32_t>(a));
}

//============================================================================
//! @brief シングルトン初期化レジストリ
//!
//! 依存関係を追跡し、初期化順序の問題を検出する。
//============================================================================
class SingletonRegistry
{
public:
    //------------------------------------------------------------------------
    //! @brief シングルトンの初期化を登録
    //! @param id シングルトンID
    //! @param dependencies 依存するシングルトン（ビットOR）
    //! @param name デバッグ用名前
    //------------------------------------------------------------------------
    static void Register(SingletonId id, SingletonId dependencies,
                         [[maybe_unused]] std::string_view name)
    {
        // 依存関係チェック
        uint32_t depBits = static_cast<uint32_t>(dependencies);
        uint32_t initBits = static_cast<uint32_t>(initialized_);

        if ((depBits & initBits) != depBits) {
            // 依存が満たされていない
            uint32_t missing = depBits & ~initBits;
#ifdef _DEBUG
            LOG_ERROR("[SingletonRegistry] " + std::string(name) +
                      " の依存関係が満たされていません (missing bits: " +
                      std::to_string(missing) + ")");
            assert(false && "Singleton dependency not initialized");
#else
            // Releaseビルドでは警告ログを出力して続行
            LOG_WARN("[SingletonRegistry] " + std::string(name) +
                     " の依存関係が満たされていません (missing bits: " +
                     std::to_string(missing) + ")");
#endif
        }

        initialized_ = initialized_ | id;
    }

    //------------------------------------------------------------------------
    //! @brief シングルトンの破棄を登録
    //! @param id シングルトンID
    //------------------------------------------------------------------------
    static void Unregister(SingletonId id) noexcept
    {
        initialized_ = static_cast<SingletonId>(
            static_cast<uint32_t>(initialized_) & ~static_cast<uint32_t>(id));
    }

    //------------------------------------------------------------------------
    //! @brief 指定シングルトンが初期化済みか確認
    //! @param id シングルトンID
    //! @return 初期化済みならtrue
    //------------------------------------------------------------------------
    [[nodiscard]] static bool IsInitialized(SingletonId id) noexcept
    {
        return (static_cast<uint32_t>(initialized_) &
                static_cast<uint32_t>(id)) != 0;
    }

    //------------------------------------------------------------------------
    //! @brief 全シングルトンがクリアされたか確認（テスト用）
    //! @return すべて破棄済みならtrue
    //------------------------------------------------------------------------
    [[nodiscard]] static bool AllCleared() noexcept
    {
        return initialized_ == SingletonId::None;
    }

    //------------------------------------------------------------------------
    //! @brief 初期化状態をリセット（テスト用）
    //------------------------------------------------------------------------
    static void Reset() noexcept
    {
        initialized_ = SingletonId::None;
    }

private:
    static inline SingletonId initialized_ = SingletonId::None;
};

//============================================================================
//! @brief シングルトン初期化ヘルパーマクロ
//!
//! @code
//! // Create()内で使用
//! SINGLETON_REGISTER(TextureManager,
//!     SingletonId::GraphicsDevice | SingletonId::GraphicsContext);
//!
//! // Destroy()内で使用
//! SINGLETON_UNREGISTER(TextureManager);
//! @endcode
//============================================================================
#define SINGLETON_REGISTER(Name, Dependencies) \
    SingletonRegistry::Register(SingletonId::Name, Dependencies, #Name)

#define SINGLETON_UNREGISTER(Name) \
    SingletonRegistry::Unregister(SingletonId::Name)

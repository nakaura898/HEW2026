#pragma once

#include "keyboard.h"
#include "mouse.h"
#include "gamepad_manager.h"
#include <memory>
#include <cassert>

/// @brief 入力管理クラス（シングルトン）
///
/// 責務:
/// - 入力システム全体の統合管理
/// - Keyboard、Mouseの保持
/// - 入力状態の毎フレーム更新
///
/// 使用方法:
/// - InputManager::Create() で生成
/// - InputManager::Get() でインスタンス取得
/// - InputManager::Destroy() で破棄
class InputManager {
public:
    /// @brief シングルトンインスタンスを取得
    static InputManager& Get()
    {
        assert(instance_ && "InputManager::Create() must be called first");
        return *instance_;
    }

    /// @brief インスタンス生成
    static void Create();

    /// @brief インスタンス破棄
    static void Destroy();

    /// @brief デストラクタ
    ~InputManager() noexcept = default;

    // コピー禁止
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // ムーブ禁止
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    /// @brief Keyboardを取得
    /// @return Keyboard参照
    Keyboard& GetKeyboard() noexcept { return *keyboard_; }

    /// @brief Keyboardを取得（const版）
    /// @return Keyboard参照
    const Keyboard& GetKeyboard() const noexcept { return *keyboard_; }

    /// @brief Mouseを取得
    /// @return Mouse参照
    Mouse& GetMouse() noexcept { return *mouse_; }

    /// @brief Mouseを取得（const版）
    /// @return Mouse参照
    const Mouse& GetMouse() const noexcept { return *mouse_; }

    /// @brief GamepadManagerを取得
    /// @return GamepadManager参照
    GamepadManager& GetGamepadManager() noexcept { return *gamepadManager_; }

    /// @brief GamepadManagerを取得（const版）
    /// @return GamepadManager参照
    const GamepadManager& GetGamepadManager() const noexcept { return *gamepadManager_; }

    /// @brief 入力状態を更新
    /// @param deltaTime 前フレームからの経過時間（秒）
    void Update(float deltaTime) noexcept;

private:
    /// @brief コンストラクタ（private）
    InputManager() noexcept;

    static inline std::unique_ptr<InputManager> instance_ = nullptr;

    std::unique_ptr<Keyboard> keyboard_;
    std::unique_ptr<Mouse> mouse_;
    std::unique_ptr<GamepadManager> gamepadManager_;
};

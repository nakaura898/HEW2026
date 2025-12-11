#pragma once

#include "gamepad.h"
#include <memory>
#include <array>

/// @brief ゲームパッド管理クラス
///
/// 責務:
/// - 複数ゲームパッドの管理（最大4台）
/// - ゲームパッドの接続・切断検出
///
/// 使用例:
/// @code
/// GamepadManager manager;
/// manager.Update();
/// if (manager.IsConnected(0)) {
///     auto& gamepad = manager.GetGamepad(0);
///     if (gamepad.IsButtonDown(GamepadButton::A)) {
///         // プレイヤー1がAボタンを押した
///     }
/// }
/// @endcode
class GamepadManager {
public:
    static constexpr int MaxGamepads = 4;  // XInputは最大4台

    /// @brief コンストラクタ
    GamepadManager() noexcept;

    /// @brief デストラクタ
    ~GamepadManager() noexcept = default;

    // コピー禁止
    GamepadManager(const GamepadManager&) = delete;
    GamepadManager& operator=(const GamepadManager&) = delete;

    // ムーブ禁止
    GamepadManager(GamepadManager&&) = delete;
    GamepadManager& operator=(GamepadManager&&) = delete;

    /// @brief 全ゲームパッドの状態を更新
    void Update() noexcept;

    /// @brief ゲームパッドが接続されているか
    /// @param index ゲームパッドインデックス (0-3)
    /// @return 接続されている場合true
    bool IsConnected(int index) const noexcept;

    /// @brief ゲームパッドを取得
    /// @param index ゲームパッドインデックス (0-3)
    /// @return Gamepad参照
    Gamepad& GetGamepad(int index) noexcept;

    /// @brief ゲームパッドを取得（const版）
    /// @param index ゲームパッドインデックス (0-3)
    /// @return Gamepad参照
    const Gamepad& GetGamepad(int index) const noexcept;

    /// @brief 接続されているゲームパッド数を取得
    /// @return 接続数
    int GetConnectedCount() const noexcept;

private:
    std::array<std::unique_ptr<Gamepad>, MaxGamepads> gamepads_;
};

#pragma once

#include <Windows.h>
#include <Xinput.h>
#include <cstdint>

/// @brief ゲームパッドボタン
enum class GamepadButton : uint16_t {
    DPadUp        = XINPUT_GAMEPAD_DPAD_UP,
    DPadDown      = XINPUT_GAMEPAD_DPAD_DOWN,
    DPadLeft      = XINPUT_GAMEPAD_DPAD_LEFT,
    DPadRight     = XINPUT_GAMEPAD_DPAD_RIGHT,
    Start         = XINPUT_GAMEPAD_START,
    Back          = XINPUT_GAMEPAD_BACK,
    LeftThumb     = XINPUT_GAMEPAD_LEFT_THUMB,
    RightThumb    = XINPUT_GAMEPAD_RIGHT_THUMB,
    LeftShoulder  = XINPUT_GAMEPAD_LEFT_SHOULDER,
    RightShoulder = XINPUT_GAMEPAD_RIGHT_SHOULDER,
    A             = XINPUT_GAMEPAD_A,
    B             = XINPUT_GAMEPAD_B,
    X             = XINPUT_GAMEPAD_X,
    Y             = XINPUT_GAMEPAD_Y,
};

/// @brief 単一ゲームパッドの入力管理クラス
///
/// 責務:
/// - XInputを使用した単一ゲームパッドの状態取得
/// - ボタン・スティック・トリガーの状態管理
/// - デッドゾーン処理
///
/// 使用例:
/// @code
/// Gamepad gamepad(0);  // プレイヤー1
/// gamepad.Update();
/// if (gamepad.IsButtonDown(GamepadButton::A)) {
///     // Aボタンが押された
/// }
/// float lx = gamepad.GetLeftStickX();
/// @endcode
class Gamepad {
public:
    /// @brief コンストラクタ
    /// @param userIndex ユーザーインデックス (0-3)
    explicit Gamepad(DWORD userIndex) noexcept;

    /// @brief デストラクタ
    ~Gamepad() noexcept = default;

    // コピー禁止
    Gamepad(const Gamepad&) = delete;
    Gamepad& operator=(const Gamepad&) = delete;

    // ムーブ禁止
    Gamepad(Gamepad&&) = delete;
    Gamepad& operator=(Gamepad&&) = delete;

    /// @brief 入力状態を更新
    void Update() noexcept;

    /// @brief 接続されているか
    /// @return 接続されている場合true
    bool IsConnected() const noexcept { return connected_; }

    /// @brief ボタンが押されているか（継続）
    /// @param button ボタン
    /// @return 押されている場合true
    bool IsButtonPressed(GamepadButton button) const noexcept;

    /// @brief ボタンが押された瞬間か（1フレームのみ）
    /// @param button ボタン
    /// @return 押された瞬間の場合true
    bool IsButtonDown(GamepadButton button) const noexcept;

    /// @brief ボタンが離された瞬間か（1フレームのみ）
    /// @param button ボタン
    /// @return 離された瞬間の場合true
    bool IsButtonUp(GamepadButton button) const noexcept;

    /// @brief 左スティックのX軸値を取得（-1.0 ~ 1.0）
    /// @return X軸値
    float GetLeftStickX() const noexcept { return leftStickX_; }

    /// @brief 左スティックのY軸値を取得（-1.0 ~ 1.0）
    /// @return Y軸値
    float GetLeftStickY() const noexcept { return leftStickY_; }

    /// @brief 右スティックのX軸値を取得（-1.0 ~ 1.0）
    /// @return X軸値
    float GetRightStickX() const noexcept { return rightStickX_; }

    /// @brief 右スティックのY軸値を取得（-1.0 ~ 1.0）
    /// @return Y軸値
    float GetRightStickY() const noexcept { return rightStickY_; }

    /// @brief 左トリガー値を取得（0.0 ~ 1.0）
    /// @return トリガー値
    float GetLeftTrigger() const noexcept { return leftTrigger_; }

    /// @brief 右トリガー値を取得（0.0 ~ 1.0）
    /// @return トリガー値
    float GetRightTrigger() const noexcept { return rightTrigger_; }

    /// @brief デッドゾーン閾値を設定（スティック用）
    /// @param threshold 閾値（0.0 ~ 1.0）
    void SetDeadZone(float threshold) noexcept { deadZone_ = threshold; }

    /// @brief デッドゾーン閾値を取得
    /// @return 閾値
    float GetDeadZone() const noexcept { return deadZone_; }

private:
    /// @brief デッドゾーン処理を適用
    /// @param value 入力値
    /// @param deadZone デッドゾーン閾値
    /// @return 処理後の値
    static float ApplyDeadZone(float value, float deadZone) noexcept;

private:
    DWORD userIndex_;               // ユーザーインデックス (0-3)
    bool connected_;                // 接続状態

    // ボタン状態
    uint16_t currentButtons_;       // 現在のボタン状態
    uint16_t previousButtons_;      // 前フレームのボタン状態

    // スティック状態
    float leftStickX_;
    float leftStickY_;
    float rightStickX_;
    float rightStickY_;

    // トリガー状態
    float leftTrigger_;
    float rightTrigger_;

    // デッドゾーン
    float deadZone_;                // デッドゾーン閾値（0.0 ~ 1.0）
};

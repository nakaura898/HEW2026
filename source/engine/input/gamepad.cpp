#include "gamepad.h"
#include <cmath>
#include <algorithm>

Gamepad::Gamepad(DWORD userIndex) noexcept
    : userIndex_(userIndex)
    , connected_(false)
    , currentButtons_(0)
    , previousButtons_(0)
    , leftStickX_(0.0f)
    , leftStickY_(0.0f)
    , rightStickX_(0.0f)
    , rightStickY_(0.0f)
    , leftTrigger_(0.0f)
    , rightTrigger_(0.0f)
    , deadZone_(0.2f)  // デフォルト20%
{
}

void Gamepad::Update() noexcept
{
    // 前フレームの状態を保存
    previousButtons_ = currentButtons_;

    // XInput状態を取得
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));

    DWORD result = XInputGetState(userIndex_, &state);

    if (result == ERROR_SUCCESS) {
        // 接続されている
        connected_ = true;

        // ボタン状態を更新
        currentButtons_ = state.Gamepad.wButtons;

        // 左スティック（-32768 ~ 32767 → -1.0 ~ 1.0）
        float lx = static_cast<float>(state.Gamepad.sThumbLX) / 32767.0f;
        float ly = static_cast<float>(state.Gamepad.sThumbLY) / 32767.0f;
        leftStickX_ = ApplyDeadZone(lx, deadZone_);
        leftStickY_ = ApplyDeadZone(ly, deadZone_);

        // 右スティック
        float rx = static_cast<float>(state.Gamepad.sThumbRX) / 32767.0f;
        float ry = static_cast<float>(state.Gamepad.sThumbRY) / 32767.0f;
        rightStickX_ = ApplyDeadZone(rx, deadZone_);
        rightStickY_ = ApplyDeadZone(ry, deadZone_);

        // トリガー（0 ~ 255 → 0.0 ~ 1.0）
        leftTrigger_ = static_cast<float>(state.Gamepad.bLeftTrigger) / 255.0f;
        rightTrigger_ = static_cast<float>(state.Gamepad.bRightTrigger) / 255.0f;
    } else {
        // 接続されていない
        connected_ = false;
        currentButtons_ = 0;
        leftStickX_ = 0.0f;
        leftStickY_ = 0.0f;
        rightStickX_ = 0.0f;
        rightStickY_ = 0.0f;
        leftTrigger_ = 0.0f;
        rightTrigger_ = 0.0f;
    }
}

bool Gamepad::IsButtonPressed(GamepadButton button) const noexcept
{
    if (!connected_) {
        return false;
    }
    return (currentButtons_ & static_cast<uint16_t>(button)) != 0;
}

bool Gamepad::IsButtonDown(GamepadButton button) const noexcept
{
    if (!connected_) {
        return false;
    }
    uint16_t buttonMask = static_cast<uint16_t>(button);
    bool currentPressed = (currentButtons_ & buttonMask) != 0;
    bool previousPressed = (previousButtons_ & buttonMask) != 0;
    return currentPressed && !previousPressed;
}

bool Gamepad::IsButtonUp(GamepadButton button) const noexcept
{
    if (!connected_) {
        return false;
    }
    uint16_t buttonMask = static_cast<uint16_t>(button);
    bool currentPressed = (currentButtons_ & buttonMask) != 0;
    bool previousPressed = (previousButtons_ & buttonMask) != 0;
    return !currentPressed && previousPressed;
}

float Gamepad::ApplyDeadZone(float value, float deadZone) noexcept
{
    // 絶対値がデッドゾーン以下なら0
    if (std::abs(value) < deadZone) {
        return 0.0f;
    }

    // デッドゾーンを考慮した値にリマップ
    // (value - deadZone) / (1.0 - deadZone)
    float sign = (value > 0.0f) ? 1.0f : -1.0f;
    float absValue = std::abs(value);
    float remapped = (absValue - deadZone) / (1.0f - deadZone);

    return sign * std::clamp(remapped, 0.0f, 1.0f);
}

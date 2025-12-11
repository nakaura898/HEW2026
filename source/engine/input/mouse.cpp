#include "mouse.h"

bool Mouse::IsButtonPressed(MouseButton button) const noexcept
{
    const int index = static_cast<int>(button);
    if (index < 0 || index >= static_cast<int>(MouseButton::ButtonCount)) {
        return false;
    }
    return buttons_[index].pressed;
}

bool Mouse::IsButtonDown(MouseButton button) const noexcept
{
    const int index = static_cast<int>(button);
    if (index < 0 || index >= static_cast<int>(MouseButton::ButtonCount)) {
        return false;
    }
    return buttons_[index].down;
}

bool Mouse::IsButtonUp(MouseButton button) const noexcept
{
    const int index = static_cast<int>(button);
    if (index < 0 || index >= static_cast<int>(MouseButton::ButtonCount)) {
        return false;
    }
    return buttons_[index].up;
}

void Mouse::Update() noexcept
{
    // デルタを計算
    deltaX_ = x_ - prevX_;
    deltaY_ = y_ - prevY_;

    // 前フレームの座標を保存
    prevX_ = x_;
    prevY_ = y_;

    // ホイールデルタをリセット（フレーム毎にリセット）
    wheelDelta_ = 0.0f;

    // downとupフラグは1フレームのみ有効
    for (auto& button : buttons_) {
        button.down = false;
        button.up = false;
    }
}

void Mouse::SetPosition(int x, int y) noexcept
{
    x_ = x;
    y_ = y;
}

void Mouse::OnButtonDown(MouseButton button) noexcept
{
    const int index = static_cast<int>(button);
    if (index < 0 || index >= static_cast<int>(MouseButton::ButtonCount)) {
        return;
    }

    auto& btn = buttons_[index];
    if (!btn.pressed) {
        btn.pressed = true;
        btn.down = true;
    }
}

void Mouse::OnButtonUp(MouseButton button) noexcept
{
    const int index = static_cast<int>(button);
    if (index < 0 || index >= static_cast<int>(MouseButton::ButtonCount)) {
        return;
    }

    auto& btn = buttons_[index];
    btn.pressed = false;
    btn.up = true;
}

void Mouse::OnWheel(float delta) noexcept
{
    wheelDelta_ += delta;
}

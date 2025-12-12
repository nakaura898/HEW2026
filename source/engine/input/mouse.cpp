#include "mouse.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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

void Mouse::Update(HWND hwnd) noexcept
{
    // 前フレームの座標を保存
    prevX_ = x_;
    prevY_ = y_;

    // Win32 APIで直接カーソル位置を取得
    POINT pt;
    if (::GetCursorPos(&pt)) {
        // ウィンドウハンドルが指定されていなければアクティブウィンドウを使用
        HWND targetHwnd = hwnd ? hwnd : ::GetActiveWindow();
        if (targetHwnd && ::ScreenToClient(targetHwnd, &pt)) {
            x_ = pt.x;
            y_ = pt.y;
        }
    }

    // デルタを計算
    deltaX_ = x_ - prevX_;
    deltaY_ = y_ - prevY_;

    // マウスボタン状態を直接取得
    bool leftPressed = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool rightPressed = (::GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    bool middlePressed = (::GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

    // 左ボタン
    {
        auto& btn = buttons_[static_cast<int>(MouseButton::Left)];
        bool wasPressed = btn.pressed;
        btn.pressed = leftPressed;
        btn.down = leftPressed && !wasPressed;
        btn.up = !leftPressed && wasPressed;
    }
    // 右ボタン
    {
        auto& btn = buttons_[static_cast<int>(MouseButton::Right)];
        bool wasPressed = btn.pressed;
        btn.pressed = rightPressed;
        btn.down = rightPressed && !wasPressed;
        btn.up = !rightPressed && wasPressed;
    }
    // 中ボタン
    {
        auto& btn = buttons_[static_cast<int>(MouseButton::Middle)];
        bool wasPressed = btn.pressed;
        btn.pressed = middlePressed;
        btn.down = middlePressed && !wasPressed;
        btn.up = !middlePressed && wasPressed;
    }

    // ホイールデルタをリセット（フレーム毎にリセット）
    wheelDelta_ = 0.0f;
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

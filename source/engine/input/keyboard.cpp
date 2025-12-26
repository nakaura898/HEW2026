#include "keyboard.h"
#include <Windows.h>

bool Keyboard::IsKeyPressed(Key key) const noexcept
{
    const int index = static_cast<int>(key);
    if (index < 0 || index >= static_cast<int>(Key::KeyCount)) {
        return false;
    }
    return keys_[index].pressed;
}

bool Keyboard::IsKeyDown(Key key) const noexcept
{
    const int index = static_cast<int>(key);
    if (index < 0 || index >= static_cast<int>(Key::KeyCount)) {
        return false;
    }
    return keys_[index].down;
}

bool Keyboard::IsKeyUp(Key key) const noexcept
{
    const int index = static_cast<int>(key);
    if (index < 0 || index >= static_cast<int>(Key::KeyCount)) {
        return false;
    }
    return keys_[index].up;
}

float Keyboard::GetKeyHoldTime(Key key) const noexcept
{
    const int index = static_cast<int>(key);
    if (index < 0 || index >= static_cast<int>(Key::KeyCount)) {
        return 0.0f;
    }
    return keys_[index].holdTime;
}

bool Keyboard::IsShiftPressed() const noexcept
{
    return IsKeyPressed(Key::LeftShift) || IsKeyPressed(Key::RightShift);
}

bool Keyboard::IsControlPressed() const noexcept
{
    return IsKeyPressed(Key::LeftControl) || IsKeyPressed(Key::RightControl);
}

bool Keyboard::IsAltPressed() const noexcept
{
    return IsKeyPressed(Key::LeftAlt) || IsKeyPressed(Key::RightAlt);
}

void Keyboard::Update(float deltaTime) noexcept
{
    // GetAsyncKeyStateでキー状態をポーリング（Windowメッセージ不要）
    // NOTE: この方式を使用する場合、OnKeyDown/OnKeyUpは呼び出さないこと
    //       両方を混在させると状態が不整合になる可能性がある
    for (int i = 0; i < static_cast<int>(Key::KeyCount); ++i) {
        bool currentlyPressed = (GetAsyncKeyState(i) & 0x8000) != 0;
        auto& key = keys_[i];

        key.down = currentlyPressed && !key.pressed;
        key.up = !currentlyPressed && key.pressed;

        if (currentlyPressed) {
            key.holdTime = key.pressed ? key.holdTime + deltaTime : 0.0f;
        } else {
            key.holdTime = 0.0f;
        }

        key.pressed = currentlyPressed;
    }
}

// NOTE: 以下のOnKeyDown/OnKeyUpはWM_KEYDOWN/WM_KEYUPメッセージ用
//       Update()のポーリング方式を使用する場合は呼び出さないこと
void Keyboard::OnKeyDown(int virtualKey) noexcept
{
    if (virtualKey < 0 || virtualKey >= static_cast<int>(Key::KeyCount)) {
        return;
    }

    auto& key = keys_[virtualKey];

    // 既に押されている場合はリピート入力なのでdownフラグは立てない
    if (!key.pressed) {
        key.pressed = true;
        key.down = true;
        key.holdTime = 0.0f;
    }
}

void Keyboard::OnKeyUp(int virtualKey) noexcept
{
    if (virtualKey < 0 || virtualKey >= static_cast<int>(Key::KeyCount)) {
        return;
    }

    auto& key = keys_[virtualKey];
    key.pressed = false;
    key.up = true;
    key.holdTime = 0.0f;
}

#include "gamepad_manager.h"

GamepadManager::GamepadManager() noexcept
{
    // 4台のゲームパッドを初期化
    for (int i = 0; i < MaxGamepads; ++i) {
        gamepads_[i] = std::make_unique<Gamepad>(i);
    }
}

void GamepadManager::Update() noexcept
{
    // 全ゲームパッドの状態を更新
    for (auto& gamepad : gamepads_) {
        if (gamepad) {
            gamepad->Update();
        }
    }
}

bool GamepadManager::IsConnected(int index) const noexcept
{
    if (index < 0 || index >= MaxGamepads) {
        return false;
    }
    return gamepads_[index] && gamepads_[index]->IsConnected();
}

Gamepad& GamepadManager::GetGamepad(int index) noexcept
{
    // 範囲外アクセスの場合は0番を返す（安全策）
    if (index < 0 || index >= MaxGamepads) {
        return *gamepads_[0];
    }
    return *gamepads_[index];
}

const Gamepad& GamepadManager::GetGamepad(int index) const noexcept
{
    // 範囲外アクセスの場合は0番を返す（安全策）
    if (index < 0 || index >= MaxGamepads) {
        return *gamepads_[0];
    }
    return *gamepads_[index];
}

int GamepadManager::GetConnectedCount() const noexcept
{
    int count = 0;
    for (const auto& gamepad : gamepads_) {
        if (gamepad && gamepad->IsConnected()) {
            ++count;
        }
    }
    return count;
}

#include "input_manager.h"

// 静的メンバの定義
InputManager* InputManager::instance_ = nullptr;

InputManager::InputManager() noexcept
    : keyboard_(std::make_unique<Keyboard>())
    , mouse_(std::make_unique<Mouse>())
    , gamepadManager_(nullptr)  
{
}

bool InputManager::Initialize() noexcept
{
    if (instance_ != nullptr) {
        // 既に初期化済み
        return false;
    }

    instance_ = new (std::nothrow) InputManager();
    return instance_ != nullptr;
}


void InputManager::Uninit() noexcept
{
    if (instance_ != nullptr) {
        delete instance_;
        instance_ = nullptr;
    }
}

void InputManager::Update(float deltaTime) noexcept
{
    if (keyboard_) {
        keyboard_->Update(deltaTime);
    }
    if (mouse_) {
        mouse_->Update();
    }
   //if (gamepadManager_) {
   //     gamepadManager_->Update();
   // }
}

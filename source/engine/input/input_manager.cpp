#include "input_manager.h"

InputManager::InputManager() noexcept
    : keyboard_(std::make_unique<Keyboard>())
    , mouse_(std::make_unique<Mouse>())
    , gamepadManager_(nullptr)
{
}

void InputManager::Create()
{
    if (!instance_) {
        instance_ = std::unique_ptr<InputManager>(new InputManager());
    }
}

void InputManager::Destroy()
{
    instance_.reset();
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

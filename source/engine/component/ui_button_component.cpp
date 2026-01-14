//----------------------------------------------------------------------------
//! @file   ui_button_component.cpp
//! @brief  UIボタンコンポーネント実装
//----------------------------------------------------------------------------
#include "ui_button_component.h"
#include "engine/component/game_object.h"
#include "engine/component/transform.h"
#include "engine/input/input_manager.h"
#include "engine/input/key.h"

//----------------------------------------------------------------------------
void UIButtonComponent::Update([[maybe_unused]] float deltaTime)
{
    if (!IsEnabled()) {
        return;
    }

    auto& input = InputManager::Get();
    State previousState = state_;

    // マウスがボタン範囲内かチェック
    if (IsMouseOver()) {
        // 左クリックが押されているか
        if (input.GetMouse().IsButtonPressed(MouseButton::Left)) {
            state_ = State::Pressed;
            currentColor_ = pressColor_;
        } else {
            // クリックが離された瞬間にコールバック実行
            if (previousState == State::Pressed &&
                input.GetMouse().IsButtonUp(MouseButton::Left)) {
                if (onClick_) {
                    onClick_();
                }
            }
            state_ = State::Hover;
            currentColor_ = hoverColor_;
        }
    } else {
        state_ = State::Normal;
        currentColor_ = normalColor_;
    }
}

//----------------------------------------------------------------------------
bool UIButtonComponent::IsMouseOver() const
{
    auto& input = InputManager::Get();
    Vector2 mouse = input.GetMouse().GetPosition();
    Vector2 pos = GetPosition();

    // ボタンの範囲を計算
    float left = pos.x - size_.x * 0.5f;
    float right = pos.x + size_.x * 0.5f;
    float top = pos.y - size_.y * 0.5f;
    float bottom = pos.y + size_.y * 0.5f;

    // マウスが範囲内か
    bool inX = (mouse.x >= left) && (mouse.x <= right);
    bool inY = (mouse.y >= top) && (mouse.y <= bottom);

    return inX && inY;
}

//----------------------------------------------------------------------------
Vector2 UIButtonComponent::GetPosition() const
{
    GameObject* owner = GetOwner();
    if (!owner) {
        return Vector2::Zero;
    }

    Transform* transform = owner->GetComponent<Transform>();
    if (!transform) {
        return Vector2::Zero;
    }

    return transform->GetPosition();
}

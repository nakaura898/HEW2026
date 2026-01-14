//----------------------------------------------------------------------------
//! @file   ui_gauge_component.cpp
//! @brief  UIゲージコンポーネント実装
//----------------------------------------------------------------------------
#include "ui_gauge_component.h"
#include "engine/component/game_object.h"
#include "engine/component/transform.h"
#include "engine/debug/debug_draw.h"
#include <algorithm>

//----------------------------------------------------------------------------
void UIGaugeComponent::Update([[maybe_unused]] float deltaTime)
{
    // 現時点ではアニメーション等の更新処理なし
    // 将来的にスムーズな値変化などを実装可能
}

//----------------------------------------------------------------------------
void UIGaugeComponent::Render()
{
    if (!IsEnabled()) {
        return;
    }

    Vector2 pos = GetPosition();

    // 背景を描画（全体サイズ）
    DEBUG_RECT_FILL(pos, size_, bgColor_);

    // ゲージ部分を描画（valueに応じて幅を変える）
    if (value_ > 0.0f) {
        // ゲージの幅 = 全体幅 × value
        float fillWidth = size_.x * value_;

        // ゲージは左詰めなので、中心位置を調整
        // 全体:  [==========]  中心 = pos.x
        // 50%:   [=====     ]  中心 = pos.x - size_.x/4
        float offsetX = (size_.x - fillWidth) * 0.5f;
        Vector2 fillPos = Vector2(pos.x - offsetX, pos.y);
        Vector2 fillSize = Vector2(fillWidth, size_.y);

        DEBUG_RECT_FILL(fillPos, fillSize, fillColor_);
    }
}

//----------------------------------------------------------------------------
void UIGaugeComponent::SetValue(float value)
{
    value_ = std::clamp(value, 0.0f, 1.0f);
}

//----------------------------------------------------------------------------
void UIGaugeComponent::AddValue(float delta)
{
    SetValue(value_ + delta);
}

//----------------------------------------------------------------------------
Vector2 UIGaugeComponent::GetPosition() const
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

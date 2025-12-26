#include "ui_gauge.h"
#include "engine/debug/debug_draw.h"

UIGauge::UIGauge(const Vector2& pos, const Vector2& size)
    : position_(pos)
    , size_(size)
{
}

void UIGauge::Render()
{
    // 背景を描画（全体サイズ）
    DEBUG_RECT_FILL(position_, size_, bgColor_);

    // ゲージ部分を描画（ratioに応じて幅を変える）
    if (ratio_ > 0.0f) {
        // ゲージの幅 = 全体幅 × ratio
        float fillWidth = size_.x * ratio_;

        // ゲージは左詰めなので、中心位置を調整
        // 全体:  [==========]  中心 = position_.x
        // 50%:   [=====     ]  中心 = position_.x - size_.x/4
        float offsetX = (size_.x - fillWidth) * 0.5f;
        Vector2 fillPos = Vector2(position_.x - offsetX, position_.y);
        Vector2 fillSize = Vector2(fillWidth, size_.y);

        DEBUG_RECT_FILL(fillPos, fillSize, fillColor_);
    }
}
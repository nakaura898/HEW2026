//----------------------------------------------------------------------------
//! @file   radial_menu.cpp
//! @brief  ラジアルメニュー実装
//----------------------------------------------------------------------------
#include "radial_menu.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/debug/debug_draw.h"
#include "engine/math/color.h"
#include "common/logging/logging.h"
#include <cmath>

// 定数
static constexpr float kPi = 3.14159265358979f;
static constexpr float kTwoPi = 2.0f * kPi;

//----------------------------------------------------------------------------
RadialMenu& RadialMenu::Get()
{
    static RadialMenu instance;
    return instance;
}

//----------------------------------------------------------------------------
void RadialMenu::Initialize()
{
    // メニュー項目を設定（3分割: Basic, Friends, Love）
    items_.clear();
    items_.push_back({ "Basic",   BondType::Basic,   Color(1.0f, 1.0f, 1.0f, 1.0f) });   // 白
    items_.push_back({ "Friends", BondType::Friends, Color(0.3f, 1.0f, 0.3f, 1.0f) });   // 緑
    items_.push_back({ "Love",    BondType::Love,    Color(1.0f, 0.5f, 0.7f, 1.0f) });   // ピンク
}

//----------------------------------------------------------------------------
void RadialMenu::Open(const Vector2& centerPos)
{
    centerPos_ = centerPos;
    hoveredIndex_ = -1;
    isOpen_ = true;
    LOG_INFO("[RadialMenu] Opened at (" + std::to_string(static_cast<int>(centerPos.x)) +
             ", " + std::to_string(static_cast<int>(centerPos.y)) + ")");
}

//----------------------------------------------------------------------------
void RadialMenu::Close()
{
    isOpen_ = false;
    hoveredIndex_ = -1;
    LOG_INFO("[RadialMenu] Closed");
}

//----------------------------------------------------------------------------
void RadialMenu::Update(const Vector2& cursorPos)
{
    if (!isOpen_) return;
    UpdateHoveredItem(cursorPos);
}

//----------------------------------------------------------------------------
void RadialMenu::Render(SpriteBatch& /*spriteBatch*/)
{
#ifdef _DEBUG
    if (!isOpen_) return;

    DebugDraw& debug = DebugDraw::Get();
    int numItems = static_cast<int>(items_.size());
    if (numItems == 0) return;

    float sectorAngle = kTwoPi / static_cast<float>(numItems);

    // 背景の暗い円
    debug.DrawCircleFilled(centerPos_, radius_ + 10.0f, Color(0.1f, 0.1f, 0.1f, 0.7f));

    // 各セクターを描画
    for (int i = 0; i < numItems; ++i) {
        float startAngle = static_cast<float>(i) * sectorAngle - kPi / 2.0f - sectorAngle / 2.0f;
        float endAngle = startAngle + sectorAngle;
        float midAngle = (startAngle + endAngle) / 2.0f;

        // セクターの色
        Color baseColor = items_[i].color;

        // セクター境界線（中心から外側へ）
        Vector2 lineEnd(
            centerPos_.x + std::cos(startAngle) * radius_,
            centerPos_.y + std::sin(startAngle) * radius_
        );
        debug.DrawLine(centerPos_, lineEnd, Color(0.9f, 0.9f, 0.9f, 0.9f), 3.0f);

        // セクター内に大きな色付き円を描画（扇形の代わり）
        float iconDist = (deadZone_ + radius_) / 2.0f;
        Vector2 iconPos(
            centerPos_.x + std::cos(midAngle) * iconDist,
            centerPos_.y + std::sin(midAngle) * iconDist
        );

        // ホバー時は大きくする
        float dotRadius = (i == hoveredIndex_) ? 80.0f : 60.0f;

        // ホバー時は明るく、通常時は暗め
        float alpha = (i == hoveredIndex_) ? 1.0f : 0.5f;
        Color drawColor(baseColor.x, baseColor.y, baseColor.z, alpha);
        debug.DrawCircleFilled(iconPos, dotRadius, drawColor);

        // 枠線（ホバー時はその縁タイプの色で太く）
        Color outlineColor = (i == hoveredIndex_)
            ? Color(baseColor.x, baseColor.y, baseColor.z, 1.0f)  // 縁タイプの色
            : Color(0.3f, 0.3f, 0.3f, 0.6f);
        float outlineWidth = (i == hoveredIndex_) ? 8.0f : 3.0f;
        debug.DrawCircleOutline(iconPos, dotRadius, outlineColor, 32, outlineWidth);
    }

    // 中心円（デッドゾーン）
    debug.DrawCircleFilled(centerPos_, deadZone_, Color(0.15f, 0.15f, 0.15f, 0.9f));
    debug.DrawCircleOutline(centerPos_, deadZone_, Color(0.6f, 0.6f, 0.6f, 0.8f), 32, 3.0f);

    // 外周円
    debug.DrawCircleOutline(centerPos_, radius_, Color(0.9f, 0.9f, 0.9f, 0.9f), 64, 4.0f);
#endif
}

//----------------------------------------------------------------------------
BondType RadialMenu::GetHoveredBondType() const
{
    if (hoveredIndex_ >= 0 && hoveredIndex_ < static_cast<int>(items_.size())) {
        return items_[hoveredIndex_].bondType;
    }
    return BondType::Basic;
}

//----------------------------------------------------------------------------
BondType RadialMenu::Confirm()
{
    BondType selected = GetHoveredBondType();

    if (onSelected_) {
        onSelected_(selected);
    }

    LOG_INFO("[RadialMenu] Confirmed: " + std::to_string(static_cast<int>(selected)));
    Close();
    return selected;
}

//----------------------------------------------------------------------------
void RadialMenu::UpdateHoveredItem(const Vector2& cursorPos)
{
    Vector2 delta = cursorPos - centerPos_;
    float distance = delta.Length();

    // デッドゾーン内なら選択なし
    if (distance < deadZone_) {
        hoveredIndex_ = -1;
        return;
    }

    // 角度を計算（-π〜π）
    float angle = std::atan2(delta.y, delta.x);

    // 角度を0〜2πに正規化し、上方向を0として調整
    angle += kPi / 2.0f;  // 上方向を基準に
    if (angle < 0.0f) angle += kTwoPi;
    if (angle >= kTwoPi) angle -= kTwoPi;

    // セクターを計算
    int numItems = static_cast<int>(items_.size());
    float sectorAngle = kTwoPi / static_cast<float>(numItems);

    // 各セクターの中心を基準にするためオフセット
    angle += sectorAngle / 2.0f;
    if (angle >= kTwoPi) angle -= kTwoPi;

    hoveredIndex_ = static_cast<int>(angle / sectorAngle);
    if (hoveredIndex_ >= numItems) {
        hoveredIndex_ = 0;
    }
}

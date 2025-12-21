//----------------------------------------------------------------------------
//! @file   formation.cpp
//! @brief  Formation - グループ内の個体配置を管理
//----------------------------------------------------------------------------
#include "formation.h"
#include "game/entities/individual.h"
#include <cmath>

// 円周率
namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = kPi * 2.0f;
}

//----------------------------------------------------------------------------
Formation::Formation()
{
}

//----------------------------------------------------------------------------
Formation::~Formation()
{
}

//----------------------------------------------------------------------------
void Formation::Initialize(const std::vector<Individual*>& individuals, const Vector2& center)
{
    center_ = center;

    // スロット生成
    GenerateSlots(individuals.size());

    // 個体をスロットに割り当て
    for (size_t i = 0; i < individuals.size() && i < slots_.size(); ++i) {
        slots_[i].owner = individuals[i];
    }
}

//----------------------------------------------------------------------------
void Formation::Rebuild(const std::vector<Individual*>& aliveIndividuals)
{
    // 生存個体数でスロットを再生成
    GenerateSlots(aliveIndividuals.size());

    // 個体をスロットに再割り当て
    for (size_t i = 0; i < aliveIndividuals.size() && i < slots_.size(); ++i) {
        slots_[i].owner = aliveIndividuals[i];
    }
}

//----------------------------------------------------------------------------
void Formation::Update(const Vector2& targetPosition, float speed, float dt)
{
    // 目標位置に向かって中心を移動
    Vector2 diff = targetPosition - center_;
    float distance = diff.Length();

    if (distance > 0.001f) {
        // 正規化
        Vector2 direction = diff / distance;

        // 移動量計算
        float moveAmount = speed * dt;

        if (moveAmount >= distance) {
            // 目標到達
            center_ = targetPosition;
        } else {
            // 移動
            center_ += direction * moveAmount;
        }
    }
}

//----------------------------------------------------------------------------
Vector2 Formation::GetSlotPosition(const Individual* individual) const
{
    // 個体のスロットを検索
    for (const FormationSlot& slot : slots_) {
        if (slot.owner == individual) {
            return center_ + slot.offset;
        }
    }

    // 見つからなければ中心位置を返す
    return center_;
}

//----------------------------------------------------------------------------
bool Formation::HasSlot(const Individual* individual) const
{
    for (const FormationSlot& slot : slots_) {
        if (slot.owner == individual) {
            return true;
        }
    }
    return false;
}

//----------------------------------------------------------------------------
void Formation::GenerateSlots(size_t count)
{
    slots_.clear();

    if (count == 0) {
        return;
    }

    slots_.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        FormationSlot slot;

        switch (formationType_) {
        case FormationType::Circle:
            slot.offset = CalculateCircleOffset(i, count);
            break;
        case FormationType::Line:
            slot.offset = CalculateLineOffset(i, count);
            break;
        case FormationType::Wedge:
            slot.offset = CalculateWedgeOffset(i, count);
            break;
        }

        slot.owner = nullptr;
        slots_.push_back(slot);
    }
}

//----------------------------------------------------------------------------
Vector2 Formation::CalculateCircleOffset(size_t index, size_t total) const
{
    if (total == 1) {
        // 1体なら中心
        return Vector2::Zero;
    }

    // 円周上に等間隔配置
    float angle = (static_cast<float>(index) / static_cast<float>(total)) * kTwoPi;

    // 半径は個体数に応じて調整
    float radius = spacing_ * static_cast<float>(total) / kTwoPi;
    // 最小半径を設定
    if (radius < spacing_) {
        radius = spacing_;
    }

    return Vector2(
        std::cos(angle) * radius,
        std::sin(angle) * radius
    );
}

//----------------------------------------------------------------------------
Vector2 Formation::CalculateLineOffset(size_t index, size_t total) const
{
    if (total == 1) {
        return Vector2::Zero;
    }

    // 中央揃えで横一列
    float totalWidth = spacing_ * static_cast<float>(total - 1);
    float startX = -totalWidth * 0.5f;
    float x = startX + spacing_ * static_cast<float>(index);

    return Vector2(x, 0.0f);
}

//----------------------------------------------------------------------------
Vector2 Formation::CalculateWedgeOffset(size_t index, size_t total) const
{
    if (total == 1) {
        return Vector2::Zero;
    }

    // V字形（先頭が前方、後方に広がる）
    // index 0 が先頭
    if (index == 0) {
        return Vector2(0.0f, -spacing_ * 0.5f);  // 少し前
    }

    // 左右交互に配置
    size_t row = (index + 1) / 2;  // 1,2 -> 1, 3,4 -> 2, ...
    bool isLeft = (index % 2 == 1);

    float x = spacing_ * static_cast<float>(row) * (isLeft ? -1.0f : 1.0f);
    float y = spacing_ * static_cast<float>(row);  // 後方に下がる

    return Vector2(x, y);
}

//----------------------------------------------------------------------------
//! @file   bondable_entity.cpp
//! @brief  BondableEntity ヘルパー関数実装
//----------------------------------------------------------------------------
#include "bondable_entity.h"
#include "game/entities/player.h"
#include "game/entities/group.h"

namespace BondableHelper
{

//----------------------------------------------------------------------------
std::string GetId(const BondableEntity& entity)
{
    return std::visit([](auto* ptr) -> std::string {
        if (ptr) return ptr->GetId();
        return "";
    }, entity);
}

//----------------------------------------------------------------------------
Vector2 GetPosition(const BondableEntity& entity)
{
    return std::visit([](auto* ptr) -> Vector2 {
        if (ptr) return ptr->GetPosition();
        return Vector2::Zero;
    }, entity);
}

//----------------------------------------------------------------------------
float GetThreat(const BondableEntity& entity)
{
    return std::visit([](auto* ptr) -> float {
        if (ptr) return ptr->GetThreat();
        return 0.0f;
    }, entity);
}

//----------------------------------------------------------------------------
bool IsPlayer(const BondableEntity& entity)
{
    return std::holds_alternative<Player*>(entity);
}

//----------------------------------------------------------------------------
bool IsGroup(const BondableEntity& entity)
{
    return std::holds_alternative<Group*>(entity);
}

//----------------------------------------------------------------------------
Player* AsPlayer(const BondableEntity& entity)
{
    if (std::holds_alternative<Player*>(entity)) {
        return std::get<Player*>(entity);
    }
    return nullptr;
}

//----------------------------------------------------------------------------
Group* AsGroup(const BondableEntity& entity)
{
    if (std::holds_alternative<Group*>(entity)) {
        return std::get<Group*>(entity);
    }
    return nullptr;
}

//----------------------------------------------------------------------------
bool IsSame(const BondableEntity& a, const BondableEntity& b)
{
    if (a.index() != b.index()) return false;

    return std::visit([&b](auto* ptrA) -> bool {
        using T = std::decay_t<decltype(ptrA)>;
        if (std::holds_alternative<T>(b)) {
            return ptrA == std::get<T>(b);
        }
        return false;
    }, a);
}

//----------------------------------------------------------------------------
bool IsNull(const BondableEntity& entity)
{
    return std::visit([](auto* ptr) -> bool {
        return ptr == nullptr;
    }, entity);
}

} // namespace BondableHelper

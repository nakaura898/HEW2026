//----------------------------------------------------------------------------
//! @file   faction.cpp
//! @brief  Faction実装
//----------------------------------------------------------------------------
#include "faction.h"
#include "game/entities/group.h"
#include "game/entities/player.h"
#include <algorithm>

//----------------------------------------------------------------------------
void Faction::AddMember(BondableEntity entity)
{
    // 重複チェック
    for (const BondableEntity& existing : members_) {
        if (BondableHelper::GetId(existing) == BondableHelper::GetId(entity)) {
            return;
        }
    }
    members_.push_back(entity);
}

//----------------------------------------------------------------------------
void Faction::Clear()
{
    members_.clear();
}

//----------------------------------------------------------------------------
bool Faction::Contains(BondableEntity entity) const
{
    std::string targetId = BondableHelper::GetId(entity);
    for (const BondableEntity& member : members_) {
        if (BondableHelper::GetId(member) == targetId) {
            return true;
        }
    }
    return false;
}

//----------------------------------------------------------------------------
bool Faction::HasPlayer() const
{
    for (const BondableEntity& member : members_) {
        if (BondableHelper::IsPlayer(member)) {
            return true;
        }
    }
    return false;
}

//----------------------------------------------------------------------------
float Faction::GetTotalThreat(float playerBonus) const
{
    float total = 0.0f;

    for (const BondableEntity& member : members_) {
        total += BondableHelper::GetThreat(member);
    }

    // プレイヤー所属時はボーナス適用
    if (HasPlayer()) {
        total *= playerBonus;
    }

    return total;
}

//----------------------------------------------------------------------------
std::vector<Group*> Faction::GetGroups() const
{
    std::vector<Group*> groups;

    for (const BondableEntity& member : members_) {
        Group* group = BondableHelper::AsGroup(member);
        if (group) {
            groups.push_back(group);
        }
    }

    return groups;
}

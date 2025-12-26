//----------------------------------------------------------------------------
//! @file   relationship_context.cpp
//! @brief  グローバル関係レジストリ実装
//----------------------------------------------------------------------------
#include "relationship_context.h"
#include "game/entities/individual.h"

//----------------------------------------------------------------------------
RelationshipContext& RelationshipContext::Get()
{
    static RelationshipContext instance;
    return instance;
}

//----------------------------------------------------------------------------
void RelationshipContext::RegisterAttack(Individual* attacker, Individual* target)
{
    if (!attacker || !target) return;

    // 既存の関係があれば解除
    UnregisterAttack(attacker);

    // 新しい関係を登録
    attackerToTarget_[attacker] = target;
    targetToAttackers_[target].insert(attacker);
}

//----------------------------------------------------------------------------
void RelationshipContext::RegisterAttackPlayer(Individual* attacker, Player* target)
{
    if (!attacker || !target) return;

    // 既存の関係があれば解除
    UnregisterAttack(attacker);

    // 新しい関係を登録
    attackerToPlayer_[attacker] = target;
    playerToAttackers_[target].insert(attacker);
}

//----------------------------------------------------------------------------
void RelationshipContext::UnregisterAttack(Individual* attacker)
{
    if (!attacker) return;

    // Individual対象の関係を解除
    auto itTarget = attackerToTarget_.find(attacker);
    if (itTarget != attackerToTarget_.end()) {
        Individual* target = itTarget->second;
        auto itAttackers = targetToAttackers_.find(target);
        if (itAttackers != targetToAttackers_.end()) {
            itAttackers->second.erase(attacker);
            if (itAttackers->second.empty()) {
                targetToAttackers_.erase(itAttackers);
            }
        }
        attackerToTarget_.erase(itTarget);
    }

    // Player対象の関係を解除
    auto itPlayer = attackerToPlayer_.find(attacker);
    if (itPlayer != attackerToPlayer_.end()) {
        Player* player = itPlayer->second;
        auto itAttackers = playerToAttackers_.find(player);
        if (itAttackers != playerToAttackers_.end()) {
            itAttackers->second.erase(attacker);
            if (itAttackers->second.empty()) {
                playerToAttackers_.erase(itAttackers);
            }
        }
        attackerToPlayer_.erase(itPlayer);
    }
}

//----------------------------------------------------------------------------
Individual* RelationshipContext::GetAttackTarget(const Individual* attacker) const
{
    auto it = attackerToTarget_.find(attacker);
    return (it != attackerToTarget_.end()) ? it->second : nullptr;
}

//----------------------------------------------------------------------------
Player* RelationshipContext::GetPlayerTarget(const Individual* attacker) const
{
    auto it = attackerToPlayer_.find(attacker);
    return (it != attackerToPlayer_.end()) ? it->second : nullptr;
}

//----------------------------------------------------------------------------
std::vector<Individual*> RelationshipContext::GetAttackers(const Individual* target) const
{
    std::vector<Individual*> result;
    auto it = targetToAttackers_.find(target);
    if (it != targetToAttackers_.end()) {
        result.reserve(it->second.size());
        for (Individual* attacker : it->second) {
            result.push_back(attacker);
        }
    }
    return result;
}

//----------------------------------------------------------------------------
bool RelationshipContext::IsUnderAttack(const Individual* target) const
{
    auto it = targetToAttackers_.find(target);
    return it != targetToAttackers_.end() && !it->second.empty();
}

//----------------------------------------------------------------------------
void RelationshipContext::Clear()
{
    attackerToTarget_.clear();
    attackerToPlayer_.clear();
    targetToAttackers_.clear();
    playerToAttackers_.clear();
}

//----------------------------------------------------------------------------
void RelationshipContext::RemoveDeadEntities()
{
    // 死亡した攻撃者を除去
    std::vector<const Individual*> deadAttackers;
    for (const auto& [attacker, target] : attackerToTarget_) {
        if (!attacker->IsAlive()) {
            deadAttackers.push_back(attacker);
        }
    }
    for (const auto& [attacker, player] : attackerToPlayer_) {
        if (!attacker->IsAlive()) {
            deadAttackers.push_back(attacker);
        }
    }
    for (const Individual* attacker : deadAttackers) {
        UnregisterAttack(const_cast<Individual*>(attacker));
    }

    // 死亡した対象のエントリを除去
    std::vector<const Individual*> deadTargets;
    for (const auto& [target, attackers] : targetToAttackers_) {
        if (!target->IsAlive()) {
            deadTargets.push_back(target);
        }
    }
    for (const Individual* target : deadTargets) {
        // この対象を攻撃している全員の関係を解除
        auto it = targetToAttackers_.find(target);
        if (it != targetToAttackers_.end()) {
            for (Individual* attacker : it->second) {
                attackerToTarget_.erase(attacker);
            }
            targetToAttackers_.erase(it);
        }
    }
}

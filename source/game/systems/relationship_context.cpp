//----------------------------------------------------------------------------
//! @file   relationship_context.cpp
//! @brief  グローバル関係レジストリ実装
//----------------------------------------------------------------------------
#include "relationship_context.h"
#include "game/entities/individual.h"
#include "engine/event/event_bus.h"
#include "game/systems/event/game_events.h"

//----------------------------------------------------------------------------
RelationshipContext& RelationshipContext::Get()
{
    assert(instance_ && "RelationshipContext::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void RelationshipContext::Create()
{
    if (!instance_) {
        instance_.reset(new RelationshipContext());
    }
}

//----------------------------------------------------------------------------
void RelationshipContext::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
void RelationshipContext::Initialize()
{
    diedSubscriptionId_ = EventBus::Get().Subscribe<IndividualDiedEvent>(
        [this](const IndividualDiedEvent& e) { OnIndividualDied(e); });
}

//----------------------------------------------------------------------------
void RelationshipContext::Shutdown()
{
    EventBus::Get().Unsubscribe<IndividualDiedEvent>(diedSubscriptionId_);
    Clear();
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
    // const_cast: 読み取り専用のfind操作なので安全
    auto it = attackerToTarget_.find(const_cast<Individual*>(attacker));
    return (it != attackerToTarget_.end()) ? it->second : nullptr;
}

//----------------------------------------------------------------------------
Player* RelationshipContext::GetPlayerTarget(const Individual* attacker) const
{
    // const_cast: 読み取り専用のfind操作なので安全
    auto it = attackerToPlayer_.find(const_cast<Individual*>(attacker));
    return (it != attackerToPlayer_.end()) ? it->second : nullptr;
}

//----------------------------------------------------------------------------
std::vector<Individual*> RelationshipContext::GetAttackers(const Individual* target) const
{
    std::vector<Individual*> result;
    // const_cast: 読み取り専用のfind操作なので安全
    auto it = targetToAttackers_.find(const_cast<Individual*>(target));
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
    // const_cast: 読み取り専用のfind操作なので安全
    auto it = targetToAttackers_.find(const_cast<Individual*>(target));
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
void RelationshipContext::OnIndividualDied(const IndividualDiedEvent& event)
{
    if (!event.individual) return;
    RemoveIndividual(event.individual);
}

//----------------------------------------------------------------------------
void RelationshipContext::RemoveIndividual(Individual* individual)
{
    if (!individual) return;

    // この個体が攻撃者として登録されていれば解除
    UnregisterAttack(individual);

    // この個体が攻撃対象として登録されていれば、攻撃者側の関係も解除
    auto it = targetToAttackers_.find(individual);
    if (it != targetToAttackers_.end()) {
        // この対象を攻撃している全員の攻撃関係を解除
        for (Individual* attacker : it->second) {
            attackerToTarget_.erase(attacker);
        }
        targetToAttackers_.erase(it);
    }
}

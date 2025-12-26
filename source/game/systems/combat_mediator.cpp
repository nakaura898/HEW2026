//----------------------------------------------------------------------------
//! @file   combat_mediator.cpp
//! @brief  戦闘調整システム実装
//----------------------------------------------------------------------------
#include "combat_mediator.h"
#include "game/systems/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "game/entities/group.h"
#include "game/entities/player.h"
#include "game/ai/group_ai.h"
#include "game/systems/stagger_system.h"
#include "game/systems/love_bond_system.h"
#include "game/systems/game_constants.h"
#include "game/bond/bond_manager.h"
#include "game/bond/bond.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
CombatMediator& CombatMediator::Get()
{
    static CombatMediator instance;
    return instance;
}

//----------------------------------------------------------------------------
void CombatMediator::Initialize()
{
    EventBus& bus = EventBus::Get();

    stateSubscriptionId_ = bus.Subscribe<AIStateChangedEvent>(
        [this](const AIStateChangedEvent& e) { OnAIStateChanged(e); });

    loveSubscriptionId_ = bus.Subscribe<LoveFollowingChangedEvent>(
        [this](const LoveFollowingChangedEvent& e) { OnLoveFollowingChanged(e); });

    LOG_INFO("[CombatMediator] Initialized");
}

//----------------------------------------------------------------------------
void CombatMediator::Shutdown()
{
    EventBus& bus = EventBus::Get();
    bus.Unsubscribe<AIStateChangedEvent>(stateSubscriptionId_);
    bus.Unsubscribe<LoveFollowingChangedEvent>(loveSubscriptionId_);

    std::unique_lock lock(mutex_);
    attackableGroups_.clear();
    groupStates_.clear();
    loveFollowingFlags_.clear();
    player_ = nullptr;

    LOG_INFO("[CombatMediator] Shutdown");
}

//----------------------------------------------------------------------------
void CombatMediator::SetPlayer(Player* player)
{
    std::unique_lock lock(mutex_);
    player_ = player;
}

//----------------------------------------------------------------------------
void CombatMediator::OnAIStateChanged(const AIStateChangedEvent& event)
{
    if (!event.group) return;

    {
        std::unique_lock lock(mutex_);
        groupStates_[event.group] = event.newState;
    }

    UpdateAttackPermission(event.group);

    LOG_DEBUG("[CombatMediator] State changed: " + event.group->GetId() +
              " -> " + std::to_string(static_cast<int>(event.newState)));
}

//----------------------------------------------------------------------------
void CombatMediator::OnLoveFollowingChanged(const LoveFollowingChangedEvent& event)
{
    if (!event.group) return;

    {
        std::unique_lock lock(mutex_);
        loveFollowingFlags_[event.group] = event.isFollowing;
    }

    UpdateAttackPermission(event.group);

    LOG_DEBUG("[CombatMediator] Love following changed: " + event.group->GetId() +
              " -> " + (event.isFollowing ? "true" : "false"));
}

//----------------------------------------------------------------------------
bool CombatMediator::CanAttack(Group* group) const
{
    std::shared_lock lock(mutex_);
    return attackableGroups_.find(group) != attackableGroups_.end();
}

//----------------------------------------------------------------------------
size_t CombatMediator::GetAttackableCount() const
{
    std::shared_lock lock(mutex_);
    return attackableGroups_.size();
}

//----------------------------------------------------------------------------
void CombatMediator::UpdateAttackPermission(Group* group)
{
    if (!group) return;

    bool canAttack = true;

    // 1. Seek状態のみ攻撃可能（Wander/Fleeは不可）
    {
        std::shared_lock lock(mutex_);
        auto stateIt = groupStates_.find(group);
        if (stateIt == groupStates_.end() || stateIt->second != AIState::Seek) {
            canAttack = false;
        }
    }

    // 2. Love追従中は攻撃不可
    if (canAttack) {
        std::shared_lock lock(mutex_);
        auto followIt = loveFollowingFlags_.find(group);
        if (followIt != loveFollowingFlags_.end() && followIt->second) {
            canAttack = false;
        }
    }

    // 3. Love相手が遠い場合は攻撃不可
    if (canAttack && CheckLoveDistance(group)) {
        canAttack = false;
    }

    // 4. Stagger中は攻撃不可
    if (canAttack && StaggerSystem::Get().IsStaggered(group)) {
        canAttack = false;
    }

    // 攻撃許可リストを更新
    {
        std::unique_lock lock(mutex_);
        if (canAttack) {
            attackableGroups_.insert(group);
        } else {
            attackableGroups_.erase(group);
        }
    }
}

//----------------------------------------------------------------------------
bool CombatMediator::CheckLoveDistance(Group* group) const
{
    if (!group) return false;

    Vector2 groupPos = group->GetPosition();

    // プレイヤー参照を安全に取得
    Player* player = nullptr;
    {
        std::shared_lock lock(mutex_);
        player = player_;
    }

    // プレイヤーとのLove縁チェック
    if (player) {
        BondableEntity groupEntity = group;
        BondableEntity playerEntity = player;
        Bond* playerBond = BondManager::Get().GetBond(groupEntity, playerEntity);
        if (playerBond && playerBond->GetType() == BondType::Love) {
            float dist = (player->GetPosition() - groupPos).Length();
            if (dist > GameConstants::kLoveInterruptDistance) {
                return true;
            }
        }
    }

    // グループ同士のLove縁チェック
    std::vector<Group*> loveCluster = LoveBondSystem::Get().GetLoveCluster(group);
    if (loveCluster.size() > 1) {
        for (Group* partner : loveCluster) {
            if (partner == group) continue;
            if (!partner || partner->IsDefeated()) continue;
            float dist = (partner->GetPosition() - groupPos).Length();
            if (dist > GameConstants::kLoveInterruptDistance) {
                return true;
            }
        }
    }

    return false;
}

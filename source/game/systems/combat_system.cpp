//----------------------------------------------------------------------------
//! @file   combat_system.cpp
//! @brief  戦闘システム実装
//----------------------------------------------------------------------------
#include "combat_system.h"
#include "stagger_system.h"
#include "faction_manager.h"
#include "game/entities/group.h"
#include "game/entities/individual.h"
#include "game/entities/player.h"
#include "game/bond/bond_manager.h"
#include "game/systems/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
CombatSystem& CombatSystem::Get()
{
    static CombatSystem instance;
    return instance;
}

//----------------------------------------------------------------------------
void CombatSystem::Update(float dt)
{
    LOG_INFO("[CombatSystem] Update called, dt=" + std::to_string(dt) + ", timer=" + std::to_string(attackTimer_));

    attackTimer_ += dt;

    // 攻撃間隔チェック
    if (attackTimer_ < attackInterval_) {
        return;
    }
    attackTimer_ = 0.0f;

    // 各グループの戦闘処理
    LOG_INFO("[CombatSystem] Attack phase - groups: " + std::to_string(groups_.size()));

    for (Group* attacker : groups_) {
        if (!attacker || attacker->IsDefeated()) continue;

        // 硬直中は攻撃しない
        if (StaggerSystem::Get().IsStaggered(attacker)) continue;

        // 脅威度ベースでターゲット選定（グループ vs プレイヤー）
        Group* groupTarget = SelectTarget(attacker);
        bool canAttackPlayer = CanAttackPlayer(attacker);

        // プレイヤーの脅威度とグループの脅威度を比較
        float playerThreat = (canAttackPlayer && player_) ? player_->GetThreat() : -1.0f;
        float groupThreat = groupTarget ? groupTarget->GetThreat() : -1.0f;

        LOG_INFO("[CombatSystem] " + attacker->GetId() +
                 " - groupTarget: " + (groupTarget ? groupTarget->GetId() : "none") +
                 ", canAttackPlayer: " + std::to_string(canAttackPlayer));

        if (playerThreat > groupThreat && canAttackPlayer) {
            // プレイヤーの脅威度が高い → プレイヤーを攻撃
            ProcessCombatAgainstPlayer(attacker, dt);
        } else if (groupTarget) {
            // グループの脅威度が高い → グループを攻撃
            ProcessCombat(attacker, groupTarget, dt);
        }
    }

    // 全滅チェック（各グループにつき一度だけ処理）
    for (Group* group : groups_) {
        if (group && group->IsDefeated()) {
            // 既に処理済みならスキップ
            if (defeatedGroups_.count(group) > 0) continue;

            // 処理済みとしてマーク
            defeatedGroups_.insert(group);

            LOG_INFO("[CombatSystem] Group defeated: " + group->GetId());

            // EventBus通知
            EventBus::Get().Publish(GroupDefeatedEvent{ group });

            // コールバック
            if (onGroupDefeated_) {
                onGroupDefeated_(group);
            }
        }
    }
}

//----------------------------------------------------------------------------
void CombatSystem::RegisterGroup(Group* group)
{
    if (!group) return;

    auto it = std::find(groups_.begin(), groups_.end(), group);
    if (it == groups_.end()) {
        groups_.push_back(group);
        LOG_INFO("[CombatSystem] Group registered: " + group->GetId());
    }
}

//----------------------------------------------------------------------------
void CombatSystem::UnregisterGroup(Group* group)
{
    auto it = std::find(groups_.begin(), groups_.end(), group);
    if (it != groups_.end()) {
        groups_.erase(it);
        LOG_INFO("[CombatSystem] Group unregistered: " + group->GetId());
    }
}

//----------------------------------------------------------------------------
void CombatSystem::ClearGroups()
{
    groups_.clear();
    defeatedGroups_.clear();
    LOG_INFO("[CombatSystem] All groups cleared");
}

//----------------------------------------------------------------------------
Group* CombatSystem::SelectTarget(Group* attacker) const
{
    if (!attacker) return nullptr;

    Group* bestTarget = nullptr;
    float highestThreat = -1.0f;
    Vector2 attackerPos = attacker->GetPosition();
    float detectionRange = attacker->GetDetectionRange();

    LOG_INFO("[SelectTarget] " + attacker->GetId() + " pos=(" +
             std::to_string(attackerPos.x) + "," + std::to_string(attackerPos.y) +
             ") range=" + std::to_string(detectionRange));

    for (Group* candidate : groups_) {
        if (!candidate || candidate == attacker) continue;
        if (candidate->IsDefeated()) continue;

        // 索敵範囲チェック
        Vector2 candidatePos = candidate->GetPosition();
        float distance = (candidatePos - attackerPos).Length();

        LOG_INFO("  -> " + candidate->GetId() + " dist=" + std::to_string(distance) +
                 " hostile=" + std::to_string(AreHostile(attacker, candidate)));

        if (distance > detectionRange) continue;

        // 縁で繋がっていたら攻撃しない
        if (!AreHostile(attacker, candidate)) continue;

        // 脅威度で比較
        float threat = candidate->GetThreat();
        if (threat > highestThreat) {
            highestThreat = threat;
            bestTarget = candidate;
        }
    }

    return bestTarget;
}

//----------------------------------------------------------------------------
bool CombatSystem::CanAttackPlayer(Group* attacker) const
{
    if (!attacker || !player_) return false;
    if (!player_->IsAlive()) return false;

    // 索敵範囲チェック
    Vector2 attackerPos = attacker->GetPosition();
    Vector2 playerPos = player_->GetPosition();
    float distance = (playerPos - attackerPos).Length();
    if (distance > attacker->GetDetectionRange()) return false;

    // プレイヤーと縁で繋がっていたら攻撃しない
    return IsHostileToPlayer(attacker);
}

//----------------------------------------------------------------------------
bool CombatSystem::AreHostile(Group* a, Group* b) const
{
    if (!a || !b) return false;

    // FactionManagerで同一陣営判定（推移的接続を考慮）
    BondableEntity entityA = a;
    BondableEntity entityB = b;

    return !FactionManager::Get().AreSameFaction(entityA, entityB);
}

//----------------------------------------------------------------------------
bool CombatSystem::IsHostileToPlayer(Group* group) const
{
    if (!group || !player_) return false;

    BondableEntity groupEntity = group;
    BondableEntity playerEntity = player_;

    return !FactionManager::Get().AreSameFaction(groupEntity, playerEntity);
}

//----------------------------------------------------------------------------
void CombatSystem::ProcessCombatAgainstPlayer(Group* attacker, float /*dt*/)
{
    if (!attacker || !player_) return;
    if (!player_->IsAlive()) return;

    // 攻撃者からランダムな個体を選択
    Individual* attackerIndividual = attacker->GetRandomAliveIndividual();
    if (!attackerIndividual) return;

    // 攻撃距離チェック
    Vector2 attackerPos = attackerIndividual->GetPosition();
    Vector2 playerPos = player_->GetPosition();
    float distance = (playerPos - attackerPos).Length();
    float attackRange = attackerIndividual->GetAttackRange();

    LOG_INFO("[ProcessCombatAgainstPlayer] " + attackerIndividual->GetId() + " -> Player" +
             " dist=" + std::to_string(distance) + " range=" + std::to_string(attackRange));

    if (distance > attackRange) {
        return; // 攻撃範囲外
    }

    // 攻撃実行（種族ごとのAttackPlayer()を呼ぶ）
    attackerIndividual->AttackPlayer(player_);
}

//----------------------------------------------------------------------------
void CombatSystem::ProcessCombat(Group* attacker, Group* defender, float /*dt*/)
{
    if (!attacker || !defender) return;

    // 攻撃者からランダムな個体を選択
    Individual* attackerIndividual = attacker->GetRandomAliveIndividual();
    if (!attackerIndividual) return;

    // 防御者からランダムな個体を選択
    Individual* defenderIndividual = defender->GetRandomAliveIndividual();
    if (!defenderIndividual) return;

    // 攻撃距離チェック
    Vector2 attackerPos = attackerIndividual->GetPosition();
    Vector2 defenderPos = defenderIndividual->GetPosition();
    float distance = (defenderPos - attackerPos).Length();
    float attackRange = attackerIndividual->GetAttackRange();

    LOG_INFO("[ProcessCombat] " + attackerIndividual->GetId() + " -> " + defenderIndividual->GetId() +
             " dist=" + std::to_string(distance) + " range=" + std::to_string(attackRange));

    if (distance > attackRange) {
        return; // 攻撃範囲外
    }

    // 攻撃実行（種族ごとのAttack()を呼ぶ）
    attackerIndividual->Attack(defenderIndividual);

    if (onAttack_) {
        onAttack_(attackerIndividual, defenderIndividual, attackerIndividual->GetAttackDamage());
    }
}

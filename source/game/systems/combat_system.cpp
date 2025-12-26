//----------------------------------------------------------------------------
//! @file   combat_system.cpp
//! @brief  戦闘システム実装
//----------------------------------------------------------------------------
#include "combat_system.h"
#include "stagger_system.h"
#include "faction_manager.h"
#include "love_bond_system.h"
#include "game_constants.h"
#include "game/entities/group.h"
#include "game/entities/individual.h"
#include "game/entities/player.h"
#include "game/bond/bond_manager.h"
#include "game/bond/bond.h"
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
    // コールバック中の変更に備えてコピーを作成
    std::vector<Group*> groupsCopy = groups_;

    // 各グループの個体クールダウン更新と戦闘処理
    for (Group* attacker : groupsCopy) {
        if (!attacker || attacker->IsDefeated()) continue;

        // 全個体のクールダウン更新
        for (Individual* individual : attacker->GetAliveIndividuals()) {
            individual->UpdateAttackCooldown(dt);
        }

        // 硬直中は攻撃しない
        if (StaggerSystem::Get().IsStaggered(attacker)) continue;

        // Love縁相手が遠い場合は攻撃しない（追従優先）
        if (ShouldSkipCombatForLove(attacker)) continue;

        // 脅威度ベースでターゲット選定（グループ vs プレイヤー）
        Group* groupTarget = SelectTarget(attacker);
        bool canAttackPlayer = CanAttackPlayer(attacker);

        // プレイヤーの脅威度とグループの脅威度を比較
        float playerThreat = (canAttackPlayer && player_) ? player_->GetThreat() : -1.0f;
        float groupThreat = groupTarget ? groupTarget->GetThreat() : -1.0f;

        if (playerThreat > groupThreat && canAttackPlayer) {
            // プレイヤーの脅威度が高い → プレイヤーを攻撃
            ProcessCombatAgainstPlayer(attacker, dt);
        } else if (groupTarget) {
            // グループの脅威度が高い → グループを攻撃
            ProcessCombat(attacker, groupTarget, dt);
        }
    }

    // 全滅チェック（各グループにつき一度だけ処理）
    for (Group* group : groupsCopy) {
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

    for (Group* candidate : groups_) {
        if (!candidate || candidate == attacker) continue;
        if (candidate->IsDefeated()) continue;

        // 索敵範囲チェック
        Vector2 candidatePos = candidate->GetPosition();
        float distance = (candidatePos - attackerPos).Length();

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
bool CombatSystem::ShouldSkipCombatForLove(Group* group) const
{
    if (!group) return false;

    Vector2 groupPos = group->GetPosition();

    // 設計意図: いずれかのLove縁相手が遠い場合は戦闘を中断し、追従を優先する
    // これにより、Love縁で結ばれた仲間が離れ離れになることを防ぐ

    // プレイヤーとのLove縁チェック（プレイヤーを最優先）
    if (player_) {
        BondableEntity groupEntity = group;
        BondableEntity playerEntity = player_;
        Bond* playerBond = BondManager::Get().GetBond(groupEntity, playerEntity);
        if (playerBond && playerBond->GetType() == BondType::Love) {
            float dist = (player_->GetPosition() - groupPos).Length();
            if (dist > GameConstants::kLoveInterruptDistance) {
                return true;
            }
        }
    }

    // グループ同士のLove縁チェック（いずれかが遠い場合は追従優先）
    std::vector<Group*> loveCluster = LoveBondSystem::Get().GetLoveCluster(group);
    if (loveCluster.size() > 1) {
        for (Group* partner : loveCluster) {
            if (partner == group) continue;
            if (!partner || partner->IsDefeated()) continue;  // null/全滅チェック
            float dist = (partner->GetPosition() - groupPos).Length();
            if (dist > GameConstants::kLoveInterruptDistance) {
                return true;
            }
        }
    }

    return false;
}

//----------------------------------------------------------------------------
void CombatSystem::ProcessCombatAgainstPlayer(Group* attacker, float /*dt*/)
{
    if (!attacker || !player_) return;
    if (!player_->IsAlive()) return;

    // 攻撃者からランダムな個体を選択
    Individual* attackerIndividual = attacker->GetRandomAliveIndividual();
    if (!attackerIndividual) return;

    // 攻撃クールダウンチェック
    if (!attackerIndividual->CanAttackNow()) return;

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

    // 攻撃クールダウン開始
    attackerIndividual->StartAttackCooldown(attackInterval_);
}

//----------------------------------------------------------------------------
void CombatSystem::ProcessCombat(Group* attacker, Group* defender, float /*dt*/)
{
    if (!attacker || !defender) return;

    // 攻撃者からランダムな個体を選択
    Individual* attackerIndividual = attacker->GetRandomAliveIndividual();
    if (!attackerIndividual) return;

    // 攻撃クールダウンチェック
    if (!attackerIndividual->CanAttackNow()) return;

    // 防御者からランダムな個体を選択
    Individual* defenderIndividual = defender->GetRandomAliveIndividual();
    if (!defenderIndividual) return;

    // 攻撃距離チェック
    Vector2 attackerPos = attackerIndividual->GetPosition();
    Vector2 defenderPos = defenderIndividual->GetPosition();
    float distance = (defenderPos - attackerPos).Length();
    float attackRange = attackerIndividual->GetAttackRange();

    LOG_DEBUG("[ProcessCombat] " + attackerIndividual->GetId() + " -> " + defenderIndividual->GetId() +
              " dist=" + std::to_string(distance) + " range=" + std::to_string(attackRange));

    if (distance > attackRange) {
        return; // 攻撃範囲外
    }

    // 攻撃実行（種族ごとのAttack()を呼ぶ）
    attackerIndividual->Attack(defenderIndividual);

    // 攻撃クールダウン開始
    attackerIndividual->StartAttackCooldown(attackInterval_);

    if (onAttack_) {
        onAttack_(attackerIndividual, defenderIndividual, attackerIndividual->GetAttackDamage());
    }
}

//----------------------------------------------------------------------------
//! @file   combat_system.cpp
//! @brief  戦闘システム実装
//----------------------------------------------------------------------------
#include "combat_system.h"
#include "combat_mediator.h"
#include "stagger_system.h"
#include "game_constants.h"
#include "game/entities/group.h"
#include "game/entities/individual.h"
#include "game/entities/player.h"
#include "game/relationships/relationship_facade.h"
#include "engine/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
CombatSystem::CombatSystem()
{
    // IndividualDiedEventを購読（attackTarget_クリア用）
    individualDiedSubscriptionId_ = EventBus::Get().Subscribe<IndividualDiedEvent>(
        [this](const IndividualDiedEvent& e) {
            OnIndividualDied(e.individual);
        });
}

//----------------------------------------------------------------------------
CombatSystem::~CombatSystem()
{
    // イベント購読を解除
    if (individualDiedSubscriptionId_ != 0) {
        EventBus::Get().Unsubscribe<IndividualDiedEvent>(individualDiedSubscriptionId_);
        individualDiedSubscriptionId_ = 0;
    }
}

//----------------------------------------------------------------------------
CombatSystem& CombatSystem::Get()
{
    assert(instance_ && "CombatSystem::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void CombatSystem::Create()
{
    if (!instance_) {
        instance_.reset(new CombatSystem());
    }
}

//----------------------------------------------------------------------------
void CombatSystem::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
void CombatSystem::Update(float dt)
{
    isUpdating_ = true;

    // コールバック中の変更に備えてコピーを作成
    std::vector<Group*> groupsCopy = groups_;

    // 各グループの個体クールダウン更新と戦闘処理
    for (Group* attacker : groupsCopy) {
        // 削除予約されたグループはスキップ
        if (!attacker || attacker->IsDefeated() || IsPendingRemoval(attacker)) continue;

        // 全個体のクールダウン更新
        for (Individual* individual : attacker->GetAliveIndividuals()) {
            individual->UpdateAttackCooldown(dt);
        }

        // CombatMediatorに攻撃許可を確認（Stagger/Love/AIState全て含む）
        if (!CombatMediator::Get().CanAttack(attacker)) continue;

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
        // 削除予約されたグループはスキップ
        if (!group || IsPendingRemoval(group)) continue;

        if (group->IsDefeated()) {
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

    isUpdating_ = false;

    // 遅延削除を実行
    FlushPendingRemovals();
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
    if (!group) return;

    // Update中は遅延削除
    if (isUpdating_) {
        // 既に予約済みならスキップ
        if (!IsPendingRemoval(group)) {
            pendingRemovals_.push_back(group);
            LOG_INFO("[CombatSystem] Group unregister deferred: " + group->GetId());
        }
        return;
    }

    // 即座に削除
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
    pendingRemovals_.clear();
    defeatedGroups_.clear();
    LOG_INFO("[CombatSystem] All groups cleared");
}

//----------------------------------------------------------------------------
void CombatSystem::FlushPendingRemovals()
{
    if (pendingRemovals_.empty()) return;

    for (Group* group : pendingRemovals_) {
        auto it = std::find(groups_.begin(), groups_.end(), group);
        if (it != groups_.end()) {
            groups_.erase(it);
            LOG_INFO("[CombatSystem] Group unregistered (deferred): " + group->GetId());
        }
    }
    pendingRemovals_.clear();
}

//----------------------------------------------------------------------------
bool CombatSystem::IsPendingRemoval(Group* group) const
{
    return std::find(pendingRemovals_.begin(), pendingRemovals_.end(), group) != pendingRemovals_.end();
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

    // 味方同士は敵対しない
    if (a->IsAlly() && b->IsAlly()) return false;

    // RelationshipFacadeで敵対判定（推移的接続を考慮）
    BondableEntity entityA = a;
    BondableEntity entityB = b;

    return RelationshipFacade::Get().AreHostile(entityA, entityB);
}

//----------------------------------------------------------------------------
bool CombatSystem::IsHostileToPlayer(Group* group) const
{
    if (!group || !player_) return false;

    // 味方グループはプレイヤーに敵対しない
    if (group->IsAlly()) return false;

    BondableEntity groupEntity = group;
    BondableEntity playerEntity = player_;

    return RelationshipFacade::Get().AreHostile(groupEntity, playerEntity);
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

//----------------------------------------------------------------------------
void CombatSystem::OnIndividualDied(Individual* diedIndividual)
{
    if (!diedIndividual) return;

    // 全グループ内の全個体を走査し、死亡した個体をattackTarget_にしていればクリア
    for (Group* group : groups_) {
        if (!group || group->IsDefeated()) continue;

        for (Individual* ind : group->GetAliveIndividuals()) {
            if (!ind) continue;

            // 死亡した個体をターゲットにしている場合はクリア
            if (ind->GetAttackTarget() == diedIndividual) {
                ind->SetAttackTarget(nullptr);
            }
        }
    }
}

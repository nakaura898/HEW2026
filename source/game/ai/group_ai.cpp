//----------------------------------------------------------------------------
//! @file   group_ai.cpp
//! @brief  グループAI実装
//----------------------------------------------------------------------------
#include "group_ai.h"
#include "game/entities/group.h"
#include "game/entities/player.h"
#include "game/systems/stagger_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/love_bond_system.h"
#include "game/bond/bond_manager.h"
#include "game/bond/bond.h"
#include "engine/component/camera2d.h"
#include "common/logging/logging.h"
#include <random>
#include <cmath>

namespace {
    //! @brief 最小近接攻撃範囲
    constexpr float kMinMeleeAttackRange = 50.0f;
    //! @brief 画面端からの可視マージン
    constexpr float kVisibilityMargin = 50.0f;
    //! @brief 円周率（徘徊角度計算用）
    constexpr float kTwoPi = 2.0f * 3.14159f;
}

//----------------------------------------------------------------------------
GroupAI::GroupAI(Group* owner)
    : owner_(owner)
{
    SetNewWanderTarget();
}

//----------------------------------------------------------------------------
void GroupAI::Update(float dt)
{
    if (!owner_) return;

    // 硬直中は行動しない
    if (StaggerSystem::Get().IsStaggered(owner_)) {
        return;
    }

    // 状態遷移チェック
    CheckStateTransition();

    // 状態に応じた更新
    switch (state_) {
    case AIState::Wander:
        UpdateWander(dt);
        break;
    case AIState::Seek:
        UpdateSeek(dt);
        break;
    case AIState::Flee:
        UpdateFlee(dt);
        break;
    }
}

//----------------------------------------------------------------------------
void GroupAI::SetState(AIState state)
{
    if (state_ == state) return;

    AIState oldState = state_;
    state_ = state;

    LOG_INFO("[GroupAI] " + owner_->GetId() + " state changed: " +
             std::to_string(static_cast<int>(oldState)) + " -> " +
             std::to_string(static_cast<int>(state)));

    if (onStateChanged_) {
        onStateChanged_(state_);
    }
}

//----------------------------------------------------------------------------
void GroupAI::EnterCombat()
{
    inCombat_ = true;
    if (state_ == AIState::Wander) {
        SetState(AIState::Seek);
    }
}

//----------------------------------------------------------------------------
void GroupAI::ExitCombat()
{
    inCombat_ = false;
    ClearTarget();
    SetState(AIState::Wander);
}

//----------------------------------------------------------------------------
void GroupAI::SetTarget(Group* target)
{
    if (target) {
        target_ = target;
        if (inCombat_) {
            SetState(AIState::Seek);
        }
    } else {
        ClearTarget();
    }
}

//----------------------------------------------------------------------------
void GroupAI::SetTargetPlayer(Player* target)
{
    if (target) {
        target_ = target;
        if (inCombat_) {
            SetState(AIState::Seek);
        }
    } else {
        ClearTarget();
    }
}

//----------------------------------------------------------------------------
bool GroupAI::HasTarget() const
{
    return !std::holds_alternative<std::monostate>(target_);
}

//----------------------------------------------------------------------------
void GroupAI::ClearTarget()
{
    target_ = std::monostate{};
}

//----------------------------------------------------------------------------
void GroupAI::FindTarget()
{
    if (!owner_) return;

    // ラブパートナーがいる場合は共有ターゲットを使用
    LoveBondSystem& loveSys = LoveBondSystem::Get();
    if (loveSys.HasLovePartners(owner_)) {
        std::vector<Group*> cluster = loveSys.GetLoveCluster(owner_);
        AITarget sharedTarget = loveSys.DetermineSharedTarget(cluster);

        // 共有ターゲットを設定
        if (std::holds_alternative<Group*>(sharedTarget)) {
            Group* targetGroup = std::get<Group*>(sharedTarget);
            if (targetGroup) {
                target_ = targetGroup;
                LOG_INFO("[GroupAI] " + owner_->GetId() + " (Love) targeting " + targetGroup->GetId());
                return;
            }
        } else if (std::holds_alternative<Player*>(sharedTarget)) {
            Player* targetPlayer = std::get<Player*>(sharedTarget);
            if (targetPlayer) {
                target_ = targetPlayer;
                LOG_INFO("[GroupAI] " + owner_->GetId() + " (Love) targeting Player");
                return;
            }
        }
        // 共有ターゲットがない場合は通常のロジックにフォールバック
    }

    // CombatSystemを使ってターゲットを検索
    CombatSystem& combat = CombatSystem::Get();

    // グループターゲットを検索
    Group* groupTarget = combat.SelectTarget(owner_);

    // プレイヤーを攻撃可能か確認
    bool canAttackPlayer = combat.CanAttackPlayer(owner_);

    // 脅威度で比較してターゲットを決定
    float groupThreat = groupTarget ? groupTarget->GetThreat() : -1.0f;
    float playerThreat = (canAttackPlayer && player_) ? player_->GetThreat() : -1.0f;

    if (playerThreat > groupThreat && canAttackPlayer && player_) {
        target_ = player_;
    } else if (groupTarget) {
        target_ = groupTarget;
    } else {
        ClearTarget();
    }
}

//----------------------------------------------------------------------------
Vector2 GroupAI::GetTargetPosition() const
{
    if (std::holds_alternative<Group*>(target_)) {
        Group* group = std::get<Group*>(target_);
        if (group) return group->GetPosition();
    } else if (std::holds_alternative<Player*>(target_)) {
        Player* player = std::get<Player*>(target_);
        if (player) return player->GetPosition();
    }
    return Vector2::Zero;
}

//----------------------------------------------------------------------------
bool GroupAI::IsTargetValid() const
{
    if (std::holds_alternative<Group*>(target_)) {
        Group* group = std::get<Group*>(target_);
        return group && !group->IsDefeated();
    } else if (std::holds_alternative<Player*>(target_)) {
        Player* player = std::get<Player*>(target_);
        return player && player->IsAlive();
    }
    return false;
}

//----------------------------------------------------------------------------
void GroupAI::UpdateWander(float dt)
{
    wanderTimer_ += dt;

    // プレイヤーとラブ縁で結ばれているかチェック
    bool followPlayer = false;
    if (player_) {
        BondableEntity groupEntity = owner_;
        BondableEntity playerEntity = player_;
        Bond* playerBond = BondManager::Get().GetBond(groupEntity, playerEntity);
        if (playerBond && playerBond->GetType() == BondType::Love) {
            followPlayer = true;
        }
    }

    // プレイヤーとラブ縁がある場合はプレイヤーを追従
    if (followPlayer) {
        Vector2 currentPos = owner_->GetPosition();
        Vector2 playerPos = player_->GetPosition();
        Vector2 direction = playerPos - currentPos;
        float distance = direction.Length();

        // プレイヤーから一定距離離れていたら近づく
        if (distance > 100.0f) {
            direction.Normalize();
            Vector2 newPos = currentPos + direction * moveSpeed_ * dt;
            owner_->SetPosition(newPos);
        }
        return;
    }

    // ラブパートナー（グループ同士）がいる場合は一緒に徘徊
    std::vector<Group*> loveCluster = LoveBondSystem::Get().GetLoveCluster(owner_);
    bool hasLovePartners = loveCluster.size() > 1;

    // 一定時間ごとに新しい目標を設定
    if (wanderTimer_ >= wanderInterval_) {
        if (hasLovePartners) {
            // ラブクラスタの中心を計算
            Vector2 clusterCenter = Vector2::Zero;
            for (Group* g : loveCluster) {
                clusterCenter = clusterCenter + g->GetPosition();
            }
            clusterCenter = clusterCenter * (1.0f / static_cast<float>(loveCluster.size()));

            // クラスタ中心から新しい目標を設定（最初のグループのみが計算）
            if (loveCluster[0] == owner_) {
                std::uniform_real_distribution<float> angleDist(0.0f, kTwoPi);
                std::uniform_real_distribution<float> radiusDist(kMinMeleeAttackRange, wanderRadius_);
                float angle = angleDist(rng_);
                float radius = radiusDist(rng_);
                wanderTarget_ = clusterCenter + Vector2(std::cos(angle) * radius, std::sin(angle) * radius);

                // 他のグループにも同じ目標を設定
                for (size_t i = 1; i < loveCluster.size(); ++i) {
                    if (GroupAI* ai = loveCluster[i]->GetAI()) {
                        ai->SetWanderTarget(wanderTarget_);
                    }
                }
            }
        } else {
            SetNewWanderTarget();
        }
        wanderTimer_ = 0.0f;
    }

    // 目標に向かって移動
    Vector2 currentPos = owner_->GetPosition();
    Vector2 direction = wanderTarget_ - currentPos;
    float distance = direction.Length();

    if (distance > 5.0f) {
        direction.Normalize();
        Vector2 newPos = currentPos + direction * moveSpeed_ * dt;
        owner_->SetPosition(newPos);
    }
}

//----------------------------------------------------------------------------
void GroupAI::UpdateSeek(float dt)
{
    // ターゲットがいない or 無効なら再検索
    if (!HasTarget() || !IsTargetValid()) {
        FindTarget();
    }

    // それでもターゲットがいなければWanderに戻る
    if (!HasTarget()) {
        SetState(AIState::Wander);
        return;
    }

    // ターゲットに向かって移動
    Vector2 currentPos = owner_->GetPosition();
    Vector2 targetPos = GetTargetPosition();

#ifdef _DEBUG
    // デバッグ: どこに向かってるか（ターゲット種別も表示）
    {
        std::string targetType = "none";
        std::string targetName = "none";
        if (std::holds_alternative<Group*>(target_)) {
            Group* g = std::get<Group*>(target_);
            targetType = "Group";
            targetName = g ? g->GetId() : "null";
        } else if (std::holds_alternative<Player*>(target_)) {
            targetType = "Player";
            targetName = "Player";
        }
        LOG_DEBUG("[UpdateSeek] " + owner_->GetId() +
                  " -> " + targetType + ":" + targetName +
                  " pos=(" + std::to_string(static_cast<int>(targetPos.x)) + "," +
                  std::to_string(static_cast<int>(targetPos.y)) + ")");
    }
#endif
    Vector2 direction = targetPos - currentPos;
    float distance = direction.Length();

    // 索敵範囲外ならターゲットを見失う
    if (distance > detectionRange_) {
        LOG_INFO("[GroupAI] " + owner_->GetId() + " lost target (out of range)");
        ClearTarget();
        return;
    }

    // 攻撃範囲内なら移動しない（遠距離攻撃ユニットは近づかない）
    float attackRange = owner_->GetMaxAttackRange();
    if (attackRange < kMinMeleeAttackRange) {
        attackRange = kMinMeleeAttackRange;  // 最低でも近接用の範囲
    }

    if (distance > attackRange) {
        direction.Normalize();
        Vector2 newPos = currentPos + direction * moveSpeed_ * dt;
        owner_->SetPosition(newPos);
    }
}

//----------------------------------------------------------------------------
void GroupAI::UpdateFlee(float dt)
{
    if (!owner_) return;

    Vector2 currentPos = owner_->GetPosition();

    // プレイヤーがいない場合はWanderに戻る
    if (!player_) {
        LOG_INFO("[GroupAI] " + owner_->GetId() + " no player to flee to, returning to Wander");
        owner_->SetThreatModifier(1.0f);
        SetState(AIState::Wander);
        return;
    }

    // カメラ内に映っていたら移動しない
    if (camera_) {
        Vector2 screenPos = camera_->WorldToScreen(currentPos);
        float viewW = camera_->GetViewportWidth();
        float viewH = camera_->GetViewportHeight();

        bool isVisible = screenPos.x >= kVisibilityMargin && screenPos.x <= viewW - kVisibilityMargin &&
                         screenPos.y >= kVisibilityMargin && screenPos.y <= viewH - kVisibilityMargin;

        if (isVisible) {
            // 画面内にいるので移動不要
            return;
        }
    }

    Vector2 playerPos = player_->GetPosition();
    Vector2 toPlayer = playerPos - currentPos;
    float distanceToPlayer = toPlayer.Length();

    // プレイヤーに十分近い場合は停止
    if (distanceToPlayer <= fleeStopDistance_) {
        return;
    }

    // プレイヤー方向に逃げる（通常より速い）
    toPlayer.Normalize();
    float fleeSpeed = moveSpeed_ * fleeSpeedMultiplier_;
    Vector2 newPos = currentPos + toPlayer * fleeSpeed * dt;
    owner_->SetPosition(newPos);
}

//----------------------------------------------------------------------------
void GroupAI::CheckStateTransition()
{
    if (!owner_) return;

    float hpRatio = owner_->GetHpRatio();

    // HP閾値チェック（Flee）- 戦闘中かつHP低下
    if (hpRatio < fleeThreshold_ && inCombat_) {
        if (state_ != AIState::Flee) {
            LOG_INFO("[GroupAI] " + owner_->GetId() + " HP low (" +
                     std::to_string(static_cast<int>(hpRatio * 100)) + "%), fleeing!");
            SetState(AIState::Flee);
            // 脅威度を下げる
            owner_->SetThreatModifier(0.5f);
        }
        return;
    }

    // HP回復後の復帰（Flee → Seek or Wander）
    if (state_ == AIState::Flee && hpRatio >= fleeThreshold_) {
        LOG_INFO("[GroupAI] " + owner_->GetId() + " HP recovered, exiting flee");
        owner_->SetThreatModifier(1.0f);

        // ターゲットを再検索
        FindTarget();
        if (HasTarget()) {
            SetState(AIState::Seek);
        } else {
            inCombat_ = false;
            SetState(AIState::Wander);
        }
        return;
    }

    // Seek中にターゲットがいなくなったらWanderに戻る
    if (state_ == AIState::Seek && !HasTarget()) {
        FindTarget();
        if (!HasTarget()) {
            LOG_INFO("[GroupAI] " + owner_->GetId() + " no targets, returning to Wander");
            inCombat_ = false;
            SetState(AIState::Wander);
        }
        return;
    }

    // Wander中に敵を発見したらSeekに移行
    if (state_ == AIState::Wander) {
        FindTarget();
        if (HasTarget()) {
            LOG_INFO("[GroupAI] " + owner_->GetId() + " found target, entering combat");
            inCombat_ = true;
            SetState(AIState::Seek);
        }
    }
}

//----------------------------------------------------------------------------
void GroupAI::SetNewWanderTarget()
{
    if (!owner_) return;

    Vector2 currentPos = owner_->GetPosition();

    std::uniform_real_distribution<float> angleDist(0.0f, kTwoPi);
    std::uniform_real_distribution<float> radiusDist(kMinMeleeAttackRange, wanderRadius_);

    float angle = angleDist(rng_);
    float radius = radiusDist(rng_);

    wanderTarget_ = currentPos + Vector2(
        std::cos(angle) * radius,
        std::sin(angle) * radius
    );
}

//----------------------------------------------------------------------------
//! @file   group_ai.cpp
//! @brief  グループAI実装
//----------------------------------------------------------------------------
#include "group_ai.h"
#include "game/entities/group.h"
#include "game/entities/individual.h"
#include "game/entities/player.h"
#include "game/systems/stagger_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/time_manager.h"
#include "game/systems/game_constants.h"
#include "game/systems/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "game/relationships/relationship_facade.h"
#include "engine/component/camera2d.h"
#include "common/logging/logging.h"
#include <random>
#include <cmath>

namespace {
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

    // GroupDefeatedEventを購読（ターゲットが倒された時にクリア）
    defeatedSubscriptionId_ = EventBus::Get().Subscribe<GroupDefeatedEvent>(
        [this](const GroupDefeatedEvent& e) {
            OnGroupDefeated(e.group);
        });
}

//----------------------------------------------------------------------------
GroupAI::~GroupAI()
{
    // イベント購読を解除
    if (defeatedSubscriptionId_ != 0) {
        EventBus::Get().Unsubscribe<GroupDefeatedEvent>(defeatedSubscriptionId_);
        defeatedSubscriptionId_ = 0;
    }
}

//----------------------------------------------------------------------------
void GroupAI::Update(float dt)
{
    if (!owner_) return;

    // 硬直中は行動しない
    if (StaggerSystem::Get().IsStaggered(owner_)) {
        return;
    }

    // 時間スケール適用
    float scaledDt = TimeManager::Get().GetScaledDeltaTime(dt);

    // 時間停止中は更新しない
    if (scaledDt <= 0.0f) {
        return;
    }

    // Love縁相手との距離チェック（離れすぎたら追従に切り替え）
    if (state_ == AIState::Seek || state_ == AIState::Flee) {
        bool tooFar = CheckLovePartnerDistance();
        if (tooFar) {
            // 全員が攻撃中断可能かチェック（攻撃開始から一定時間経過）
            bool canInterrupt = true;
            for (Individual* ind : owner_->GetAliveIndividuals()) {
                if (!ind->CanInterruptAttack()) {
                    canInterrupt = false;
                    break;
                }
            }

            if (canInterrupt) {
                LOG_INFO("[GroupAI] " + owner_->GetId() + " returning to Wander (Love follow)");
                // 攻撃中の個体は中断
                for (Individual* ind : owner_->GetAliveIndividuals()) {
                    if (ind->IsAttacking()) {
                        ind->InterruptAttack();
                    }
                }
                SetState(AIState::Wander);
                ClearTarget();
                inCombat_ = false;
                // 追従開始状態にリセット
                isLoveFollowing_ = true;
                loveFollowTimer_ = 0.0f;
                EventBus::Get().Publish(LoveFollowingChangedEvent{ owner_, true });
            }
        }
    }

    // 状態遷移チェック
    CheckStateTransition();

    // 状態に応じた更新（スケール済み時間で）
    // LOG_DEBUG("[GroupAI::Update] " + owner_->GetId() + " state=" +
    //           std::to_string(static_cast<int>(state_)) + " (0=Wander,1=Seek,2=Flee)");
    switch (state_) {
    case AIState::Wander:
        UpdateWander(scaledDt);
        break;
    case AIState::Seek:
        UpdateSeek(scaledDt);
        break;
    case AIState::Flee:
        UpdateFlee(scaledDt);
        break;
    }

    // 移動状態の変化を個体に通知
    NotifyMovementChange();
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

    // EventBusに状態変更を通知
    EventBus::Get().Publish(AIStateChangedEvent{ owner_, state_ });

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
    RelationshipFacade& facade = RelationshipFacade::Get();
    if (facade.HasLovePartners(owner_)) {
        std::vector<Group*> cluster = facade.GetLoveCluster(owner_);
        AITarget sharedTarget = facade.DetermineSharedTarget(cluster);

        // 共有ターゲットを設定
        if (std::holds_alternative<Group*>(sharedTarget)) {
            Group* targetGroup = std::get<Group*>(sharedTarget);
            if (targetGroup) {
                target_ = targetGroup;
                return;
            }
        } else if (std::holds_alternative<Player*>(sharedTarget)) {
            Player* targetPlayer = std::get<Player*>(sharedTarget);
            if (targetPlayer) {
                target_ = targetPlayer;
                return;
            }
        }
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
    // Wander状態の場合
    if (state_ == AIState::Wander) {
        // プレイヤーとラブ縁で結ばれている場合はプレイヤー位置を返す
        if (player_) {
            BondableEntity groupEntity = owner_;
            BondableEntity playerEntity = player_;
            const EdgeData* edge = RelationshipFacade::Get().GetEdge(groupEntity, playerEntity);
            if (edge && edge->type == BondType::Love) {
                return player_->GetPosition();
            }
        }
        return wanderTarget_;
    }

    // Seek/Flee状態の場合はターゲットの位置を返す
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
        const EdgeData* edge = RelationshipFacade::Get().GetEdge(groupEntity, playerEntity);
        if (edge) {
            // LOG_DEBUG("[UpdateWander] " + owner_->GetId() + " has bond with player, type=" +
            //           std::to_string(static_cast<int>(edge->type)) + " (2=Love)");
            if (edge->type == BondType::Love) {
                followPlayer = true;
            }
        }
    }

    // デバッグ: state確認
    // LOG_DEBUG("[UpdateWander] " + owner_->GetId() + " state=Wander, followPlayer=" +
    //           std::string(followPlayer ? "true" : "false") +
    //           ", player_=" + std::string(player_ ? "valid" : "null"));

    // プレイヤーとラブ縁がある場合はプレイヤーを追従
    if (followPlayer) {
        Vector2 currentPos = owner_->GetPosition();
        Vector2 playerPos = player_->GetPosition();
        Vector2 direction = playerPos - currentPos;
        float distance = direction.Length();

        // プレイヤーから一定距離離れていたら追従開始
        if (distance > GameConstants::kLoveFollowStartDistance) {
            // 追従開始時にタイマーをリセット
            if (!isLoveFollowing_) {
                isLoveFollowing_ = true;
                loveFollowTimer_ = 0.0f;
                EventBus::Get().Publish(LoveFollowingChangedEvent{ owner_, true });
                // LOG_DEBUG("[UpdateWander] " + owner_->GetId() + " started Love follow");
            }

            // 追従中はタイマーを更新
            loveFollowTimer_ += dt;

            direction.Normalize();
            float moveAmount = GameConstants::kLoveFollowSpeed * dt;
            Vector2 newPos = currentPos + direction * moveAmount;
            owner_->SetPosition(newPos);
            // LOG_DEBUG("[UpdateWander] " + owner_->GetId() + " MOVED to (" +
            //           std::to_string(newPos.x) + "," + std::to_string(newPos.y) +
            //           "), followTimer=" + std::to_string(loveFollowTimer_));
        } else {
            // プレイヤーに十分近い場合も追従中ならタイマーを更新
            if (isLoveFollowing_) {
                loveFollowTimer_ += dt;
                // LOG_DEBUG("[UpdateWander] " + owner_->GetId() +
                //           " within threshold, followTimer=" + std::to_string(loveFollowTimer_));
            }
        }
        return;
    } else {
        // プレイヤー追従していない場合はフラグをリセット
        if (isLoveFollowing_) {
            isLoveFollowing_ = false;
            loveFollowTimer_ = 0.0f;
            EventBus::Get().Publish(LoveFollowingChangedEvent{ owner_, false });
        }
    }

    // ラブパートナー（グループ同士）がいる場合
    std::vector<Group*> loveCluster = RelationshipFacade::Get().GetLoveCluster(owner_);
    bool hasLovePartners = loveCluster.size() > 1;

    // グループ同士のLove縁：離れすぎたらお互いを追いかける
    if (hasLovePartners) {
        Vector2 currentPos = owner_->GetPosition();

        // クラスタ中心を計算
        Vector2 clusterCenter = Vector2::Zero;
        for (Group* g : loveCluster) {
            clusterCenter = clusterCenter + g->GetPosition();
        }
        clusterCenter = clusterCenter * (1.0f / static_cast<float>(loveCluster.size()));

        // 中心から離れすぎていたら中心に向かって移動（プレイヤー速度で）
        Vector2 toCenter = clusterCenter - currentPos;
        float distToCenter = toCenter.Length();
        if (distToCenter > GameConstants::kLoveFollowStartDistance) {
            toCenter.Normalize();
            float moveAmount = GameConstants::kLoveFollowSpeed * dt;
            Vector2 newPos = currentPos + toCenter * moveAmount;
            owner_->SetPosition(newPos);
            return;
        }
    }

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
                std::uniform_real_distribution<float> radiusDist(GameConstants::kMinMeleeAttackRange, wanderRadius_);
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

    if (distance > GameConstants::kLoveStopDistance) {
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
        // LOG_DEBUG("[UpdateSeek] " + owner_->GetId() +
        //           " -> " + targetType + ":" + targetName +
        //           " pos=(" + std::to_string(static_cast<int>(targetPos.x)) + "," +
        //           std::to_string(static_cast<int>(targetPos.y)) + ")");
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
    if (attackRange < GameConstants::kMinMeleeAttackRange) {
        attackRange = GameConstants::kMinMeleeAttackRange;  // 最低でも近接用の範囲
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
        // カメラ範囲内ならSeek状態を維持（攻撃継続）
        if (IsInCameraView()) {
            if (state_ != AIState::Seek) {
                LOG_INFO("[GroupAI] " + owner_->GetId() + " HP low but in camera view, staying in Seek");
                SetState(AIState::Seek);
            }
            return;
        }

        // カメラ範囲外ならFlee
        if (state_ != AIState::Flee) {
            LOG_INFO("[GroupAI] " + owner_->GetId() + " HP low (" +
                     std::to_string(static_cast<int>(hpRatio * 100)) + "%) and out of view, fleeing!");
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

    // Flee中にカメラ範囲内に入ったらSeekに戻る
    if (state_ == AIState::Flee && IsInCameraView()) {
        LOG_INFO("[GroupAI] " + owner_->GetId() + " entered camera view while fleeing, returning to Seek");
        FindTarget();
        if (HasTarget()) {
            SetState(AIState::Seek);
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
    // ただしLove縁相手との距離が離れすぎていたら戦闘に入らない
    if (state_ == AIState::Wander) {
        if (CheckLovePartnerDistance()) {
            // Love相手が遠いので追従優先
            return;
        }

        // Love追従中は最小追従時間が経過するまで攻撃に戻らない
        if (isLoveFollowing_ && loveFollowTimer_ < kMinLoveFollowDuration) {
            // LOG_DEBUG("[GroupAI] " + owner_->GetId() +
            //           " Love following, timer=" + std::to_string(loveFollowTimer_) +
            //           "/" + std::to_string(kMinLoveFollowDuration));
            return;
        }

        FindTarget();
        if (HasTarget()) {
            LOG_INFO("[GroupAI] " + owner_->GetId() + " found target, entering combat");
            inCombat_ = true;
            // 追従状態をリセット
            if (isLoveFollowing_) {
                isLoveFollowing_ = false;
                loveFollowTimer_ = 0.0f;
                EventBus::Get().Publish(LoveFollowingChangedEvent{ owner_, false });
            }
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
    std::uniform_real_distribution<float> radiusDist(GameConstants::kMinMeleeAttackRange, wanderRadius_);

    float angle = angleDist(rng_);
    float radius = radiusDist(rng_);

    wanderTarget_ = currentPos + Vector2(
        std::cos(angle) * radius,
        std::sin(angle) * radius
    );
}

//----------------------------------------------------------------------------
bool GroupAI::IsMoving() const
{
    if (!owner_) return false;

    Vector2 currentPos = owner_->GetPosition();
    Vector2 targetPos = GetTargetPosition();
    Vector2 diff = targetPos - currentPos;
    float distance = diff.Length();

    if (state_ == AIState::Wander) {
        // プレイヤーとのLove縁
        if (player_) {
            BondableEntity groupEntity = owner_;
            BondableEntity playerEntity = player_;
            const EdgeData* edge = RelationshipFacade::Get().GetEdge(groupEntity, playerEntity);
            if (edge && edge->type == BondType::Love) {
                Vector2 playerPos = player_->GetPosition();
                float playerDist = (playerPos - owner_->GetPosition()).Length();
                return playerDist > GameConstants::kLoveFollowStartDistance;
            }
        }

        // グループ同士のLove縁（クラスタ中心への移動）
        std::vector<Group*> loveCluster = RelationshipFacade::Get().GetLoveCluster(owner_);
        if (loveCluster.size() > 1) {
            // クラスタ中心を計算
            Vector2 clusterCenter = Vector2::Zero;
            for (Group* g : loveCluster) {
                clusterCenter = clusterCenter + g->GetPosition();
            }
            clusterCenter = clusterCenter * (1.0f / static_cast<float>(loveCluster.size()));

            // 中心からの距離で判定
            float distToCenter = (clusterCenter - owner_->GetPosition()).Length();
            if (distToCenter > GameConstants::kLoveFollowStartDistance) {
                return true;  // クラスタ中心に向かって移動中
            }

            // 通常のwander移動
            return distance > GameConstants::kLoveStopDistance;
        }
    }

    // Seek状態では攻撃範囲内なら移動しない（ただしプレイヤーターゲット時は除く）
    if (state_ == AIState::Seek) {
        // プレイヤーをターゲットにしている場合は通常の判定
        if (std::holds_alternative<Player*>(target_)) {
            bool result = distance > GameConstants::kLoveStopDistance;
            // LOG_DEBUG("[IsMoving] " + owner_->GetId() + " Seek->Player dist=" +
            //           std::to_string(distance) + " result=" + (result ? "true" : "false"));
            return result;
        }
        // グループターゲットの場合は攻撃範囲で判定
        float attackRange = owner_->GetMaxAttackRange();
        if (attackRange < GameConstants::kMinMeleeAttackRange) {
            attackRange = GameConstants::kMinMeleeAttackRange;
        }
        bool result = distance > attackRange;
        // LOG_DEBUG("[IsMoving] " + owner_->GetId() + " Seek->Group dist=" +
        //           std::to_string(distance) + " attackRange=" + std::to_string(attackRange) +
        //           " result=" + (result ? "true" : "false"));
        return result;
    }

    // Flee状態では実際に移動条件を満たしているかチェック
    if (state_ == AIState::Flee) {
        // カメラ内にいたら移動しない
        if (camera_) {
            Vector2 screenPos = camera_->WorldToScreen(currentPos);
            float viewW = camera_->GetViewportWidth();
            float viewH = camera_->GetViewportHeight();
            bool isVisible = screenPos.x >= kVisibilityMargin && screenPos.x <= viewW - kVisibilityMargin &&
                             screenPos.y >= kVisibilityMargin && screenPos.y <= viewH - kVisibilityMargin;
            if (isVisible) {
                return false;  // カメラ内なので移動しない
            }
        }
        // プレイヤーに十分近い場合も停止
        if (player_) {
            float distToPlayer = (player_->GetPosition() - currentPos).Length();
            if (distToPlayer <= fleeStopDistance_) {
                return false;
            }
        }
        return true;  // 移動条件を満たしている
    }

    // 通常は停止距離以上で移動中
    return distance > GameConstants::kLoveStopDistance;
}

//----------------------------------------------------------------------------
void GroupAI::NotifyMovementChange()
{
    if (!owner_) return;

    bool isMoving = IsMoving();

    // 状態変化があれば全個体に通知
    if (wasMoving_ != isMoving) {
        // LOG_DEBUG("[NotifyMovementChange] " + owner_->GetId() +
        //           " isMoving changed: " + (wasMoving_ ? "true" : "false") +
        //           " -> " + (isMoving ? "true" : "false"));
        wasMoving_ = isMoving;

        for (Individual* ind : owner_->GetAliveIndividuals()) {
            ind->SetGroupMoving(isMoving);
        }
    }
}

//----------------------------------------------------------------------------
bool GroupAI::CheckLovePartnerDistance() const
{
    if (!owner_) return false;

    Vector2 myPos = owner_->GetPosition();

    // プレイヤーとのLove縁チェック
    if (player_) {
        BondableEntity groupEntity = owner_;
        BondableEntity playerEntity = player_;
        const EdgeData* edge = RelationshipFacade::Get().GetEdge(groupEntity, playerEntity);
        if (edge && edge->type == BondType::Love) {
            float dist = (player_->GetPosition() - myPos).Length();
            if (dist > GameConstants::kLoveInterruptDistance) {
                return true;
            }
        }
    }

    // グループ同士のLove縁チェック
    std::vector<Group*> loveCluster = RelationshipFacade::Get().GetLoveCluster(owner_);
    if (loveCluster.size() > 1) {
        for (Group* partner : loveCluster) {
            if (partner == owner_) continue;
            float dist = (partner->GetPosition() - myPos).Length();
            if (dist > GameConstants::kLoveInterruptDistance) {
                return true;
            }
        }
    }

    return false;
}

//----------------------------------------------------------------------------
bool GroupAI::HasLoveBondWithPlayer() const
{
    if (!owner_ || !player_) return false;

    BondableEntity groupEntity = owner_;
    BondableEntity playerEntity = player_;
    const EdgeData* edge = RelationshipFacade::Get().GetEdge(groupEntity, playerEntity);
    return edge && edge->type == BondType::Love;
}

//----------------------------------------------------------------------------
void GroupAI::OnGroupDefeated(Group* defeatedGroup)
{
    // ターゲットが倒されたグループならクリア
    if (std::holds_alternative<Group*>(target_)) {
        Group* currentTarget = std::get<Group*>(target_);
        if (currentTarget == defeatedGroup) {
            LOG_INFO("[GroupAI] " + (owner_ ? owner_->GetId() : "unknown") +
                     " target defeated, clearing");
            ClearTarget();
        }
    }
}

//----------------------------------------------------------------------------
bool GroupAI::IsInCameraView(float margin) const
{
    if (!camera_ || !owner_) return false;

    Vector2 minBounds, maxBounds;
    camera_->GetWorldBounds(minBounds, maxBounds);
    Vector2 pos = owner_->GetPosition();

    return (pos.x >= minBounds.x - margin && pos.x <= maxBounds.x + margin &&
            pos.y >= minBounds.y - margin && pos.y <= maxBounds.y + margin);
}

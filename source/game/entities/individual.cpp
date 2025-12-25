//----------------------------------------------------------------------------
//! @file   individual.cpp
//! @brief  Individual基底クラス実装
//----------------------------------------------------------------------------
#include "individual.h"
#include "group.h"
#include "player.h"
#include "game/ai/group_ai.h"
#include "game/systems/bind_system.h"
#include "game/systems/friends_damage_sharing.h"
#include "game/systems/time_manager.h"
#include "game/bond/bondable_entity.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/c_systems/collision_manager.h"
#include "engine/c_systems/collision_layers.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
Individual::Individual(const std::string& id)
    : id_(id)
{
}

//----------------------------------------------------------------------------
Individual::~Individual()
{
    Shutdown();
}

//----------------------------------------------------------------------------
void Individual::Initialize(const Vector2& position)
{
    // GameObject作成
    gameObject_ = std::make_unique<GameObject>(id_);

    // Transform2D
    transform_ = gameObject_->AddComponent<Transform2D>();
    transform_->SetPosition(position);

    // SpriteRenderer
    sprite_ = gameObject_->AddComponent<SpriteRenderer>();

    // テクスチャセットアップ（派生クラスで実装）
    SetupTexture();

    // Animator（派生クラスで設定された値を使用）
    if (animRows_ > 1 || animCols_ > 1) {
        animator_ = gameObject_->AddComponent<Animator>(animRows_, animCols_, animFrameInterval_);
        SetupAnimator();
    }

    // AnimationController
    SetupAnimationController();

    // Collider
    SetupCollider();

    // 前フレーム位置を初期化
    prevPosition_ = position;

    LOG_INFO("[Individual] " + id_ + " initialized");
}

//----------------------------------------------------------------------------
void Individual::Shutdown()
{
    gameObject_.reset();
    transform_ = nullptr;
    sprite_ = nullptr;
    animator_ = nullptr;
    collider_ = nullptr;
    texture_.reset();
    ownerGroup_ = nullptr;
    attackTarget_ = nullptr;
}

//----------------------------------------------------------------------------
void Individual::Update(float dt)
{
    if (!gameObject_) return;

    // 時間スケール適用
    float scaledDt = TimeManager::Get().GetScaledDeltaTime(dt);

    // 時間停止中は更新しない
    if (scaledDt <= 0.0f) {
        return;
    }

    // 実際の位置変化を検出（前フレームの位置と比較）
    constexpr float kActualMoveThreshold = 0.5f;
    Vector2 currentPos = GetPosition();
    float actualMoveDist = (currentPos - prevPosition_).Length();
    isActuallyMoving_ = (actualMoveDist > kActualMoveThreshold);

    // 死亡状態でもアニメは更新
    if (!IsAlive()) {
        animationController_.RequestState(AnimationState::Death);
        animationController_.Update(scaledDt);
        gameObject_->Update(scaledDt);
        prevPosition_ = GetPosition();
        return;
    }

    // 攻撃クールダウン更新
    UpdateAttackCooldown(scaledDt);

    // 攻撃持続タイマー更新
    if (isAttacking_ && attackDurationTimer_ > 0.0f) {
        attackDurationTimer_ -= scaledDt;
        if (attackDurationTimer_ <= 0.0f) {
            // 攻撃終了
            EndAttack();
        }
    }

    // 行動状態を更新（グループAIの状態に基づく）
    UpdateAction();

    // 目標速度を計算（行動状態に基づく）
    UpdateDesiredVelocity();

    // 実際の速度 = 目標速度 + 分離オフセット
    Vector2 actualVelocity = desiredVelocity_ + separationOffset_;

    // 位置更新（スケール済み時間で）
    if (transform_ && (actualVelocity.x != 0.0f || actualVelocity.y != 0.0f)) {
        Vector2 pos = transform_->GetPosition();
        pos.x += actualVelocity.x * scaledDt;
        pos.y += actualVelocity.y * scaledDt;
        transform_->SetPosition(pos);
    }

    // 向き更新（意図ベース）
    UpdateFacingDirection();

    // AnimationController更新（アニメ終了検出→ロック解除）
    animationController_.Update(scaledDt);

    // アニメーション状態決定（意図ベース）- ロック解除後に実行
    AnimationState animState = DetermineAnimationState();
    animationController_.RequestState(animState);

    // GameObjectの更新（Animatorの更新など、スケール済み時間で）
    gameObject_->Update(scaledDt);

    // 前フレーム位置を更新
    prevPosition_ = GetPosition();
}

//----------------------------------------------------------------------------
void Individual::Render(SpriteBatch& spriteBatch)
{
    if (!transform_ || !sprite_ || !IsAlive()) return;

    if (animator_) {
        spriteBatch.Draw(*sprite_, *transform_, *animator_);
    } else {
        spriteBatch.Draw(*sprite_, *transform_);
    }
}

//----------------------------------------------------------------------------
void Individual::AttackPlayer(Player* target)
{
    // デフォルト実装: 直接ダメージ（近接攻撃）
    if (!target || !target->IsAlive()) return;
    if (!IsAlive()) return;

    target->TakeDamage(attackDamage_);
    LOG_INFO("[Individual] " + id_ + " attacks Player");
}

//----------------------------------------------------------------------------
void Individual::TakeDamage(float damage)
{
    if (!IsAlive()) return;

    // フレンズ縁による分配ダメージを受信中でなければ、分配処理を試みる
    if (!isReceivingSharedDamage_) {
        FriendsDamageSharing& friendsSystem = FriendsDamageSharing::Get();
        if (friendsSystem.HasFriendsPartners(ownerGroup_)) {
            // フレンズ縁で繋がっているのでダメージを分配
            friendsSystem.ApplyDamageWithSharing(this, damage);
            return;
        }
    }

    // 直接ダメージ適用（分配済み or フレンズ縁なし）
    hp_ -= damage;
    if (hp_ < 0.0f) {
        hp_ = 0.0f;
    }

    if (hp_ <= 0.0f) {
        action_ = IndividualAction::Death;
        LOG_INFO("[Individual] " + id_ + " died");
        // TODO: OnIndividualDiedイベント発行
    }
}

//----------------------------------------------------------------------------
Vector2 Individual::GetPosition() const
{
    if (transform_) {
        return transform_->GetPosition();
    }
    return Vector2::Zero;
}

//----------------------------------------------------------------------------
void Individual::SetPosition(const Vector2& position)
{
    if (transform_) {
        transform_->SetPosition(position);
    }
}

//----------------------------------------------------------------------------
void Individual::SetupAnimator()
{
    // デフォルト実装: 各行1フレーム
    // 派生クラスでオーバーライドして具体的なアニメーション設定
}

//----------------------------------------------------------------------------
void Individual::SetupAnimationController()
{
    // Animatorを設定
    animationController_.SetAnimator(animator_);

    // デフォルトの行マッピング（派生クラスでオーバーライド可能）
    // Row 0: Idle, Row 1: Walk, Row 2: Attack, Row 3: Death
    animationController_.SetRowMapping(AnimationState::Idle, 0);
    animationController_.SetRowMapping(AnimationState::Walk, 1);
    animationController_.SetRowMapping(AnimationState::Attack, 2);
    animationController_.SetRowMapping(AnimationState::Death, 3);

    // アニメーション終了時のコールバック
    animationController_.SetOnAnimationFinished([this]() {
        // 攻撃終了処理
        if (action_ == IndividualAction::Attack) {
            EndAttack();
        }
    });
}

//----------------------------------------------------------------------------
void Individual::SetupCollider()
{
    if (!gameObject_) return;

    collider_ = gameObject_->AddComponent<Collider2D>(Vector2(kDefaultColliderSize, kDefaultColliderSize));
    collider_->SetLayer(CollisionLayer::Individual);
    collider_->SetMask(CollisionLayer::IndividualMask);

    // 衝突コールバック：Playerとの衝突時に結びシステムを処理
    collider_->SetOnCollisionEnter([this](Collider2D* /*self*/, Collider2D* other) {
        // Playerレイヤーとの衝突か確認
        uint32_t otherLayer = CollisionManager::Get().GetLayer(other->GetHandle());
        if ((otherLayer & CollisionLayer::Player) == 0) return;

        // 結びモードでなければ無視
        if (!BindSystem::Get().IsEnabled()) return;

        // 所属グループを取得してマーク
        Group* group = GetOwnerGroup();
        if (group == nullptr || group->IsDefeated()) return;

        BondableEntity entity = group;
        bool created = BindSystem::Get().MarkEntity(entity);
        if (created) {
            LOG_INFO("[Individual] Bond created via collision!");
        } else if (BindSystem::Get().HasMark()) {
            LOG_INFO("[Individual] Marked group: " + group->GetId());
        }
    });
}

//----------------------------------------------------------------------------
void Individual::CalculateSeparation(const std::vector<Individual*>& others)
{
    separationOffset_ = Vector2::Zero;

    if (!IsAlive()) return;

    Vector2 myPos = GetPosition();

    for (Individual* other : others) {
        if (!other || other == this || !other->IsAlive()) continue;

        Vector2 otherPos = other->GetPosition();
        Vector2 diff = Vector2(myPos.x - otherPos.x, myPos.y - otherPos.y);
        float distance = diff.Length();

        // 距離が分離半径内かつ0より大きい場合
        if (distance < separationRadius_ && distance > kMinDistanceThreshold) {
            // 離れる方向に力を加える
            diff.Normalize();
            float strength = (separationRadius_ - distance) / separationRadius_;
            separationOffset_.x += diff.x * strength * separationForce_;
            separationOffset_.y += diff.y * strength * separationForce_;
        }
    }
}

//----------------------------------------------------------------------------
void Individual::UpdateAction()
{
    // 前フレームのアクションを保存
    prevAction_ = action_;

    // 死亡チェック
    if (hp_ <= 0.0f) {
        action_ = IndividualAction::Death;
        attackTarget_ = nullptr;
        justEnteredAttackRange_ = false;
        return;
    }

    // 所属グループがない場合はIdle
    if (!ownerGroup_) {
        action_ = IndividualAction::Idle;
        justEnteredAttackRange_ = false;
        return;
    }

    // グループのAI状態を取得
    GroupAI* ai = ownerGroup_->GetAI();
    if (!ai) {
        action_ = IndividualAction::Idle;
        justEnteredAttackRange_ = false;
        return;
    }

    // 攻撃モーション中は状態変えない（AnimationControllerがロック中の場合のみ）
    if (action_ == IndividualAction::Attack && animationController_.IsLocked()) {
        return;
    }

    AIState groupState = ai->GetState();

    // グループがFlee → Walk
    if (groupState == AIState::Flee) {
        action_ = IndividualAction::Walk;
        attackTarget_ = nullptr;
        justEnteredAttackRange_ = false;
        return;
    }

    // グループがWander → Walk（グループは徘徊中で移動している）
    if (groupState == AIState::Wander) {
        action_ = IndividualAction::Walk;
        attackTarget_ = nullptr;
        justEnteredAttackRange_ = false;
        return;
    }

    // グループがSeek → 射程判定

    // ターゲットGroupを取得
    AITarget aiTarget = ai->GetTarget();
    Group* targetGroup = nullptr;

    if (std::holds_alternative<Group*>(aiTarget)) {
        targetGroup = std::get<Group*>(aiTarget);
    }

    // ターゲットがない/無効な場合はWalk（グループは移動中）
    if (!targetGroup || targetGroup->IsDefeated()) {
        action_ = IndividualAction::Walk;
        attackTarget_ = nullptr;
        justEnteredAttackRange_ = false;
        return;
    }

    // ターゲットGroupの中心位置との距離で判定
    Vector2 myPos = GetPosition();
    Vector2 targetPos = targetGroup->GetPosition();
    Vector2 diff = targetPos - myPos;
    float distance = diff.Length();

    float attackRange = GetAttackRange();

    if (distance <= attackRange) {
        // 射程内 → Attack
        if (prevAction_ != IndividualAction::Attack) {
            // 攻撃範囲に入った瞬間
            justEnteredAttackRange_ = true;
            attackCooldown_ = 0.0f;  // クールダウンリセット
        }
        action_ = IndividualAction::Attack;
        // 攻撃開始時にターゲット個体を選択
        SelectAttackTarget();
        // ターゲットが見つかったら攻撃開始
        if (attackTarget_) {
            StartAttack();
        }
    } else {
        // 射程外 → Walk
        action_ = IndividualAction::Walk;
        attackTarget_ = nullptr;
        justEnteredAttackRange_ = false;
    }
}

//----------------------------------------------------------------------------
bool Individual::CanAttackNow() const
{
    // 攻撃範囲に入った直後、またはクールダウン完了
    return justEnteredAttackRange_ || attackCooldown_ <= 0.0f;
}

//----------------------------------------------------------------------------
void Individual::StartAttackCooldown(float duration)
{
    attackCooldown_ = duration;
    justEnteredAttackRange_ = false;
}

//----------------------------------------------------------------------------
void Individual::UpdateAttackCooldown(float dt)
{
    if (attackCooldown_ > 0.0f) {
        attackCooldown_ -= dt;
    }
}

//----------------------------------------------------------------------------
void Individual::UpdateDesiredVelocity()
{
    desiredVelocity_ = Vector2::Zero;

    if (!IsAlive() || !ownerGroup_) return;

    // 攻撃中は移動しない
    if (isAttacking_) {
        return;
    }

    switch (action_) {
    case IndividualAction::Idle:
        {
            // Formationスロットに留まる
            Formation& formation = ownerGroup_->GetFormation();
            Vector2 targetPos = formation.GetSlotPosition(this);
            Vector2 myPos = GetPosition();
            Vector2 diff = targetPos - myPos;
            float distance = diff.Length();

            // 閾値以上離れていれば移動
            if (distance > kFormationThreshold) {
                diff.Normalize();
                desiredVelocity_ = diff * moveSpeed_;
            }
        }
        break;

    case IndividualAction::Walk:
        {
            // Formation維持しながら移動
            Formation& formation = ownerGroup_->GetFormation();
            Vector2 targetPos = formation.GetSlotPosition(this);
            Vector2 myPos = GetPosition();
            Vector2 diff = targetPos - myPos;
            float distance = diff.Length();

            if (distance > kMinDistanceThreshold) {
                diff.Normalize();
                desiredVelocity_ = diff * moveSpeed_;
            }
        }
        break;

    case IndividualAction::Attack:
        {
            // Formation無視、個別にターゲットへ
            if (!attackTarget_ || !attackTarget_->IsAlive()) {
                // ターゲットがいなければ停止
                desiredVelocity_ = Vector2::Zero;
            } else {
                Vector2 myPos = GetPosition();
                Vector2 targetPos = attackTarget_->GetPosition();
                Vector2 diff = targetPos - myPos;
                float distance = diff.Length();

                float attackRange = GetAttackRange();

                if (distance > attackRange) {
                    // 射程外なら近づく
                    diff.Normalize();
                    desiredVelocity_ = diff * moveSpeed_;
                } else {
                    // 射程内なら停止
                    desiredVelocity_ = Vector2::Zero;
                }
            }
        }
        break;

    case IndividualAction::Death:
        desiredVelocity_ = Vector2::Zero;
        break;
    }
}

//----------------------------------------------------------------------------
bool Individual::GetCurrentAttackTargetPosition(Vector2& outPosition) const
{
    // 基底クラス実装: attackTarget_を使用
    if (attackTarget_ && attackTarget_->IsAlive()) {
        outPosition = attackTarget_->GetPosition();
        return true;
    }

    // フォールバック: グループAIのターゲット
    if (ownerGroup_) {
        GroupAI* ai = ownerGroup_->GetAI();
        if (ai && ai->HasTarget()) {
            outPosition = ai->GetTargetPosition();
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------
void Individual::SelectAttackTarget()
{
    attackTarget_ = nullptr;

    if (!ownerGroup_) return;

    GroupAI* ai = ownerGroup_->GetAI();
    if (!ai) return;

    AITarget aiTarget = ai->GetTarget();
    Group* targetGroup = nullptr;

    if (std::holds_alternative<Group*>(aiTarget)) {
        targetGroup = std::get<Group*>(aiTarget);
    }

    if (!targetGroup || targetGroup->IsDefeated()) return;

    // ターゲットグループからランダムに生存個体を選ぶ
    attackTarget_ = targetGroup->GetRandomAliveIndividual();
}

//----------------------------------------------------------------------------
void Individual::StartAttack()
{
    isAttacking_ = true;
    attackDurationTimer_ = kAttackDuration;  // 攻撃持続時間を設定
    // AnimationControllerが攻撃アニメをロック中になる
    // RequestState(Attack)は Update() で呼ばれる
}

//----------------------------------------------------------------------------
void Individual::EndAttack()
{
    isAttacking_ = false;
    attackDurationTimer_ = 0.0f;

    // アニメーションロックを解除
    animationController_.ForceUnlock();

    // 攻撃終了後、ターゲットが死亡していれば再選択
    if (attackTarget_ && !attackTarget_->IsAlive()) {
        SelectAttackTarget();
    }

    // アクションを非攻撃状態にリセット（呼び出し側で必要に応じて上書き可能）
    action_ = IndividualAction::Idle;
}

//----------------------------------------------------------------------------
void Individual::InterruptAttack()
{
    // 攻撃状態をリセット
    isAttacking_ = false;
    attackDurationTimer_ = 0.0f;
    action_ = IndividualAction::Idle;
    attackTarget_ = nullptr;

    // アニメーションロックを強制解除し、即座にIdleアニメーションをリクエスト
    animationController_.ForceUnlock();
    animationController_.RequestState(AnimationState::Idle);
}

//----------------------------------------------------------------------------
GroupIntent Individual::GetGroupIntent() const
{
    // GroupAIからのイベント通知で設定されたフラグを使用
    if (!isGroupMoving_) {
        return GroupIntent::Idle;
    }

    // グループが移動中の場合、AI状態を返す
    if (!ownerGroup_) {
        return GroupIntent::Idle;
    }

    GroupAI* ai = ownerGroup_->GetAI();
    if (!ai) {
        return GroupIntent::Idle;
    }

    switch (ai->GetState()) {
    case AIState::Wander:
        return GroupIntent::Wander;
    case AIState::Seek:
        return GroupIntent::Seek;
    case AIState::Flee:
        return GroupIntent::Flee;
    default:
        return GroupIntent::Idle;
    }
}

//----------------------------------------------------------------------------
IndividualIntent Individual::GetIndividualIntent() const
{
    // 死亡チェック
    if (!IsAlive()) {
        return IndividualIntent::Dead;
    }

    // 攻撃モーション中
    if (action_ == IndividualAction::Attack) {
        return IndividualIntent::Attacking;
    }

    // ターゲット追跡中（攻撃対象がいて、攻撃範囲外の場合）
    if (attackTarget_ && attackTarget_->IsAlive()) {
        float distToTarget = (attackTarget_->GetPosition() - GetPosition()).Length();
        if (distToTarget > GetAttackRange()) {
            return IndividualIntent::ChasingTarget;
        }
    }

    // スロット到達判定
    if (ownerGroup_) {
        Formation& formation = ownerGroup_->GetFormation();
        Vector2 slotPos = formation.GetSlotPosition(this);
        Vector2 diff = slotPos - GetPosition();
        float dist = diff.Length();

        if (dist >= kFormationThreshold) {
            return IndividualIntent::MovingToSlot;
        }
    }

    return IndividualIntent::AtSlot;
}

//----------------------------------------------------------------------------
AnimationState Individual::DetermineAnimationState() const
{
    IndividualIntent indIntent = GetIndividualIntent();
    GroupIntent grpIntent = GetGroupIntent();

#ifdef _DEBUG
    // デバッグ: 意図とアニメーション状態をログ
    if (++debugLogCounter_ % 60 == 0) {  // 1秒に1回
        std::string indIntentStr;
        switch (indIntent) {
        case IndividualIntent::AtSlot: indIntentStr = "AtSlot"; break;
        case IndividualIntent::MovingToSlot: indIntentStr = "MovingToSlot"; break;
        case IndividualIntent::ChasingTarget: indIntentStr = "ChasingTarget"; break;
        case IndividualIntent::Attacking: indIntentStr = "Attacking"; break;
        case IndividualIntent::Dead: indIntentStr = "Dead"; break;
        }
        std::string grpIntentStr;
        switch (grpIntent) {
        case GroupIntent::Idle: grpIntentStr = "Idle"; break;
        case GroupIntent::Wander: grpIntentStr = "Wander"; break;
        case GroupIntent::Seek: grpIntentStr = "Seek"; break;
        case GroupIntent::Flee: grpIntentStr = "Flee"; break;
        }
        std::string ctrlStateStr;
        switch (animationController_.GetState()) {
        case AnimationState::Idle: ctrlStateStr = "Idle"; break;
        case AnimationState::Walk: ctrlStateStr = "Walk"; break;
        case AnimationState::Attack: ctrlStateStr = "Attack"; break;
        case AnimationState::Death: ctrlStateStr = "Death"; break;
        default: ctrlStateStr = "?"; break;
        }
        LOG_INFO("[AnimState] " + id_ + " IndIntent=" + indIntentStr + " GrpIntent=" + grpIntentStr +
                 " action=" + std::to_string(static_cast<int>(action_)) +
                 " ctrlState=" + ctrlStateStr + " locked=" + (animationController_.IsLocked() ? "Y" : "N"));
    }
#endif

    // 死亡・攻撃は最優先
    if (indIntent == IndividualIntent::Dead) {
        return AnimationState::Death;
    }
    if (indIntent == IndividualIntent::Attacking) {
        return AnimationState::Attack;
    }

    // グループが実際に移動中 → Walk
    if (grpIntent != GroupIntent::Idle) {
        return AnimationState::Walk;
    }

    // グループ停止中：個体の状態で判定
    if (indIntent == IndividualIntent::MovingToSlot ||
        indIntent == IndividualIntent::ChasingTarget) {
        return AnimationState::Walk;
    }

    // 実際に位置が変化している場合もWalk（LovePull等による移動）
    if (isActuallyMoving_) {
        return AnimationState::Walk;
    }

    return AnimationState::Idle;
}

//----------------------------------------------------------------------------
void Individual::UpdateFacingDirection()
{
    if (!sprite_) return;

    IndividualIntent intent = GetIndividualIntent();

    // 1. 攻撃中はターゲット方向（仮想メソッドで派生クラス対応）
    if (intent == IndividualIntent::Attacking) {
        Vector2 targetPos;
        if (GetCurrentAttackTargetPosition(targetPos)) {
            float dx = targetPos.x - GetPosition().x;
            if (dx > 0.0f) {
                facingRight_ = true;
            } else if (dx < 0.0f) {
                facingRight_ = false;
            }
        }
    }
    // 2. グループ移動中はグループの移動先方向を向く（個体速度ではなくAIターゲット方向）
    else if (isGroupMoving_ && ownerGroup_) {
        GroupAI* ai = ownerGroup_->GetAI();
        if (ai) {
            Vector2 targetPos = ai->GetTargetPosition();
            Vector2 groupPos = ownerGroup_->GetPosition();
            float dx = targetPos.x - groupPos.x;
            if (dx > 0.0f) {
                facingRight_ = true;
            } else if (dx < 0.0f) {
                facingRight_ = false;
            }
        }
    }
    // 3. 実際に移動中（LovePull等）は移動方向を向く
    else if (isActuallyMoving_) {
        Vector2 currentPos = GetPosition();
        float dx = currentPos.x - prevPosition_.x;
        if (dx > 0.0f) {
            facingRight_ = true;
        } else if (dx < 0.0f) {
            facingRight_ = false;
        }
    }
    // それ以外は現在の向きを維持

    sprite_->SetFlipX(facingRight_);
}

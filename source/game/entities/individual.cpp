//----------------------------------------------------------------------------
//! @file   individual.cpp
//! @brief  Individual基底クラス実装
//----------------------------------------------------------------------------
#include "individual.h"
#include "group.h"
#include "player.h"
#include "game/ai/group_ai.h"
#include "engine/c_systems/sprite_batch.h"
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

    // 死亡状態でもアニメは更新
    if (!IsAlive()) {
        // 死亡アニメをリクエスト
        animationController_.RequestState(AnimationState::Death);
        animationController_.Update(dt);
        gameObject_->Update(dt);
        return;
    }

    // 実際の速度 = 目標速度 + 分離オフセット
    Vector2 actualVelocity = desiredVelocity_ + separationOffset_;

    // 位置更新
    if (transform_ && (actualVelocity.x != 0.0f || actualVelocity.y != 0.0f)) {
        Vector2 pos = transform_->GetPosition();
        pos.x += actualVelocity.x * dt;
        pos.y += actualVelocity.y * dt;
        transform_->SetPosition(pos);
    }

    // IndividualActionからAnimationStateへ変換してリクエスト
    AnimationState animState = AnimationState::Idle;
    switch (action_) {
    case IndividualAction::Idle:
        animState = AnimationState::Idle;
        break;
    case IndividualAction::Walk:
        animState = AnimationState::Walk;
        break;
    case IndividualAction::Attack:
        animState = AnimationState::Attack;
        break;
    case IndividualAction::Death:
        animState = AnimationState::Death;
        break;
    }
    animationController_.RequestState(animState);

    // AnimationController更新（アニメ終了検出）
    animationController_.Update(dt);

    // GameObjectの更新（Animatorの更新など）
    gameObject_->Update(dt);
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

    collider_ = gameObject_->AddComponent<Collider2D>(Vector2(32, 32));
    collider_->SetLayer(0x04);  // Individual用レイヤー
    collider_->SetMask(0x04);   // 他のIndividualと衝突

    // 衝突コールバック（デバッグ用）
    collider_->SetOnCollisionEnter([this](Collider2D* /*self*/, Collider2D* /*other*/) {
        // LOG_INFO("[Individual] " + id_ + " collision enter");
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
        if (distance < separationRadius_ && distance > 0.001f) {
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
    // 死亡チェック
    if (hp_ <= 0.0f) {
        action_ = IndividualAction::Death;
        attackTarget_ = nullptr;
        return;
    }

    // 所属グループがない場合はIdle
    if (!ownerGroup_) {
        action_ = IndividualAction::Idle;
        return;
    }

    // グループのAI状態を取得
    GroupAI* ai = ownerGroup_->GetAI();
    if (!ai) {
        action_ = IndividualAction::Idle;
        return;
    }

    AIState groupState = ai->GetState();

    // グループがFlee → Walk
    if (groupState == AIState::Flee) {
        action_ = IndividualAction::Walk;
        attackTarget_ = nullptr;
        return;
    }

    // グループがWander → Idle
    if (groupState == AIState::Wander) {
        action_ = IndividualAction::Idle;
        attackTarget_ = nullptr;
        return;
    }

    // グループがSeek → 射程判定
    // 攻撃モーション中は状態変えない
    if (action_ == IndividualAction::Attack && isAttacking_) {
        return;
    }

    // ターゲットGroupを取得
    AITarget aiTarget = ai->GetTarget();
    Group* targetGroup = nullptr;

    if (std::holds_alternative<Group*>(aiTarget)) {
        targetGroup = std::get<Group*>(aiTarget);
    }

    if (!targetGroup || targetGroup->IsDefeated()) {
        action_ = IndividualAction::Idle;
        attackTarget_ = nullptr;
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
        if (action_ != IndividualAction::Attack) {
            action_ = IndividualAction::Attack;
            // 攻撃開始時にターゲット個体を選択
            SelectAttackTarget();
            // ターゲットが見つかったら攻撃開始
            if (attackTarget_) {
                StartAttack();
            }
        }
    } else {
        // 射程外 → Walk
        action_ = IndividualAction::Walk;
        attackTarget_ = nullptr;
    }
}

//----------------------------------------------------------------------------
void Individual::UpdateDesiredVelocity()
{
    desiredVelocity_ = Vector2::Zero;

    if (!IsAlive() || !ownerGroup_) return;

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
            constexpr float kThreshold = 5.0f;
            if (distance > kThreshold) {
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

            if (distance > 0.001f) {
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
    // AnimationControllerが攻撃アニメをロック中になる
    // RequestState(Attack)は Update() で呼ばれる
}

//----------------------------------------------------------------------------
void Individual::EndAttack()
{
    isAttacking_ = false;

    // 攻撃終了後、ターゲットが死亡していれば再選択
    if (attackTarget_ && !attackTarget_->IsAlive()) {
        SelectAttackTarget();

        // グループ全滅ならIdle/Walk に戻る
        if (!attackTarget_) {
            action_ = IndividualAction::Idle;
        }
    }
}

//----------------------------------------------------------------------------
//! @file   individual_state_machine.cpp
//! @brief  個体アニメーション状態マシン実装
//----------------------------------------------------------------------------
#include "individual_state_machine.h"
#include "animation_decision_context.h"
#include "attack_behavior.h"
#include "engine/component/animator.h"
#include "game/entities/individual.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
IndividualStateMachine::IndividualStateMachine(Individual* owner, Animator* animator)
    : owner_(owner)
    , animator_(animator)
{
    // デフォルト行マッピング（Idle=0, Walk=1, Attack*=2, Death=3）
    rowMapping_[static_cast<size_t>(AnimState::Idle)] = 0;
    rowMapping_[static_cast<size_t>(AnimState::Walk)] = 1;
    rowMapping_[static_cast<size_t>(AnimState::AttackWindup)] = 2;
    rowMapping_[static_cast<size_t>(AnimState::AttackActive)] = 2;
    rowMapping_[static_cast<size_t>(AnimState::AttackRecovery)] = 2;
    rowMapping_[static_cast<size_t>(AnimState::Death)] = 3;

    // 初期状態（Idle）のAnimatorセットアップ
    if (animator_) {
        animator_->SetRow(rowMapping_[static_cast<size_t>(AnimState::Idle)]);
        animator_->SetLooping(true);
        animator_->SetPlaying(true);
    }
}

//----------------------------------------------------------------------------
void IndividualStateMachine::SetAttackBehavior(std::unique_ptr<IAttackBehavior> behavior)
{
    attackBehavior_ = std::move(behavior);
}

//----------------------------------------------------------------------------
void IndividualStateMachine::SetRowMapping(AnimState state, uint8_t row)
{
    if (state < AnimState::Count) {
        rowMapping_[static_cast<size_t>(state)] = row;
    }
}

//----------------------------------------------------------------------------
void IndividualStateMachine::SetOnAttackEnd(std::function<void()> callback)
{
    onAttackEnd_ = std::move(callback);
}

//----------------------------------------------------------------------------
void IndividualStateMachine::SetOnDeath(std::function<void()> callback)
{
    onDeath_ = std::move(callback);
}

//----------------------------------------------------------------------------
bool IndividualStateMachine::RequestTransition(AnimState newState)
{
    // 死亡は最優先
    if (newState == AnimState::Death) {
        EnterState(AnimState::Death);
        return true;
    }

    // ロック中は遷移不可
    if (IsLocked()) {
        return false;
    }

    // 同じ状態なら何もしない
    if (currentState_ == newState) {
        return false;
    }

    // 攻撃フェーズへの直接遷移は StartAttack() 経由のみ
    if (IsAttackState(newState)) {
        return false;
    }

    EnterState(newState);
    return true;
}

//----------------------------------------------------------------------------
void IndividualStateMachine::UpdateWithContext(const AnimationDecisionContext& ctx)
{
    // ロック中または攻撃中は判定しない
    if (IsLocked() || IsAttacking()) {
        walkRequestFrames_ = 0;
        idleRequestFrames_ = 0;
        return;
    }

    // コンテキストのShouldWalk()を使用して判定
    bool shouldWalk = ctx.ShouldWalk();

    // ヒステリシス: 一定フレーム継続でのみ遷移
    if (shouldWalk) {
        walkRequestFrames_++;
        idleRequestFrames_ = 0;

        if (currentState_ == AnimState::Idle &&
            walkRequestFrames_ >= kWalkHysteresisFrames) {
            EnterState(AnimState::Walk);
        }
    } else {
        idleRequestFrames_++;
        walkRequestFrames_ = 0;

        if (currentState_ == AnimState::Walk &&
            idleRequestFrames_ >= kIdleHysteresisFrames) {
            EnterState(AnimState::Idle);
        }
    }
}

//----------------------------------------------------------------------------
bool IndividualStateMachine::StartAttack(Individual* target)
{
    if (!attackBehavior_) {
        LOG_WARN("[IndividualStateMachine] No attack behavior set");
        return false;
    }

    // ロック中は攻撃開始不可
    if (IsLocked()) {
        return false;
    }

    // 攻撃開始
    attackBehavior_->OnAttackStart(owner_, target);
    damageFrameFired_ = false;

    // Windup持続時間が0なら直接Activeへ
    float windupDuration = attackBehavior_->GetWindupDuration();
    if (windupDuration <= 0.0f) {
        EnterState(AnimState::AttackActive);
    } else {
        EnterState(AnimState::AttackWindup);
    }

    return true;
}

//----------------------------------------------------------------------------
bool IndividualStateMachine::StartAttackPlayer(Player* target)
{
    if (!attackBehavior_) {
        LOG_WARN("[IndividualStateMachine] No attack behavior set");
        return false;
    }

    // ロック中は攻撃開始不可
    if (IsLocked()) {
        return false;
    }

    // 攻撃開始
    attackBehavior_->OnAttackStartPlayer(owner_, target);
    damageFrameFired_ = false;

    // Windup持続時間が0なら直接Activeへ
    float windupDuration = attackBehavior_->GetWindupDuration();
    if (windupDuration <= 0.0f) {
        EnterState(AnimState::AttackActive);
    } else {
        EnterState(AnimState::AttackWindup);
    }

    return true;
}

//----------------------------------------------------------------------------
bool IndividualStateMachine::ForceInterrupt()
{
    // 死亡中は割り込み不可
    if (currentState_ == AnimState::Death) {
        return false;
    }

    // 攻撃中でない場合は何もしない
    if (!IsAttacking()) {
        return true;
    }

    // 最小攻撃時間が経過していない場合は割り込み不可
    if (!CanInterruptAttack()) {
        return false;
    }

    // 攻撃を中断
    if (attackBehavior_) {
        attackBehavior_->OnAttackInterrupt();
    }

    EnterState(AnimState::Idle);
    return true;
}

//----------------------------------------------------------------------------
void IndividualStateMachine::Die()
{
    EnterState(AnimState::Death);
}

//----------------------------------------------------------------------------
void IndividualStateMachine::Update(float dt)
{
    stateTimer_ += dt;

    // 攻撃フェーズの更新
    if (IsAttacking()) {
        UpdateAttackPhase(dt);
    }

    // Animatorの更新はGameObject::Update()で行われる
}

//----------------------------------------------------------------------------
bool IndividualStateMachine::CanInterruptAttack() const
{
    if (!IsAttacking()) {
        return true;
    }

    // AttackRecoveryフェーズ かつ 最小時間経過
    if (currentState_ == AnimState::AttackRecovery) {
        return true;
    }

    // 攻撃開始からの累計時間をチェック
    // （簡略化: stateTimer_が各フェーズ時間の合計に対してチェック）
    return stateTimer_ >= kMinAttackTime;
}

//----------------------------------------------------------------------------
void IndividualStateMachine::EnterState(AnimState state)
{
    if (currentState_ == state) return;

    // 前の状態から出る
    ExitState(currentState_);

    // 新しい状態に入る
    currentState_ = state;
    stateTimer_ = 0.0f;

    // Animator行を適用
    ApplyAnimatorRow(state);

    // 状態別の初期化
    switch (state) {
    case AnimState::Idle:
    case AnimState::Walk:
        walkRequestFrames_ = 0;
        idleRequestFrames_ = 0;
        if (animator_) {
            animator_->SetLooping(true);
            animator_->SetPlaying(true);
        }
        break;

    case AnimState::AttackWindup:
    case AnimState::AttackActive:
        if (animator_) {
            animator_->SetLooping(false);
            animator_->Reset();
            animator_->SetPlaying(true);
        }
        break;

    case AnimState::AttackRecovery:
        // Recovery中はアニメーションを停止または最終フレーム維持
        break;

    case AnimState::Death:
        if (animator_) {
            animator_->SetLooping(false);
            animator_->Reset();
            animator_->SetPlaying(true);
        }
        if (onDeath_) {
            onDeath_();
        }
        break;

    default:
        break;
    }
}

//----------------------------------------------------------------------------
void IndividualStateMachine::ExitState(AnimState state)
{
    // 攻撃フェーズからの離脱時
    if (IsAttackState(state) && !IsAttackState(currentState_)) {
        // 攻撃終了処理
        if (attackBehavior_) {
            attackBehavior_->OnAttackEnd();
        }
        if (onAttackEnd_) {
            onAttackEnd_();
        }
    }
}

//----------------------------------------------------------------------------
void IndividualStateMachine::UpdateAttackPhase(float dt)
{
    if (!attackBehavior_) return;

    // 攻撃行動を更新
    attackBehavior_->OnAttackUpdate(dt, currentState_, stateTimer_);

    switch (currentState_) {
    case AnimState::AttackWindup: {
        float windupDuration = attackBehavior_->GetWindupDuration();
        if (stateTimer_ >= windupDuration) {
            EnterState(AnimState::AttackActive);
        }
        break;
    }

    case AnimState::AttackActive: {
        // ダメージフレームチェック
        float damageFrameTime = attackBehavior_->GetDamageFrameTime();
        if (!damageFrameFired_ && stateTimer_ >= damageFrameTime) {
            damageFrameFired_ = attackBehavior_->OnDamageFrame();
        }

        // Active終了チェック
        float activeDuration = attackBehavior_->GetActiveDuration();
        if (stateTimer_ >= activeDuration) {
            EnterState(AnimState::AttackRecovery);
        }
        break;
    }

    case AnimState::AttackRecovery: {
        float recoveryDuration = attackBehavior_->GetRecoveryDuration();
        if (stateTimer_ >= recoveryDuration) {
            FinishAttack();
        }
        break;
    }

    default:
        break;
    }
}

//----------------------------------------------------------------------------
void IndividualStateMachine::ApplyAnimatorRow(AnimState state)
{
    if (!animator_) return;

    uint8_t row = rowMapping_[static_cast<size_t>(state)];
    animator_->SetRow(row);
}

//----------------------------------------------------------------------------
void IndividualStateMachine::FinishAttack()
{
    // 攻撃終了
    if (attackBehavior_) {
        attackBehavior_->OnAttackEnd();
    }
    if (onAttackEnd_) {
        onAttackEnd_();
    }

    // Idle状態へ（次のUpdate()でWalk/Idleが適切に選択される）
    EnterState(AnimState::Idle);
}

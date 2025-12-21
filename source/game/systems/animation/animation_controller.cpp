//----------------------------------------------------------------------------
//! @file   animation_controller.cpp
//! @brief  AnimationController実装
//----------------------------------------------------------------------------
#include "animation_controller.h"
#include "engine/component/animator.h"

//----------------------------------------------------------------------------
AnimationController::AnimationController()
    : currentState_(AnimationState::Idle)
    , isLocked_(false)
    , wasPlaying_(true)
{
}

//----------------------------------------------------------------------------
void AnimationController::SetRowMapping(AnimationState state, uint8_t row)
{
    uint8_t index = static_cast<uint8_t>(state);
    if (index < 4) {
        rowMapping_[index] = row;
    }
}

//----------------------------------------------------------------------------
void AnimationController::RequestState(AnimationState newState)
{
    // 死亡は最優先
    if (newState == AnimationState::Death) {
        PlayAnimation(AnimationState::Death);
        isLocked_ = true;
        return;
    }

    // ロック中は無視（攻撃モーション中など）
    if (isLocked_) {
        return;
    }

    // 攻撃はロックする
    if (newState == AnimationState::Attack) {
        PlayAnimation(AnimationState::Attack);
        isLocked_ = true;
        return;
    }

    // Idle / Walk は自由に切り替え
    if (currentState_ != newState) {
        PlayAnimation(newState);
    }
}

//----------------------------------------------------------------------------
void AnimationController::Update(float /*dt*/)
{
    if (!animator_) return;

    // アニメ終了検出（再生中→停止の遷移）
    bool isPlaying = animator_->IsPlaying();

    if (wasPlaying_ && !isPlaying) {
        // 非ループアニメが終了した
        OnAnimationFinished();
    }

    wasPlaying_ = isPlaying;
}

//----------------------------------------------------------------------------
bool AnimationController::IsPlaying() const
{
    if (!animator_) return false;
    return animator_->IsPlaying();
}

//----------------------------------------------------------------------------
void AnimationController::PlayAnimation(AnimationState state)
{
    if (!animator_) return;

    currentState_ = state;

    // 行番号を取得
    uint8_t row = rowMapping_[static_cast<uint8_t>(state)];
    animator_->SetRow(row);

    // ループ設定
    switch (state) {
    case AnimationState::Idle:
    case AnimationState::Walk:
        animator_->SetLooping(true);
        break;

    case AnimationState::Attack:
    case AnimationState::Death:
        animator_->SetLooping(false);
        break;
    }

    // 再生開始
    animator_->Reset();

    wasPlaying_ = true;
}

//----------------------------------------------------------------------------
void AnimationController::OnAnimationFinished()
{
    isLocked_ = false;

    // コールバック呼び出し
    if (onAnimationFinished_) {
        onAnimationFinished_();
    }
}

//----------------------------------------------------------------------------
//! @file   individual_state_machine.h
//! @brief  個体アニメーション状態マシン
//----------------------------------------------------------------------------
#pragma once

#include "anim_state.h"
#include "attack_behavior.h"
#include <memory>
#include <array>
#include <functional>

// 前方宣言
class Individual;
class Player;
class Animator;
struct AnimationDecisionContext;

//----------------------------------------------------------------------------
//! @brief 個体アニメーション状態マシン
//! @details 状態管理を一元化し、攻撃行動を抽象化
//----------------------------------------------------------------------------
class IndividualStateMachine
{
public:
    //! @brief コンストラクタ
    //! @param owner 所有者（Individual）
    //! @param animator Animatorコンポーネント（nullptr可）
    IndividualStateMachine(Individual* owner, Animator* animator);

    //! @brief デストラクタ
    ~IndividualStateMachine() = default;

    //------------------------------------------------------------------------
    // 設定
    //------------------------------------------------------------------------

    //! @brief 攻撃行動を設定
    //! @param behavior 攻撃行動実装
    void SetAttackBehavior(std::unique_ptr<IAttackBehavior> behavior);

    //! @brief Animator行マッピングを設定
    //! @param state アニメーション状態
    //! @param row Animator行番号
    void SetRowMapping(AnimState state, uint8_t row);

    //! @brief 攻撃終了時コールバックを設定
    //! @param callback コールバック関数
    void SetOnAttackEnd(std::function<void()> callback);

    //! @brief 死亡時コールバックを設定
    //! @param callback コールバック関数
    void SetOnDeath(std::function<void()> callback);

    //------------------------------------------------------------------------
    // 状態遷移
    //------------------------------------------------------------------------

    //! @brief 状態遷移をリクエスト
    //! @param newState 新しい状態
    //! @return 遷移が受理されたらtrue
    bool RequestTransition(AnimState newState);

    //! @brief コンテキストを使用してWalk/Idle判定
    //! @param ctx アニメーション判定コンテキスト
    //! @note BuildAnimationContext()で構築したコンテキストを渡す
    void UpdateWithContext(const AnimationDecisionContext& ctx);

    //! @brief 攻撃を開始（Individual対象）
    //! @param target 攻撃対象
    //! @return 攻撃開始できたらtrue
    bool StartAttack(Individual* target);

    //! @brief 攻撃を開始（Player対象）
    //! @param target 攻撃対象プレイヤー
    //! @return 攻撃開始できたらtrue
    bool StartAttackPlayer(Player* target);

    //! @brief 強制割り込み（Love追従などで攻撃中断）
    //! @return 割り込み成功したらtrue
    bool ForceInterrupt();

    //! @brief 死亡状態に遷移
    void Die();

    //------------------------------------------------------------------------
    // 更新
    //------------------------------------------------------------------------

    //! @brief 更新処理
    //! @param dt デルタタイム
    void Update(float dt);

    //------------------------------------------------------------------------
    // 状態取得
    //------------------------------------------------------------------------

    //! @brief 現在の状態を取得
    [[nodiscard]] AnimState GetState() const { return currentState_; }

    //! @brief 割り込み不可状態かどうか
    [[nodiscard]] bool IsLocked() const { return IsLockedState(currentState_); }

    //! @brief 攻撃中かどうか
    [[nodiscard]] bool IsAttacking() const { return IsAttackState(currentState_); }

    //! @brief 死亡状態かどうか
    [[nodiscard]] bool IsDead() const { return currentState_ == AnimState::Death; }

    //! @brief 現在状態の経過時間を取得
    [[nodiscard]] float GetStateTime() const { return stateTimer_; }

    //! @brief 攻撃の最小時間が経過したかどうか
    [[nodiscard]] bool CanInterruptAttack() const;

    //! @brief 攻撃行動を取得
    [[nodiscard]] IAttackBehavior* GetAttackBehavior() const { return attackBehavior_.get(); }

private:
    //------------------------------------------------------------------------
    // 内部処理
    //------------------------------------------------------------------------

    //! @brief 状態に入る
    void EnterState(AnimState state);

    //! @brief 状態遷移時のクリーンアップ処理
    //! @param newState 次に入る状態（攻撃終了判定に使用）
    void HandleStateExit(AnimState newState);

    //! @brief 攻撃フェーズを更新
    void UpdateAttackPhase(float dt);

    //! @brief Animator行を適用
    void ApplyAnimatorRow(AnimState state);

    //! @brief 攻撃終了処理
    void FinishAttack();

    //------------------------------------------------------------------------
    // メンバ変数
    //------------------------------------------------------------------------

    Individual* owner_ = nullptr;
    Animator* animator_ = nullptr;
    std::unique_ptr<IAttackBehavior> attackBehavior_;

    // 状態
    AnimState currentState_ = AnimState::Idle;
    float stateTimer_ = 0.0f;
    bool damageFrameFired_ = false;

    // 攻撃の最小時間（この時間が経過するまで割り込み不可）
    static constexpr float kMinAttackTime = 0.3f;

    // Walk/Idle揺らぎ対策（ヒステリシス）
    int walkRequestFrames_ = 0;
    int idleRequestFrames_ = 0;
    static constexpr int kWalkHysteresisFrames = 3;
    static constexpr int kIdleHysteresisFrames = 5;

    // Animator行マッピング
    std::array<uint8_t, static_cast<size_t>(AnimState::Count)> rowMapping_{};

    // コールバック
    std::function<void()> onAttackEnd_;
    std::function<void()> onDeath_;
};

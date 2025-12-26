//----------------------------------------------------------------------------
//! @file   animation_controller.h
//! @brief  AnimationController - 論理状態とアニメーション再生を仲介
//----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <functional>

// 前方宣言
class Animator;

//----------------------------------------------------------------------------
//! @brief アニメーション状態
//----------------------------------------------------------------------------
enum class AnimationState : uint8_t
{
    Idle,       //!< 待機（ループ、ロックなし）
    Walk,       //!< 移動（ループ、ロックなし）
    Attack,     //!< 攻撃（非ループ、ロックする）
    Death,      //!< 死亡（非ループ、ロックする）

    Count       //!< 状態の総数（配列サイズ用）
};

//----------------------------------------------------------------------------
//! @brief AnimationController - 論理状態とアニメーション再生を仲介
//! @details 割り込み制御により、攻撃中に別アニメが入らないようにする。
//!          Animatorコンポーネントと連携してアニメーションを制御する。
//----------------------------------------------------------------------------
class AnimationController
{
public:
    //! @brief コンストラクタ
    AnimationController();

    //! @brief デストラクタ
    ~AnimationController() = default;

    //------------------------------------------------------------------------
    // 初期化
    //------------------------------------------------------------------------

    //! @brief Animatorを設定
    //! @param animator Animatorコンポーネント
    void SetAnimator(Animator* animator) { animator_ = animator; }

    //! @brief 行番号マッピングを設定
    //! @param state アニメ状態
    //! @param row Animatorの行番号
    void SetRowMapping(AnimationState state, uint8_t row);

    //------------------------------------------------------------------------
    // 状態制御
    //------------------------------------------------------------------------

    //! @brief アニメーション状態変更をリクエスト
    //! @param newState 新しい状態
    //! @details ロック中は無視（死亡は最優先）
    void RequestState(AnimationState newState);

    //! @brief 更新処理（アニメ終了検出）
    //! @param dt デルタタイム
    void Update(float dt);

    //------------------------------------------------------------------------
    // アクセサ
    //------------------------------------------------------------------------

    //! @brief 現在のアニメ状態を取得
    [[nodiscard]] AnimationState GetState() const { return currentState_; }

    //! @brief ロック中か判定
    [[nodiscard]] bool IsLocked() const { return isLocked_; }

    //! @brief ロックを強制解除（攻撃中断時などに使用）
    //! @details Death状態では解除不可
    void ForceUnlock() {
        if (currentState_ == AnimationState::Death) return;
        isLocked_ = false;
    }

    //! @brief アニメーション再生中か判定
    [[nodiscard]] bool IsPlaying() const;

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief アニメーション終了時のコールバックを設定
    void SetOnAnimationFinished(std::function<void()> callback) {
        onAnimationFinished_ = std::move(callback);
    }

private:
    //! @brief アニメーションを再生
    //! @param state アニメ状態
    void PlayAnimation(AnimationState state);

    //! @brief アニメーション終了時の処理
    void OnAnimationFinished();

    // Animatorへの参照
    Animator* animator_ = nullptr;

    // 現在の状態
    AnimationState currentState_ = AnimationState::Idle;

    // ロック中か（Attack/Death中）
    bool isLocked_ = false;

    // 前フレームの再生状態（終了検出用）
    bool wasPlaying_ = true;

    // 行番号マッピング（AnimationState -> Animator Row）
    uint8_t rowMapping_[static_cast<size_t>(AnimationState::Count)] = { 0, 1, 2, 3 };  // デフォルト: Idle=0, Walk=1, Attack=2, Death=3

    // コールバック
    std::function<void()> onAnimationFinished_;
};

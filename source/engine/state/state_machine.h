//----------------------------------------------------------------------------
//! @file   state_machine.h
//! @brief  汎用ステートマシン - 状態遷移管理テンプレート
//----------------------------------------------------------------------------
#pragma once

#include <functional>

//----------------------------------------------------------------------------
//! @brief 汎用ステートマシン
//! @tparam TState 状態を表すenum型
//! @details AI、アニメーション、UIなどの状態管理に使用
//----------------------------------------------------------------------------
template<typename TState>
class StateMachine
{
public:
    //! @brief デフォルトコンストラクタ
    StateMachine() = default;

    //! @brief 初期状態を指定するコンストラクタ
    //! @param initialState 初期状態
    explicit StateMachine(TState initialState)
        : currentState_(initialState)
    {
    }

    //! @brief デストラクタ
    ~StateMachine() = default;

    //------------------------------------------------------------------------
    // 状態遷移
    //------------------------------------------------------------------------

    //! @brief 状態を設定
    //! @param state 新しい状態
    //! @return 遷移成功したらtrue（ロック中はfalse）
    bool SetState(TState state)
    {
        if (isLocked_) {
            return false;
        }

        if (currentState_ == state) {
            return true;  // 同じ状態は成功扱い
        }

        TState oldState = currentState_;
        currentState_ = state;

        if (onStateChanged_) {
            onStateChanged_(oldState, currentState_);
        }

        return true;
    }

    //! @brief 現在の状態を取得
    //! @return 現在の状態
    [[nodiscard]] TState GetState() const { return currentState_; }

    //------------------------------------------------------------------------
    // ロック機能
    //------------------------------------------------------------------------

    //! @brief 状態遷移をロック（攻撃中など遷移禁止時に使用）
    void Lock() { isLocked_ = true; }

    //! @brief 状態遷移のロックを解除
    void Unlock() { isLocked_ = false; }

    //! @brief ロック状態を取得
    //! @return ロック中ならtrue
    [[nodiscard]] bool IsLocked() const { return isLocked_; }

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 状態変更時のコールバックを設定
    //! @param callback コールバック関数（引数: 旧状態, 新状態）
    void SetOnStateChanged(std::function<void(TState, TState)> callback)
    {
        onStateChanged_ = std::move(callback);
    }

    //------------------------------------------------------------------------
    // 比較
    //------------------------------------------------------------------------

    //! @brief 指定状態かどうか判定
    //! @param state 比較する状態
    //! @return 一致したらtrue
    [[nodiscard]] bool IsState(TState state) const
    {
        return currentState_ == state;
    }

private:
    TState currentState_{};                              //!< 現在の状態
    bool isLocked_ = false;                              //!< ロック状態
    std::function<void(TState, TState)> onStateChanged_; //!< 状態変更コールバック
};

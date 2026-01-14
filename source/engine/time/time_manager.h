//----------------------------------------------------------------------------
//! @file   time_manager.h
//! @brief  時間管理システム - ゲーム全体の時間の流れを制御
//----------------------------------------------------------------------------
#pragma once

#include <functional>
#include <memory>
#include <cassert>

//----------------------------------------------------------------------------
//! @brief 時間状態
//----------------------------------------------------------------------------
enum class TimeState
{
    Normal,     //!< 通常進行（タイムスケール1.0）
    Frozen,     //!< 時間停止（結/切モード中）
    SlowMo      //!< スローモーション（将来用）
};

//----------------------------------------------------------------------------
//! @brief 時間管理システム（シングルトン）
//! @details 結/切モード中の時間停止などを管理する。
//!          Timerへの参照を持ち、スケーリングされた時間を提供する。
//----------------------------------------------------------------------------
class TimeManager
{
public:
    //! @brief シングルトンインスタンス取得
    static TimeManager& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~TimeManager() = default;

    //------------------------------------------------------------------------
    // 時間制御
    //------------------------------------------------------------------------

    //! @brief 時間を停止
    void Freeze();

    //! @brief 時間を再開
    void Resume();

    //! @brief スローモーションを設定
    //! @param scale タイムスケール（0.1〜0.9）
    void SetSlowMotion(float scale);

    //------------------------------------------------------------------------
    // 状態取得
    //------------------------------------------------------------------------

    //! @brief 現在の時間状態を取得
    [[nodiscard]] TimeState GetState() const { return state_; }

    //! @brief 現在のタイムスケールを取得
    [[nodiscard]] float GetTimeScale() const { return timeScale_; }

    //! @brief 時間が停止しているか判定
    [[nodiscard]] bool IsFrozen() const { return state_ == TimeState::Frozen; }

    //! @brief 通常状態か判定
    [[nodiscard]] bool IsNormal() const { return state_ == TimeState::Normal; }

    //------------------------------------------------------------------------
    // デルタタイム取得
    //------------------------------------------------------------------------

    //! @brief 生のデルタタイムを取得（Timer経由）
    [[nodiscard]] float GetRawDeltaTime() const;

    //! @brief スケーリングされたデルタタイムを取得（Timer経由）
    [[nodiscard]] float GetDeltaTime() const;

    //! @brief スケーリングされたデルタタイムを計算
    //! @param rawDeltaTime 生のデルタタイム
    //! @return スケーリング後のデルタタイム
    [[nodiscard]] float GetScaledDeltaTime(float rawDeltaTime) const;

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 状態変更時コールバック設定
    void SetOnStateChanged(std::function<void(TimeState)> callback) { onStateChanged_ = callback; }

private:
    TimeManager() = default;
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    static inline std::unique_ptr<TimeManager> instance_ = nullptr;

    TimeState state_ = TimeState::Normal;   //!< 現在の時間状態
    float timeScale_ = 1.0f;                //!< タイムスケール

    std::function<void(TimeState)> onStateChanged_;
};

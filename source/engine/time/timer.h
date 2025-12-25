//----------------------------------------------------------------------------
//! @file   timer.h
//! @brief  タイマークラス - 生のデルタタイム計測（静的クラス）
//----------------------------------------------------------------------------
#pragma once

#include <chrono>
#include <cstdint>

//----------------------------------------------------------------------------
//! @brief タイマークラス（静的クラス）
//! @details フレーム間の経過時間、総時間、FPSを計測する
//----------------------------------------------------------------------------
class Timer
{
public:
    Timer() = delete;
    ~Timer() = delete;

    //------------------------------------------------------------------------
    // ライフサイクル
    //------------------------------------------------------------------------

    //! @brief 初期化（タイマー開始）
    static void Start();

    //! @brief フレーム更新（毎フレーム呼び出し）
    //! @param maxDeltaTime デルタタイム上限（秒）
    static void Update(float maxDeltaTime = 0.25f);

    //------------------------------------------------------------------------
    // 時間取得
    //------------------------------------------------------------------------

    //! @brief 前フレームからの経過時間（秒）
    [[nodiscard]] static float GetDeltaTime() noexcept { return deltaTime_; }

    //! @brief 開始からの経過時間（秒）
    [[nodiscard]] static float GetTotalTime() noexcept { return totalTime_; }

    //! @brief 現在のFPS
    [[nodiscard]] static float GetFPS() noexcept { return fps_; }

    //! @brief フレームカウント
    [[nodiscard]] static uint64_t GetFrameCount() noexcept { return frameCount_; }

private:
    static std::chrono::high_resolution_clock::time_point startTime_;
    static std::chrono::high_resolution_clock::time_point lastFrameTime_;

    static float deltaTime_;
    static float totalTime_;
    static uint64_t frameCount_;

    // FPS計算
    static float fps_;
    static uint32_t fpsFrameCount_;
    static float fpsTimer_;
};

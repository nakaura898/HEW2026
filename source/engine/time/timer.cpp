//----------------------------------------------------------------------------
//! @file   timer.cpp
//! @brief  タイマークラス実装（静的クラス）
//----------------------------------------------------------------------------
#include "timer.h"
#include <algorithm>

// 静的メンバ変数の定義
std::chrono::high_resolution_clock::time_point Timer::startTime_;
std::chrono::high_resolution_clock::time_point Timer::lastFrameTime_;
float Timer::deltaTime_ = 0.0f;
float Timer::totalTime_ = 0.0f;
uint64_t Timer::frameCount_ = 0;
float Timer::fps_ = 0.0f;
uint32_t Timer::fpsFrameCount_ = 0;
float Timer::fpsTimer_ = 0.0f;

//----------------------------------------------------------------------------
void Timer::Start()
{
    startTime_ = std::chrono::high_resolution_clock::now();
    lastFrameTime_ = startTime_;
    deltaTime_ = 0.0f;
    totalTime_ = 0.0f;
    frameCount_ = 0;
    fps_ = 0.0f;
    fpsFrameCount_ = 0;
    fpsTimer_ = 0.0f;
}

//----------------------------------------------------------------------------
void Timer::Update(float maxDeltaTime)
{
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = now - lastFrameTime_;
    lastFrameTime_ = now;

    // デルタタイム更新（上限あり）
    deltaTime_ = (std::min)(elapsed.count(), maxDeltaTime);
    totalTime_ += deltaTime_;
    ++frameCount_;

    // FPS計算（1秒ごと）
    ++fpsFrameCount_;
    fpsTimer_ += deltaTime_;
    while (fpsTimer_ >= 1.0f) {
        fps_ = static_cast<float>(fpsFrameCount_) / fpsTimer_;
        fpsFrameCount_ = 0;
        fpsTimer_ -= 1.0f;  // 端数を保持して精度向上
    }
}

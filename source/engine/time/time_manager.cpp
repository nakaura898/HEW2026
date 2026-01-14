//----------------------------------------------------------------------------
//! @file   time_manager.cpp
//! @brief  時間管理システム実装
//----------------------------------------------------------------------------
#include "time_manager.h"
#include "engine/time/timer.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
TimeManager& TimeManager::Get()
{
    assert(instance_ && "TimeManager::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void TimeManager::Create()
{
    if (!instance_) {
        instance_.reset(new TimeManager());
    }
}

//----------------------------------------------------------------------------
void TimeManager::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
void TimeManager::Freeze()
{
    if (state_ == TimeState::Frozen) return;

    state_ = TimeState::Frozen;
    timeScale_ = 0.0f;

    LOG_INFO("[TimeManager] Time frozen");

    if (onStateChanged_) {
        onStateChanged_(state_);
    }
}

//----------------------------------------------------------------------------
void TimeManager::Resume()
{
    if (state_ == TimeState::Normal) return;

    state_ = TimeState::Normal;
    timeScale_ = 1.0f;

    LOG_INFO("[TimeManager] Time resumed");

    if (onStateChanged_) {
        onStateChanged_(state_);
    }
}

//----------------------------------------------------------------------------
void TimeManager::SetSlowMotion(float scale)
{
    // スケールを0.1〜0.9に制限
    scale = std::clamp(scale, 0.1f, 0.9f);

    state_ = TimeState::SlowMo;
    timeScale_ = scale;

    LOG_INFO("[TimeManager] Slow motion: " + std::to_string(scale));

    if (onStateChanged_) {
        onStateChanged_(state_);
    }
}

//----------------------------------------------------------------------------
float TimeManager::GetRawDeltaTime() const
{
    return Timer::GetDeltaTime();
}

//----------------------------------------------------------------------------
float TimeManager::GetDeltaTime() const
{
    return GetScaledDeltaTime(GetRawDeltaTime());
}

//----------------------------------------------------------------------------
float TimeManager::GetScaledDeltaTime(float rawDeltaTime) const
{
    return rawDeltaTime * timeScale_;
}

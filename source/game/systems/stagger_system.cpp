//----------------------------------------------------------------------------
//! @file   stagger_system.cpp
//! @brief  硬直システム実装
//----------------------------------------------------------------------------
#include "stagger_system.h"
#include "engine/time/time_manager.h"
#include "game/entities/group.h"
#include "common/logging/logging.h"
#include <vector>

//----------------------------------------------------------------------------
StaggerSystem& StaggerSystem::Get()
{
    assert(instance_ && "StaggerSystem::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void StaggerSystem::Create()
{
    if (!instance_) {
        instance_.reset(new StaggerSystem());
    }
}

//----------------------------------------------------------------------------
void StaggerSystem::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
void StaggerSystem::Update(float dt)
{
    // 時間停止中は硬直時間が減らない
    if (TimeManager::Get().IsFrozen()) {
        return;
    }

    // 解除対象を収集
    std::vector<Group*> toRemove;

    for (auto& [group, timer] : staggerTimers_) {
        timer -= dt;
        if (timer <= 0.0f) {
            toRemove.push_back(group);
        }
    }

    // 硬直解除
    for (Group* group : toRemove) {
        staggerTimers_.erase(group);

        LOG_INFO("[StaggerSystem] Stagger removed: " + group->GetId());

        if (onStaggerRemoved_) {
            onStaggerRemoved_(group);
        }
    }
}

//----------------------------------------------------------------------------
void StaggerSystem::ApplyStagger(Group* group, float duration)
{
    if (!group) {
        LOG_WARN("[StaggerSystem] BUG: ApplyStagger called with null group");
        return;
    }

    if (group->IsDefeated()) {
        LOG_WARN("[StaggerSystem] BUG: ApplyStagger called on defeated group: " + group->GetId());
        return;
    }

    if (duration <= 0.0f) {
        LOG_WARN("[StaggerSystem] BUG: Invalid stagger duration: " + std::to_string(duration));
        return;
    }

    // 二重硬直チェック
    if (staggerTimers_.find(group) != staggerTimers_.end()) {
        LOG_WARN("[StaggerSystem] Double stagger on " + group->GetId() + ", overwriting");
    }

    staggerTimers_[group] = duration;

    LOG_INFO("[StaggerSystem] Stagger applied to " + group->GetId() +
             " for " + std::to_string(duration) + "s");

    if (onStaggerApplied_) {
        onStaggerApplied_(group, duration);
    }
}

//----------------------------------------------------------------------------
void StaggerSystem::RemoveStagger(Group* group)
{
    if (!group) return;

    auto it = staggerTimers_.find(group);
    if (it != staggerTimers_.end()) {
        staggerTimers_.erase(it);

        LOG_INFO("[StaggerSystem] Stagger manually removed: " + group->GetId());

        if (onStaggerRemoved_) {
            onStaggerRemoved_(group);
        }
    }
}

//----------------------------------------------------------------------------
bool StaggerSystem::IsStaggered(Group* group) const
{
    if (!group) return false;
    return staggerTimers_.find(group) != staggerTimers_.end();
}

//----------------------------------------------------------------------------
float StaggerSystem::GetRemainingTime(Group* group) const
{
    if (!group) return 0.0f;

    auto it = staggerTimers_.find(group);
    if (it != staggerTimers_.end()) {
        return it->second;
    }
    return 0.0f;
}

//----------------------------------------------------------------------------
void StaggerSystem::OnGroupDefeated(Group* group)
{
    if (!group) return;

    auto it = staggerTimers_.find(group);
    if (it != staggerTimers_.end()) {
        staggerTimers_.erase(it);
        LOG_INFO("[StaggerSystem] Removed stagger for defeated group: " + group->GetId());
    }
}

//----------------------------------------------------------------------------
void StaggerSystem::Clear()
{
    staggerTimers_.clear();
    LOG_INFO("[StaggerSystem] All stagger states cleared");
}

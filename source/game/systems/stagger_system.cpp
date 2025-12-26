//----------------------------------------------------------------------------
//! @file   stagger_system.cpp
//! @brief  硬直システム実装
//----------------------------------------------------------------------------
#include "stagger_system.h"
#include "time_manager.h"
#include "game/entities/group.h"
#include "common/logging/logging.h"
#include <vector>

//----------------------------------------------------------------------------
StaggerSystem& StaggerSystem::Get()
{
    static StaggerSystem instance;
    return instance;
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
    if (!group) return;

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

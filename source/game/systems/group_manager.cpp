//----------------------------------------------------------------------------
//! @file   group_manager.cpp
//! @brief  グループ一元管理システム実装
//----------------------------------------------------------------------------
#include "group_manager.h"
#include "game/entities/group.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
Group* GroupManager::AddGroup(std::unique_ptr<Group> group)
{
    if (!group) {
        LOG_WARN("[GroupManager] Attempted to add null group");
        return nullptr;
    }

    Group* ptr = group.get();
    std::string id = ptr->GetId();
    groups_.push_back(std::move(group));

    LOG_INFO("[GroupManager] Added group: " + id);
    return ptr;
}

//----------------------------------------------------------------------------
void GroupManager::RemoveGroup(Group* group)
{
    if (!group) return;

    // ウェーブ割り当てを削除
    waveAssignments_.erase(group);

    // グループを検索して削除
    auto it = std::find_if(groups_.begin(), groups_.end(),
        [group](const std::unique_ptr<Group>& g) {
            return g.get() == group;
        });

    if (it != groups_.end()) {
        std::string id = (*it)->GetId();
        groups_.erase(it);
        LOG_INFO("[GroupManager] Removed group: " + id);
    }
}

//----------------------------------------------------------------------------
void GroupManager::Clear()
{
    waveAssignments_.clear();
    groups_.clear();
    LOG_INFO("[GroupManager] All groups cleared");
}

//----------------------------------------------------------------------------
std::vector<Group*> GroupManager::GetEnemyGroups() const
{
    std::vector<Group*> result;
    for (const auto& group : groups_) {
        if (group && group->IsEnemy()) {
            result.push_back(group.get());
        }
    }
    return result;
}

//----------------------------------------------------------------------------
std::vector<Group*> GroupManager::GetAllyGroups() const
{
    std::vector<Group*> result;
    for (const auto& group : groups_) {
        if (group && group->IsAlly()) {
            result.push_back(group.get());
        }
    }
    return result;
}

//----------------------------------------------------------------------------
std::vector<Group*> GroupManager::GetAliveGroups() const
{
    std::vector<Group*> result;
    for (const auto& group : groups_) {
        if (group && !group->IsDefeated()) {
            result.push_back(group.get());
        }
    }
    return result;
}

//----------------------------------------------------------------------------
Group* GroupManager::FindById(const std::string& id) const
{
    for (const auto& group : groups_) {
        if (group && group->GetId() == id) {
            return group.get();
        }
    }
    return nullptr;
}

//----------------------------------------------------------------------------
void GroupManager::AssignToWave(Group* group, int waveNumber)
{
    if (!group) {
        LOG_WARN("[GroupManager] AssignToWave: group is null");
        return;
    }
    if (waveNumber < 1) {
        LOG_WARN("[GroupManager] AssignToWave: waveNumber must be >= 1 (got " +
                 std::to_string(waveNumber) + ")");
        return;
    }
    waveAssignments_[group] = waveNumber;
}

//----------------------------------------------------------------------------
std::vector<Group*> GroupManager::GetGroupsForWave(int waveNumber) const
{
    std::vector<Group*> result;
    for (const auto& [group, wave] : waveAssignments_) {
        if (wave == waveNumber && group) {
            result.push_back(group);
        }
    }
    return result;
}

//----------------------------------------------------------------------------
int GroupManager::GetWaveNumber(Group* group) const
{
    if (!group) {
        return kWaveUnassigned;
    }

    auto it = waveAssignments_.find(group);
    if (it != waveAssignments_.end()) {
        return it->second;
    }
    return kWaveUnassigned;
}

//----------------------------------------------------------------------------
void GroupManager::ClearWaveAssignments()
{
    waveAssignments_.clear();
}

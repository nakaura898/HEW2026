//----------------------------------------------------------------------------
//! @file   friends_damage_sharing.cpp
//! @brief  フレンズ効果システム実装
//----------------------------------------------------------------------------
#include "friends_damage_sharing.h"
#include "game/bond/bond_manager.h"
#include "game/entities/group.h"
#include "game/entities/individual.h"
#include "common/logging/logging.h"
#include <queue>
#include <set>

//----------------------------------------------------------------------------
FriendsDamageSharing& FriendsDamageSharing::Get()
{
    static FriendsDamageSharing instance;
    return instance;
}

//----------------------------------------------------------------------------
std::vector<Group*> FriendsDamageSharing::GetFriendsCluster(Group* group) const
{
    if (!group) return {};
    return BuildFriendsClusterBFS(group);
}

//----------------------------------------------------------------------------
bool FriendsDamageSharing::HasFriendsPartners(Group* group) const
{
    if (!group) return false;

    std::vector<Group*> cluster = GetFriendsCluster(group);
    return cluster.size() > 1;
}

//----------------------------------------------------------------------------
std::vector<Group*> FriendsDamageSharing::BuildFriendsClusterBFS(Group* start) const
{
    std::vector<Group*> cluster;
    std::queue<Group*> toVisit;
    std::set<Group*> visited;

    toVisit.push(start);
    visited.insert(start);

    while (!toVisit.empty()) {
        Group* current = toVisit.front();
        toVisit.pop();
        cluster.push_back(current);

        // フレンズ縁で繋がった隣接グループを探索
        BondableEntity currentEntity = current;
        std::vector<Bond*> bonds = BondManager::Get().GetBondsFor(currentEntity);

        for (Bond* bond : bonds) {
            // フレンズ縁のみを辿る
            if (bond->GetType() != BondType::Friends) continue;

            BondableEntity otherEntity = bond->GetOther(currentEntity);
            Group* otherGroup = BondableHelper::AsGroup(otherEntity);

            if (otherGroup && visited.find(otherGroup) == visited.end()) {
                visited.insert(otherGroup);
                toVisit.push(otherGroup);
            }
        }
    }

    return cluster;
}

//----------------------------------------------------------------------------
void FriendsDamageSharing::ApplyDamageWithSharing(Individual* targetIndividual, float damage)
{
    if (!targetIndividual) return;

    Group* targetGroup = targetIndividual->GetOwnerGroup();
    if (!targetGroup) {
        // グループに属していない場合は直接ダメージ
        ApplySharedDamage(targetIndividual, damage);
        return;
    }

    std::vector<Group*> friendsCluster = GetFriendsCluster(targetGroup);

    // フレンズ縁がない場合は直接ダメージ
    if (friendsCluster.size() <= 1) {
        ApplySharedDamage(targetIndividual, damage);
        return;
    }

    LOG_INFO("[FriendsDamageSharing] Distributing " + std::to_string(static_cast<int>(damage)) +
             " damage across " + std::to_string(friendsCluster.size()) + " groups");

    // 生存グループ数をカウント
    int aliveGroupCount = 0;
    for (Group* group : friendsCluster) {
        if (group && !group->IsDefeated() && group->GetAliveCount() > 0) {
            aliveGroupCount++;
        }
    }

    if (aliveGroupCount == 0) return;

    // グループ間で均等分配
    float damagePerGroup = damage / static_cast<float>(aliveGroupCount);

    for (Group* group : friendsCluster) {
        if (!group || group->IsDefeated()) continue;

        std::vector<Individual*> aliveIndividuals = group->GetAliveIndividuals();
        if (aliveIndividuals.empty()) continue;

        // グループ内で均等分配
        float damagePerIndividual = damagePerGroup / static_cast<float>(aliveIndividuals.size());

        LOG_INFO("[FriendsDamageSharing] Group " + group->GetId() +
                 ": " + std::to_string(static_cast<int>(damagePerIndividual)) +
                 " damage per individual (" + std::to_string(aliveIndividuals.size()) + " individuals)");

        for (Individual* individual : aliveIndividuals) {
            ApplySharedDamage(individual, damagePerIndividual);
        }
    }
}

//----------------------------------------------------------------------------
void FriendsDamageSharing::ApplySharedDamage(Individual* individual, float damage)
{
    if (!individual || !individual->IsAlive()) return;

    // 無限ループ防止フラグを設定してダメージ適用
    individual->SetReceivingSharedDamage(true);
    individual->TakeDamage(damage);
    individual->SetReceivingSharedDamage(false);
}

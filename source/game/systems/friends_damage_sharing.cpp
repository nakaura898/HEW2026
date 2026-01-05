//----------------------------------------------------------------------------
//! @file   friends_damage_sharing.cpp
//! @brief  フレンズ効果システム実装
//----------------------------------------------------------------------------
#include "friends_damage_sharing.h"
#include "game/bond/bond_manager.h"
#include "game/entities/group.h"
#include "game/entities/individual.h"
#include "game/entities/player.h"
#include "common/logging/logging.h"
#include <queue>
#include <set>

//----------------------------------------------------------------------------
FriendsDamageSharing& FriendsDamageSharing::Get()
{
    assert(instance_ && "FriendsDamageSharing::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void FriendsDamageSharing::Create()
{
    if (!instance_) {
        instance_.reset(new FriendsDamageSharing());
    }
}

//----------------------------------------------------------------------------
void FriendsDamageSharing::Destroy()
{
    instance_.reset();
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
    bool playerTraversed = false;  // プレイヤー経由の探索は一度だけ

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

            // Group同士の直接接続
            Group* otherGroup = BondableHelper::AsGroup(otherEntity);
            if (otherGroup && visited.find(otherGroup) == visited.end()) {
                visited.insert(otherGroup);
                toVisit.push(otherGroup);
                continue;
            }

            // Player経由の接続（一度だけ探索）
            Player* player = BondableHelper::AsPlayer(otherEntity);
            if (player && !playerTraversed) {
                playerTraversed = true;

                // プレイヤーのFriends縁を全て取得
                BondableEntity playerEntity = player;
                std::vector<Bond*> playerBonds = BondManager::Get().GetBondsFor(playerEntity);

                for (Bond* playerBond : playerBonds) {
                    if (playerBond->GetType() != BondType::Friends) continue;

                    BondableEntity playerOther = playerBond->GetOther(playerEntity);
                    Group* playerConnectedGroup = BondableHelper::AsGroup(playerOther);

                    if (playerConnectedGroup && visited.find(playerConnectedGroup) == visited.end()) {
                        visited.insert(playerConnectedGroup);
                        toVisit.push(playerConnectedGroup);
                    }
                }
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

    // 生存グループ数をカウント
    int aliveGroupCount = 0;
    for (Group* group : friendsCluster) {
        if (group && !group->IsDefeated() && group->GetAliveCount() > 0) {
            aliveGroupCount++;
        }
    }

    if (aliveGroupCount == 0) {
        LOG_WARN("[FriendsDamageSharing] BUG: Cluster has no alive groups but was called");
        return;
    }

    // グループ間で均等分配
    float damagePerGroup = damage / static_cast<float>(aliveGroupCount);

    for (Group* group : friendsCluster) {
        if (!group || group->IsDefeated()) continue;

        std::vector<Individual*> aliveIndividuals = group->GetAliveIndividuals();
        if (aliveIndividuals.empty()) continue;

        // グループ内で均等分配
        float damagePerIndividual = damagePerGroup / static_cast<float>(aliveIndividuals.size());

        for (Individual* individual : aliveIndividuals) {
            ApplySharedDamage(individual, damagePerIndividual);
        }
    }
}

//----------------------------------------------------------------------------
void FriendsDamageSharing::ApplySharedDamage(Individual* individual, float damage)
{
    if (!individual || !individual->IsAlive()) return;

    // RAII: 無限ループ防止フラグを確実にリセット（例外発生時も含む）
    class SharedDamageGuard
    {
    public:
        explicit SharedDamageGuard(Individual* ind) : individual_(ind)
        {
            individual_->SetReceivingSharedDamage(true);
        }
        ~SharedDamageGuard()
        {
            individual_->SetReceivingSharedDamage(false);
        }

    private:
        Individual* individual_;
    };

    SharedDamageGuard guard(individual);
    individual->TakeDamage(damage);
}

//----------------------------------------------------------------------------
//! @file   love_bond_system.cpp
//! @brief  ラブ効果システム実装
//----------------------------------------------------------------------------
#include "love_bond_system.h"
#include "game/bond/bond_manager.h"
#include "game/entities/group.h"
#include "game/entities/player.h"
#include "game/systems/combat_system.h"
#include "common/logging/logging.h"
#include <queue>
#include <set>

//----------------------------------------------------------------------------
LoveBondSystem& LoveBondSystem::Get()
{
    assert(instance_ && "LoveBondSystem::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void LoveBondSystem::Create()
{
    if (!instance_) {
        instance_.reset(new LoveBondSystem());
    }
}

//----------------------------------------------------------------------------
void LoveBondSystem::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
void LoveBondSystem::RebuildLoveGroups()
{
    loveClusters_.clear();
    clusterIndexCache_.clear();

    // ラブ縁を取得
    std::vector<Bond*> loveBonds = BondManager::Get().GetBondsByType(BondType::Love);
    if (loveBonds.empty()) {
        return;
    }

    // 全てのグループを収集
    std::set<Group*> allGroups;
    for (Bond* bond : loveBonds) {
        Group* groupA = BondableHelper::AsGroup(bond->GetEntityA());
        Group* groupB = BondableHelper::AsGroup(bond->GetEntityB());
        if (groupA) allGroups.insert(groupA);
        if (groupB) allGroups.insert(groupB);
    }

    // BFSでクラスタを構築
    std::set<Group*> visited;
    for (Group* group : allGroups) {
        if (visited.find(group) == visited.end()) {
            std::vector<Group*> cluster = BuildClusterBFS(group, visited);
            if (cluster.size() > 1) {  // 2つ以上のグループがあるクラスタのみ
                size_t clusterIndex = loveClusters_.size();
                loveClusters_.push_back(cluster);

                // キャッシュを構築
                for (Group* g : cluster) {
                    clusterIndexCache_[g] = clusterIndex;
                }

                // クラスタ全体で共通のwanderTargetを設定（接続直後に動き出すように）
                SyncClusterWanderTarget(cluster);

                LOG_INFO("[LoveBondSystem] Built cluster with " +
                         std::to_string(cluster.size()) + " groups");
            }
        }
    }
}

//----------------------------------------------------------------------------
std::vector<Group*> LoveBondSystem::BuildClusterBFS(Group* start, std::set<Group*>& visited) const
{
    std::vector<Group*> cluster;
    std::queue<Group*> toVisit;

    toVisit.push(start);
    visited.insert(start);

    while (!toVisit.empty()) {
        Group* current = toVisit.front();
        toVisit.pop();
        cluster.push_back(current);

        // ラブ縁で繋がった隣接グループを探索
        BondableEntity currentEntity = current;
        std::vector<Bond*> bonds = BondManager::Get().GetBondsFor(currentEntity);

        for (Bond* bond : bonds) {
            // ラブ縁のみを辿る
            if (bond->GetType() != BondType::Love) continue;

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
std::vector<Group*> LoveBondSystem::GetLoveCluster(Group* group) const
{
    if (group == nullptr) {
        return {};
    }

    // キャッシュからO(1)で検索
    auto it = clusterIndexCache_.find(group);
    if (it != clusterIndexCache_.end()) {
        return loveClusters_[it->second];
    }

    // 見つからない場合は自身のみを返す
    return { group };
}

//----------------------------------------------------------------------------
bool LoveBondSystem::HasLovePartners(Group* group) const
{
    if (group == nullptr) {
        return false;
    }

    // キャッシュからO(1)で判定
    return clusterIndexCache_.find(group) != clusterIndexCache_.end();
}

//----------------------------------------------------------------------------
AITarget LoveBondSystem::DetermineSharedTarget(const std::vector<Group*>& cluster) const
{
    AITarget bestTarget{};  // 明示的にデフォルト初期化
    float highestThreat = -1.0f;

    CombatSystem& combat = CombatSystem::Get();

    for (Group* group : cluster) {
        if (!group || group->IsDefeated()) continue;

        // このグループが攻撃可能なターゲットを検索
        Group* groupTarget = combat.SelectTarget(group);
        bool canAttackPlayer = combat.CanAttackPlayer(group);

        // グループターゲットの脅威度
        if (groupTarget) {
            float threat = groupTarget->GetThreat();
            if (threat > highestThreat) {
                highestThreat = threat;
                bestTarget = groupTarget;
            }
        }

        // プレイヤーの脅威度
        if (canAttackPlayer && player_) {
            float threat = player_->GetThreat();
            if (threat > highestThreat) {
                highestThreat = threat;
                bestTarget = player_;
            }
        }
    }

    return bestTarget;
}

//----------------------------------------------------------------------------
void LoveBondSystem::SyncClusterTarget(const std::vector<Group*>& cluster, const AITarget& target)
{
    for (Group* group : cluster) {
        if (!group || group->IsDefeated()) continue;

        GroupAI* ai = group->GetAI();
        if (!ai) continue;

        // ターゲットを設定
        if (std::holds_alternative<Group*>(target)) {
            ai->SetTarget(std::get<Group*>(target));
        } else if (std::holds_alternative<Player*>(target)) {
            ai->SetTargetPlayer(std::get<Player*>(target));
        } else {
            ai->ClearTarget();
        }
    }
}

//----------------------------------------------------------------------------
float LoveBondSystem::GetTargetThreat(const AITarget& target) const
{
    if (std::holds_alternative<Group*>(target)) {
        Group* group = std::get<Group*>(target);
        return (group != nullptr) ? group->GetThreat() : -1.0f;
    }
    if (std::holds_alternative<Player*>(target)) {
        Player* player = std::get<Player*>(target);
        return (player != nullptr) ? player->GetThreat() : -1.0f;
    }
    return -1.0f;
}

//----------------------------------------------------------------------------
void LoveBondSystem::SyncClusterWanderTarget(const std::vector<Group*>& cluster)
{
    if (cluster.empty()) return;

    // クラスタの中心位置を計算
    Vector2 clusterCenter = Vector2::Zero;
    int validCount = 0;
    for (Group* g : cluster) {
        if (g && !g->IsDefeated()) {
            clusterCenter = clusterCenter + g->GetPosition();
            ++validCount;
        }
    }
    if (validCount == 0) return;
    clusterCenter = clusterCenter * (1.0f / static_cast<float>(validCount));

    // 共通のwanderTargetを設定 & 攻撃状態をリセット
    for (Group* g : cluster) {
        if (!g || g->IsDefeated()) continue;

        // wanderTarget設定
        GroupAI* ai = g->GetAI();
        if (ai) {
            ai->SetWanderTarget(clusterCenter);
        }

        // 全個体の攻撃状態をリセット（攻撃中に接続されても動けるように）
        for (Individual* ind : g->GetAliveIndividuals()) {
            if (ind->IsAttacking()) {
                ind->EndAttack();
                ind->SetAction(IndividualAction::Walk);
            }
        }
    }

}

//----------------------------------------------------------------------------
void LoveBondSystem::Clear()
{
    loveClusters_.clear();
    clusterIndexCache_.clear();
    player_ = nullptr;
    LOG_INFO("[LoveBondSystem] Cleared all caches");
}

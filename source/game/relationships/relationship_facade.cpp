//----------------------------------------------------------------------------
//! @file   relationship_facade.cpp
//! @brief  RelationshipFacade実装
//----------------------------------------------------------------------------
#include "relationship_facade.h"
#include "game/entities/group.h"
#include "game/entities/player.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
RelationshipFacade& RelationshipFacade::Get()
{
    assert(instance_ && "RelationshipFacade::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void RelationshipFacade::Create()
{
    if (!instance_) {
        instance_.reset(new RelationshipFacade());
    }
}

//----------------------------------------------------------------------------
void RelationshipFacade::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
void RelationshipFacade::Initialize()
{
    graph_.Clear();
    player_ = nullptr;
    LOG_INFO("[RelationshipFacade] Initialized");
}

//----------------------------------------------------------------------------
void RelationshipFacade::Shutdown()
{
    graph_.Clear();
    player_ = nullptr;
    onBondCreated_ = nullptr;
    onBondRemoved_ = nullptr;
    LOG_INFO("[RelationshipFacade] Shutdown");
}

//----------------------------------------------------------------------------
bool RelationshipFacade::Bind(const BondableEntity& a, const BondableEntity& b, BondType type)
{
    // 同一エンティティは不可
    if (BondableHelper::IsSame(a, b)) {
        LOG_WARN("[RelationshipFacade] Cannot bind same entity");
        return false;
    }

    // 既に接続済みかチェック
    if (AreDirectlyConnected(a, b)) {
        LOG_WARN("[RelationshipFacade] Already connected: " +
                 BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b));
        return false;
    }

    // グラフにエッジ追加
    uint32_t edgeId = graph_.AddEdge(a, b, type);
    if (edgeId == 0) {
        return false;
    }

    LOG_INFO("[RelationshipFacade] Bind: " +
             BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b) +
             " (type=" + std::to_string(static_cast<int>(type)) + ")");

    // コールバック
    if (onBondCreated_) {
        onBondCreated_(a, b, type);
    }

    return true;
}

//----------------------------------------------------------------------------
bool RelationshipFacade::Cut(const BondableEntity& a, const BondableEntity& b)
{
    // エッジを削除
    bool removed = graph_.RemoveEdge(a, b);
    if (!removed) {
        return false;
    }

    LOG_INFO("[RelationshipFacade] Cut: " +
             BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b));

    // コールバック
    if (onBondRemoved_) {
        onBondRemoved_(a, b);
    }

    return true;
}

//----------------------------------------------------------------------------
void RelationshipFacade::CutAll(const BondableEntity& entity)
{
    // このエンティティの全エッジを取得
    std::vector<const EdgeData*> edges = graph_.GetEdgesFor(entity);

    // 各エッジを削除（コールバック付き）
    for (const EdgeData* edge : edges) {
        if (!edge) continue;

        BondableEntity other = (BondableHelper::GetId(edge->entityA) == BondableHelper::GetId(entity))
            ? edge->entityB : edge->entityA;

        graph_.RemoveEdge(edge->id);

        if (onBondRemoved_) {
            onBondRemoved_(entity, other);
        }
    }

    LOG_INFO("[RelationshipFacade] CutAll: " + BondableHelper::GetId(entity));
}

//----------------------------------------------------------------------------
bool RelationshipFacade::AreFriendly(const BondableEntity& a, const BondableEntity& b) const
{
    // 同一エンティティは味方
    if (BondableHelper::IsSame(a, b)) {
        return true;
    }

    // 推移的接続（任意の縁タイプ）
    return graph_.AreConnected(a, b);
}

//----------------------------------------------------------------------------
bool RelationshipFacade::AreHostile(const BondableEntity& a, const BondableEntity& b) const
{
    return !AreFriendly(a, b);
}

//----------------------------------------------------------------------------
bool RelationshipFacade::AreDirectlyConnected(const BondableEntity& a, const BondableEntity& b) const
{
    return graph_.HasEdge(a, b);
}

//----------------------------------------------------------------------------
std::vector<BondableEntity> RelationshipFacade::GetCluster(const BondableEntity& start, BondType type) const
{
    Cluster cluster = graph_.GetConnectedComponentByType(start, type);
    return cluster.entities;
}

//----------------------------------------------------------------------------
std::vector<BondableEntity> RelationshipFacade::GetAllies(const BondableEntity& start) const
{
    Cluster cluster = graph_.GetConnectedComponent(start);
    return cluster.entities;
}

//----------------------------------------------------------------------------
std::vector<Group*> RelationshipFacade::GetLoveCluster(Group* group) const
{
    if (!group) return {};

    BondableEntity entity = group;
    Cluster cluster = graph_.GetConnectedComponentByType(entity, BondType::Love);

    std::vector<Group*> result;
    for (const BondableEntity& e : cluster.entities) {
        if (std::holds_alternative<Group*>(e)) {
            Group* g = std::get<Group*>(e);
            if (g) {
                result.push_back(g);
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------------
bool RelationshipFacade::HasLovePartners(Group* group) const
{
    if (!group) return false;

    BondableEntity entity = group;
    std::vector<BondableEntity> neighbors = graph_.GetNeighborsByType(entity, BondType::Love);

    return !neighbors.empty();
}

//----------------------------------------------------------------------------
std::vector<Cluster> RelationshipFacade::FindAllClusters(BondType type) const
{
    return graph_.FindClustersByType(type);
}

//----------------------------------------------------------------------------
AITarget RelationshipFacade::DetermineSharedTarget(const std::vector<Group*>& cluster) const
{
    AITarget bestTarget;
    float highestThreat = -1.0f;

    for (Group* group : cluster) {
        if (!group) continue;

        GroupAI* ai = group->GetAI();
        if (!ai) continue;

        AITarget currentTarget = ai->GetTarget();

        // monostate（ターゲットなし）はスキップ
        if (std::holds_alternative<std::monostate>(currentTarget)) {
            continue;
        }

        float threat = GetTargetThreat(currentTarget);
        if (threat > highestThreat) {
            highestThreat = threat;
            bestTarget = currentTarget;
        }
    }

    return bestTarget;
}

//----------------------------------------------------------------------------
void RelationshipFacade::SyncClusterTarget(const std::vector<Group*>& cluster, const AITarget& target)
{
    for (Group* group : cluster) {
        if (!group) continue;

        GroupAI* ai = group->GetAI();
        if (!ai) continue;

        // 既に同じターゲットならスキップ
        AITarget currentTarget = ai->GetTarget();

        // ターゲットタイプが異なる場合は更新
        if (std::holds_alternative<Group*>(target)) {
            Group* targetGroup = std::get<Group*>(target);
            if (!std::holds_alternative<Group*>(currentTarget) ||
                std::get<Group*>(currentTarget) != targetGroup) {
                ai->SetTarget(targetGroup);
            }
        } else if (std::holds_alternative<Player*>(target)) {
            Player* targetPlayer = std::get<Player*>(target);
            if (!std::holds_alternative<Player*>(currentTarget) ||
                std::get<Player*>(currentTarget) != targetPlayer) {
                ai->SetTargetPlayer(targetPlayer);
            }
        }
    }
}

//----------------------------------------------------------------------------
std::vector<BondableEntity> RelationshipFacade::GetNeighbors(const BondableEntity& entity) const
{
    return graph_.GetNeighbors(entity);
}

//----------------------------------------------------------------------------
std::vector<BondableEntity> RelationshipFacade::GetNeighborsByType(const BondableEntity& entity, BondType type) const
{
    return graph_.GetNeighborsByType(entity, type);
}

//----------------------------------------------------------------------------
const EdgeData* RelationshipFacade::GetEdge(const BondableEntity& a, const BondableEntity& b) const
{
    return graph_.GetEdge(a, b);
}

//----------------------------------------------------------------------------
std::vector<const EdgeData*> RelationshipFacade::GetAllEdges() const
{
    return graph_.GetAllEdges();
}

//----------------------------------------------------------------------------
std::vector<const EdgeData*> RelationshipFacade::GetEdgesByType(BondType type) const
{
    return graph_.GetEdgesByType(type);
}

//----------------------------------------------------------------------------
size_t RelationshipFacade::GetEdgeCount() const
{
    return graph_.GetEdgeCount();
}

//----------------------------------------------------------------------------
float RelationshipFacade::GetTargetThreat(const AITarget& target) const
{
    if (std::holds_alternative<Group*>(target)) {
        Group* group = std::get<Group*>(target);
        if (group && !group->IsDefeated()) {
            return group->GetThreat();
        }
    } else if (std::holds_alternative<Player*>(target)) {
        Player* p = std::get<Player*>(target);
        if (p && p->IsAlive()) {
            return p->GetThreat();
        }
    }

    return -1.0f;
}

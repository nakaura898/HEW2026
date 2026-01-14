//----------------------------------------------------------------------------
//! @file   relationship_graph.cpp
//! @brief  関係グラフ実装
//----------------------------------------------------------------------------
#include "relationship_graph.h"
#include "common/logging/logging.h"
#include <queue>
#include <algorithm>

//----------------------------------------------------------------------------
uint32_t RelationshipGraph::AddEdge(const BondableEntity& a, const BondableEntity& b, BondType type)
{
    std::string idA = BondableHelper::GetId(a);
    std::string idB = BondableHelper::GetId(b);

    // 同一ノードは不可
    if (idA == idB) {
        LOG_WARN("[RelationshipGraph] Cannot add edge between same node");
        return 0;
    }

    // 既存エッジチェック
    if (HasEdge(a, b)) {
        LOG_WARN("[RelationshipGraph] Edge already exists: " + idA + " <-> " + idB);
        return 0;
    }

    // エッジ作成
    uint32_t edgeId = nextEdgeId_++;
    EdgeData edge;
    edge.id = edgeId;
    edge.nodeA = idA;
    edge.nodeB = idB;
    edge.type = type;
    edge.entityA = a;
    edge.entityB = b;

    edges_[edgeId] = edge;

    // 隣接リスト更新（双方向）
    adjacency_[idA].push_back({ idB, edgeId, type });
    adjacency_[idB].push_back({ idA, edgeId, type });

    // タイプインデックス更新
    typeIndex_[static_cast<int>(type)].push_back(edgeId);

    // ノードエンティティ登録
    nodeEntities_[idA] = a;
    nodeEntities_[idB] = b;

    LOG_INFO("[RelationshipGraph] Edge added: " + idA + " <-> " + idB +
             " (type=" + std::to_string(static_cast<int>(type)) + ")");

    return edgeId;
}

//----------------------------------------------------------------------------
bool RelationshipGraph::RemoveEdge(uint32_t edgeId)
{
    auto it = edges_.find(edgeId);
    if (it == edges_.end()) {
        return false;
    }

    const EdgeData& edge = it->second;
    std::string idA = edge.nodeA;
    std::string idB = edge.nodeB;
    BondType type = edge.type;

    // 隣接リストから削除
    auto& adjA = adjacency_[idA];
    adjA.erase(std::remove_if(adjA.begin(), adjA.end(),
        [edgeId](const AdjacencyEntry& e) { return e.edgeId == edgeId; }), adjA.end());

    auto& adjB = adjacency_[idB];
    adjB.erase(std::remove_if(adjB.begin(), adjB.end(),
        [edgeId](const AdjacencyEntry& e) { return e.edgeId == edgeId; }), adjB.end());

    // タイプインデックスから削除
    auto& typeVec = typeIndex_[static_cast<int>(type)];
    typeVec.erase(std::remove(typeVec.begin(), typeVec.end(), edgeId), typeVec.end());

    LOG_INFO("[RelationshipGraph] Edge removed: " + idA + " <-> " + idB);

    // エッジ削除
    edges_.erase(it);

    return true;
}

//----------------------------------------------------------------------------
bool RelationshipGraph::RemoveEdge(const BondableEntity& a, const BondableEntity& b)
{
    const EdgeData* edge = GetEdge(a, b);
    if (!edge) {
        return false;
    }
    return RemoveEdge(edge->id);
}

//----------------------------------------------------------------------------
void RelationshipGraph::RemoveAllEdgesFor(const BondableEntity& entity)
{
    std::string nodeId = BondableHelper::GetId(entity);

    // このノードに関連する全エッジIDを収集
    std::vector<uint32_t> toRemove;
    auto it = adjacency_.find(nodeId);
    if (it != adjacency_.end()) {
        for (const AdjacencyEntry& entry : it->second) {
            toRemove.push_back(entry.edgeId);
        }
    }

    // 削除
    for (uint32_t edgeId : toRemove) {
        RemoveEdge(edgeId);
    }
}

//----------------------------------------------------------------------------
void RelationshipGraph::Clear()
{
    edges_.clear();
    adjacency_.clear();
    typeIndex_.clear();
    nodeEntities_.clear();
    nextEdgeId_ = 1;
    LOG_INFO("[RelationshipGraph] Cleared");
}

//----------------------------------------------------------------------------
bool RelationshipGraph::HasEdge(const BondableEntity& a, const BondableEntity& b) const
{
    return GetEdge(a, b) != nullptr;
}

//----------------------------------------------------------------------------
const EdgeData* RelationshipGraph::GetEdge(const BondableEntity& a, const BondableEntity& b) const
{
    std::string idA = BondableHelper::GetId(a);
    std::string idB = BondableHelper::GetId(b);

    auto it = adjacency_.find(idA);
    if (it == adjacency_.end()) {
        return nullptr;
    }

    for (const AdjacencyEntry& entry : it->second) {
        if (entry.neighborId == idB) {
            auto edgeIt = edges_.find(entry.edgeId);
            if (edgeIt != edges_.end()) {
                return &edgeIt->second;
            }
        }
    }

    return nullptr;
}

//----------------------------------------------------------------------------
std::vector<BondableEntity> RelationshipGraph::GetNeighbors(const BondableEntity& node) const
{
    std::vector<BondableEntity> result;
    std::string nodeId = BondableHelper::GetId(node);

    auto it = adjacency_.find(nodeId);
    if (it == adjacency_.end()) {
        return result;
    }

    for (const AdjacencyEntry& entry : it->second) {
        auto entityIt = nodeEntities_.find(entry.neighborId);
        if (entityIt != nodeEntities_.end()) {
            result.push_back(entityIt->second);
        }
    }

    return result;
}

//----------------------------------------------------------------------------
std::vector<BondableEntity> RelationshipGraph::GetNeighborsByType(const BondableEntity& node, BondType type) const
{
    std::vector<BondableEntity> result;
    std::string nodeId = BondableHelper::GetId(node);

    auto it = adjacency_.find(nodeId);
    if (it == adjacency_.end()) {
        return result;
    }

    for (const AdjacencyEntry& entry : it->second) {
        if (entry.type == type) {
            auto entityIt = nodeEntities_.find(entry.neighborId);
            if (entityIt != nodeEntities_.end()) {
                result.push_back(entityIt->second);
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------------
std::vector<const EdgeData*> RelationshipGraph::GetEdgesFor(const BondableEntity& node) const
{
    std::vector<const EdgeData*> result;
    std::string nodeId = BondableHelper::GetId(node);

    auto it = adjacency_.find(nodeId);
    if (it == adjacency_.end()) {
        return result;
    }

    for (const AdjacencyEntry& entry : it->second) {
        auto edgeIt = edges_.find(entry.edgeId);
        if (edgeIt != edges_.end()) {
            result.push_back(&edgeIt->second);
        }
    }

    return result;
}

//----------------------------------------------------------------------------
std::vector<const EdgeData*> RelationshipGraph::GetAllEdges() const
{
    std::vector<const EdgeData*> result;
    result.reserve(edges_.size());

    for (const auto& pair : edges_) {
        result.push_back(&pair.second);
    }

    return result;
}

//----------------------------------------------------------------------------
std::vector<const EdgeData*> RelationshipGraph::GetEdgesByType(BondType type) const
{
    std::vector<const EdgeData*> result;

    auto it = typeIndex_.find(static_cast<int>(type));
    if (it == typeIndex_.end()) {
        return result;
    }

    for (uint32_t edgeId : it->second) {
        auto edgeIt = edges_.find(edgeId);
        if (edgeIt != edges_.end()) {
            result.push_back(&edgeIt->second);
        }
    }

    return result;
}

//----------------------------------------------------------------------------
bool RelationshipGraph::AreConnected(const BondableEntity& a, const BondableEntity& b) const
{
    Cluster cluster = GetConnectedComponent(a);
    std::string targetId = BondableHelper::GetId(b);

    for (const std::string& nodeId : cluster.nodeIds) {
        if (nodeId == targetId) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------
bool RelationshipGraph::AreConnectedByType(const BondableEntity& a, const BondableEntity& b, BondType type) const
{
    Cluster cluster = GetConnectedComponentByType(a, type);
    std::string targetId = BondableHelper::GetId(b);

    for (const std::string& nodeId : cluster.nodeIds) {
        if (nodeId == targetId) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------
Cluster RelationshipGraph::GetConnectedComponent(const BondableEntity& start) const
{
    std::string startId = BondableHelper::GetId(start);
    return BFS(startId, nullptr);
}

//----------------------------------------------------------------------------
Cluster RelationshipGraph::GetConnectedComponentByType(const BondableEntity& start, BondType type) const
{
    std::string startId = BondableHelper::GetId(start);
    BondType filterType = type;
    return BFS(startId, &filterType);
}

//----------------------------------------------------------------------------
std::vector<Cluster> RelationshipGraph::FindClustersByType(BondType type) const
{
    std::vector<Cluster> clusters;
    std::unordered_set<std::string> visited;

    // 指定タイプのエッジを持つノードを全て探索
    auto it = typeIndex_.find(static_cast<int>(type));
    if (it == typeIndex_.end()) {
        return clusters;
    }

    for (uint32_t edgeId : it->second) {
        auto edgeIt = edges_.find(edgeId);
        if (edgeIt == edges_.end()) continue;

        const EdgeData& edge = edgeIt->second;

        // まだ訪問していないノードからBFS
        if (visited.find(edge.nodeA) == visited.end()) {
            BondType filterType = type;
            Cluster cluster = BFS(edge.nodeA, &filterType);

            // 訪問済みに追加
            for (const std::string& nodeId : cluster.nodeIds) {
                visited.insert(nodeId);
            }

            // 2ノード以上のクラスターのみ追加
            if (cluster.nodeIds.size() > 1) {
                clusters.push_back(std::move(cluster));
            }
        }
    }

    return clusters;
}

//----------------------------------------------------------------------------
Cluster RelationshipGraph::BFS(const std::string& startId, BondType* filterType) const
{
    Cluster result;

    // ノードが存在しない場合
    auto startIt = adjacency_.find(startId);
    if (startIt == adjacency_.end()) {
        // 孤立ノードとして返す
        auto entityIt = nodeEntities_.find(startId);
        if (entityIt != nodeEntities_.end()) {
            result.nodeIds.push_back(startId);
            result.entities.push_back(entityIt->second);
        }
        return result;
    }

    std::queue<std::string> toVisit;
    std::unordered_set<std::string> visited;

    toVisit.push(startId);
    visited.insert(startId);

    while (!toVisit.empty()) {
        std::string current = toVisit.front();
        toVisit.pop();

        result.nodeIds.push_back(current);
        auto entityIt = nodeEntities_.find(current);
        if (entityIt != nodeEntities_.end()) {
            result.entities.push_back(entityIt->second);
        }

        // 隣接ノードを探索
        auto adjIt = adjacency_.find(current);
        if (adjIt != adjacency_.end()) {
            for (const AdjacencyEntry& entry : adjIt->second) {
                // タイプフィルター
                if (filterType && entry.type != *filterType) {
                    continue;
                }

                if (visited.find(entry.neighborId) == visited.end()) {
                    visited.insert(entry.neighborId);
                    toVisit.push(entry.neighborId);
                }
            }
        }
    }

    return result;
}

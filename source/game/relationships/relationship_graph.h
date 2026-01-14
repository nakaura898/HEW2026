//----------------------------------------------------------------------------
//! @file   relationship_graph.h
//! @brief  関係グラフ - エンティティ間の縁をグラフ構造で管理
//----------------------------------------------------------------------------
#pragma once

#include "game/bond/bond.h"
#include "game/bond/bondable_entity.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdint>

//----------------------------------------------------------------------------
//! @brief エッジデータ（縁情報）
//----------------------------------------------------------------------------
struct EdgeData
{
    uint32_t id = 0;            //!< エッジID
    std::string nodeA;          //!< ノードA（エンティティID）
    std::string nodeB;          //!< ノードB（エンティティID）
    BondType type = BondType::Basic;  //!< 縁タイプ
    BondableEntity entityA;     //!< エンティティA（実体参照）
    BondableEntity entityB;     //!< エンティティB（実体参照）
};

//----------------------------------------------------------------------------
//! @brief クラスター（連結成分）
//----------------------------------------------------------------------------
struct Cluster
{
    std::vector<std::string> nodeIds;       //!< ノードIDリスト
    std::vector<BondableEntity> entities;   //!< エンティティリスト
};

//----------------------------------------------------------------------------
//! @brief 関係グラフ
//! @details エンティティ（ノード）と縁（エッジ）をグラフ構造で管理
//!          - 隣接リストによる効率的なクエリ
//!          - タイプ別のエッジ検索
//!          - クラスター（連結成分）検出
//----------------------------------------------------------------------------
class RelationshipGraph
{
public:
    RelationshipGraph() = default;
    ~RelationshipGraph() = default;

    //------------------------------------------------------------------------
    // エッジ操作
    //------------------------------------------------------------------------

    //! @brief エッジを追加
    //! @param a エンティティA
    //! @param b エンティティB
    //! @param type 縁タイプ
    //! @return エッジID（0は失敗）
    uint32_t AddEdge(const BondableEntity& a, const BondableEntity& b, BondType type);

    //! @brief エッジを削除（ID指定）
    //! @param edgeId エッジID
    //! @return 削除成功ならtrue
    bool RemoveEdge(uint32_t edgeId);

    //! @brief エッジを削除（ノード指定）
    //! @param a エンティティA
    //! @param b エンティティB
    //! @return 削除成功ならtrue
    bool RemoveEdge(const BondableEntity& a, const BondableEntity& b);

    //! @brief ノードに関連する全エッジを削除
    //! @param entity 対象エンティティ
    void RemoveAllEdgesFor(const BondableEntity& entity);

    //! @brief 全エッジをクリア
    void Clear();

    //------------------------------------------------------------------------
    // 基本クエリ
    //------------------------------------------------------------------------

    //! @brief 2ノード間にエッジがあるか判定
    [[nodiscard]] bool HasEdge(const BondableEntity& a, const BondableEntity& b) const;

    //! @brief 2ノード間のエッジを取得
    //! @return エッジデータへのポインタ（なければnullptr）
    [[nodiscard]] const EdgeData* GetEdge(const BondableEntity& a, const BondableEntity& b) const;

    //! @brief ノードの全隣接ノードを取得
    [[nodiscard]] std::vector<BondableEntity> GetNeighbors(const BondableEntity& node) const;

    //! @brief ノードの指定タイプの隣接ノードを取得
    [[nodiscard]] std::vector<BondableEntity> GetNeighborsByType(const BondableEntity& node, BondType type) const;

    //! @brief ノードの全エッジを取得
    [[nodiscard]] std::vector<const EdgeData*> GetEdgesFor(const BondableEntity& node) const;

    //! @brief エッジ数を取得
    [[nodiscard]] size_t GetEdgeCount() const { return edges_.size(); }

    //! @brief 全エッジを取得
    [[nodiscard]] std::vector<const EdgeData*> GetAllEdges() const;

    //! @brief 指定タイプの全エッジを取得
    [[nodiscard]] std::vector<const EdgeData*> GetEdgesByType(BondType type) const;

    //------------------------------------------------------------------------
    // グラフアルゴリズム
    //------------------------------------------------------------------------

    //! @brief 2ノードが推移的に接続されているか判定（任意の縁タイプ）
    [[nodiscard]] bool AreConnected(const BondableEntity& a, const BondableEntity& b) const;

    //! @brief 2ノードが推移的に接続されているか判定（指定タイプのみ）
    [[nodiscard]] bool AreConnectedByType(const BondableEntity& a, const BondableEntity& b, BondType type) const;

    //! @brief 連結成分を取得（任意の縁タイプ）
    [[nodiscard]] Cluster GetConnectedComponent(const BondableEntity& start) const;

    //! @brief 連結成分を取得（指定タイプのみ）
    [[nodiscard]] Cluster GetConnectedComponentByType(const BondableEntity& start, BondType type) const;

    //! @brief 指定タイプの全クラスターを検出
    [[nodiscard]] std::vector<Cluster> FindClustersByType(BondType type) const;

private:
    //! @brief 隣接ノード情報
    struct AdjacencyEntry
    {
        std::string neighborId;     //!< 隣接ノードID
        uint32_t edgeId;            //!< エッジID
        BondType type;              //!< 縁タイプ
    };

    //! @brief BFSで連結成分を探索
    Cluster BFS(const std::string& startId, BondType* filterType) const;

    uint32_t nextEdgeId_ = 1;  //!< 次のエッジID

    //! @brief 全エッジ（ID→データ）
    std::unordered_map<uint32_t, EdgeData> edges_;

    //! @brief 隣接リスト（ノードID→隣接情報リスト）
    std::unordered_map<std::string, std::vector<AdjacencyEntry>> adjacency_;

    //! @brief タイプ別エッジインデックス（タイプ→エッジIDリスト）
    std::unordered_map<int, std::vector<uint32_t>> typeIndex_;

    //! @brief ノードID→エンティティ マッピング
    std::unordered_map<std::string, BondableEntity> nodeEntities_;
};

//----------------------------------------------------------------------------
//! @file   relationship_facade.h
//! @brief  RelationshipFacade - 縁システムの高レベルAPI
//----------------------------------------------------------------------------
#pragma once

#include "relationship_graph.h"
#include "game/bond/bondable_entity.h"
#include "game/ai/group_ai.h"
#include <vector>
#include <functional>
#include <memory>
#include <cassert>

// 前方宣言
class Group;
class Player;
class Bond;

//----------------------------------------------------------------------------
//! @brief RelationshipFacade（シングルトン）
//! @details 縁システムの高レベルAPIを提供
//!          - BondManager/FactionManager/LoveBondSystemの機能を統合
//!          - グラフベースの効率的なクエリ
//!          - イベント駆動の自動更新
//----------------------------------------------------------------------------
class RelationshipFacade
{
public:
    //! @brief シングルトンインスタンス取得
    static RelationshipFacade& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~RelationshipFacade() = default;

    //------------------------------------------------------------------------
    // 初期化・シャットダウン
    //------------------------------------------------------------------------

    //! @brief 初期化（シーン開始時に呼び出し）
    void Initialize();

    //! @brief シャットダウン（シーン終了時に呼び出し）
    void Shutdown();

    //! @brief プレイヤー参照を設定
    void SetPlayer(Player* player) { player_ = player; }

    //! @brief プレイヤー参照を取得
    [[nodiscard]] Player* GetPlayer() const { return player_; }

    //------------------------------------------------------------------------
    // 縁操作（高レベルAPI）
    //------------------------------------------------------------------------

    //! @brief 縁を結ぶ
    //! @param a エンティティA
    //! @param b エンティティB
    //! @param type 縁タイプ
    //! @return 成功ならtrue
    bool Bind(const BondableEntity& a, const BondableEntity& b, BondType type = BondType::Basic);

    //! @brief 縁を切る
    //! @param a エンティティA
    //! @param b エンティティB
    //! @return 成功ならtrue
    bool Cut(const BondableEntity& a, const BondableEntity& b);

    //! @brief エンティティの全縁を切る
    //! @param entity 対象エンティティ
    void CutAll(const BondableEntity& entity);

    //------------------------------------------------------------------------
    // 敵味方判定（ゲームロジック用）
    //------------------------------------------------------------------------

    //! @brief 2つのエンティティが味方か判定（縁で繋がっている）
    //! @details 推移的接続を考慮（A-B-C → AとCも味方）
    [[nodiscard]] bool AreFriendly(const BondableEntity& a, const BondableEntity& b) const;

    //! @brief 2つのエンティティが敵か判定（縁で繋がっていない）
    [[nodiscard]] bool AreHostile(const BondableEntity& a, const BondableEntity& b) const;

    //! @brief 2つのエンティティが直接縁で繋がっているか判定
    [[nodiscard]] bool AreDirectlyConnected(const BondableEntity& a, const BondableEntity& b) const;

    //------------------------------------------------------------------------
    // クラスター取得（グループ化）
    //------------------------------------------------------------------------

    //! @brief 指定タイプの縁で繋がったエンティティを取得
    //! @param start 開始エンティティ
    //! @param type 縁タイプ
    //! @return 接続されたエンティティのリスト（start自身を含む）
    [[nodiscard]] std::vector<BondableEntity> GetCluster(const BondableEntity& start, BondType type) const;

    //! @brief 味方（任意の縁で繋がった）エンティティを取得
    //! @param start 開始エンティティ
    //! @return 接続されたエンティティのリスト
    [[nodiscard]] std::vector<BondableEntity> GetAllies(const BondableEntity& start) const;

    //! @brief ラブ縁で繋がったグループを取得
    //! @param group 対象グループ
    //! @return ラブ縁で繋がった全グループ（自身を含む）
    [[nodiscard]] std::vector<Group*> GetLoveCluster(Group* group) const;

    //! @brief グループがラブ縁を持っているか判定
    [[nodiscard]] bool HasLovePartners(Group* group) const;

    //! @brief 指定タイプの全クラスターを取得
    [[nodiscard]] std::vector<Cluster> FindAllClusters(BondType type) const;

    //------------------------------------------------------------------------
    // ラブ効果（ターゲット同期）
    //------------------------------------------------------------------------

    //! @brief ラブクラスタ内で共有ターゲットを決定
    //! @param cluster ラブ縁で繋がったグループリスト
    //! @return 共有ターゲット（最も脅威度が高いターゲット）
    [[nodiscard]] AITarget DetermineSharedTarget(const std::vector<Group*>& cluster) const;

    //! @brief クラスタ内の全グループに同じターゲットを設定
    void SyncClusterTarget(const std::vector<Group*>& cluster, const AITarget& target);

    //------------------------------------------------------------------------
    // エッジクエリ
    //------------------------------------------------------------------------

    //! @brief エンティティの隣接エンティティを取得
    [[nodiscard]] std::vector<BondableEntity> GetNeighbors(const BondableEntity& entity) const;

    //! @brief エンティティの指定タイプ隣接エンティティを取得
    [[nodiscard]] std::vector<BondableEntity> GetNeighborsByType(const BondableEntity& entity, BondType type) const;

    //! @brief 2エンティティ間のエッジ情報を取得
    [[nodiscard]] const EdgeData* GetEdge(const BondableEntity& a, const BondableEntity& b) const;

    //! @brief 全エッジを取得
    [[nodiscard]] std::vector<const EdgeData*> GetAllEdges() const;

    //! @brief 指定タイプの全エッジを取得
    [[nodiscard]] std::vector<const EdgeData*> GetEdgesByType(BondType type) const;

    //! @brief エッジ数を取得
    [[nodiscard]] size_t GetEdgeCount() const;

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 縁作成時コールバック設定
    void SetOnBondCreated(std::function<void(const BondableEntity&, const BondableEntity&, BondType)> callback)
    {
        onBondCreated_ = callback;
    }

    //! @brief 縁削除時コールバック設定
    void SetOnBondRemoved(std::function<void(const BondableEntity&, const BondableEntity&)> callback)
    {
        onBondRemoved_ = callback;
    }

    //------------------------------------------------------------------------
    // 内部アクセス（互換性用）
    //------------------------------------------------------------------------

    //! @brief 内部グラフへのアクセス（上級者向け）
    [[nodiscard]] const RelationshipGraph& GetGraph() const { return graph_; }

private:
    RelationshipFacade() = default;
    RelationshipFacade(const RelationshipFacade&) = delete;
    RelationshipFacade& operator=(const RelationshipFacade&) = delete;

    //! @brief ターゲットの脅威度を取得
    [[nodiscard]] float GetTargetThreat(const AITarget& target) const;

    static inline std::unique_ptr<RelationshipFacade> instance_ = nullptr;

    RelationshipGraph graph_;           //!< 内部グラフ
    Player* player_ = nullptr;          //!< プレイヤー参照

    // コールバック
    std::function<void(const BondableEntity&, const BondableEntity&, BondType)> onBondCreated_;
    std::function<void(const BondableEntity&, const BondableEntity&)> onBondRemoved_;
};

//----------------------------------------------------------------------------
//! @file   love_bond_system.h
//! @brief  ラブ効果システム - ラブ縁で繋がったグループのターゲット同期
//----------------------------------------------------------------------------
#pragma once

#include "game/ai/group_ai.h"
#include <vector>
#include <set>
#include <unordered_map>
#include <functional>

// 前方宣言
class Group;
class Player;
class Bond;

//----------------------------------------------------------------------------
//! @brief ラブ効果システム（シングルトン）
//! @details ラブ縁で繋がったグループは同じターゲットを攻撃する
//----------------------------------------------------------------------------
class LoveBondSystem
{
public:
    //! @brief シングルトンインスタンス取得
    static LoveBondSystem& Get();

    //------------------------------------------------------------------------
    // 初期化・更新
    //------------------------------------------------------------------------

    //! @brief プレイヤー参照を設定
    void SetPlayer(Player* player) { player_ = player; }

    //! @brief ラブグループを再構築（縁の作成/削除時に呼び出す）
    void RebuildLoveGroups();

    //------------------------------------------------------------------------
    // クエリ
    //------------------------------------------------------------------------

    //! @brief 指定グループのラブパートナーを取得
    //! @param group 対象グループ
    //! @return ラブ縁で繋がった全グループ（自身を含む）
    [[nodiscard]] std::vector<Group*> GetLoveCluster(Group* group) const;

    //! @brief グループがラブ縁を持っているか判定
    //! @param group 対象グループ
    //! @return ラブ縁を持っていればtrue
    [[nodiscard]] bool HasLovePartners(Group* group) const;

    //------------------------------------------------------------------------
    // ターゲット同期
    //------------------------------------------------------------------------

    //! @brief ラブクラスタ内で共有ターゲットを決定
    //! @param cluster ラブ縁で繋がったグループリスト
    //! @return 共有ターゲット（最も脅威度が高いターゲット）
    [[nodiscard]] AITarget DetermineSharedTarget(const std::vector<Group*>& cluster) const;

    //! @brief クラスタ内の全グループに同じターゲットを設定
    //! @param cluster ラブ縁で繋がったグループリスト
    //! @param target 設定するターゲット
    void SyncClusterTarget(const std::vector<Group*>& cluster, const AITarget& target);

    //------------------------------------------------------------------------
    // デバッグ
    //------------------------------------------------------------------------

    //! @brief ラブクラスタの数を取得
    [[nodiscard]] size_t GetClusterCount() const { return loveClusters_.size(); }

private:
    LoveBondSystem() = default;
    ~LoveBondSystem() = default;
    LoveBondSystem(const LoveBondSystem&) = delete;
    LoveBondSystem& operator=(const LoveBondSystem&) = delete;

    //! @brief BFSでラブ縁のみをたどってクラスタを構築
    //! @param start 開始グループ
    //! @param visited 訪問済みセット
    //! @return クラスタ内の全グループ
    std::vector<Group*> BuildClusterBFS(Group* start, std::set<Group*>& visited) const;

    //! @brief ターゲットの脅威度を取得
    [[nodiscard]] float GetTargetThreat(const AITarget& target) const;

    Player* player_ = nullptr;                              //!< プレイヤー参照
    std::vector<std::vector<Group*>> loveClusters_;         //!< ラブ縁で繋がったグループのクラスタ
    std::unordered_map<Group*, size_t> clusterIndexCache_;  //!< Group→クラスタインデックスのキャッシュ
};

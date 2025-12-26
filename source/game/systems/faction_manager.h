//----------------------------------------------------------------------------
//! @file   faction_manager.h
//! @brief  FactionManager - 陣営を管理
//----------------------------------------------------------------------------
#pragma once

#include "faction.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

//----------------------------------------------------------------------------
//! @brief FactionManager（シングルトン）
//! @details 縁ネットワークから陣営を構築・管理する
//----------------------------------------------------------------------------
class FactionManager
{
public:
    //! @brief シングルトンインスタンス取得
    static FactionManager& Get();

    //------------------------------------------------------------------------
    // エンティティ管理
    //------------------------------------------------------------------------

    //! @brief エンティティを登録
    void RegisterEntity(BondableEntity entity);

    //! @brief エンティティを登録解除
    void UnregisterEntity(BondableEntity entity);

    //! @brief 全エンティティをクリア
    void ClearEntities();

    //------------------------------------------------------------------------
    // Faction構築
    //------------------------------------------------------------------------

    //! @brief 縁ネットワークからFactionを再構築
    //! @details 縁が作成/削除された時に呼び出す
    void RebuildFactions();

    //------------------------------------------------------------------------
    // 判定
    //------------------------------------------------------------------------

    //! @brief 2つのエンティティが同じFactionか判定
    [[nodiscard]] bool AreSameFaction(BondableEntity a, BondableEntity b) const;

    //! @brief エンティティが所属するFactionを取得
    //! @return 所属Faction。見つからない場合はnullptr
    [[nodiscard]] Faction* GetFaction(BondableEntity entity) const;

    //! @brief 全Factionを取得
    [[nodiscard]] const std::vector<std::unique_ptr<Faction>>& GetFactions() const { return factions_; }

    //! @brief Faction数を取得
    [[nodiscard]] size_t GetFactionCount() const { return factions_.size(); }

private:
    FactionManager() = default;
    ~FactionManager() = default;
    FactionManager(const FactionManager&) = delete;
    FactionManager& operator=(const FactionManager&) = delete;

    //! @brief BFS/DFSで接続成分を探索してFactionを構築
    void BuildFactionFromEntity(BondableEntity start,
                                 std::vector<bool>& visited,
                                 Faction& faction);

    //! @brief エンティティのインデックスを取得
    [[nodiscard]] int GetEntityIndex(BondableEntity entity) const;

    std::vector<BondableEntity> entities_;              //!< 全エンティティ
    std::vector<std::unique_ptr<Faction>> factions_;    //!< 構築されたFaction群
    std::unordered_map<std::string, Faction*> factionCache_;  //!< エンティティID→Factionのキャッシュ
};

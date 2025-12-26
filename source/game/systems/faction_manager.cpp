//----------------------------------------------------------------------------
//! @file   faction_manager.cpp
//! @brief  FactionManager実装
//----------------------------------------------------------------------------
#include "faction_manager.h"
#include "game/bond/bond_manager.h"
#include "common/logging/logging.h"
#include <queue>
#include <algorithm>

//----------------------------------------------------------------------------
FactionManager& FactionManager::Get()
{
    static FactionManager instance;
    return instance;
}

//----------------------------------------------------------------------------
void FactionManager::RegisterEntity(BondableEntity entity)
{
    // 重複チェック
    for (const BondableEntity& existing : entities_) {
        if (BondableHelper::GetId(existing) == BondableHelper::GetId(entity)) {
            return;
        }
    }

    entities_.push_back(entity);
    LOG_INFO("[FactionManager] Entity registered: " + BondableHelper::GetId(entity));

    // Factionを再構築
    RebuildFactions();
}

//----------------------------------------------------------------------------
void FactionManager::UnregisterEntity(BondableEntity entity)
{
    std::string targetId = BondableHelper::GetId(entity);

    auto it = std::remove_if(entities_.begin(), entities_.end(),
        [&targetId](const BondableEntity& e) {
            return BondableHelper::GetId(e) == targetId;
        });

    if (it != entities_.end()) {
        entities_.erase(it, entities_.end());
        LOG_INFO("[FactionManager] Entity unregistered: " + targetId);

        // Factionを再構築
        RebuildFactions();
    }
}

//----------------------------------------------------------------------------
void FactionManager::ClearEntities()
{
    entities_.clear();
    factions_.clear();
    factionCache_.clear();
    LOG_INFO("[FactionManager] All entities cleared");
}

//----------------------------------------------------------------------------
void FactionManager::RebuildFactions()
{
    factions_.clear();
    factionCache_.clear();

    if (entities_.empty()) {
        return;
    }

    // 訪問済みフラグ
    std::vector<bool> visited(entities_.size(), false);

    // 各エンティティを起点にBFSで接続成分を探索
    for (size_t i = 0; i < entities_.size(); ++i) {
        if (visited[i]) continue;

        auto faction = std::make_unique<Faction>();
        BuildFactionFromEntity(entities_[i], visited, *faction);

        if (faction->GetMemberCount() > 0) {
            factions_.push_back(std::move(faction));
        }
    }

    // キャッシュを構築（O(1)ルックアップ用）
    for (const auto& faction : factions_) {
        for (const BondableEntity& member : faction->GetMembers()) {
            factionCache_[BondableHelper::GetId(member)] = faction.get();
        }
    }

    LOG_INFO("[FactionManager] Rebuilt " + std::to_string(factions_.size()) + " factions");

    // デバッグ出力
    for (size_t i = 0; i < factions_.size(); ++i) {
        std::string members;
        for (const BondableEntity& member : factions_[i]->GetMembers()) {
            if (!members.empty()) members += ", ";
            members += BondableHelper::GetId(member);
        }
        LOG_INFO("  Faction " + std::to_string(i) + ": [" + members + "]");
    }
}

//----------------------------------------------------------------------------
void FactionManager::BuildFactionFromEntity(BondableEntity start,
                                             std::vector<bool>& visited,
                                             Faction& faction)
{
    std::queue<BondableEntity> queue;
    queue.push(start);

    int startIdx = GetEntityIndex(start);
    if (startIdx >= 0) {
        visited[static_cast<size_t>(startIdx)] = true;
    }

    while (!queue.empty()) {
        BondableEntity current = queue.front();
        queue.pop();

        faction.AddMember(current);

        // このエンティティに繋がっている縁を取得
        std::vector<Bond*> bonds = BondManager::Get().GetBondsFor(current);

        for (Bond* bond : bonds) {
            // 縁の相手を取得
            BondableEntity other = bond->GetOther(current);

            // 訪問済みかチェック
            int otherIdx = GetEntityIndex(other);
            if (otherIdx < 0) continue;
            if (visited[static_cast<size_t>(otherIdx)]) continue;

            // キューに追加
            visited[static_cast<size_t>(otherIdx)] = true;
            queue.push(other);
        }
    }
}

//----------------------------------------------------------------------------
int FactionManager::GetEntityIndex(BondableEntity entity) const
{
    std::string targetId = BondableHelper::GetId(entity);

    for (size_t i = 0; i < entities_.size(); ++i) {
        if (BondableHelper::GetId(entities_[i]) == targetId) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

//----------------------------------------------------------------------------
bool FactionManager::AreSameFaction(BondableEntity a, BondableEntity b) const
{
    Faction* factionA = GetFaction(a);
    Faction* factionB = GetFaction(b);

    if (!factionA || !factionB) {
        return false;
    }

    return factionA == factionB;
}

//----------------------------------------------------------------------------
Faction* FactionManager::GetFaction(BondableEntity entity) const
{
    // キャッシュを使用してO(1)でルックアップ
    auto it = factionCache_.find(BondableHelper::GetId(entity));
    if (it != factionCache_.end()) {
        return it->second;
    }
    return nullptr;
}

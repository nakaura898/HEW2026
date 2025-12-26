//----------------------------------------------------------------------------
//! @file   bond_manager.cpp
//! @brief  縁マネージャー実装
//----------------------------------------------------------------------------
#include "bond_manager.h"
#include "game/entities/group.h"
#include "common/logging/logging.h"
#include <algorithm>
#include <queue>
#include <unordered_set>

//----------------------------------------------------------------------------
BondManager& BondManager::Get()
{
    static BondManager instance;
    return instance;
}

//----------------------------------------------------------------------------
Bond* BondManager::CreateBond(BondableEntity a, BondableEntity b, BondType type)
{
    // 同一エンティティ同士は結べない
    if (BondableHelper::IsSame(a, b)) {
        LOG_WARN("[BondManager] Cannot create bond between same entity");
        return nullptr;
    }

    // 既に接続済みかチェック
    if (AreDirectlyConnected(a, b)) {
        LOG_WARN("[BondManager] Bond already exists between " +
                    BondableHelper::GetId(a) + " and " + BondableHelper::GetId(b));
        return nullptr;
    }

    // 縁を作成
    std::unique_ptr<Bond> bond = std::make_unique<Bond>(a, b, type);
    Bond* bondPtr = bond.get();
    bonds_.push_back(std::move(bond));

    // グループの状態をリセット（攻撃中でも正常に動作するように）
    if (std::holds_alternative<Group*>(a)) {
        Group* group = std::get<Group*>(a);
        if (group) {
            group->ResetOnBond();
        }
    }
    if (std::holds_alternative<Group*>(b)) {
        Group* group = std::get<Group*>(b);
        if (group) {
            group->ResetOnBond();
        }
    }

    // キャッシュを再構築
    RebuildCache();

    LOG_INFO("[BondManager] Bond created: " +
             BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b));

    // コールバック呼び出し
    if (onBondCreated_) {
        onBondCreated_(bondPtr);
    }

    return bondPtr;
}

//----------------------------------------------------------------------------
bool BondManager::RemoveBond(Bond* bond)
{
    if (!bond) return false;

    auto it = std::find_if(bonds_.begin(), bonds_.end(),
        [bond](const std::unique_ptr<Bond>& b) { return b.get() == bond; });

    if (it != bonds_.end()) {
        BondableEntity a = bond->GetEntityA();
        BondableEntity b = bond->GetEntityB();

        LOG_INFO("[BondManager] Bond removed: " +
                 BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b));

        bonds_.erase(it);

        // キャッシュを再構築
        RebuildCache();

        // コールバック呼び出し
        if (onBondRemoved_) {
            onBondRemoved_(a, b);
        }

        return true;
    }

    return false;
}

//----------------------------------------------------------------------------
bool BondManager::RemoveBond(const BondableEntity& a, const BondableEntity& b)
{
    Bond* bond = GetBond(a, b);
    return RemoveBond(bond);
}

//----------------------------------------------------------------------------
void BondManager::RemoveAllBondsFor(const BondableEntity& entity)
{
    std::vector<Bond*> toRemove = GetBondsFor(entity);
    for (Bond* bond : toRemove) {
        RemoveBond(bond);
    }
}

//----------------------------------------------------------------------------
void BondManager::Clear()
{
    bonds_.clear();
    entityBondsCache_.clear();
    typeBondsCache_.clear();
    LOG_INFO("[BondManager] All bonds cleared");
}

//----------------------------------------------------------------------------
void BondManager::RebuildCache()
{
    entityBondsCache_.clear();
    typeBondsCache_.clear();

    for (const std::unique_ptr<Bond>& bond : bonds_) {
        Bond* bondPtr = bond.get();

        // エンティティ別キャッシュ
        std::string idA = BondableHelper::GetId(bond->GetEntityA());
        std::string idB = BondableHelper::GetId(bond->GetEntityB());
        entityBondsCache_[idA].push_back(bondPtr);
        entityBondsCache_[idB].push_back(bondPtr);

        // タイプ別キャッシュ
        int typeInt = static_cast<int>(bond->GetType());
        typeBondsCache_[typeInt].push_back(bondPtr);
    }
}

//----------------------------------------------------------------------------
bool BondManager::AreDirectlyConnected(const BondableEntity& a, const BondableEntity& b) const
{
    return GetBond(a, b) != nullptr;
}

//----------------------------------------------------------------------------
Bond* BondManager::GetBond(const BondableEntity& a, const BondableEntity& b) const
{
    for (const std::unique_ptr<Bond>& bond : bonds_) {
        if (bond->Connects(a, b)) {
            return bond.get();
        }
    }
    return nullptr;
}

//----------------------------------------------------------------------------
std::vector<Bond*> BondManager::GetBondsFor(const BondableEntity& entity) const
{
    // キャッシュからO(1)で取得
    std::string id = BondableHelper::GetId(entity);
    auto it = entityBondsCache_.find(id);
    if (it != entityBondsCache_.end()) {
        return it->second;
    }
    return {};
}

//----------------------------------------------------------------------------
std::vector<Bond*> BondManager::GetBondsByType(BondType type) const
{
    // キャッシュからO(1)で取得
    int typeInt = static_cast<int>(type);
    auto it = typeBondsCache_.find(typeInt);
    if (it != typeBondsCache_.end()) {
        return it->second;
    }
    return {};
}

//----------------------------------------------------------------------------
std::vector<BondableEntity> BondManager::GetConnectedNetwork(const BondableEntity& start) const
{
    std::vector<BondableEntity> result;
    std::queue<BondableEntity> toVisit;
    std::unordered_set<std::string> visited;

    toVisit.push(start);
    visited.insert(BondableHelper::GetId(start));

    while (!toVisit.empty()) {
        BondableEntity current = toVisit.front();
        toVisit.pop();
        result.push_back(current);

        // 隣接エンティティを探索
        std::vector<Bond*> bonds = GetBondsFor(current);
        for (Bond* bond : bonds) {
            BondableEntity other = bond->GetOther(current);
            std::string otherId = BondableHelper::GetId(other);

            if (visited.find(otherId) == visited.end()) {
                visited.insert(otherId);
                toVisit.push(other);
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------------
bool BondManager::AreTransitivelyConnected(const BondableEntity& a, const BondableEntity& b) const
{
    std::vector<BondableEntity> network = GetConnectedNetwork(a);
    std::string targetId = BondableHelper::GetId(b);

    for (const BondableEntity& entity : network) {
        if (BondableHelper::GetId(entity) == targetId) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------
//! @file   insulation_system.cpp
//! @brief  絶縁システム実装
//----------------------------------------------------------------------------
#include "insulation_system.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
InsulationSystem& InsulationSystem::Get()
{
    assert(instance_ && "InsulationSystem::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void InsulationSystem::Create()
{
    if (!instance_) {
        instance_.reset(new InsulationSystem());
    }
}

//----------------------------------------------------------------------------
void InsulationSystem::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
std::pair<std::string, std::string> InsulationSystem::MakePairKey(
    const BondableEntity& a, const BondableEntity& b) const
{
    std::string idA = BondableHelper::GetId(a);
    std::string idB = BondableHelper::GetId(b);

    // 順序を統一（小さい方を先に）
    if (idA > idB) {
        std::swap(idA, idB);
    }

    return { idA, idB };
}

//----------------------------------------------------------------------------
void InsulationSystem::AddInsulation(const BondableEntity& a, const BondableEntity& b)
{
    if (BondableHelper::IsNull(a) || BondableHelper::IsNull(b)) {
        LOG_WARN("[InsulationSystem] BUG: AddInsulation called with null entity");
        return;
    }

    if (BondableHelper::IsSame(a, b)) {
        LOG_WARN("[InsulationSystem] BUG: AddInsulation called with same entity");
        return;
    }

    auto key = MakePairKey(a, b);

    auto [it, inserted] = insulatedPairs_.insert(key);
    if (!inserted) {
        LOG_WARN("[InsulationSystem] Already insulated: " + key.first + " <-> " + key.second);
        return;
    }

    LOG_INFO("[InsulationSystem] Insulation added: " + key.first + " <-> " + key.second);

    if (onInsulationAdded_) {
        onInsulationAdded_(a, b);
    }
}

//----------------------------------------------------------------------------
bool InsulationSystem::IsInsulated(const BondableEntity& a, const BondableEntity& b) const
{
    auto key = MakePairKey(a, b);
    return insulatedPairs_.find(key) != insulatedPairs_.end();
}

//----------------------------------------------------------------------------
void InsulationSystem::RemoveInsulation(const BondableEntity& a, const BondableEntity& b)
{
    auto key = MakePairKey(a, b);
    auto it = insulatedPairs_.find(key);

    if (it != insulatedPairs_.end()) {
        insulatedPairs_.erase(it);
        LOG_INFO("[InsulationSystem] Insulation removed: " + key.first + " <-> " + key.second);
    }
}

//----------------------------------------------------------------------------
void InsulationSystem::OnEntityDefeated(const BondableEntity& entity)
{
    std::string entityId = BondableHelper::GetId(entity);

    // このエンティティに関連する全ての絶縁を削除
    auto it = insulatedPairs_.begin();
    while (it != insulatedPairs_.end()) {
        if (it->first == entityId || it->second == entityId) {
            LOG_INFO("[InsulationSystem] Removed insulation for defeated entity: " +
                     it->first + " <-> " + it->second);
            it = insulatedPairs_.erase(it);
        } else {
            ++it;
        }
    }
}

//----------------------------------------------------------------------------
void InsulationSystem::Clear()
{
    insulatedPairs_.clear();
    LOG_INFO("[InsulationSystem] All insulations cleared");
}

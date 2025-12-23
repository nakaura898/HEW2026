//----------------------------------------------------------------------------
//! @file   bond_type_selector.cpp
//! @brief  縁タイプ選択システム実装
//----------------------------------------------------------------------------
#include "bond_type_selector.h"
#include "bind_system.h"
#include "game/systems/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
BondTypeSelector& BondTypeSelector::Get()
{
    static BondTypeSelector instance;
    return instance;
}

//----------------------------------------------------------------------------
void BondTypeSelector::CycleNextType()
{
    switch (currentType_) {
        case BondType::Basic:
            SetCurrentType(BondType::Friends);
            break;
        case BondType::Friends:
            SetCurrentType(BondType::Love);
            break;
        case BondType::Love:
            SetCurrentType(BondType::Basic);
            break;
    }
}

//----------------------------------------------------------------------------
void BondTypeSelector::CyclePrevType()
{
    switch (currentType_) {
        case BondType::Basic:
            SetCurrentType(BondType::Love);
            break;
        case BondType::Friends:
            SetCurrentType(BondType::Basic);
            break;
        case BondType::Love:
            SetCurrentType(BondType::Friends);
            break;
    }
}

//----------------------------------------------------------------------------
void BondTypeSelector::SetCurrentType(BondType type)
{
    if (currentType_ == type) return;

    currentType_ = type;

    // BindSystemにも反映
    BindSystem::Get().SetPendingBondType(type);

    LOG_INFO("[BondTypeSelector] Type changed to: " + std::string(GetTypeName(type)));

    // EventBus通知
    EventBus::Get().Publish(BondTypeSelectedEvent{ type });

    if (onTypeChanged_) {
        onTypeChanged_(type);
    }
}

//----------------------------------------------------------------------------
void BondTypeSelector::Reset()
{
    SetCurrentType(BondType::Basic);
}

//----------------------------------------------------------------------------
const char* BondTypeSelector::GetTypeName(BondType type)
{
    switch (type) {
        case BondType::Basic:   return "Basic";
        case BondType::Friends: return "Friends";
        case BondType::Love:    return "Love";
        default:                return "Unknown";
    }
}

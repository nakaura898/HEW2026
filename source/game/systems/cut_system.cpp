//----------------------------------------------------------------------------
//! @file   cut_system.cpp
//! @brief  切システム実装
//----------------------------------------------------------------------------
#include "cut_system.h"
#include "time_manager.h"
#include "bind_system.h"
#include "fe_system.h"
#include "stagger_system.h"
#include "insulation_system.h"
#include "game/bond/bond_manager.h"
#include "game/entities/group.h"
#include "game/systems/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
CutSystem& CutSystem::Get()
{
    static CutSystem instance;
    return instance;
}

//----------------------------------------------------------------------------
void CutSystem::Enable()
{
    if (isEnabled_) return;

    // 排他制御：結モードを無効化
    if (BindSystem::Get().IsEnabled()) {
        BindSystem::Get().Disable();
    }

    isEnabled_ = true;
    ClearSelection();

    // 時間停止
    TimeManager::Get().Freeze();

    LOG_INFO("[CutSystem] Cut mode enabled");

    // EventBus通知
    EventBus::Get().Publish(CutModeChangedEvent{ true });

    if (onModeChanged_) {
        onModeChanged_(true);
    }
}

//----------------------------------------------------------------------------
void CutSystem::Disable()
{
    if (!isEnabled_) return;

    isEnabled_ = false;
    ClearSelection();

    // 時間再開
    TimeManager::Get().Resume();

    LOG_INFO("[CutSystem] Cut mode disabled");

    // EventBus通知
    EventBus::Get().Publish(CutModeChangedEvent{ false });

    if (onModeChanged_) {
        onModeChanged_(false);
    }
}

//----------------------------------------------------------------------------
void CutSystem::Toggle()
{
    if (isEnabled_) {
        Disable();
    } else {
        Enable();
    }
}

//----------------------------------------------------------------------------
void CutSystem::SelectBond(Bond* bond)
{
    if (!isEnabled_ || !bond) return;

    selectedBond_ = bond;

    LOG_INFO("[CutSystem] Bond selected: " +
             BondableHelper::GetId(bond->GetEntityA()) + " <-> " +
             BondableHelper::GetId(bond->GetEntityB()));

    if (onBondSelected_) {
        onBondSelected_(bond);
    }
}

//----------------------------------------------------------------------------
bool CutSystem::CutSelectedBond()
{
    if (!selectedBond_) return false;
    return CutBond(selectedBond_);
}

//----------------------------------------------------------------------------
bool CutSystem::CutBond(Bond* bond)
{
    if (!isEnabled_ || !bond) return false;

    if (!CanCut(bond)) {
        LOG_WARN("[CutSystem] Cannot cut this bond");
        return false;
    }

    // FEチェック・消費
    if (!FESystem::Get().CanConsume(cutCost_)) {
        LOG_WARN("[CutSystem] Not enough FE to cut");
        return false;
    }
    FESystem::Get().Consume(cutCost_);

    BondableEntity a = bond->GetEntityA();
    BondableEntity b = bond->GetEntityB();

    // 縁を削除
    bool removed = BondManager::Get().RemoveBond(bond);
    if (removed) {
        LOG_INFO("[CutSystem] Bond cut between " +
                 BondableHelper::GetId(a) + " and " + BondableHelper::GetId(b));

        // 硬直を付与（グループのみ）
        float staggerDuration = StaggerSystem::Get().GetDefaultDuration();
        if (Group* groupA = BondableHelper::AsGroup(a)) {
            StaggerSystem::Get().ApplyStagger(groupA, staggerDuration);
        }
        if (Group* groupB = BondableHelper::AsGroup(b)) {
            StaggerSystem::Get().ApplyStagger(groupB, staggerDuration);
        }

        // 絶縁を追加
        InsulationSystem::Get().AddInsulation(a, b);

        // EventBus通知
        EventBus::Get().Publish(BondRemovedEvent{ a, b });

        if (onBondCut_) {
            onBondCut_(a, b);
        }

        ClearSelection();

        // 切断後に自動でモード終了（時間再開）
        Disable();

        return true;
    }

    return false;
}

//----------------------------------------------------------------------------
void CutSystem::ClearSelection()
{
    selectedBond_ = nullptr;
}

//----------------------------------------------------------------------------
bool CutSystem::CanCut(Bond* bond) const
{
    if (!bond) return false;

    // TODO: 追加の条件（例：特定の縁タイプは切れない等）

    return true;
}

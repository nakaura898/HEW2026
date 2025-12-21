//----------------------------------------------------------------------------
//! @file   bind_system.cpp
//! @brief  結システム実装
//----------------------------------------------------------------------------
#include "bind_system.h"
#include "time_manager.h"
#include "cut_system.h"
#include "fe_system.h"
#include "insulation_system.h"
#include "game/bond/bond_manager.h"
#include "game/systems/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
BindSystem& BindSystem::Get()
{
    static BindSystem instance;
    return instance;
}

//----------------------------------------------------------------------------
void BindSystem::Enable()
{
    if (isEnabled_) return;

    // 排他制御：切モードを無効化
    if (CutSystem::Get().IsEnabled()) {
        CutSystem::Get().Disable();
    }

    isEnabled_ = true;
    ClearMark();

    // 時間停止
    TimeManager::Get().Freeze();

    LOG_INFO("[BindSystem] Bind mode enabled");

    // EventBus通知
    EventBus::Get().Publish(BindModeChangedEvent{ true });

    if (onModeChanged_) {
        onModeChanged_(true);
    }
}

//----------------------------------------------------------------------------
void BindSystem::Disable()
{
    if (!isEnabled_) return;

    isEnabled_ = false;
    ClearMark();

    // 時間再開
    TimeManager::Get().Resume();

    LOG_INFO("[BindSystem] Bind mode disabled");

    // EventBus通知
    EventBus::Get().Publish(BindModeChangedEvent{ false });

    if (onModeChanged_) {
        onModeChanged_(false);
    }
}

//----------------------------------------------------------------------------
void BindSystem::Toggle()
{
    if (isEnabled_) {
        Disable();
    } else {
        Enable();
    }
}

//----------------------------------------------------------------------------
bool BindSystem::MarkEntity(BondableEntity entity)
{
    if (!isEnabled_) return false;

    // 1体目のマーク
    if (!markedEntity_.has_value()) {
        markedEntity_ = entity;

        LOG_INFO("[BindSystem] Entity marked: " + BondableHelper::GetId(entity));

        // EventBus通知
        EventBus::Get().Publish(EntityMarkedEvent{ entity });

        if (onEntityMarked_) {
            onEntityMarked_(entity);
        }

        return false;
    }

    // 2体目 - 縁を作成
    BondableEntity first = markedEntity_.value();

    // 同一エンティティチェック
    if (BondableHelper::IsSame(first, entity)) {
        LOG_INFO("[BindSystem] Same entity selected, clearing mark");
        ClearMark();
        return false;
    }

    // 結べるかチェック
    if (!CanBind(first, entity)) {
        LOG_WARN("[BindSystem] Cannot bind these entities");
        return false;
    }

    // FEチェック・消費
    if (!FESystem::Get().CanConsume(bindCost_)) {
        LOG_WARN("[BindSystem] Not enough FE to bind");
        return false;
    }
    FESystem::Get().Consume(bindCost_);

    // 縁を作成
    Bond* bond = BondManager::Get().CreateBond(first, entity, BondType::Basic);
    if (bond) {
        LOG_INFO("[BindSystem] Bond created between " +
                 BondableHelper::GetId(first) + " and " + BondableHelper::GetId(entity));

        // EventBus通知
        EventBus::Get().Publish(BondCreatedEvent{ first, entity, bond });

        if (onBondCreated_) {
            onBondCreated_(first, entity);
        }

        ClearMark();

        // 縁作成後に自動でモード終了（時間再開）
        Disable();

        return true;
    }

    return false;
}

//----------------------------------------------------------------------------
void BindSystem::ClearMark()
{
    markedEntity_.reset();
}

//----------------------------------------------------------------------------
bool BindSystem::CanBind(const BondableEntity& a, const BondableEntity& b) const
{
    // 同一エンティティは結べない
    if (BondableHelper::IsSame(a, b)) {
        return false;
    }

    // 既に接続済みは結べない
    if (BondManager::Get().AreDirectlyConnected(a, b)) {
        return false;
    }

    // 絶縁状態は結べない
    if (InsulationSystem::Get().IsInsulated(a, b)) {
        LOG_WARN("[BindSystem] Entities are insulated");
        return false;
    }

    return true;
}

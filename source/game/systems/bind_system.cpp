//----------------------------------------------------------------------------
//! @file   bind_system.cpp
//! @brief  結システム実装
//----------------------------------------------------------------------------
#include "bind_system.h"
#include "engine/time/time_manager.h"
#include "cut_system.h"
#include "fe_system.h"
#include "insulation_system.h"
#include "game/bond/bond_manager.h"
#include "game/relationships/relationship_facade.h"
#include "game/entities/group.h"
#include "engine/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
BindSystem& BindSystem::Get()
{
    assert(instance_ && "BindSystem::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void BindSystem::Create()
{
    if (!instance_) {
        instance_.reset(new BindSystem());
    }
}

//----------------------------------------------------------------------------
void BindSystem::Destroy()
{
    instance_.reset();
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

    // 回数制限チェック
    if (!CanBindWithLimit()) {
        LOG_WARN("[BindSystem] Bind limit reached (" + std::to_string(currentBindCount_) +
                 "/" + std::to_string(maxBindCount_) + ")");
        return false;
    }

    // FEチェック・消費
    if (!FESystem::Get().CanConsume(bindCost_)) {
        LOG_WARN("[BindSystem] Not enough FE to bind");
        return false;
    }
    FESystem::Get().Consume(bindCost_);

    // 縁を作成（選択中のタイプで）
    Bond* bond = BondManager::Get().CreateBond(first, entity, pendingBondType_);
    if (!bond) {
        LOG_WARN("[BindSystem] Failed to create bond");
        return false;
    }

    // RelationshipFacadeにも同期
    bool syncSuccess = RelationshipFacade::Get().Bind(first, entity, pendingBondType_);
    if (!syncSuccess) {
        // ロールバック: BondManagerから削除 + FEリファンド
        LOG_WARN("[BindSystem] Failed to sync with RelationshipFacade, rolling back");
        BondManager::Get().RemoveBond(bond);
        FESystem::Get().Recover(bindCost_);
        LOG_INFO("[BindSystem] Refunded " + std::to_string(bindCost_) + " FE");
        return false;
    }

    // 使用回数インクリメント
    currentBindCount_++;

    LOG_INFO("[BindSystem] Bond created between " +
             BondableHelper::GetId(first) + " and " + BondableHelper::GetId(entity) +
             " (bind " + std::to_string(currentBindCount_) + "/" +
             (maxBindCount_ < 0 ? "unlimited" : std::to_string(maxBindCount_)) + ")");

    // プレイヤーと縁を結んだグループを味方化
    Group* groupToConvert = nullptr;
    if (BondableHelper::IsPlayer(first)) {
        groupToConvert = BondableHelper::AsGroup(entity);
    } else if (BondableHelper::IsPlayer(entity)) {
        groupToConvert = BondableHelper::AsGroup(first);
    }

    if (groupToConvert && groupToConvert->IsEnemy()) {
        groupToConvert->SetFaction(GroupFaction::Ally);
        LOG_INFO("[BindSystem] Group " + groupToConvert->GetId() + " became ally");

        // EventBus通知
        EventBus::Get().Publish(GroupBecameAllyEvent{ groupToConvert });
    }

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

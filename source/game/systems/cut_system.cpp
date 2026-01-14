//----------------------------------------------------------------------------
//! @file   cut_system.cpp
//! @brief  切システム実装
//----------------------------------------------------------------------------
#include "cut_system.h"
#include "engine/time/time_manager.h"
#include "bind_system.h"
#include "fe_system.h"
#include "stagger_system.h"
#include "insulation_system.h"
#include "game/bond/bond_manager.h"
#include "game/relationships/relationship_facade.h"
#include "game/entities/group.h"
#include "engine/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
CutSystem::CutSystem()
{
    // BondRemovedEventを購読（選択中のBondが外部から削除された時にクリア）
    bondRemovedSubscriptionId_ = EventBus::Get().Subscribe<BondRemovedEvent>(
        [this](const BondRemovedEvent& e) {
            OnBondRemoved(e.entityA, e.entityB);
        });
}

//----------------------------------------------------------------------------
CutSystem::~CutSystem()
{
    // イベント購読を解除
    if (bondRemovedSubscriptionId_ != 0) {
        EventBus::Get().Unsubscribe<BondRemovedEvent>(bondRemovedSubscriptionId_);
        bondRemovedSubscriptionId_ = 0;
    }
}

//----------------------------------------------------------------------------
CutSystem& CutSystem::Get()
{
    assert(instance_ && "CutSystem::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void CutSystem::Create()
{
    if (!instance_) {
        instance_.reset(new CutSystem());
    }
}

//----------------------------------------------------------------------------
void CutSystem::Destroy()
{
    instance_.reset();
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
    // use-after-free防止: 選択時にエンティティをコピー
    selectedEntityA_ = bond->GetEntityA();
    selectedEntityB_ = bond->GetEntityB();

    LOG_INFO("[CutSystem] Bond selected: " +
             BondableHelper::GetId(selectedEntityA_) + " <-> " +
             BondableHelper::GetId(selectedEntityB_));

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

    // 回数制限チェック
    if (!CanCutWithLimit()) {
        LOG_WARN("[CutSystem] Cut limit reached (" + std::to_string(currentCutCount_) +
                 "/" + std::to_string(maxCutCount_) + ")");
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

    // 先にRelationshipFacadeから削除
    bool cutSuccess = RelationshipFacade::Get().Cut(a, b);
    if (!cutSuccess) {
        // FEをリファンドして早期リターン
        LOG_WARN("[CutSystem] Failed to cut from RelationshipFacade, rolling back");
        FESystem::Get().Recover(cutCost_);
        LOG_INFO("[CutSystem] Refunded " + std::to_string(cutCost_) + " FE");
        return false;
    }

    // 縁を削除
    bool removed = BondManager::Get().RemoveBond(bond);
    if (removed) {
        // 使用回数インクリメント
        currentCutCount_++;

        LOG_INFO("[CutSystem] Bond cut between " +
                 BondableHelper::GetId(a) + " and " + BondableHelper::GetId(b) +
                 " (cut " + std::to_string(currentCutCount_) + "/" +
                 (maxCutCount_ < 0 ? "unlimited" : std::to_string(maxCutCount_)) + ")");

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
    selectedEntityA_ = static_cast<Player*>(nullptr);
    selectedEntityB_ = static_cast<Player*>(nullptr);
}

//----------------------------------------------------------------------------
bool CutSystem::CanCut(Bond* bond) const
{
    if (!bond) return false;

    // TODO: 追加の条件（例：特定の縁タイプは切れない等）

    return true;
}

//----------------------------------------------------------------------------
void CutSystem::OnBondRemoved(const BondableEntity& a, const BondableEntity& b)
{
    // 選択中のBondが削除されたらクリア
    if (!selectedBond_) return;

    // use-after-free防止: 保存済みのエンティティIDで比較
    // selectedBond_を直接参照しない（既に削除されている可能性があるため）
    bool match = (selectedEntityA_ == a && selectedEntityB_ == b) ||
                 (selectedEntityA_ == b && selectedEntityB_ == a);

    if (match) {
        LOG_INFO("[CutSystem] Selected bond was removed externally, clearing selection");
        ClearSelection();
    }
}

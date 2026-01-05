//----------------------------------------------------------------------------
//! @file   system_manager.cpp
//! @brief  ゲームシステムの一括管理実装
//----------------------------------------------------------------------------
#include "system_manager.h"

// Level 1: 基盤システム
#include "engine/event/event_bus.h"
#include "engine/time/time_manager.h"

// Level 2: 基本システム
#include "game/systems/group_manager.h"
#include "game/systems/fe_system.h"
#include "game/bond/bond_manager.h"
#include "game/systems/faction_manager.h"
#include "game/systems/insulation_system.h"
#include "game/systems/game_state_manager.h"
#include "game/systems/stage_progress_manager.h"

// Level 3: 関係性システム
#include "game/relationships/relationship_facade.h"
#include "game/systems/relationship_context.h"

// Level 4: 戦闘関連
#include "game/systems/cut_system.h"
#include "game/systems/stagger_system.h"
#include "game/systems/love_bond_system.h"
#include "game/systems/combat_mediator.h"

// Level 5: 高レベルシステム
#include "game/systems/bind_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/friends_damage_sharing.h"
#include "game/systems/wave_manager.h"

// Level 6: UI・エンティティ
#include "game/entities/arrow_manager.h"
#include "game/ui/radial_menu.h"
#include "game/systems/bond_type_selector.h"

#include "common/logging/logging.h"

//----------------------------------------------------------------------------
void SystemManager::CreateAll()
{
    if (created_) return;

    LOG_INFO("[SystemManager] Creating all systems...");

    // Level 1: 基盤システム（他の全てが依存）
    EventBus::Create();
    TimeManager::Create();

    // Level 2: 基本システム
    GroupManager::Create();
    FESystem::Create();
    BondManager::Create();
    FactionManager::Create();
    InsulationSystem::Create();
    GameStateManager::Create();
    StageProgressManager::Create();

    // Level 3: 関係性システム（BondManager等に依存）
    RelationshipFacade::Create();
    RelationshipContext::Create();

    // Level 4: 戦闘関連（RelationshipFacade, TimeManager等に依存）
    CutSystem::Create();
    StaggerSystem::Create();
    LoveBondSystem::Create();
    CombatMediator::Create();

    // Level 5: 高レベルシステム（Level 4に依存）
    BindSystem::Create();
    CombatSystem::Create();
    FriendsDamageSharing::Create();
    WaveManager::Create();

    // Level 6: UI・エンティティ
    ArrowManager::Create();
    RadialMenu::Create();
    BondTypeSelector::Create();

    created_ = true;
    LOG_INFO("[SystemManager] All systems created");
}

//----------------------------------------------------------------------------
void SystemManager::DestroyAll()
{
    if (!created_) return;

    LOG_INFO("[SystemManager] Destroying all systems...");

    // Level 6: UI・エンティティ（逆順）
    BondTypeSelector::Destroy();
    RadialMenu::Destroy();
    ArrowManager::Destroy();

    // Level 5: 高レベルシステム
    WaveManager::Destroy();
    FriendsDamageSharing::Destroy();
    CombatSystem::Destroy();
    BindSystem::Destroy();

    // Level 4: 戦闘関連
    CombatMediator::Destroy();
    LoveBondSystem::Destroy();
    StaggerSystem::Destroy();
    CutSystem::Destroy();

    // Level 3: 関係性システム
    RelationshipContext::Destroy();
    RelationshipFacade::Destroy();

    // Level 2: 基本システム
    StageProgressManager::Destroy();
    GameStateManager::Destroy();
    InsulationSystem::Destroy();
    FactionManager::Destroy();
    BondManager::Destroy();
    FESystem::Destroy();
    GroupManager::Destroy();

    // Level 1: 基盤システム（最後に破棄）
    TimeManager::Destroy();
    EventBus::Destroy();

    created_ = false;
    LOG_INFO("[SystemManager] All systems destroyed");
}

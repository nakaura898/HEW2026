//----------------------------------------------------------------------------
//! @file   game_state_manager.cpp
//! @brief  ゲーム状態管理実装
//----------------------------------------------------------------------------
#include "game_state_manager.h"
#include "game/entities/group.h"
#include "game/entities/player.h"
#include "game/bond/bond_manager.h"
#include "game/systems/wave_manager.h"
#include "game/systems/group_manager.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
GameStateManager& GameStateManager::Get()
{
    assert(instance_ && "GameStateManager::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void GameStateManager::Create()
{
    if (!instance_) {
        instance_.reset(new GameStateManager());
    }
}

//----------------------------------------------------------------------------
void GameStateManager::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
void GameStateManager::Initialize()
{
    state_ = GameState::Playing;
    LOG_INFO("[GameStateManager] Game initialized");
}

//----------------------------------------------------------------------------
void GameStateManager::Update()
{
    if (state_ != GameState::Playing) return;

    // 敗北チェック（プレイヤーHP0）
    if (CheckDefeatCondition()) {
        SetState(GameState::Defeat);
        return;
    }

    // 勝利チェック
    if (CheckVictoryCondition()) {
        SetState(GameState::Victory);
        return;
    }
}

//----------------------------------------------------------------------------
void GameStateManager::Reset()
{
    state_ = GameState::Playing;
    player_ = nullptr;
    LOG_INFO("[GameStateManager] Game reset");
}

//----------------------------------------------------------------------------
bool GameStateManager::CheckVictoryCondition() const
{
    // ウェーブシステムが有効な場合、全ウェーブクリアが必要
    if (WaveManager::Get().GetTotalWaves() > 0) {
        // 全ウェーブクリア + トランジション中でない
        if (!WaveManager::Get().IsAllWavesCleared()) {
            return false;
        }
        if (WaveManager::Get().IsTransitioning()) {
            return false;
        }
    }

    // 条件1: 全敵全滅
    if (AreAllEnemiesDefeated()) {
        return true;
    }

    // 条件2: 全生存敵がプレイヤーネットワーク内
    if (AreAllEnemiesInPlayerNetwork()) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------------
bool GameStateManager::CheckDefeatCondition() const
{
    if (!player_) return false;

    return !player_->IsAlive();
}

//----------------------------------------------------------------------------
void GameStateManager::SetState(GameState state)
{
    if (state_ == state) return;

    state_ = state;

    // 結果を保存（Result_Scene用）
    if (state == GameState::Victory || state == GameState::Defeat) {
        lastResult_ = state;
    }

    switch (state) {
    case GameState::Victory:
        LOG_INFO("[GameStateManager] VICTORY!");
        if (onVictory_) {
            onVictory_();
        }
        break;

    case GameState::Defeat:
        LOG_INFO("[GameStateManager] DEFEAT!");
        if (onDefeat_) {
            onDefeat_();
        }
        break;

    default:
        break;
    }

    if (onStateChanged_) {
        onStateChanged_(state);
    }
}

//----------------------------------------------------------------------------
bool GameStateManager::AreAllEnemiesDefeated() const
{
    std::vector<Group*> enemyGroups = GroupManager::Get().GetEnemyGroups();
    if (enemyGroups.empty()) return true;

    for (Group* group : enemyGroups) {
        if (!group) continue;
        // GetEnemyGroups()は既にIsEnemy()==trueのみ返すので、IsAllyチェックは不要
        if (!group->IsDefeated()) {
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------
bool GameStateManager::AreAllEnemiesInPlayerNetwork() const
{
    std::vector<Group*> enemyGroups = GroupManager::Get().GetEnemyGroups();
    if (!player_ || enemyGroups.empty()) return false;

    // プレイヤーの縁ネットワークを取得
    BondableEntity playerEntity = player_;
    std::vector<BondableEntity> network = BondManager::Get().GetConnectedNetwork(playerEntity);

    // 全生存敵がネットワーク内にいるかチェック
    for (Group* group : enemyGroups) {
        if (!group || group->IsDefeated()) continue;
        // GetEnemyGroups()は既にIsEnemy()==trueのみ返すので、IsAllyチェックは不要

        BondableEntity groupEntity = group;
        bool inNetwork = false;

        for (const BondableEntity& entity : network) {
            if (BondableHelper::IsSame(entity, groupEntity)) {
                inNetwork = true;
                break;
            }
        }

        if (!inNetwork) {
            return false;
        }
    }

    return true;
}

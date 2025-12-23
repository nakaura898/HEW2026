//----------------------------------------------------------------------------
//! @file   game_state_manager.cpp
//! @brief  ゲーム状態管理実装
//----------------------------------------------------------------------------
#include "game_state_manager.h"
#include "game/entities/group.h"
#include "game/entities/player.h"
#include "game/bond/bond_manager.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
GameStateManager& GameStateManager::Get()
{
    static GameStateManager instance;
    return instance;
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
    enemyGroups_.clear();
    player_ = nullptr;
    LOG_INFO("[GameStateManager] Game reset");
}

//----------------------------------------------------------------------------
void GameStateManager::RegisterEnemyGroup(Group* group)
{
    if (!group) return;

    auto it = std::find(enemyGroups_.begin(), enemyGroups_.end(), group);
    if (it == enemyGroups_.end()) {
        enemyGroups_.push_back(group);
    }
}

//----------------------------------------------------------------------------
void GameStateManager::UnregisterEnemyGroup(Group* group)
{
    auto it = std::find(enemyGroups_.begin(), enemyGroups_.end(), group);
    if (it != enemyGroups_.end()) {
        enemyGroups_.erase(it);
    }
}

//----------------------------------------------------------------------------
void GameStateManager::ClearEnemyGroups()
{
    enemyGroups_.clear();
}

//----------------------------------------------------------------------------
bool GameStateManager::CheckVictoryCondition() const
{
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
    if (enemyGroups_.empty()) return true;

    for (Group* group : enemyGroups_) {
        if (group && !group->IsDefeated()) {
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------
bool GameStateManager::AreAllEnemiesInPlayerNetwork() const
{
    if (!player_ || enemyGroups_.empty()) return false;

    // プレイヤーの縁ネットワークを取得
    BondableEntity playerEntity = player_;
    std::vector<BondableEntity> network = BondManager::Get().GetConnectedNetwork(playerEntity);

    // 全生存敵がネットワーク内にいるかチェック
    for (Group* group : enemyGroups_) {
        if (!group || group->IsDefeated()) continue;

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

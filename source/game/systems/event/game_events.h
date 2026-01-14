//----------------------------------------------------------------------------
//! @file   game_events.h
//! @brief  ゲームイベント定義
//----------------------------------------------------------------------------
#pragma once

#include "game/bond/bond.h"
#include "game/bond/bondable_entity.h"

// 前方宣言
class Player;
class Group;
class Individual;
class Bond;
enum class AIState;

//============================================================================
// モード関連イベント
//============================================================================

//! @brief 結モード変更イベント
struct BindModeChangedEvent
{
    bool enabled;  //!< 有効か
};

//! @brief 切モード変更イベント
struct CutModeChangedEvent
{
    bool enabled;  //!< 有効か
};

//! @brief エンティティマークイベント
struct EntityMarkedEvent
{
    BondableEntity entity;  //!< マークされたエンティティ
};

//! @brief マークキャンセルイベント
struct MarkCancelledEvent
{
};

//! @brief 縁タイプ選択イベント
struct BondTypeSelectedEvent
{
    BondType newType;  //!< 選択されたタイプ
};

//============================================================================
// 縁関連イベント
//============================================================================

//! @brief 縁作成イベント
struct BondCreatedEvent
{
    BondableEntity entityA;
    BondableEntity entityB;
    Bond* bond;
};

//! @brief 縁削除イベント
struct BondRemovedEvent
{
    BondableEntity entityA;
    BondableEntity entityB;
};

//! @brief 縁切断予定イベント
struct BondMarkedForCutEvent
{
    Bond* bond;
};

//============================================================================
// ダメージ関連イベント
//============================================================================

//! @brief ダメージ発生イベント
struct DamageDealtEvent
{
    Individual* attacker;  //!< 攻撃者（nullptrの場合あり）
    Individual* target;    //!< ターゲット
    float damage;          //!< ダメージ量
};

//! @brief プレイヤー被弾イベント
struct PlayerDamagedEvent
{
    Player* player;
    float damage;
    float remainingHp;
};

//============================================================================
// 死亡関連イベント
//============================================================================

//! @brief 個体死亡イベント
struct IndividualDiedEvent
{
    Individual* individual;
    Group* ownerGroup;
};

//! @brief グループ全滅イベント
struct GroupDefeatedEvent
{
    Group* group;
};

//! @brief プレイヤー死亡イベント
struct PlayerDiedEvent
{
    Player* player;
};

//============================================================================
// 硬直関連イベント
//============================================================================

//! @brief 硬直付与イベント
struct StaggerAppliedEvent
{
    BondableEntity entity;
    float duration;
};

//! @brief 硬直解除イベント
struct StaggerRemovedEvent
{
    BondableEntity entity;
};

//============================================================================
// その他イベント
//============================================================================

//! @brief 脅威度変化イベント
struct ThreatChangedEvent
{
    Group* group;
    float oldThreat;
    float newThreat;
};

//! @brief FE変化イベント
struct FEChangedEvent
{
    Player* player;
    float oldFE;
    float newFE;
};

//! @brief ゲーム終了イベント
struct GameOverEvent
{
    bool isVictory;
};

//! @brief グループ味方化イベント
struct GroupBecameAllyEvent
{
    Group* group;  //!< 味方化したグループ
};

//============================================================================
// AI状態関連イベント
//============================================================================

//! @brief AI状態変更イベント
struct AIStateChangedEvent
{
    Group* group;      //!< 対象グループ
    AIState newState;  //!< 新しい状態
};

//! @brief Love追従状態変更イベント
struct LoveFollowingChangedEvent
{
    Group* group;      //!< 対象グループ
    bool isFollowing;  //!< 追従中かどうか
};

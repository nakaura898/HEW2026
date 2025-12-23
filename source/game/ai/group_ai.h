//----------------------------------------------------------------------------
//! @file   group_ai.h
//! @brief  グループAI - グループの行動を制御
//----------------------------------------------------------------------------
#pragma once

#include "engine/math/math_types.h"
#include <functional>
#include <variant>

// 前方宣言
class Group;
class Player;
class Camera2D;

//----------------------------------------------------------------------------
//! @brief ターゲット種別（Group または Player）
//----------------------------------------------------------------------------
using AITarget = std::variant<std::monostate, Group*, Player*>;

//----------------------------------------------------------------------------
//! @brief AI状態
//----------------------------------------------------------------------------
enum class AIState
{
    Wander,     //!< 徘徊（平和時）
    Seek,       //!< 追跡（戦闘時）
    Flee        //!< 逃走（HP低下時）
};

//----------------------------------------------------------------------------
//! @brief グループAI
//! @details グループの行動（Wander/Seek/Flee）を制御する
//----------------------------------------------------------------------------
class GroupAI
{
public:
    //! @brief コンストラクタ
    //! @param owner 所有するGroup
    explicit GroupAI(Group* owner);

    //------------------------------------------------------------------------
    // 更新
    //------------------------------------------------------------------------

    //! @brief AIを更新
    //! @param dt デルタタイム
    void Update(float dt);

    //------------------------------------------------------------------------
    // 状態制御
    //------------------------------------------------------------------------

    //! @brief 現在の状態を取得
    [[nodiscard]] AIState GetState() const { return state_; }

    //! @brief 状態を強制設定
    void SetState(AIState state);

    //! @brief 戦闘状態に移行
    void EnterCombat();

    //! @brief 平和状態に移行
    void ExitCombat();

    //------------------------------------------------------------------------
    // ターゲット管理
    //------------------------------------------------------------------------

    //! @brief 攻撃ターゲットを設定（Group）
    void SetTarget(Group* target);

    //! @brief 攻撃ターゲットを設定（Player）
    void SetTargetPlayer(Player* target);

    //! @brief 攻撃ターゲットを取得
    [[nodiscard]] AITarget GetTarget() const { return target_; }

    //! @brief ターゲットがいるか判定
    [[nodiscard]] bool HasTarget() const;

    //! @brief ターゲットをクリア
    void ClearTarget();

    //! @brief プレイヤー参照を設定（Flee時の逃走方向計算用）
    void SetPlayer(Player* player) { player_ = player; }

    //! @brief カメラ参照を設定（Flee時の可視判定用）
    void SetCamera(Camera2D* camera) { camera_ = camera; }

    //! @brief ターゲットを自動選択
    void FindTarget();

    //------------------------------------------------------------------------
    // パラメータ
    //------------------------------------------------------------------------

    //! @brief 逃走開始HP閾値を取得
    [[nodiscard]] float GetFleeThreshold() const { return fleeThreshold_; }

    //! @brief 逃走開始HP閾値を設定
    void SetFleeThreshold(float threshold) { fleeThreshold_ = threshold; }

    //! @brief 移動速度を取得
    [[nodiscard]] float GetMoveSpeed() const { return moveSpeed_; }

    //! @brief 移動速度を設定
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    //! @brief 索敵範囲を取得
    [[nodiscard]] float GetDetectionRange() const { return detectionRange_; }

    //! @brief 索敵範囲を設定
    void SetDetectionRange(float range) { detectionRange_ = range; }

    //! @brief 逃走速度倍率を取得
    [[nodiscard]] float GetFleeSpeedMultiplier() const { return fleeSpeedMultiplier_; }

    //! @brief 逃走速度倍率を設定
    void SetFleeSpeedMultiplier(float multiplier) { fleeSpeedMultiplier_ = multiplier; }

    //! @brief 逃走停止距離を取得（プレイヤーにこれ以上近づかない）
    [[nodiscard]] float GetFleeStopDistance() const { return fleeStopDistance_; }

    //! @brief 逃走停止距離を設定
    void SetFleeStopDistance(float distance) { fleeStopDistance_ = distance; }

    //! @brief 徘徊目標を設定（ラブ同期用）
    void SetWanderTarget(const Vector2& target) { wanderTarget_ = target; wanderTimer_ = 0.0f; }

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 状態変更時コールバック
    void SetOnStateChanged(std::function<void(AIState)> callback) { onStateChanged_ = callback; }

private:
    //! @brief Wander状態の更新
    void UpdateWander(float dt);

    //! @brief Seek状態の更新
    void UpdateSeek(float dt);

    //! @brief Flee状態の更新
    void UpdateFlee(float dt);

    //! @brief 状態遷移チェック
    void CheckStateTransition();

    //! @brief 新しい徘徊目標を設定
    void SetNewWanderTarget();

    //! @brief ターゲットの位置を取得
    [[nodiscard]] Vector2 GetTargetPosition() const;

    //! @brief ターゲットが有効か判定（生存しているか）
    [[nodiscard]] bool IsTargetValid() const;

    Group* owner_ = nullptr;        //!< 所有Group
    AITarget target_;               //!< 攻撃ターゲット（Group* or Player*）
    Player* player_ = nullptr;      //!< プレイヤー参照（Flee時の逃走方向）
    Camera2D* camera_ = nullptr;    //!< カメラ参照（Flee時の可視判定）

    AIState state_ = AIState::Wander;   //!< 現在の状態
    bool inCombat_ = false;             //!< 戦闘中フラグ

    // Wanderパラメータ
    Vector2 wanderTarget_;              //!< 徘徊目標位置
    float wanderTimer_ = 0.0f;          //!< 徘徊タイマー
    float wanderInterval_ = 3.0f;       //!< 目標変更間隔

    // 共通パラメータ
    float moveSpeed_ = 100.0f;          //!< 移動速度
    float detectionRange_ = 300.0f;     //!< 索敵範囲
    float fleeThreshold_ = 0.6f;        //!< 逃走開始HP閾値（60%）
    float wanderRadius_ = 150.0f;       //!< 徘徊半径
    float fleeSpeedMultiplier_ = 1.2f;  //!< 逃走速度倍率（通常より速い）
    float fleeStopDistance_ = 80.0f;    //!< 逃走停止距離（プレイヤーに近すぎたら止まる）

    std::function<void(AIState)> onStateChanged_;
};

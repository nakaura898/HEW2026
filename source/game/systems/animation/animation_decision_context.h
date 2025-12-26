//----------------------------------------------------------------------------
//! @file   animation_decision_context.h
//! @brief  アニメーション判定コンテキスト
//----------------------------------------------------------------------------
#pragma once

#include "engine/math/math_types.h"
#include "anim_state.h"
#include <vector>
#include <string>

// 前方宣言
class Individual;
class Player;
enum class AIState;

//----------------------------------------------------------------------------
//! @brief アニメーション判定に必要な全コンテキスト
//! @details 毎フレームIndividualが構築し、StateMachineに渡す。
//!          「個人の状態」と「関係性」を統合した判定データ。
//----------------------------------------------------------------------------
struct AnimationDecisionContext
{
    //------------------------------------------------------------------------
    // 個体状態
    //------------------------------------------------------------------------
    Vector2 velocity = Vector2::Zero;           //!< 実効速度 (desiredVelocity + separationOffset)
    Vector2 desiredVelocity = Vector2::Zero;    //!< 希望速度
    float distanceToSlot = 0.0f;                //!< フォーメーションスロットまでの距離
    bool isActuallyMoving = false;              //!< 前フレームで実際に移動したか

    //------------------------------------------------------------------------
    // グループ状態
    //------------------------------------------------------------------------
    bool isGroupMoving = false;                 //!< グループの移動フラグ
    AIState groupAIState{};                     //!< グループAIの状態
    Vector2 groupTargetPosition = Vector2::Zero;//!< グループの目標位置

    //------------------------------------------------------------------------
    // Loveクラスター状態（関係性）
    //------------------------------------------------------------------------
    bool isInLoveCluster = false;               //!< Love絆クラスターに属しているか
    bool isLoveClusterMoving = false;           //!< クラスターが移動中か
    Vector2 loveClusterCenter = Vector2::Zero;  //!< クラスターの中心位置
    float distanceToClusterCenter = 0.0f;       //!< クラスター中心までの距離

    //------------------------------------------------------------------------
    // 戦闘関係（関係性）
    //------------------------------------------------------------------------
    bool isAttacking = false;                   //!< 攻撃中か
    bool isUnderAttack = false;                 //!< 攻撃されているか
    Individual* attackTarget = nullptr;         //!< 攻撃対象（Individual）
    Player* playerTarget = nullptr;             //!< 攻撃対象（Player）
    Vector2 attackTargetPosition = Vector2::Zero; //!< 攻撃対象の位置
    std::vector<Individual*> attackers;         //!< 自分を攻撃している敵リスト

    //------------------------------------------------------------------------
    // 判定メソッド
    //------------------------------------------------------------------------

    //! @brief Walk状態にすべきか判定
    //! @return Walk状態にすべきならtrue
    [[nodiscard]] bool ShouldWalk() const;

    //! @brief 向くべき方向を取得
    //! @return 向くべき方向（正規化されていない場合あり）
    [[nodiscard]] Vector2 GetFacingDirection() const;

    //------------------------------------------------------------------------
    // デバッグ
    //------------------------------------------------------------------------

    //! @brief コンテキストを文字列化
    //! @return デバッグ用文字列
    [[nodiscard]] std::string ToString() const;

private:
    //! @brief 速度がこの値より大きければ移動中と判定
    static constexpr float kVelocityEpsilon = 2.0f;

    //! @brief スロット距離がこの値より大きければ移動すべき
    static constexpr float kSlotThreshold = 5.0f;
};

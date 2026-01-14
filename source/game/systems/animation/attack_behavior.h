//----------------------------------------------------------------------------
//! @file   attack_behavior.h
//! @brief  攻撃行動インターフェース
//----------------------------------------------------------------------------
#pragma once

#include "anim_state.h"
#include "engine/math/math_types.h"

// 前方宣言
class Individual;
class Player;

//----------------------------------------------------------------------------
//! @brief 攻撃行動インターフェース
//! @details 派生クラスごとの攻撃実装を抽象化（Strategy パターン）
//----------------------------------------------------------------------------
class IAttackBehavior
{
public:
    //! @brief デストラクタ
    virtual ~IAttackBehavior() = default;

    //------------------------------------------------------------------------
    // ライフサイクルコールバック
    //------------------------------------------------------------------------

    //! @brief 攻撃開始時の処理（Individual対象）
    //! @param attacker 攻撃者
    //! @param target 攻撃対象
    virtual void OnAttackStart(Individual* attacker, Individual* target) = 0;

    //! @brief 攻撃開始時の処理（Player対象）
    //! @param attacker 攻撃者
    //! @param target 攻撃対象プレイヤー
    virtual void OnAttackStartPlayer(Individual* attacker, Player* target) = 0;

    //! @brief 攻撃更新（毎フレーム呼ばれる）
    //! @param dt デルタタイム
    //! @param phase 現在の攻撃フェーズ
    //! @param phaseTime 現在フェーズの経過時間
    virtual void OnAttackUpdate(float dt, AnimState phase, float phaseTime) = 0;

    //! @brief ダメージフレーム到達時の処理
    //! @return ダメージが適用されたらtrue
    virtual bool OnDamageFrame() = 0;

    //! @brief 攻撃終了時の処理
    virtual void OnAttackEnd() = 0;

    //! @brief 攻撃中断時の処理
    virtual void OnAttackInterrupt() = 0;

    //------------------------------------------------------------------------
    // フェーズ時間設定
    //------------------------------------------------------------------------

    //! @brief 予備動作の持続時間を取得
    [[nodiscard]] virtual float GetWindupDuration() const = 0;

    //! @brief 攻撃実行の持続時間を取得
    [[nodiscard]] virtual float GetActiveDuration() const = 0;

    //! @brief 後隙の持続時間を取得
    [[nodiscard]] virtual float GetRecoveryDuration() const = 0;

    //! @brief ダメージフレームのタイミングを取得（Active開始からの時間）
    [[nodiscard]] virtual float GetDamageFrameTime() const = 0;

    //------------------------------------------------------------------------
    // 状態取得
    //------------------------------------------------------------------------

    //! @brief 現在の攻撃対象を取得
    [[nodiscard]] virtual Individual* GetTarget() const = 0;

    //! @brief 現在の攻撃対象（プレイヤー）を取得
    [[nodiscard]] virtual Player* GetPlayerTarget() const = 0;

    //! @brief 攻撃対象の位置を取得
    //! @param outPosition 位置出力
    //! @return 対象が存在すればtrue
    [[nodiscard]] virtual bool GetTargetPosition(Vector2& outPosition) const = 0;
};

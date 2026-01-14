//----------------------------------------------------------------------------
//! @file   ranged_attack_behavior.h
//! @brief  遠距離攻撃行動（Elf用）
//----------------------------------------------------------------------------
#pragma once

#include "attack_behavior.h"

// 前方宣言
class Elf;
class ArrowManager;

//----------------------------------------------------------------------------
//! @brief 遠距離攻撃行動（Elf用）
//! @details 矢発射ロジックを管理
//----------------------------------------------------------------------------
class RangedAttackBehavior : public IAttackBehavior
{
public:
    //! @brief コンストラクタ
    //! @param owner 所有者（Elf）
    explicit RangedAttackBehavior(Elf* owner);

    //! @brief デストラクタ
    ~RangedAttackBehavior() override = default;

    //------------------------------------------------------------------------
    // IAttackBehavior実装
    //------------------------------------------------------------------------

    void OnAttackStart(Individual* attacker, Individual* target) override;
    void OnAttackStartPlayer(Individual* attacker, Player* target) override;
    void OnAttackUpdate(float dt, AnimState phase, float phaseTime) override;
    bool OnDamageFrame() override;
    void OnAttackEnd() override;
    void OnAttackInterrupt() override;

    [[nodiscard]] float GetWindupDuration() const override;
    [[nodiscard]] float GetActiveDuration() const override;
    [[nodiscard]] float GetRecoveryDuration() const override;
    [[nodiscard]] float GetDamageFrameTime() const override;

    [[nodiscard]] Individual* GetTarget() const override { return pendingTarget_; }
    [[nodiscard]] Player* GetPlayerTarget() const override { return pendingTargetPlayer_; }
    [[nodiscard]] bool GetTargetPosition(Vector2& outPosition) const override;

private:
    //! @brief 矢を発射
    void ShootArrow();

    Elf* owner_ = nullptr;
    Individual* pendingTarget_ = nullptr;
    Player* pendingTargetPlayer_ = nullptr;
    bool arrowShot_ = false;

    // アニメーション設定（8Fフレーム間隔、3フレーム）
    static constexpr float kFrameInterval = 8.0f / 60.0f;  //!< 1フレームあたりの時間
    static constexpr int kShootFrame = 1;                   //!< 矢発射フレーム
    static constexpr int kAttackFrames = 3;                 //!< 攻撃アニメーションのフレーム数
};

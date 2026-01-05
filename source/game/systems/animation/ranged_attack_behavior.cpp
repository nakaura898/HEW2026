//----------------------------------------------------------------------------
//! @file   ranged_attack_behavior.cpp
//! @brief  遠距離攻撃行動（Elf用）実装
//----------------------------------------------------------------------------
#include "ranged_attack_behavior.h"
#include "game/entities/elf.h"
#include "game/entities/player.h"
#include "game/entities/arrow_manager.h"
#include "game/systems/relationship_context.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
RangedAttackBehavior::RangedAttackBehavior(Elf* owner)
    : owner_(owner)
{
}

//----------------------------------------------------------------------------
void RangedAttackBehavior::OnAttackStart(Individual* attacker, Individual* target)
{
    pendingTarget_ = target;
    pendingTargetPlayer_ = nullptr;
    arrowShot_ = false;

    // 関係レジストリに攻撃関係を登録
    if (attacker && target) {
        RelationshipContext::Get().RegisterAttack(attacker, target);
    }
}

//----------------------------------------------------------------------------
void RangedAttackBehavior::OnAttackStartPlayer(Individual* attacker, Player* target)
{
    pendingTarget_ = nullptr;
    pendingTargetPlayer_ = target;
    arrowShot_ = false;

    // 関係レジストリに攻撃関係を登録
    if (attacker && target) {
        RelationshipContext::Get().RegisterAttackPlayer(attacker, target);
    }
}

//----------------------------------------------------------------------------
void RangedAttackBehavior::OnAttackUpdate(float /*dt*/, AnimState /*phase*/, float /*phaseTime*/)
{
    // 更新処理は特になし（ダメージフレームでの処理のみ）
}

//----------------------------------------------------------------------------
bool RangedAttackBehavior::OnDamageFrame()
{
    if (arrowShot_) {
        return false;  // 既に発射済み
    }

    ShootArrow();
    return arrowShot_;
}

//----------------------------------------------------------------------------
void RangedAttackBehavior::OnAttackEnd()
{
    // 関係レジストリから攻撃関係を解除
    if (owner_) {
        RelationshipContext::Get().UnregisterAttack(owner_);
    }

    pendingTarget_ = nullptr;
    pendingTargetPlayer_ = nullptr;
    arrowShot_ = false;
}

//----------------------------------------------------------------------------
void RangedAttackBehavior::OnAttackInterrupt()
{
    // 中断時も同様にクリア
    OnAttackEnd();
}

//----------------------------------------------------------------------------
float RangedAttackBehavior::GetWindupDuration() const
{
    // Elfは予備動作なし
    return 0.0f;
}

//----------------------------------------------------------------------------
float RangedAttackBehavior::GetActiveDuration() const
{
    // 3フレーム × 8F間隔
    return kFrameInterval * kAttackFrames;
}

//----------------------------------------------------------------------------
float RangedAttackBehavior::GetRecoveryDuration() const
{
    // 後隙
    return 0.2f;
}

//----------------------------------------------------------------------------
float RangedAttackBehavior::GetDamageFrameTime() const
{
    // フレーム1（0-indexed）で発射
    return kFrameInterval * kShootFrame;
}

//----------------------------------------------------------------------------
bool RangedAttackBehavior::GetTargetPosition(Vector2& outPosition) const
{
    if (pendingTarget_ && pendingTarget_->IsAlive()) {
        outPosition = pendingTarget_->GetPosition();
        return true;
    }

    if (pendingTargetPlayer_ && pendingTargetPlayer_->IsAlive()) {
        outPosition = pendingTargetPlayer_->GetPosition();
        return true;
    }

    return false;
}

//----------------------------------------------------------------------------
void RangedAttackBehavior::ShootArrow()
{
    if (!owner_) return;

    Vector2 startPos = owner_->GetPosition();
    float damage = owner_->GetAttackDamage();

    if (pendingTarget_ && pendingTarget_->IsAlive()) {
        ArrowManager::Get().Shoot(owner_, pendingTarget_, startPos, damage);
        arrowShot_ = true;
    } else if (pendingTargetPlayer_ && pendingTargetPlayer_->IsAlive()) {
        ArrowManager::Get().ShootAtPlayer(owner_, pendingTargetPlayer_, startPos, damage);
        arrowShot_ = true;
    }
}
